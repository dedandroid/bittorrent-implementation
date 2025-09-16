#include <stdio.h>
#include <stdlib.h>
#include "net/packet.h"
#include "net/config.h"
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "net/commands.h"
#include "chk/pkgchk.h"
#include "net/peers.h"
#include <pthread.h>
#include "net/packages.h"

#define MAX_COMMAND_SIZE (5520)
#define INITIAL_PACKAGE_ARRAY_SIZE (2)


//combine functionality of connect and disconnect for code reusability.
int connect_or_disconnect_to_client(struct config* config_file, char* arguments, int disconnect_flag, 
                                    struct arrays * req_arrays, struct locks * req_locks){
    
    //parse the argument:
    char* ip_check = strtok(arguments, ":");
    char* port = strtok(NULL, ":");
    // printf("IP: %s, port: %s\n", ip_check, port);
    if ( ip_check == NULL || port == NULL ){
        printf("Missing address and port argument\n");
        return -1;
    }
    //attempt to convert port to number. Also do a check here to make sure this is a numeric string
    if (!is_an_int(port)){
        printf("Not an integer.\n");
        return 0;
    }
    int port_num = atoi(port);
    errno = 0;
    if (errno){
        printf("Failed to convert %s to int\n", port);
        return -1;
    }
    if (disconnect_flag){
        return peer_disconnect(ip_check, port_num, config_file->max_peers, req_arrays, req_locks);
    } else {
        char* ip = malloc(strlen(ip_check)+1);
        strcpy(ip, ip_check);
        return peer_connect(ip, port_num, config_file->max_peers, req_arrays, req_locks);
    }
}

//Fetch parsing and managing.
int fetch_parser(struct config* config_file, char* arguments, struct managed_packages* packages,
                struct arrays* req_arrays, struct locks* all_locks){

    struct bpkg_obj * chosen_bpkg = NULL;
    
    //Separate the arguments.
    char* peer = strtok(arguments, " ");
    char peer_buffer[strlen(peer) +1];
    strcpy(peer_buffer, peer);

    char* ip = strtok(peer_buffer, ":");
    char* port = strtok(NULL, "");

    char* resume_ptr = arguments + strlen(arguments) + 1;
    char* ident = strtok(resume_ptr, " ");
    char* chunk_hash = strtok(NULL, " ");
    char* offset = strtok(NULL, " ");

    // printf("\npeer: %s\n", peer);
    // printf("IP: %s\n", ip);
    // printf("port: %s\n", port);
    // printf("ident: %s\n", ident);
    // printf("chunk hash: %s\n", chunk_hash);
    // printf("Offset: %s\n", offset);

    if ( ip == NULL || port == NULL || ident == NULL || chunk_hash == NULL ){
        printf("Missing arguments from command\n");
        return 0;
    }
    if (!is_an_int(port)){
        printf("Port is not an integer.\n");
        return 0;
    }
    int port_num = atoi(port);
    unsigned int offset_num = 0;
    errno = 0;
    if (errno){
        printf("Failed to convert %s to int\n", port);
        return -1;
    }
    //Make sure peer connection exists.
    if (!in_array(ip, port_num, req_arrays->peer_array, config_file->max_peers, all_locks->peer_array_lock)){
        printf("Unable to request chunk, peer not in list\n");
        return 0;
    }

    int ret_val = chunk_in_memory(packages, ident, chunk_hash, &chosen_bpkg, all_locks->managed_packages_lock);

    if (ret_val == -2){
        printf("Unable to request chunk, package is not managed\n");
        return 0;
    } if (ret_val == -1){
        printf("Unable to request chunk, chunk hash does not belong to package\n");
        return 0;
    }

    //CHECK IF PORT AND OFFSET ARE NUMERIC
    
    if (offset){
        if (!is_an_int(offset)){
            printf("Offset is not an integer.\n");
            return 0;
        }
        offset_num = atoi(offset);
        errno = 0;
        if (errno){
            printf("Failed to convert %s to int\n", port);
            return -1;
        }
        if (offset_num < 0){
            printf("Invalid offset\n");
        }
    }
    
    struct req_args * req_args = NULL;
    if (offset){
        req_args = calculate_data_len(chosen_bpkg, chunk_hash, &offset_num, all_locks->managed_packages_lock);
    } else {
        req_args = calculate_data_len(chosen_bpkg, chunk_hash, NULL, all_locks->managed_packages_lock);
    }
    if (req_args){
        printf("Data length is %d\n", req_args->data_len);
        printf("Offset in file is %d\n", req_args->file_offset);
        req_args->ident = ident;
        req_args->chunk_hash = chunk_hash;
    } else {
        printf("Failed!\n");
        return 0;
    }
    int fd = get_fd(ip, port_num, req_arrays->peer_array, config_file->max_peers, all_locks->peer_array_lock);
    //Now we can send the REQ packet.
    send_req(ip, port_num, fd, req_args);
    
    return 1;
}


