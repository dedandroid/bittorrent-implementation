#ifndef PEERS_H
#define PEERS_H

#include <stdio.h>
#include <stdlib.h>
#include "net/packet.h"
#include "net/config.h"
#include "net/packages.h"
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "net/commands.h"
#include "chk/pkgchk.h"
#include <pthread.h>

struct peer {
    char* ip;
    int port;
    //If 0, then overwrite position in peer array with new peer.
    int socket_fd;
    //Use to shut a thread down when disconnecting.
    int thread_id;
};

//A neat array containing all the structs
struct locks {
    pthread_mutex_t * thread_array_lock; 
    pthread_mutex_t * thread_status_lock;
    pthread_mutex_t * peer_array_lock;
    pthread_mutex_t * managed_packages_lock;
};

struct arrays {
    pthread_t * thread_array;
    int * thread_status_array;
    struct peer ** peer_array;
    struct managed_packages * managed_packages;
};

//Structure for listener args
struct listener_args{
    struct config* config_file;
    struct arrays* req_arrays;
    struct locks* all_locks;
};

//args for every other thread for clients:
struct client_thread_args{
    char* ip;
    int port;
    int client_socket_fd;
    int thread_id;
    int max_peers;
    struct arrays* req_arrays;
    struct locks* all_locks;
};


//Simpy connect/disconnect from a peer.
int peer_connect(char* ip, int port, int max_peers, struct arrays * req_arrays, struct locks * all_locks);
int peer_disconnect(char* ip, int port, int max_peers, struct arrays * req_arrays, struct locks * all_locks);

//Listener:
void * listener(void * p);

//Peer handler:
void * handle_peers(void *);

//peer array checking operations:
int in_array(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock);
int add_to_array(char* ip, int port, int socket_fd, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock);
int get_fd(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock);
void remove_from_array(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock);

//Thread handlers:
int set_thread_id(char* ip, int port, struct peer ** peer_array, int max_peers, int thread_id, pthread_mutex_t * peer_array_lock);
int assign_thread(int max_peers, char* ip, int port, int new_socket, struct arrays * req_arrays, struct locks * all_locks);
int get_thread_id(char* ip, int port, struct peer ** peer_array, int max_peers, pthread_mutex_t * peer_array_lock);

int threads_available(int * thread_status_array, pthread_mutex_t * status_array_lock, int max_peers);
int get_number_of_connections(struct peer ** peer_array, pthread_mutex_t * peer_lock, int max_peers);


//Upon listener error, QUIT & EOF
void destroy_all(struct config * config_file, struct arrays* req_arrays, struct locks* all_locks);

//REQ and RES :
void send_req(char* ip, int port, int fd, struct req_args * req_args);
void send_res(char* ip, int port, int fd, struct res_args * res_args, int error);

#endif