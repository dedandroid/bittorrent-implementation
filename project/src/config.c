#include "net/config.h"
#include "chk/pkgchk.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

//Max path length is 4095 characters in linux
#define MAX_PATH_LEN 4095 
#define MAX_PEER_RANGE 2048
#define MIN_PORT_RANGE 1024
#define MAX_PORT_RANGE 65535



struct config * config_load(const char* path){
    struct config * config_file = malloc(sizeof(struct config));

    char buffer[MAX_PATH_LEN];
    char* key = NULL;
    char* value = NULL;

    FILE* file = fopen(path, "r");
    
    if (file != NULL){
        while (fgets(buffer, MAX_PATH_LEN, file)){
            key = strtok(buffer, ":");
            value = strtok(NULL, ":");
            strip_whitespace(&value);

            if (strcmp(key, "directory") == 0){

                char* str = malloc(strlen(value)+1);
                strcpy(str, value);

                struct stat s;
                
                if (!( stat(value, &s) == 0 && S_ISDIR(s.st_mode) )){
                    //Directory doesn't exist. Creating.
                    int rv = mkdir(value, 0777);

                    if (rv != 0){
                        //can't create the directory
                        free(str);
                        free(config_file);
                        exit(3);
                    }
                }
                config_file->directory = str;

            } else if (strcmp(key, "max_peers") == 0){

                //Handle cases of decimals/non numerals in strings
                if (!is_an_int(value)){
                    printf("Max peers is not an integer.\n");
                    free(config_file->directory);
                    free(config_file);
                    exit(-1);
                }

                int val = atoi(value);
                
                if ((val < 1) || (val > MAX_PEER_RANGE)){
                    printf("Incorrect max peers range [1,1024]\n");
                    free(config_file->directory);
                    free(config_file);
                    exit(4);
                } else{
                    config_file->max_peers = val;
                }

            } else if (strcmp(key, "port") == 0){

                if (!is_an_int(value)){
                    printf("Port is not an integer.\n");
                    free(config_file->directory);
                    free(config_file);
                    exit(-1);
                }

                int val = atoi(value);

                //Handle cases of decimals/no numerals in strings
                if ((val <= MIN_PORT_RANGE ) || (val > MAX_PORT_RANGE)){
                    printf("Incorrect port range (1024, 65535]\n");
                    free(config_file->directory);
                    free(config_file);
                    exit(5);
                } else {
                    config_file->port = val;
                }
            }
        }
    } else {
        printf("Path doesn't exist\n");
    }

    return config_file;

}


