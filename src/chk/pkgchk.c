#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "chk/pkgchk.h"
#include <string.h>
#include "crypt/sha256.h"

#define MAX_INPUT_SIZE (1035)
#define MAX_FILENAME (257) //includes null

// PART 1
/**
 * Loads the package for when a valid path is given
 */
struct bpkg_obj* bpkg_load(const char* path, char* dir) {
    struct bpkg_obj* bpkg_ptr = malloc(sizeof(struct bpkg_obj));
    bpkg_ptr->tree = NULL;
    bpkg_ptr->chunks = NULL;
    bpkg_ptr->hashes = NULL;

    //Maximum buffer we will require.
    char buffer[MAX_INPUT_SIZE];
    int nhashes_counter = 0;
    int nchunks_counter = 0;
    char* key = NULL;
    char* value = NULL;
    //Iterator to ensure the values are in the correct order.
    int order_iterator = 0;
    int error_flag = 0;
    //^^ Allocate value in heap if string.

    FILE * file = fopen(path, "r");
    if (file != NULL){
        //Read line one:
        // printf("printing out bpkg file\n");
        while(fgets(buffer, sizeof(buffer), file) != NULL){

            key = strtok(buffer, ":");
            value = strtok(NULL, ":");
            strip_whitespace(&key);
            strip_whitespace(&value);

            if (strcmp(key, "ident") == 0){

                if (order_iterator != 0){
                    error_flag = 1;
                    break;
                }
                order_iterator++;

                //Include null byte
                char* loc_ptr = malloc(sizeof(char) * 1025);
                strcpy(loc_ptr, value);
                bpkg_ptr->ident = loc_ptr;

                

            } else if (strcmp(key, "filename") == 0){
                if (order_iterator != 1){
                    error_flag = 1;
                    break;
                }
                order_iterator++;

                char* loc_ptr = NULL;
                if (dir){
                    // printf("Directory given is %s\n", dir);
                    loc_ptr = malloc(strlen(value) + strlen(dir) + 2); // for / and null terminator
                    //First string
                    strcpy(loc_ptr, dir);
                    loc_ptr[strlen(dir)] = '/';
                    loc_ptr[strlen(dir)+1] = '\0';
                    strcat(loc_ptr, value);

                } else {
                    loc_ptr = malloc(strlen(value) + 1);
                    strcpy(loc_ptr, value);
                }
                
                bpkg_ptr->filename = loc_ptr;
                

            } else if (strcmp(key, "size") == 0){
                if (order_iterator != 2){
                    error_flag = 1;
                    break;
                }
                order_iterator++;
                
                if (!is_an_int(value)){
                    printf("Size is not an integer.\n");
                    error_flag = 1;
                    break;
                }

                int int_val = atoi(value);
                bpkg_ptr->size = int_val;

                

            } else if (strcmp(key, "nhashes") == 0){

                if (!is_an_int(value)){
                    printf("nhashes is not an integer.\n");
                    error_flag = 1;
                    break;
                }
                
                int int_val = atoi(value);
                bpkg_ptr->nhashes = int_val;
                if (order_iterator != 3){
                    error_flag = 1;
                    break;
                }
                order_iterator++;

            } else if (strcmp(key, "hashes") == 0){
                
                char** hash_pointer_array = (char**) malloc(sizeof(char*) * bpkg_ptr->nhashes);                
                while(((nhashes_counter < bpkg_ptr->nhashes) && fgets(buffer, sizeof(buffer), file) != NULL)){
                    value = strtok(buffer, " ");
                    strip_whitespace(&value);
                    
                    //allocated memory of hash including null byte.
                    char* hash = malloc(sizeof(char)*65);
                    strcpy(hash, value);
                    
                    hash_pointer_array[nhashes_counter] = hash;
                    
                    nhashes_counter += 1;
                }
                bpkg_ptr->hashes = hash_pointer_array;
                if (order_iterator != 4){
                    error_flag = 1;
                    break;
                }
                order_iterator++;

            } else if (strcmp(key, "nchunks") == 0){
                
                if (!is_an_int(value)){
                    printf("nchunks is not an integer.\n");
                    error_flag = 1;
                    break;
                }

                int int_val = atoi(value);
                bpkg_ptr->nchunks = int_val;

                if (order_iterator != 5){
                    error_flag = 1;
                    break;
                }
                order_iterator++;


            } else if (strcmp(key, "chunks") == 0){
                
                struct chunks** chunk_pointer_array = (struct chunks**) malloc(sizeof(struct chunks*) * bpkg_ptr->nchunks);
                for (int i = 0; i<bpkg_ptr->nchunks; i++){
                    chunk_pointer_array[i] = NULL;
                }

                while(((nchunks_counter < bpkg_ptr->nchunks) && fgets(buffer, sizeof(buffer), file) != NULL)){
                    value = strtok(buffer, " ");
                    strip_whitespace(&value);
                    
                    char* hash = malloc(sizeof(char)*65);
                    char* hash_val = strtok(value, ",");

                    char* offset_val = strtok(NULL, ",");
                    char* size_val = strtok(NULL, ",");

                    if (!is_an_int(offset_val)){
                        printf("Offset is not an integer.\n");
                        error_flag = 1;
                        free(hash);
                        break;
                    } if (!is_an_int(size_val)){
                        printf("Hash size is not an integer.\n");
                        free(hash);
                        error_flag = 1;
                        break;
                    }

                    int offset = atoi(offset_val);
                    int size = atoi(size_val);
                    struct chunks * chunk = (struct chunks*) malloc(sizeof(struct chunks));
                    chunk_pointer_array[nchunks_counter] = chunk;
                    
                    strcpy(hash, hash_val);
                    
                    chunk_pointer_array[nchunks_counter]->hash = hash;
                    chunk_pointer_array[nchunks_counter]->offset = offset;
                    chunk_pointer_array[nchunks_counter]->size = size;
                    chunk_pointer_array[nchunks_counter]->node_ptr = NULL;
                    
                    nchunks_counter++;
                }
                bpkg_ptr->chunks = chunk_pointer_array;

                if (order_iterator != 6){
                    error_flag = 1;
                    break;
                } if (error_flag){
                    //Flag may have been set inside the loop.
                    break;
                }
                order_iterator++;

            } else {
                //Invalid bpkg file
                error_flag = 1;
                break;
            }
        }

    } else {
        printf("Cannot open file\n");
        free(bpkg_ptr);
        return NULL;
    }

