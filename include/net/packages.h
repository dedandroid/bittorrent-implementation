#ifndef PACKAGES_H
#define PACKAGES_H

#include "chk/pkgchk.h"
#include <stdio.h>
#include <pthread.h>

#define MAX_IDENT_SIZE (1025) // includes null
#define DATA_MAX_SIZE (2998)

struct managed_packages {
    struct bpkg_obj ** managed_array;
    int size;
    int capacity;
};

//Represents a sender's information about the package
struct req_args {
    uint32_t data_len;
    char* ident;
    char* chunk_hash;
    uint32_t file_offset;
};

//Represents a reciever's information about the package. 
//Will need to compare with their own to make sure.
struct res_args {
    uint32_t data_len; // Sender's req_len to send in total.
    uint16_t data_recieved; //Reciever's amount recieved.
    char* ident;
    char* chunk_hash;
    uint32_t file_offset;
    uint8_t * data;
    int bytes_read; // how much you read over all iterations
    uint16_t current_read; // how much you read in that iteration
};

//manage packages
int add_package(char* dir, char* path, struct managed_packages * managed_packages, pthread_mutex_t * package_lock);

int remove_package(struct managed_packages * managed_packages, char ident[MAX_IDENT_SIZE], 
                   pthread_mutex_t * package_lock);

int package_status(struct managed_packages* managed_packages, pthread_mutex_t * package_lock);

struct req_args * calculate_data_len(struct bpkg_obj* bpkg, char* hash, unsigned int* offset,
                                     pthread_mutex_t * package_lock);

int chunk_in_memory(struct managed_packages* packages, char* ident, char* chunk_hash, 
                    struct bpkg_obj ** chosen_bpkg, pthread_mutex_t * package_lock);


//File I/O functions after FETCHING
FILE* get_data_from_file(struct managed_packages* packages, struct res_args * res_args,
                         FILE* file, pthread_mutex_t * package_lock);

void add_data_to_file(struct managed_packages* packages, struct res_args* res_args,
                      pthread_mutex_t * package_lock);

#endif