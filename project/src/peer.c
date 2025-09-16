#include "net/peers.h"
#include "net/packet.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <stdlib.h>
#include "chk/pkgchk.h"
#include <math.h>

#define NUM_OF_LOCKS (4);


int peer_connect(char* ip, int port, int max_peers, struct arrays * req_arrays, struct locks * all_locks){
    struct sockaddr_in serv_addr;
    int sock = 0;
    int already_in_array = in_array(ip, port, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);
    if (already_in_array){
        printf("Already connected to peer\n");
        free(ip);
        return -1;
    }
    
    // Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Unable to connect to request peer\n");
        free(ip);
        return -1;
    }

    // Configure server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("Unable to connect to request peer\n");
        free(ip);
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Unable to connect to request peer\n");
        free(ip);
        return -1;
    }

    struct btide_packet response = {0};
    ssize_t ret_val = 0;
    // printf("Connected to %s:%d\n", ip, port);

    //Wait for ACP signal back:
    ret_val = recv(sock, &response, MAX_PACKET_SIZE, MSG_WAITALL);
    // printf("Recieved: %d, %d\n", response.error, response.msg_code);

    if (ret_val <= 0){
        printf("Unable to connect to request peer\n");
        printf("recv ret val: %ld", ret_val);
        free(ip);
        close(sock);
        return -1;

    } else if (response.msg_code == PKT_MSG_ACP){
        //Send back ACK:
        struct btide_packet send_pack = { PKT_MSG_ACK, 0, { { 0 } } };
        ret_val = send(sock, &send_pack, MAX_PACKET_SIZE, 0);
        if (ret_val <= 0){
            printf("Unable to connect to request peer\n");
            // printf("send ret val: %ld", ret_val);
            free(ip);
            close(sock);
            return -1;
        }       

        //Not in the array
        int ret = add_to_array(ip, port, sock, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);
        if (ret == 0){
            //array is full
            printf("Unable to connect to request peer\n");
            free(ip);
            close(sock);
            return -1;
        } else if (ret == -1){
            //Malloc somehow failed
            printf("Unable to connect to request peer\n");
            free(ip);
            close(sock);
            return -1;
        } else {
            //SUCCES!!. Make thread.
            // printf("Success. Assigning thread.\n");
            assign_thread(max_peers, ip, port, sock, req_arrays, all_locks);
            return 1;
        }
    } else {
        printf("Unable to connect to request peer\n");
        free(ip);
        close(sock);
        return -1;
    }
}

//If listener fails, or if SIGINT is sent, deallocate all. This means handle SIGINT too!!
void destroy_all(struct config * config_file, struct arrays* req_arrays, struct locks* all_locks){
    free(config_file->directory);

    //Delete every peer pointed by peer_array's array of pointers, and their ip.
    for (int i = 0; i<config_file->max_peers; i++){
        if (req_arrays->peer_array[i]){
            free(req_arrays->peer_array[i]->ip);
            free(req_arrays->peer_array[i]);
        }
    }

    int managed_packages_capacity = req_arrays->managed_packages->capacity;
    for (int i = 0; i<managed_packages_capacity; i++){
        // printf("removing package %d\n", i);
        struct bpkg_obj * bpkg = req_arrays->managed_packages->managed_array[i];
        if (bpkg){
            // printf("Package Exists! it's %s\n", bpkg->filename);
            bpkg_obj_destroy(bpkg);
            req_arrays->managed_packages->managed_array[i] = NULL;
        }
    }

    //Free the locks
    free(all_locks->managed_packages_lock);
    free(all_locks->peer_array_lock);
    free(all_locks->thread_array_lock);
    free(all_locks->thread_status_lock);
    free(all_locks);


    free(req_arrays->managed_packages->managed_array);
    free(req_arrays->managed_packages);

    free(config_file);
    //Delete array of pointers
    free(req_arrays->peer_array);
    //Delete array of contiguous threads
    free(req_arrays->thread_array);
    //Delete array of contigious integers (Change to byte type later!!)
    free(req_arrays->thread_status_array);
    //Delete array of pointers to the other arrays.
    free(req_arrays);
    // printf("Destroy Done!\n");
}

