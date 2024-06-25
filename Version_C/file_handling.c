
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <linux/kvm.h>
#include "file_handling.h"

void check_already_exists(char *path_name, struct fileList *localFiles)
{
    for (int i = 0; i < localFiles->cnt; i++)
    {
        if (strcmp(path_name, localFiles->names[i]) == 0)
        {
            perror("File already opened.");
            exit(-1);
        }
    }
};

void addCharBeforeLastDot(char *str, char ch)
{
    char *lastDot = strrchr(str, '.'); // Find the last occurrence of '.'

    if (lastDot != NULL)
    {
        // Calculate the position to insert 'ch'
        size_t insertPos = lastDot - str;

        // Shift characters to the right to make space for 'ch'
        memmove(str + insertPos + 1, str + insertPos, strlen(str) - insertPos + 1);

        // Insert 'ch' before the last '.'
        str[insertPos] = ch;
    }
}

void handle_fopen(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct pathName *pn)
{
    uint8_t input = *((uint8_t *)vm->kvm_run + vm->kvm_run->io.data_offset);

    if (pn->path_name == NULL)
    {
        pn->path_length = input;
        pn->path_name = (char *)malloc((pn->path_length + 1) * sizeof(char));
        pn->path_name_ptr = 0;
    }
    else
    {
        pn->path_name[pn->path_name_ptr++] = (char)input;
        if (pn->path_name_ptr != pn->path_length)
            return;

        pn->path_name[pn->path_name_ptr] = '\0';

        for (int i = 0; i < globalFiles->cnt; i++)
        {
            if (strcmp(pn->path_name, globalFiles->names[i]) == 0)
            {
                *retVal = MAX_FILES_CNT + i; // return value is the index in the array of global files + MAX_FILES_CNT
                return;
            }
        }

        check_already_exists(pn->path_name, localFiles);

        FILE *fd = fopen(pn->path_name, "w+");
        if (!fd)
        {
            perror("File opening failed");
            exit(-1);
        }
        localFiles->names[localFiles->cnt] = (char *)malloc((pn->path_length + 1) * sizeof(char));
        strcpy(localFiles->names[localFiles->cnt], pn->path_name);
        localFiles->list[localFiles->cnt] = fd;
        *retVal = localFiles->cnt; // return value is the index in the array of local files
        localFiles->cnt++;

        free(pn->path_name);
        pn->path_name = NULL;
    }
};

void handle_fwrite(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct fwriteArgs *fwa, char charId)
{
    uint8_t input = *((uint8_t *)vm->kvm_run + vm->kvm_run->io.data_offset);

    if (fwa->fd_initialised == 0)
    {
        fwa->fd = input;
        fwa->fd_initialised = 1;
        return;
    }

    if (fwa->cntToWrite == 0)
    {
        fwa->cntToWrite = input;
        fwa->writeBuffer = (uint8_t *)malloc(fwa->cntToWrite);
        fwa->cntWritten = 0;
    }
    else
    {
        fwa->writeBuffer[fwa->cntWritten++] = input;

        if (fwa->cntWritten == fwa->cntToWrite)
        {
            if (fwa->fd >= MAX_FILES_CNT)
            {
                char *name = globalFiles->names[fwa->fd - MAX_FILES_CNT];
                size_t len = strlen(name) + 2;
                localFiles->names[localFiles->cnt] = (char *)malloc(len * sizeof(char));
                strcpy(localFiles->names[localFiles->cnt], name);
                addCharBeforeLastDot(localFiles->names[localFiles->cnt], charId);
                localFiles->list[localFiles->cnt] = fopen(localFiles->names[localFiles->cnt], "w+");

                fwrite(fwa->writeBuffer, fwa->cntToWrite, 1, localFiles->list[localFiles->cnt]);
                *retVal = localFiles->cnt;
                localFiles->cnt++;
            }
            else
                fwrite(fwa->writeBuffer, fwa->cntToWrite, 1, localFiles->list[fwa->fd]);

            fwa->fd_initialised = 0;
            fwa->cntToWrite = 0;
            free(fwa->writeBuffer);
        }
    }
}

void handle_fclose(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct fcloseArgs *fca)
{
    uint8_t input = *((uint8_t *)vm->kvm_run + vm->kvm_run->io.data_offset);

    if (fca->fd_initialised == 0)
    {
        fca->fd = input;
        fca->fd_initialised = 1;
        return;
    }
    else
    {
        if (fca->fd >= MAX_FILES_CNT)
        {
            return;
        }
        else
        {
            fca->fd_initialised = 0;
            fclose(localFiles->list[fca->fd]);
            localFiles->list[fca->fd] = NULL;
            localFiles->names[fca->fd] = NULL;
        }
    }
}

void handle_fread(int *retVal, struct vm *vm, struct fileList *localFiles, struct fileList *globalFiles, struct freadArgs *fra)
{
    uint8_t input = *((uint8_t *)vm->kvm_run + vm->kvm_run->io.data_offset);

    if (fra->fd_initialised == 0)
    {
        fra->fd = input;
        fra->fd_initialised = 1;
        return;
    }

    if (fra->cntToRead == 0)
    {
        fra->cntToRead = input;
        fra->cntRead = 0;

        fra->readBuffer = malloc(fra->cntToRead);

        if (fra->fd >= MAX_FILES_CNT)
        {
            fseek(globalFiles->list[fra->fd - MAX_FILES_CNT], 0, SEEK_SET);
            fread(fra->readBuffer, fra->cntToRead, 1, globalFiles->list[fra->fd - MAX_FILES_CNT]);
        }
        else
        {
            fread(fra->readBuffer, fra->cntToRead, 1, localFiles->list[fra->fd]);
        }
        fra->fd_initialised = 0;
    }
}