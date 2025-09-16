#ifndef PKGCHK_H
#define PKGCHK_H

#include <stddef.h>
#include <stdint.h>
#include "tree/merkletree.h"
#include "crypt/sha256.h"

#define SHA256_CHUNK_SIZE_WTH_NLL (65)

//Simple helper functions
int is_any_space(char word);
void strip_whitespace(char** word);
int is_an_int(char* str);


/**
 * Query object, allows you to assign
 * hash strings to it.
 * Typically: malloc N number of strings for hashes
 *    after malloc the space for each string
 *    Make sure you deallocate in the destroy function
 */
struct bpkg_query {
	char** hashes;
	size_t len;
};

//TODO: Provide a definition
struct bpkg_obj{
	char* ident;
	char* filename;
	uint32_t size;
	uint32_t nhashes;
	//pointer to an array of char pointers
	char** hashes;
	uint32_t nchunks;

	//Array of chunks
	struct chunks ** chunks;
	struct merkle_tree * tree;
};


/**
 * Loads the package for when a value path is given
 */
struct bpkg_obj* bpkg_load(const char* path, char* dir);
/**
 * Checks to see if the referenced filename in the bpkg file
 * exists or not.
 * @param bpkg, constructed bpkg object
 * @return query_result, a single string should be
 *      printable in hashes with len sized to 1.
 * 		If the file exists, hashes[0] should contain "File Exists"
 *		If the file does not exist, hashes[0] should contain "File Created"
 */
struct bpkg_query* bpkg_file_check(struct bpkg_obj* bpkg);

/**
 * Retrieves a list of all hashes within the package/tree
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query* bpkg_get_all_hashes(struct bpkg_obj* bpkg);

/**
 * Retrieves all completed chunks of a package object
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query* bpkg_get_completed_chunks(struct bpkg_obj* bpkg);


/**
 * Gets the mininum of hashes to represented the current completion state
 * Example: If chunks representing start to mid have been completed but
 * 	mid to end have not been, then we will have (N_CHUNKS/2) + 1 hashes
 * 	outputted
 *
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query* bpkg_get_min_completed_hashes(struct bpkg_obj* bpkg); 


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
struct bpkg_query* bpkg_get_all_chunk_hashes_from_hash(struct bpkg_obj* bpkg, char* hash);


/**
 * Deallocates the query result after it has been constructed from
 * the relevant queries above.
 */
void bpkg_query_destroy(struct bpkg_query* qry);

/**
 * Deallocates memory at the end of the program,
 * make sure it has been completely deallocated
 */
void bpkg_obj_destroy(struct bpkg_obj* obj);


//functions that are function pointers input to in-order search visits
void all_hash_func(struct merkle_tree_node* node, char** hashes, int* i);
void get_chunks_from_hash_func(struct merkle_tree_node* node, char** hashes, int* i);
void get_completed_chunks(struct merkle_tree_node* node, char** hashes, int* i);


//Hash computation
void find_hash(char* chunk_buffer, int buffer_size, char final_hash[SHA256_CHUNK_SIZE_WTH_NLL]);
void compute_hashes_for_chunks(struct bpkg_obj* bpkg);
char* compute_hashes_for_tree(struct merkle_tree_node* root);



#endif