//SEND the REQ packet:
void send_req(char* ip, int port, int fd, struct req_args * req_args){
    //Construct the packet payload:
    struct btide_packet packet = { PKT_MSG_REQ, 0, {{0}} };
    *((uint32_t *)(packet.pl.data)) = req_args->file_offset;
    *((uint32_t *)(packet.pl.data + sizeof(u_int32_t))) = req_args->data_len;

    memcpy(packet.pl.data+2*sizeof(u_int32_t), req_args->chunk_hash, CHUNK_HASH_SIZE);
    memcpy(packet.pl.data+2*sizeof(u_int32_t)+CHUNK_HASH_SIZE, req_args->ident, IDENT_SIZE);

    // printf("\npacket contains: \n");
    // printf("offset: %u\n", *((uint32_t *)(packet.pl.data)));
    // printf("data_len: %u\n", *((uint32_t *)(packet.pl.data+4)));
    // printf("hash %.64s\n", packet.pl.data + 8);
    // printf("ident %s\n", packet.pl.data +72);

    // printf("Data sent!\n");
    int ret_val = send(fd, &packet, MAX_PACKET_SIZE, 0);
    if (ret_val<= 0){
        printf("Error in sending the REQ packet.\n");
    }
    //char* variables are local to the stack of the calling function.
    //They will be removed when the calling function ends.
    free(req_args);

    return;
}

//Send the RES packet
void send_res(char* ip, int port, int fd, struct res_args * res_args, int error){
    struct btide_packet packet = { PKT_MSG_RES, 0, {{0}} };
    if (!error){
        //Construct the packet payload:
        size_t total_size = 0;
        // printf("Total size of packet: %lu\n", total_size);
        *((uint32_t *)(packet.pl.data)) = res_args->file_offset;
        total_size += sizeof(u_int32_t);
        // printf("Total size of packet: %lu\n", total_size);
        memcpy(packet.pl.data+total_size, res_args->data, DATA_MAX_SIZE);
        total_size += DATA_MAX_SIZE;
        // printf("Total size of packet: %lu\n", total_size);
        *((uint32_t *)(packet.pl.data + total_size)) = res_args->current_read;
        total_size += sizeof(u_int16_t);
        // printf("Total size of packet: %lu\n", total_size);
        memcpy(packet.pl.data + total_size, res_args->chunk_hash, CHUNK_HASH_SIZE);
        total_size += CHUNK_HASH_SIZE;
        // printf("Total size of packet: %lu\n", total_size);
        memcpy(packet.pl.data + total_size, res_args->ident, IDENT_SIZE);
        total_size += IDENT_SIZE;
        
        // printf("Total size of packet: %lu\n", total_size);
    } else {
        printf("oof!\n");
        packet.error = 1;
    }
    // printf("Sending!\n");
    int ret_val = send(fd, &packet, MAX_PACKET_SIZE, 0);
    if (ret_val<= 0){
        printf("Error in sending the REQ packet.\n");
    }

}


//INCOMING CONNECTIONS
void * listener(void * p){
    struct listener_args * args = (struct listener_args *) p;

    struct config * config_file = args->config_file;
    struct arrays* req_arrays = args->req_arrays;
    struct locks* all_locks = args->all_locks;

    // printf("Port: %d\n", config_file->port);

    //Should be constantly listening to accept any connections.
    // printf("Creating socket listener!\n");
    int server_fd = 0;
    struct sockaddr_in address;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        printf("Socket creation failed");
        destroy_all(config_file, req_arrays, all_locks);
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config_file->port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failed");
        destroy_all(config_file, req_arrays, all_locks);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        printf("Listen failed");
        destroy_all(config_file, req_arrays, all_locks);
        exit(EXIT_FAILURE);
    }
    // printf("Server listening on port %d...\n", config_file->port);

    //Constant listen:
    int new_socket;
    while (1) {
        // printf("waiting for connections\n");
        socklen_t addrlen = sizeof(struct sockaddr);

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            printf("Accept failed");
        }

        //Allocate ip address as a string in heap.
        char* ip = malloc(strlen(inet_ntoa(address.sin_addr)) + 1);
        strcpy(ip, inet_ntoa(address.sin_addr));
        uint16_t port = ntohs(address.sin_port);
        // printf("New connection, socket fd is %d, IP is : %s, port : %d\n", new_socket, ip, port);

        //Read the contents.
        struct btide_packet pkt = {0};
        int ret_val = 0;

        //Send back ACP:
        // printf("Sending ACP\n");
        struct btide_packet ret_pkt = { PKT_MSG_ACP, 0, { { 0 } } };
        ret_val = send(new_socket, &ret_pkt, MAX_PACKET_SIZE, 0);
        if (ret_val <= 0) {
            printf("could not write entire message to client: socket fd %d\n", new_socket);
            close(new_socket);
            new_socket = 0;
        }

        //Get back ACK:
        memset(&pkt, 0, sizeof(struct btide_packet));
        ret_val = recv(new_socket, &pkt, MAX_PACKET_SIZE, 0);
        if (ret_val <= 0){
            printf("Client disconnected: socket fd %d\n", new_socket);
            close(new_socket);
            new_socket = 0;
        }

        if (pkt.msg_code == PKT_MSG_ACK){
            // printf("ACK recieved.\n");
            //Not in the array
            int ret = add_to_array(ip, port, new_socket, req_arrays->peer_array, config_file->max_peers, all_locks->peer_array_lock);
            if (ret == 0){
                //array is full
                printf("Peer array is full!\n");
                close(new_socket);

            } else if (ret == -1){
                //Malloc somehow failed
                printf("Malloc failed!!\n");
                close(new_socket);

            } else {
                //SUCCES!!
                // printf("Connect successful, sending to another thread:\n");
                assign_thread(config_file->max_peers, ip, port, new_socket, req_arrays, all_locks);
            }
        }
    }
}