    if (error_flag){
        printf("Unable to parse bpkg file\n");
        bpkg_obj_destroy(bpkg_ptr);
        fclose(file);
        return NULL;
    }

    bpkg_ptr->tree = create_tree(bpkg_ptr->hashes, bpkg_ptr->nhashes, bpkg_ptr->chunks, bpkg_ptr->nchunks);
    // show_tree(bpkg_ptr->tree);
    fclose(file);
    return bpkg_ptr;
}

/**
 * Checks to see if the referenced filename in the bpkg file
 * exists or not.
 * @param bpkg, constructed bpkg object
 * @return query_result, a single string should be
 *      printable in hashes with len sized to 1.
 * 		If the file exists, hashes[0] should contain "File Exists"
 *		If the file does not exist, hashes[0] should contain "File Created"
 */
struct bpkg_query* bpkg_file_check(struct bpkg_obj* bpkg){
    struct bpkg_query* obj = malloc(sizeof(struct bpkg_query));
    char** str = malloc(sizeof(char*));
    strip_whitespace(&bpkg->filename);
    FILE* file = fopen(bpkg->filename, "r");
    if (file != NULL){
        str[0] = "File Exists";
        fclose(file);
    } else {
        str[0] = "File Created";
        //Extra code to create the file.
        FILE* f2 = fopen(bpkg->filename, "wb");
        // printf("Opening file of name %s\n", bpkg->filename);

        if (f2){
            if (fseek(f2, bpkg->size-1, SEEK_SET) == 0) {
                if (fwrite("", 1, 1, f2) != 1){
                    printf("Failed to write to file\n");
                }
            }
            fclose(f2);
        }
    }
    
    obj->hashes = str;
    obj->len = 1;
    return obj;
}


//MERKLE TREE FUNCTIONS.

