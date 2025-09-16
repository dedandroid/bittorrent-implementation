#include "net/packages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


#define INCREASE_FACTOR (2) // resize a package managing array by this much.

//Add a dynamic array that includes all managed bpkg objects.
int add_package(char* dir, char* path, struct managed_packages * managed_packages, pthread_mutex_t * package_lock){
    pthread_mutex_lock(package_lock);
    struct bpkg_obj * bpkg = NULL;
    bpkg = bpkg_load(path, dir);

    if (bpkg){
        //Make sure the file exists.
        struct bpkg_query* qry = bpkg_file_check(bpkg);
        // printf("%s\n", qry->hashes[0]);
        bpkg_query_destroy(qry);

        for (int i = 0; i<managed_packages->capacity; i++){
            if ( (managed_packages->managed_array[i]) == NULL){
                managed_packages->managed_array[i] = bpkg;
                managed_packages->size++;
                pthread_mutex_unlock(package_lock);
                return 1;
            }
        }
        //Array full:
        int new_capacity = (managed_packages->capacity) * INCREASE_FACTOR;
        struct bpkg_obj ** realloced_array = realloc(managed_packages->managed_array, new_capacity* sizeof(struct bpkg_obj *));
        if (realloced_array == NULL){
            printf("Allocation failed!\n");
            pthread_mutex_unlock(package_lock);
            return 0;
        }
        //Set all the non-used pointers to null.
        for (int k = managed_packages->capacity; k<new_capacity; k++){
            realloced_array[k] = NULL;
        }
        managed_packages->capacity = new_capacity;
        managed_packages->managed_array = realloced_array;
        //Assign new pkg:
        managed_packages->managed_array[managed_packages->size] = bpkg;
        managed_packages->size++;
        // printf("Array size: %d\n\n", managed_packages->size);
        pthread_mutex_unlock(package_lock);
        return 1;
    }
    free(bpkg);
    pthread_mutex_unlock(package_lock);
    return 0;
}