//We disconnect
int peer_disconnect(char* ip, int port, int max_peers, struct arrays * req_arrays, struct locks * all_locks){
    //Check if peer is even in the array:

    int already_in_array = in_array(ip, port, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);
    if (!already_in_array){
        printf("Unknown peer, not connected\n");
        return -1;
    }
    //Get the file descriptor:
    // printf("getting the fd of peer:\n");
    int fd = get_fd(ip, port, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);
    int thread_id = get_thread_id(ip, port, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);

    //If sending DSN, disconnect immediately.
    // printf("Sending DSN!!!\n");
    struct btide_packet packet = { PKT_MSG_DSN, 0, {{0}} };
    int ret_val = send(fd, &packet, MAX_PACKET_SIZE, 0);
    if (ret_val <= 0) {
        printf("could not write entire message to client: socket fd %d\n", fd);
    }
    // printf("Closing fd %d\n", fd);
    //Set thread id to 0 to make it available again
    // printf("Freeing thread %d\n\n", thread_id);

    pthread_mutex_lock(all_locks->thread_status_lock);

    pthread_join(req_arrays->thread_array[thread_id], NULL);
    req_arrays->thread_status_array[thread_id] = 0;

    pthread_mutex_unlock(all_locks->thread_status_lock);

    // int n = threads_available(req_arrays->thread_status_array, all_locks->thread_status_lock, max_peers);
    // int nc = get_number_of_connections(req_arrays->peer_array, all_locks->peer_array_lock, max_peers);
    
    // printf("The number of threads available are %d\n", n);
    // printf("The number of connections are %d\n\n", nc);
    return 1;
}





