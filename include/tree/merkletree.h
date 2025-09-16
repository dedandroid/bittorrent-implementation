#ifndef MERKLE_TREE_H
#define MERKLE_TREE_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_HEXLEN (64)

struct merkle_tree_node {
    int key;
    char* value; // Value must always be a string
    struct merkle_tree_node* left;
    struct merkle_tree_node* right;
    struct merkle_tree_node* parent;
    int is_leaf;
    // char expected_hash[SHA256_HEXLEN];
    char computed_hash[SHA256_HEXLEN];
    // char * computed_hash;
};


struct merkle_tree {
    struct merkle_tree_node* root;
    size_t n_nodes;
};

struct chunks{
	char* hash;
	uint32_t offset;
	uint32_t size;
    struct merkle_tree_node* node_ptr;
};

//Tree creation definitions
struct merkle_tree* create_tree(char** hashes, int nhashes, struct chunks** chunks, int nchunks);
struct merkle_tree_node * insert_node(char** hashes, int n, struct merkle_tree_node* parent, int i, struct chunks** chunks);

void show_tree(struct merkle_tree * tree);
void remove_tree(struct merkle_tree_node* root);
void print_current_level(struct merkle_tree_node* root, int lvl);
void inorder_search(struct merkle_tree_node * root, void (*func_ptr)(struct merkle_tree_node*, char**, int*), char** hashes, int* i);
struct merkle_tree_node* inorder_find_node(struct merkle_tree_node* root, char* hash);
int min_hash_find(struct merkle_tree_node* root, char** hashes, int* i);

#endif