//Doesn't change the size of the dynamic array.
int remove_package(struct managed_packages * managed_packages, char ident[MAX_IDENT_SIZE], pthread_mutex_t * package_lock){
    pthread_mutex_lock(package_lock);
    //Only go through the values needed.
    for (int i = 0; i<managed_packages->capacity; i++){
        struct bpkg_obj * current_bpkg = managed_packages->managed_array[i];
        if (current_bpkg != NULL) {
            if ( strncmp(current_bpkg->ident, ident, 20) == 0 ){
                //Package found
                bpkg_obj_destroy(current_bpkg);
                managed_packages->managed_array[i] = NULL;
                managed_packages->size--;
                printf("Package has been removed\n");
                pthread_mutex_unlock(package_lock);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(package_lock);
    printf("Identifier provided does not match managed packages\n\n");
    return 0;
}


int package_status(struct managed_packages* managed_packages, pthread_mutex_t * package_lock){
    pthread_mutex_lock(package_lock);

    if (managed_packages->size == 0){
        printf("No packages managed\n");
        pthread_mutex_unlock(package_lock);
        return 0;
    }
    int counter = 1; // keep track of elements
    for (int i = 0; i<managed_packages->capacity; i++){
        struct bpkg_obj * current_bpkg = managed_packages->managed_array[i];
        if (current_bpkg != NULL){
            struct bpkg_query * qry = bpkg_get_min_completed_hashes(current_bpkg);
            // printf("Root node value %s\n", current_bpkg->tree->root->value);
            // printf("Query result:\n");
            // for(int i = 0; i < qry->len; i++) {
		    //     printf("%.64s\n", qry->hashes[i]);
	        // }
            // printf("OOGa\n");
            
            if ( (qry->len > 0) && strcmp(qry->hashes[0], current_bpkg->tree->root->value) == 0){
                printf("%d. %.32s, %s : COMPLETED\n", counter, current_bpkg->ident, current_bpkg->filename);
            } else {
                printf("%d. %.32s, %s : INCOMPLETE\n", counter, current_bpkg->ident, current_bpkg->filename);
            }
            counter++;
            bpkg_query_destroy(qry);
        }
    }
    pthread_mutex_unlock(package_lock);
    return 1;
}


struct req_args * calculate_data_len(struct bpkg_obj* bpkg, char* hash, unsigned int* offset, pthread_mutex_t * package_lock){

    pthread_mutex_lock(package_lock); // Don't want a thread deleting the bpkg i am working with.
    struct req_args * req_args = malloc(sizeof(struct req_args));

    for (int i = 0; i<bpkg->nchunks; i++){
        struct chunks* chunk = bpkg->chunks[i];
        if ( strcmp(chunk->hash, hash) == 0 ){
            if (offset){
                uint32_t chunk_end = chunk->offset + chunk->size;
                //THe chunk must contain the offset
                if ( chunk->offset <= *offset && *offset <= (chunk_end) ){
                    req_args->data_len = (chunk_end) - *(offset);
                    req_args->file_offset = *offset;
                    pthread_mutex_unlock(package_lock);
                    return req_args;
                } else {
                    continue;
                }
            } else {
                // printf("Offset not specified\n\n");
                req_args->data_len = chunk->size;
                req_args->file_offset = chunk->offset;
                pthread_mutex_unlock(package_lock);
                return req_args;
            }
        }
    }
    // printf("Could not find the hash in the chunk!\n");
    free(req_args);
    pthread_mutex_unlock(package_lock);
    return NULL;
}

int chunk_in_memory(struct managed_packages* packages, char* ident, char* chunk_hash, 
                    struct bpkg_obj ** chosen_bpkg, pthread_mutex_t * package_lock){

    pthread_mutex_lock(package_lock);
    //Make sure ident is in managed packages list:
    for (int i = 0; i<packages->capacity; i++){
        struct bpkg_obj * current_bpkg = packages->managed_array[i];
        if (current_bpkg){
            if (strncmp(current_bpkg->ident, ident, 20) == 0){
                // printf("Package found!\n");
                char* root_hash = current_bpkg->tree->root->value;
                struct bpkg_query* qry = bpkg_get_all_chunk_hashes_from_hash(current_bpkg, root_hash);
                // printf("Query results:\n");
                for (int k = 0; k<qry->len; k++){
                    if (strcmp(qry->hashes[k], chunk_hash) == 0){
                        // printf("Hash matches with %s\n", qry->hashes[k]);
                        *chosen_bpkg = current_bpkg;
                        //Chunk and hash found
                        bpkg_query_destroy(qry);
                        pthread_mutex_unlock(package_lock);
                        return 1;
                    }
                }
                bpkg_query_destroy(qry);
                //Indicate hash not found
                pthread_mutex_unlock(package_lock);
                return -1;
            }
        }
    }
    pthread_mutex_unlock(package_lock);
    //indicate chunk not found
    return -2;
}

void add_data_to_file(struct managed_packages* packages, struct res_args* res_args, pthread_mutex_t * package_lock){
    
    struct bpkg_obj* chosen_bpkg = NULL;
    int ret_val = chunk_in_memory(packages, res_args->ident, res_args->chunk_hash, &chosen_bpkg, package_lock);
    if (ret_val != 1)
        return;
    struct req_args * req_args = calculate_data_len(chosen_bpkg, res_args->chunk_hash, &res_args->file_offset, package_lock);
    if (!req_args){
        return;
    }
    // printf("File offset: %u\n", req_args->file_offset);
    // printf("Data length: %u\n", req_args->data_len);

    if (req_args->file_offset == res_args->file_offset && req_args->data_len >= res_args->data_recieved){
        // printf("We both have matching data lenghts and offsets\n");
        ;
    } else {
        // printf("OH NO! Data length they ask for is not what the chunk is.!!\n");
        free(req_args);
        return;
    }

    //All good. Write to the file:
    FILE* file_obj = fopen(chosen_bpkg->filename, "r+b");
    if (file_obj){
        fseek(file_obj, req_args->file_offset, SEEK_SET);
        free(req_args);
        int ret_val = fwrite(res_args->data, 1, res_args->data_recieved, file_obj);
        if (ret_val < res_args->data_recieved){
            printf("Error writing into file!\n");
        } else {
            printf("Successfully written %d bytes at offset %d\n", res_args->data_recieved, res_args->file_offset);
        }
        fclose(file_obj);
    } else {
        printf("Cannot open file\n");
    }
}


FILE* get_data_from_file(struct managed_packages* packages, struct res_args * res_args, FILE* file, pthread_mutex_t * package_lock){

    if (file == NULL){
        struct bpkg_obj * chosen_bpkg = NULL;
        uint8_t * data = malloc(DATA_MAX_SIZE);
        //Make sure we have the chunk.
        int ret_val = chunk_in_memory(packages, res_args->ident, res_args->chunk_hash, &chosen_bpkg, package_lock);
        if (ret_val != 1){
            //Indicate error bit
            free(data);
            return NULL;
        }
        //Make sure we have the computed hash ourselves.
        compute_hashes_for_chunks(chosen_bpkg);
        for (int i = 0; i<chosen_bpkg->nchunks; i++){
            if (strcmp(chosen_bpkg->chunks[i]->node_ptr->computed_hash, res_args->chunk_hash) == 0){
                // printf("Nice! We have the hash!\n");

                // Make sure the offset and hash is correct.
                struct req_args * req_args = calculate_data_len(chosen_bpkg, res_args->chunk_hash, &res_args->file_offset, package_lock);
                if (!req_args){
                    free(data);
                    return NULL;
                }

                //Not sure if Error should be sent or not.
                if (req_args->file_offset == res_args->file_offset && req_args->data_len == res_args->data_len){
                    // printf("We both have matching data lengths and offsets\n");
                    ;
                } else {
                    // printf("OH NO! Data length they ask for is not what the chunk is.!!\n");
                    ;
                }

                //open the filename in bpkg and set pointer to the offset
                FILE* file_obj = fopen(chosen_bpkg->filename, "r");
                if (file_obj){
                    fseek(file_obj, req_args->file_offset, SEEK_SET);
                    free(req_args);
                    res_args->data = data;
                    return file_obj;
                }
                printf("Cannot open file :(\n");
                free(req_args);
                free(data);
                return NULL; // error opening the file
            }
        }
        //loop ended, we didn't find any.
        free(data);
        printf("We don't have this chunk completed either!\n");
        return NULL;
    } else {
        //file is already open. Simply read 

        if (res_args->data_len - res_args->bytes_read < DATA_MAX_SIZE){
            //We have less to read than 2998 bytes
            int read_amount = res_args->data_len - res_args->bytes_read;
            int num_read = fread(res_args->data, 1, read_amount, file);
            // printf("Successfully read %d bytes\n", num_read);
            if (num_read < read_amount){
                printf("Read failed.\n");
                free(res_args->data);
                res_args->data = NULL; //Indicates that an error occurred.
                fclose(file);
            } else {
                (res_args->bytes_read) += num_read;
                (res_args->current_read) = num_read;
                //Zero out the rest of the data.
                memset(res_args->data+num_read, 0, DATA_MAX_SIZE-num_read);
                // printf("Final yippie!\n");
            }
        } else {
            //We can still read more than 2998
            int num_read = fread(res_args->data, 1, DATA_MAX_SIZE, file);
            // printf("Successfully read %d bytes\n", num_read);
            if (num_read < DATA_MAX_SIZE){
                printf("Failed to read file\n");
                free(res_args->data);
                res_args->data = NULL; //Indicates that an error occurred.
                fclose(file);
            } else{
                // printf("Yippe.\n");
                (res_args->bytes_read) += num_read;
                (res_args->current_read) = num_read;
            } 
        }
        return NULL;
    }
    
}