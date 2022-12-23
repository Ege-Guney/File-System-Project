#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "disk_emu.h"


FILE* fp = NULL;
double L, p;
double r;
int BLOCK_SIZE, MAX_BLOCK, MAX_RETRY;

/*----------------------------------------------------------*/
/*Close the disk file filled when you don't need it anymore. */
/*----------------------------------------------------------*/
int close_disk()
{
    if(NULL != fp)
    {
        fclose(fp);
    }
    return 0;
}

/*---------------------------------------*/
/*Initializes a disk file filled with 0's*/
/*---------------------------------------*/
int init_fresh_disk(char *filename, int block_size, int num_blocks)
{
    int i, j;

    BLOCK_SIZE = block_size;
    MAX_BLOCK = num_blocks;
    
    /*Initializes the random number generator*/
    srand((unsigned int)(time( 0 )) );
    /*Creates a new file*/
    fp = fopen (filename, "w+b");

    if (fp == NULL)
    {
        printf("Could not create new disk file %s\n\n", filename);
        return -1;
    }
    
    /*Fills the file with 0's to its given size*/
    for (i = 0; i < MAX_BLOCK; i++)
    {
        for (j = 0; j < BLOCK_SIZE; j++)
        {
            fputc(0, fp);
        }
    }
    return 0;
}
/*----------------------------*/
/*Initializes an existing disk*/
/*----------------------------*/
int init_disk(char *filename, int block_size, int num_blocks)
{
    BLOCK_SIZE = block_size;
    MAX_BLOCK = num_blocks;
    
    /*Opens a file*/
    fp = fopen (filename, "r+b");

    if (fp == NULL)
    {
        printf("Could not open %s\n\n", filename);
        return -1;
    }
    return 0;
}

/*-------------------------------------------------------------------*/
/*Reads a series of blocks from the disk into the buffer             */
/*-------------------------------------------------------------------*/
int read_blocks(int start_address, int nblocks, void *buffer)
{
    int i, s;
    s = 0;

    /*Sets up a temporary buffer*/
    void* blockRead = (void*) malloc(BLOCK_SIZE);

    /*Checks that the data requested is within the range of addresses of the disk*/
    if (start_address + nblocks > MAX_BLOCK)
    {
        printf("out of bound error %d\n", start_address);
        return -1;
    }

    /*Goto the data requested from the disk*/
    fseek(fp, start_address * BLOCK_SIZE, SEEK_SET);

    /*For every block requested*/
    for (i = 0; i < nblocks; ++i)
    {
        s++;
        fread(blockRead, BLOCK_SIZE, 1, fp);
        memcpy((char *)buffer+(i*BLOCK_SIZE), blockRead, BLOCK_SIZE);  
    }

    free(blockRead);
    return s;
}

/*------------------------------------------------------------------*/
/*Writes a series of blocks to the disk from the buffer             */
/*------------------------------------------------------------------*/
int write_blocks(int start_address, int nblocks, void *buffer)
{
    int i, s;
    s = 0;

    void* blockWrite = (void*) malloc(BLOCK_SIZE);

    /*Checks that the data requested is within the range of addresses of the disk*/
    if (start_address + nblocks > MAX_BLOCK)
    {
        printf("out of bound error\n");
        return -1;
    }

    /*Goto where the data is to be written on the disk*/        
    fseek(fp, start_address * BLOCK_SIZE, SEEK_SET);

    /*For every block requested*/        
    for (i = 0; i < nblocks; ++i)
    {
        /*Pause until the latency duration is elapsed*/
        usleep(L);
        
        memcpy(blockWrite, (char *)buffer+(i*BLOCK_SIZE), BLOCK_SIZE);
        
        fwrite(blockWrite, BLOCK_SIZE, 1, fp);
        fflush(fp);
        s++;
    }
    free(blockWrite);
    return s;
}
