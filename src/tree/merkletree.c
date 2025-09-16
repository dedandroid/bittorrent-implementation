#include "tree/merkletree.h"
#include "chk/pkgchk.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "crypt/sha256.h"

//64 + 64 + 1
#define NODE_HASH_BUFFER_SIZE (129)

//POssible function pointer to call what type of function to call when visiting node.
void inorder_search(struct merkle_tree_node * root, void (*func_ptr)(struct merkle_tree_node*, char**, int*), char** hashes, int* i) {
    if (root->left){
        inorder_search(root->left, func_ptr, hashes, i);
    }
    func_ptr(root, hashes, i);
    if (root->right){
        inorder_search(root->right, func_ptr, hashes, i);
    }
}

struct merkle_tree_node* inorder_find_node(struct merkle_tree_node* root, char* hash){
    if (root == NULL)
        return NULL;
    // else {
    //     printf("On node %s\n", root->value);
    //     printf("Hash is %s\n\n", hash);
    // }

    struct merkle_tree_node* node = inorder_find_node(root->left, hash);

    //Cascade return
    if (node != NULL){
        return node;    
    }

    if (strcmp(root->value, hash) == 0){
        // printf("Nice\n");
        return root;
    }

    return inorder_find_node(root->right, hash);
}

int min_hash_find(struct merkle_tree_node* root, char** hashes, int* i){

    if (root->is_leaf){

        if (strcmp(root->computed_hash, root->value)==0){
            return 1;
        } else {
            return 0;
        }   
    }

    int left_complete = min_hash_find(root->left, hashes, i);
    // printf("left: %d\n", left_complete);
    int right_complete = min_hash_find(root->right, hashes, i);
    // printf("right: %d\n", right_complete);

    if (left_complete && right_complete){
        //Special case for root:
        if (root->parent == NULL){
            hashes[*i] = root->value;
            (*i)++;
        }
        return 1;
    } else if (left_complete){
        hashes[*i] = root->left->value;
        (*i)++;
    } else if (right_complete){
        hashes[*i] = root->right->value;
        (*i)++;
    }
    return 0;
}

void find_hash(char* chunk_buffer, int buffer_size, char final_hash[SHA256_CHUNK_SIZE_WTH_NLL]){
    struct sha256_compute_data sha_buff = {0};    
    uint8_t hashout[SHA256_INT_SZ];

    sha256_compute_data_init(&sha_buff);
    sha256_update(&sha_buff, &chunk_buffer, buffer_size);
    sha256_finalize(&sha_buff, hashout);
    sha256_output_hex(&sha_buff, final_hash);

    printf("Your hash: \n%s\n", final_hash);
}

//Hash computation functions.
void compute_hashes_for_chunks(struct bpkg_obj* bpkg){
    strip_whitespace(&bpkg->filename);
    // printf("Looking for %s\n", bpkg->filename);
    FILE* file = fopen(bpkg->filename, "r");
    
    size_t buffer_size = bpkg->chunks[0]->size;
    char chunk_buffer[buffer_size + 1]; // account for null character    
    char final_hash[SHA256_CHUNK_SIZE_WTH_NLL] = {0};
    struct sha256_compute_data sha_buff = {0};
    uint8_t hashout[SHA256_INT_SZ];

    size_t r_bytes = 0;
    if (file == NULL){
        printf("Unable to open file.\n");
        // fclose(file);
        return;
    }
    int i = 0;
    //Read until the end of file of if read input is less than buffer size.
    while ((r_bytes = fread(chunk_buffer, 1, buffer_size, file)) == buffer_size){
        sha256_compute_data_init(&sha_buff);
        sha256_update(&sha_buff, &chunk_buffer, buffer_size);
        sha256_finalize(&sha_buff, hashout);
        sha256_output_hex(&sha_buff, final_hash);
        
        for (int k = 0; k<sizeof(final_hash); k++){
            bpkg->chunks[i]->node_ptr->computed_hash[k] = final_hash[k];
        }
        // printf("final hash in chunk is:\n%s\n\n", bpkg->chunks[i]->node_ptr->computed_hash);
        i++;
    }

    fclose(file);
}


//Tree creation functions
struct merkle_tree_node * insert_node(char** hashes, int n, struct merkle_tree_node* parent, int i, struct chunks** chunks){
    if (i < n){
        struct merkle_tree_node * current_node = malloc(sizeof(struct merkle_tree_node));
        *current_node = (struct merkle_tree_node)
        { .key = i,
            .value = hashes[i],
            .right = NULL,
            .left = NULL,
            .parent = parent, 
            .is_leaf = 0,
            .computed_hash = "\0"
        };

        current_node->left = insert_node(hashes, n, current_node, 2*i+1, chunks);
        current_node->right = insert_node(hashes, n, current_node, 2*i+2, chunks);

        return current_node;
    } else {
        //i is part of the second chunks array.
        struct merkle_tree_node * current_node = malloc(sizeof(struct merkle_tree_node));
        *current_node = (struct merkle_tree_node)
        { .key = i,
            .value = chunks[i-n]->hash,
            .right = NULL,
            .left = NULL,
            .parent = parent, 
            .is_leaf = 1,
            .computed_hash = "\0"
        };

        chunks[i-n]->node_ptr = current_node;
        
        return current_node;
    }
}

//Deallocate the tree.
//Find a way to use threads here.
void remove_tree(struct merkle_tree_node* root){
    if (root->left){
        remove_tree(root->left);
    } if (root->right){
        remove_tree(root->right);
    }
    // free(root->value); //Value would have been free by now as pointer to hash would have been deleted.
    free(root);
}


void show_tree(struct merkle_tree * tree){
    int h = log2(tree->n_nodes + 1);
    for (int i = 1; i<= h; i++){
        print_current_level(tree->root, i);
    }
}

void print_current_level(struct merkle_tree_node* root, int lvl){
    if (root == NULL)
        return;
    if (lvl == 1)
        printf("%d: %s. is chunk: %d\n", root->key, root->value, root->is_leaf);
    else if (lvl > 1){
        print_current_level(root->left, lvl-1);
        print_current_level(root->right, lvl-1);
    }
}

//Your code here!
struct merkle_tree* create_tree(char** hashes, int nhashes, struct chunks** chunks, int nchunks){
    struct merkle_tree* tree = malloc(sizeof(struct merkle_tree));
    //Create merkle tree for non-chunk nodes.
    struct merkle_tree_node * root = insert_node(hashes, nhashes, NULL, 0, chunks);
    tree->n_nodes = nhashes + nchunks;
    tree->root = root;
    return tree;
}