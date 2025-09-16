#include "chk/pkgchk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "net/peers.h"
#include "net/packet.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>



int threads_available(int * thread_status_array, pthread_mutex_t * status_array_lock, int max_peers){
    int n = 0;
    pthread_mutex_lock(status_array_lock);
    for (int i = 0; i<max_peers; i++){
        if (thread_status_array[i] == 0) 
            n++;
    }
    pthread_mutex_unlock(status_array_lock);
    return n;  
}

int get_number_of_connections(struct peer ** peer_array, pthread_mutex_t * peer_lock, int max_peers){
    int n = 0;
    pthread_mutex_lock(peer_lock);
    for (int i = 0; i<max_peers; i++){
        if (peer_array[i]) 
            n++;
    }
    pthread_mutex_unlock(peer_lock);
    return n;  
}

//Get the file descriptor socket of the peer.
int get_fd(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock){
    pthread_mutex_lock(peer_array_lock);
    // printf("Looking for the fd of %s:%d\n", ip, port);
    for (int i = 0; i<max_peers; i++){
        if (peer_array[i] != NULL){        
            if (strcmp(peer_array[i]->ip, ip) == 0){
                // printf("IP matches\n");
                if (peer_array[i]->port == port){
                    // printf("port matches too\n");
                    // printf("Getting peer fd %d:\n", peer_array[i]->socket_fd);
                    //ip is malloced. Since the peer already exists, free this malloc.
                    pthread_mutex_unlock(peer_array_lock);
                    return peer_array[i]->socket_fd;
                }
            }
        }
    }
    pthread_mutex_unlock(peer_array_lock);
    return 0;
}

//Check if peer is in the peer array.
int in_array(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock){
    pthread_mutex_lock(peer_array_lock);
    for (int i = 0; i<max_peers; i++){
        if (peer_array[i] != NULL){        
            if (strcmp(peer_array[i]->ip, ip) == 0){
                // printf("IP matches\n");
                if (peer_array[i]->port == port){
                    // printf("port matches too\n");
                    // printf("Already connected to peer\n");
                    pthread_mutex_unlock(peer_array_lock);
                    return 1;
                }
            }
        }
    }
    pthread_mutex_unlock(peer_array_lock);
    return 0;
}

//Add peer to the peer array.
int add_to_array(char* ip, int port, int socket_fd, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock){
    pthread_mutex_lock(peer_array_lock);
    for (int i = 0; i<max_peers; i++){
        //Replace the first instance of a disconnected user:
        if (peer_array[i] == NULL){
            peer_array[i] = malloc(sizeof(struct peer));
            if (peer_array[i] == NULL){
                printf("malloc failed\n");
                return -1;
            }
            peer_array[i]->ip = ip;
            peer_array[i]->port = port;
            peer_array[i]->socket_fd = socket_fd;
            pthread_mutex_unlock(peer_array_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(peer_array_lock);
    return 0;
}

//Remove peer from the peer array.
void remove_from_array(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock){
    pthread_mutex_lock(peer_array_lock);
    for (int i = 0; i < max_peers; i++){
        if (peer_array[i] != NULL){
            if ( (strcmp(peer_array[i]->ip, ip) == 0) && (peer_array[i]->port == port) ){
                // printf("Found %s:%d that is to be deleted from the array\n", ip, port);
                free(peer_array[i]->ip);
                peer_array[i]->ip = NULL;
                free(peer_array[i]);
                peer_array[i] = NULL;
                pthread_mutex_unlock(peer_array_lock);
                return;
            }
        }
    }
    pthread_mutex_unlock(peer_array_lock);
}

//Set the thread ID for the peer being added
int set_thread_id(char* ip, int port, struct peer ** peer_array, int max_peers, int thread_id, pthread_mutex_t * peer_array_lock){
    pthread_mutex_lock(peer_array_lock);
    for (int i = 0; i<max_peers; i++){
        if (peer_array[i] != NULL){
            if ( (strcmp(peer_array[i]->ip, ip) == 0) && (peer_array[i]->port == port) ){
                // printf("Found peer in the array when setting thread id!\n");
                peer_array[i]->thread_id = thread_id;
                pthread_mutex_unlock(peer_array_lock);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(peer_array_lock);
    return 0;
}


// Make a mutex here to make sure main thread and listener don't both interfere.
int assign_thread(int max_peers, char* ip, int port, int new_socket, struct arrays * req_arrays, struct locks * all_locks){
    pthread_mutex_lock(all_locks->thread_status_lock);
    for (int j = 0; j<max_peers; j++){
        //Thread is available for use:
        // printf("On thread %d\n", j);
        int* current_thread_status = &req_arrays->thread_status_array[j];
        // printf("Status is %d\n", *current_thread_status);
        if (*current_thread_status == 0){
            // printf("YEAY\n");
            *current_thread_status = 1;

            struct client_thread_args * thread_args = malloc(sizeof(struct client_thread_args));
            thread_args->ip = ip; 
            thread_args->port = port;
            thread_args->client_socket_fd = new_socket;
            thread_args->max_peers = max_peers;
            thread_args->thread_id = j;
            thread_args->req_arrays = req_arrays;
            thread_args->all_locks = all_locks;

            set_thread_id(ip, port, req_arrays->peer_array, max_peers, j, all_locks->peer_array_lock);

            //Make sure no other thread can modify this thread array. Thread array lock is useless!!
            // pthread_mutex_lock(all_locks->thread_array_lock);
            pthread_create(&(req_arrays->thread_array[j]), NULL, &handle_peers, thread_args);
            // pthread_mutex_unlock(all_locks->thread_array_lock);
            // printf("Connection established with peer\n");
            pthread_mutex_unlock(all_locks->thread_status_lock);
            return 0;
        }
    }
    pthread_mutex_unlock(all_locks->thread_status_lock);
    printf("All threads are in use!");
    close(new_socket);
    return -1;
}


int get_thread_id(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock){
    //Set mutex here too:
    pthread_mutex_lock(peer_array_lock);
    for (int i = 0; i<max_peers; i++){
        if (peer_array[i] != NULL){
            if ( (strcmp(peer_array[i]->ip, ip) == 0) && (peer_array[i]->port == port) ){
                // printf("Found peer in the array when getting thread id!: %d\n", peer_array[i]->thread_id);
                pthread_mutex_unlock(peer_array_lock);
                return peer_array[i]->thread_id;
            }
        }
    }
    pthread_mutex_unlock(peer_array_lock);
    return 0;
}





//Extra helpers:
int is_any_space(char word){    
    if (isspace(word) || word == '\n' || word == '\0'){
        // printf("Yep\n");
        return 1;
    } else{
        return 0;
    }
}

//WE wish to be able to modify the pointer itself.
void strip_whitespace(char** word){
    if (*word != NULL){
        while (isspace(**word)){
            (*word)++;
        }
        //Handle empty strings
        if (**word == '\0'){
            *word = "";
            return;
        }

        char* end_ptr = *word + (strlen(*word));
        while (is_any_space(*end_ptr)){
            end_ptr--;
        }
        *(end_ptr+1) = '\0';
    }      
}

int is_an_int(char* str){
    if (*str == '\0'){
        return 0;
    }

    for (int i = 0; i<strlen(str); i++){
        if (!isdigit(str[i])){
            return 0;
        }
    }
    return 1;
}