/**
 * Retrieves a list of all hashes within the package/tree
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query * bpkg_get_all_hashes(struct bpkg_obj* bpkg) {
    struct bpkg_query * qry = malloc(sizeof(struct bpkg_query));

    int n = bpkg->nhashes + bpkg->nchunks;

    char** hashes = (char**) malloc(sizeof(char*)*(n));
    int i = 0;
    inorder_search(bpkg->tree->root, all_hash_func, hashes, &i);
    qry->hashes = hashes;
    qry->len = n;
    return qry;
}
//Function pointers mini functions
void all_hash_func(struct merkle_tree_node* node, char** hashes, int* i){
    hashes[*i] = node->value;
    (*i)++;
}


/**
 * Retrieves all completed chunks of a package object
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query* bpkg_get_completed_chunks(struct bpkg_obj* bpkg) { 
    struct bpkg_query* qry = malloc(sizeof(struct bpkg_query));

    compute_hashes_for_chunks(bpkg); 
    int n = bpkg->nchunks;
    char** hashes = (char**) malloc(sizeof(char*)*(n));

    int i = 0;
    inorder_search(bpkg->tree->root, get_completed_chunks, hashes, &i);
    qry->hashes = hashes;
    qry->len = i;

    return qry;
}
//Above's mini function
void get_completed_chunks(struct merkle_tree_node* node, char** hashes, int* i){
    if (node->is_leaf){
        if (strcmp(node->value, node->computed_hash) == 0){
            hashes[*i] = node->value;
            (*i)++;
        } 
    }
}



/**
 * Gets only the required/min hashes to represent the current completion state
 * Return the smallest set of hashes of completed branches to represent
 * the completion state of the file.
 *
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query* bpkg_get_min_completed_hashes(struct bpkg_obj* bpkg) {
    struct bpkg_query* qry = malloc(sizeof(struct bpkg_query));
    compute_hashes_for_chunks(bpkg);
    int n = bpkg->nchunks;
    char** hashes = (char**) malloc(sizeof(char*)*(n));

    int i = 0;
    min_hash_find(bpkg->tree->root, hashes, &i);
    qry->hashes = hashes;
    qry->len = i;

    return qry;
}


/**
 * Retrieves all chunk hashes given a certain an ancestor hash (or itself)
 * Example: If the root hash was given, all chunk hashes will be outputted
 * 	If the root's left child hash was given, all chunks corresponding to
 * 	the first half of the file will be outputted
 * 	If the root's right child hash was given, all chunks corresponding to
 * 	the second half of the file will be outputted
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query* bpkg_get_all_chunk_hashes_from_hash(struct bpkg_obj* bpkg, 
    char* hash) {
    struct bpkg_query* qry = malloc(sizeof(struct bpkg_query));
    //Qry hashes of size nchunks at most:
    int n = bpkg->nchunks;
    char** hashes = (char**) malloc(sizeof(char*)*(n));
    //Traverse the tree until hash is encountered (in-order):
    struct merkle_tree_node* found_node = inorder_find_node(bpkg->tree->root, hash);

    if (found_node == NULL){
        //Do something
        // printf("OHOH! HASH IS WRONG!!\n");
        free(qry);
        free(hashes);
        return NULL;
    }
    int i = 0;
    inorder_search(found_node, get_chunks_from_hash_func, hashes, &i);
    qry->hashes = hashes;
    qry->len = i;

    return qry;
}
//Above's mini function
void get_chunks_from_hash_func(struct merkle_tree_node* node, char** hashes, int* i){
    if (node->is_leaf){
        hashes[*i] = node->value;
        (*i)++;
    }
}



//deallocations

/**
 * Deallocates the query result after it has been constructed from
 * the relevant queries above.
 */
void bpkg_query_destroy(struct bpkg_query* qry) {
    //TODO: Deallocate here!
    //Don't delete hashes as those pointers are still being pointed to by the bpkg_obj.
    //Only destroy the bpkg_query array of pointers.
    if (qry){
        free(qry->hashes);
        free(qry);
    }
    
}

/**
 * Deallocates memory at the end of the program,
 * make sure it has been completely deallocated
 */
void bpkg_obj_destroy(struct bpkg_obj* obj) {
    //TODO: Deallocate here!
    free((obj->ident));
    free((obj->filename));
    int i = 0;

    //Remove hashes array
    if (obj->hashes){
        while (i <  obj->nhashes){
            if (obj->hashes[i]){
                free((obj->hashes)[i]);
            }
            i++;
        }
        free((obj->hashes));
    }

    //Remove chunks array
    if (obj->chunks){
        i = 0;
        while (i < obj->nchunks){
            if (obj->chunks[i]){
                free((obj->chunks)[i]->hash);
                free((obj->chunks)[i]);
            }
            i++;
        }
        free((obj->chunks));
    }

    //Remove tree nodes and tree
    if (obj->tree){
        remove_tree(obj->tree->root);
        free(obj->tree);
    }

    free(obj);
}