//Function to manage every peer. Every individual thread works here.
void * handle_peers(void * p){
    struct client_thread_args * args = (struct client_thread_args *) p;
    //Remember ip is dynamically allocated. IP's mem location is the same as the one pointed by peers_array.
    char* ip = args->ip;
    int port = args->port;
    int client_socket_fd = args->client_socket_fd;
    int thread_id = args->thread_id;
    int max_peers = args->max_peers;
    struct arrays * req_arrays = args->req_arrays;
    struct locks * all_locks = args->all_locks;
    

    // printf("\nthread id: %d\n", thread_id);
    // printf("max peers: %d\n", max_peers);
    // printf("fd: %d\n", client_socket_fd);
    // printf("ip: %s\n", ip);
    // printf("port: %d\n", port);

    // printf("Thread %d is managing <%s:%d> using socket fd %d\n", thread_id, ip, port, client_socket_fd);
    // int n = threads_available(req_arrays->thread_status_array, all_locks->thread_status_lock, max_peers);
    // int nc = get_number_of_connections(req_arrays->peer_array, all_locks->peer_array_lock, max_peers);
    // printf("The number of threads available are %d\n", n);
    // printf("The number of connections are %d\n\n", nc);

    while (1){
        struct btide_packet buffer = {0};
        ssize_t ret_val = 0;

        ret_val = recv(client_socket_fd, &buffer, MAX_PACKET_SIZE, 0);

        if (ret_val < 0){
            // printf("Error has occurred. In the connection of thread %d (forcefully closed)\n", thread_id);
            remove_from_array(ip, port, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);
            close(client_socket_fd);
            free(args);
            // printf("Exiting thread.\n");
            pthread_mutex_lock(all_locks->thread_status_lock);
            req_arrays->thread_status_array[thread_id] = 0;
            pthread_mutex_unlock(all_locks->thread_status_lock);
            pthread_exit(NULL);

        } else if (ret_val == 0){
            //Client has been sent the DSN.
            // printf("Client has disconnected it seems. Not sure but terminating thread  %d\n", thread_id);
            remove_from_array(ip, port, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);
            close(client_socket_fd);
            free(args);
            // printf("Exiting thread.\n");
            // pthread_mutex_lock(all_locks->thread_status_lock);
            req_arrays->thread_status_array[thread_id] = 0;
            // pthread_mutex_unlock(all_locks->thread_status_lock);
            pthread_exit(NULL);
        }

        // printf("\nRecieved something from %s:%d\n", ip, port);

        if (buffer.msg_code == PKT_MSG_DSN){
            // printf("Disconnecting from %s:%d! on thread %d\n", ip, port, thread_id);
            remove_from_array(ip, port, req_arrays->peer_array, max_peers, all_locks->peer_array_lock);
            close(client_socket_fd);
            free(args);
            // printf("Exiting thread.\n");
            pthread_mutex_lock(all_locks->thread_status_lock);
            req_arrays->thread_status_array[thread_id] = 0;
            pthread_mutex_unlock(all_locks->thread_status_lock);
            pthread_exit(NULL);

        } else if (buffer.msg_code == PKT_MSG_PNG){
            // printf("Ping from %s:%d\n", ip, port);
            // printf("Sending pong to %s:%d\n", ip, port);
            struct btide_packet send_buffer = { PKT_MSG_POG, 0, {{0}} };
            ret_val = send(client_socket_fd, &send_buffer, MAX_PACKET_SIZE, 0);
            if (ret_val <= 0) {
                printf("could not write entire message to client: socket fd %d\n", client_socket_fd);
            }
        } else if (buffer.msg_code == PKT_MSG_POG){
            // printf("Got the pong from %s:%d\n", ip, port);
            //Do nothing if pong is recieved.
        }
        //FETCHERS!!
        else if (buffer.msg_code == PKT_MSG_REQ){
            printf("Got a request of REQ from %s:%d\n", ip, port);
            uint32_t * offset = (uint32_t *) buffer.pl.data;
            uint32_t * req_len = (uint32_t *) (buffer.pl.data + 4);
            char hash[65];
            memcpy(hash, buffer.pl.data + 8, 64);
            char * ident = (char*) buffer.pl.data+72;
            // See the data_len. This will determine how many chunks you send back.
            printf("\npacket contains: \n");
            printf("offset: %u\n", *offset);
            printf("data_len: %u\n", *req_len);
            printf("hash %s\n", hash);
            printf("ident %s\n", ident);

            int n = 1;
            if (*req_len > DATA_MAX_SIZE){
                n = ceil(((double)*req_len) / DATA_MAX_SIZE);
            }
            printf("We will need to send %d packets\n", n);

            struct res_args res_args = {*req_len, 0, ident, hash, *offset, NULL, 0, 0};
            //Current stack persists even after the function being called ends. 
            //In this case, it's easier to use the stack memory rather than heap.
            FILE* file = get_data_from_file(req_arrays->managed_packages, &res_args, NULL, all_locks->managed_packages_lock);
            if (file){
                printf("Success!\n");
                while(res_args.bytes_read < res_args.data_len){
                    get_data_from_file(req_arrays->managed_packages, &res_args, file, all_locks->managed_packages_lock);
                    if (res_args.data){
                        send_res(ip, port, client_socket_fd, &res_args, 0);
                        (res_args.file_offset) += res_args.current_read;
                    }
                    else
                        send_res(ip, port, client_socket_fd, &res_args, 1);
                }
                // printf("That's all folks.\n");
                free(res_args.data);
            } else {
                printf("Failed! Sending error code.\n");
                send_res(ip, port, client_socket_fd, &res_args, 1);
            }

        } else if (buffer.msg_code == PKT_MSG_RES){
            //Parse the file into needed RES args. If valid hash and ident, write.
            struct res_args res_args = { 0 };
            printf("Got a request of RES from %s:%d\n", ip, port);

            if (!buffer.error){

                uint8_t data[DATA_MAX_SIZE]= {0};
                char hash[CHUNK_HASH_SIZE+1] = {0}; //attached null byte

                int total_size_read = 0;
                uint32_t * offset = (uint32_t *) buffer.pl.data;
                total_size_read += sizeof(uint32_t);
                memcpy(data, buffer.pl.data+total_size_read, DATA_MAX_SIZE);
                total_size_read += DATA_MAX_SIZE;
                uint16_t * data_len = (uint16_t *) (buffer.pl.data + total_size_read);
                total_size_read += sizeof(uint16_t);            
                memcpy(hash, buffer.pl.data + total_size_read, CHUNK_HASH_SIZE);
                total_size_read += CHUNK_HASH_SIZE;
                char * ident = (char*) buffer.pl.data+total_size_read;
                printf("Data completely read.\n");

                res_args.file_offset = *offset;
                res_args.data_recieved = *data_len;
                res_args.data = data;
                res_args.chunk_hash = hash;
                res_args.ident = ident;

                // See the data_len. This will determine how many chunks you send back.
                printf("\npacket contains: \n");
                printf("offset: %u\n", res_args.file_offset);
                printf("data_len: %u\n", res_args.data_recieved);
                printf("hash %s\n", res_args.chunk_hash);
                printf("ident %s\n", res_args.ident);

                add_data_to_file(req_arrays->managed_packages, &res_args, all_locks->managed_packages_lock);
            } else {
                printf("Error detected!\n");
            }
        } else {
            printf("Recieved a strange message...\n");
            
        }
    }
}