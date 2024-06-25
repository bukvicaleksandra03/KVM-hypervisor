#ifndef file_handling_h
#define file_handling_h

#include <stdio.h>
#include <stdlib.h>
#include "mini_hypervisor.h"

#define NO_OP 0
#define FOPEN_CODE 1
#define FCLOSE_CODE 2
#define FREAD_CODE 3
#define FWRITE_CODE 4

#define MAX_FILES_CNT 10

struct fileList
{
    FILE **list;
    char **names;
    int cnt;
};

struct pathName
{
    char *path_name;
    uint8_t path_length;
    uint8_t path_name_ptr;
};

struct fwriteArgs
{
    uint8_t *writeBuffer;
    uint8_t cntToWrite;
    uint8_t cntWritten;
    uint8_t fd;
    uint8_t fd_initialised;
};

struct fcloseArgs
{
    uint8_t fd;
    uint8_t fd_initialised;
};

struct freadArgs
{
    uint8_t *readBuffer;
    uint8_t cntToRead;
    uint8_t cntRead;
    uint8_t fd;
    uint8_t fd_initialised;
};

void check_already_exists(char *path_name, struct fileList *localFiles);

void handle_fopen(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct pathName *pn);

void handle_fwrite(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct fwriteArgs *fwa, char charId);

void handle_fclose(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct fcloseArgs *fca);

void handle_fread(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct freadArgs *fra);

#endif