int main(int argc, char** argv) {
    char buffer[MAX_COMMAND_SIZE] = {0};
    if (argc != 2){
        printf("Missing config file.\n");
        exit(1);
    }

    struct config * config_file = config_load(argv[1]);

    // printf("Max peers: %d\n", config_file->max_peers);
    // printf("ports: %d\n\n", config_file->port);

    //This thread always runs in parallel with main.
    pthread_t listener_thread;
    //Create the array of threads:
    pthread_t * threadIDs = (pthread_t *) malloc( sizeof(pthread_t)*config_file->max_peers);
    pthread_mutex_t * thread_array_lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(thread_array_lock, NULL);

    //An array of each thread's status.
    int * thread_status = (int*) malloc(sizeof(int)*config_file->max_peers);
    pthread_mutex_t * thread_status_lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(thread_status_lock, NULL);
    memset(thread_status, 0, sizeof(int)*config_file->max_peers);

    //Make peer list.
    struct peer ** peer_array = (struct peer **) malloc(sizeof(struct peer* )*config_file->max_peers);
    pthread_mutex_t * peer_array_lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(peer_array_lock, NULL);

    //Set all pointers to peers to NULL
    for (int k = 0; k<config_file->max_peers; k++){
        peer_array[k] = NULL;
    }

    //Array of managed packages. This is a dynamic array.
    struct bpkg_obj ** bpkg_array = (struct bpkg_obj **) malloc(sizeof(struct bpkg_obj * )*INITIAL_PACKAGE_ARRAY_SIZE);
    //Set all pointers to packages to NULL
    for (int k = 0; k<INITIAL_PACKAGE_ARRAY_SIZE; k++){
        bpkg_array[k] = NULL;
    }

    struct managed_packages * managed_packages = malloc(sizeof(struct managed_packages));
    managed_packages->managed_array = bpkg_array;
    managed_packages->size = 0;
    managed_packages->capacity = INITIAL_PACKAGE_ARRAY_SIZE;
    pthread_mutex_t * managed_packages_lock = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(managed_packages_lock, NULL);

    //put all locks in a neat struct:
    struct locks * all_locks = malloc(sizeof(struct locks));
    all_locks->peer_array_lock = peer_array_lock;
    all_locks->thread_array_lock = thread_array_lock;
    all_locks->thread_status_lock = thread_status_lock;
    all_locks->managed_packages_lock = managed_packages_lock;

    //Put all arrays into a neat structure:
    struct arrays * required_arrays = malloc(sizeof(struct arrays));
    required_arrays->peer_array = peer_array;
    required_arrays->thread_array = threadIDs;
    required_arrays->thread_status_array = thread_status;
    required_arrays->managed_packages = managed_packages;

    //listener arguments. On main stack which will always be here.
    struct listener_args args1 = { config_file, required_arrays, all_locks };

    // printf("Giving listener thread these args:\n");
    // printf("%s, port: %d, max peers: %d\n\n", config_file->directory, config_file->port, config_file->max_peers);

    //Open a constant listening socket:
    pthread_create(&listener_thread, NULL, &listener, &args1);

    
    //parse the commands
    while (fgets(buffer, MAX_COMMAND_SIZE, stdin)){
        char* key = NULL;
        char* value = NULL;

        key = strtok(buffer, " ");
        value = strtok(NULL, "");

        strip_whitespace(&key);
        strip_whitespace(&value);

        // printf("Key is: %s\n", key);
        // printf("Value is: %s\n", value);

        int ret = 0;


        if (strcmp(key, "CONNECT") == 0){
            ret = connect_or_disconnect_to_client(config_file, value, 0, required_arrays, all_locks);
            if (ret == 1){
                printf("Connection established with peer\n");
            }
            
        } else if (strcmp(key, "DISCONNECT") == 0){
            ret = connect_or_disconnect_to_client(config_file, value, 1, required_arrays, all_locks);
            if (ret == 1){
                printf("Disconnected from peer\n");
            }
            
        } else if (strcmp(key, "ADDPACKAGE") == 0){
            if (value == NULL){
                printf("Missing file argument\n");
            } else {
                add_package(config_file->directory, value, managed_packages, managed_packages_lock);
            }

        } else if (strcmp(key, "REMPACKAGE") == 0){
            if (value == NULL){
                printf("Missing identifier argument, please specify whole 1024 character or at least 20 characters.\n");
            } else {
                if (strlen(value) >= 20){
                    remove_package(managed_packages, value, managed_packages_lock);
                } else {
                    printf("Need more values to know.\n");
                }
            }
            
        } else if (strcmp(key, "PACKAGES") == 0){
            package_status(managed_packages, managed_packages_lock);

        } else if (strcmp(key, "PEERS") == 0){
            pthread_mutex_lock(peer_array_lock);
            int count = 0;
            int once = 1;
            for (int i = 0; i<config_file->max_peers; i++){
                if (peer_array[i] != NULL){
                    int fd = peer_array[i]->socket_fd;
                    // printf("Sending ping to %s:%d\n", peer_array[i]->ip, peer_array[i]->port);
                    struct btide_packet send_buffer = { PKT_MSG_PNG, 0, {{0}} };
                    int ret_val = send(fd, &send_buffer, MAX_PACKET_SIZE, 0);
                    if (ret_val <= 0) {
                        printf("could not write entire message to client: socket fd %d\n", fd);
                    }
                    count++;
                    if (once){
                        printf("Connected to:\n\n");
                        once = 0;
                    }
                    printf("%d. %s:%d\n", count, peer_array[i]->ip, peer_array[i]->port);
                }
            }
            if (count == 0)
                printf("Not connected to any peers\n");
            pthread_mutex_unlock(peer_array_lock);
            
        } else if (strcmp(key, "FETCH") == 0){
            fetch_parser(config_file, value, managed_packages, required_arrays, all_locks);

        } else if (strcmp(key, "QUIT") == 0){
            //Deallocate
            destroy_all(config_file, required_arrays, all_locks);
            exit(0);
        } else {
            printf("Invalid Input\n\n");
        }
    }
    destroy_all(config_file, required_arrays, all_locks);

}
