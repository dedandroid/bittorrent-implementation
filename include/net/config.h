#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

struct config {
    char* directory;
    int max_peers;
    uint16_t port;
};

struct config * config_load(const char* path);

#endif