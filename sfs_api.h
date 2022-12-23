#ifndef SFS_API_H
#define SFS_API_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//constants
#define FILE_SYSTEM_NAME "Ege_file_system"
#define BLOCK_SIZE 1024 //recommended size
#define NUM_OF_BLOCKS 5120 //blocks > block_size x5 because extra space is good
#define I_NODE_TABLE_LENGTH_BLOCKS 5 //blocks, 80 i_nodes, 80 x 64 bytes
#define I_NODE_TABLE_LENGTH 80 //total of 80 i_nodes
#define I_NODES_IN_A_BLOCK 16 //by bytes
#define MAX_FILE_NUM 79 //80 i_nodes, 1 for directory, 79 files
#define MAX_FILENAME 54 //upper limit of filename 54 + pointers => 56
#define ROOT_DIR 0
#define DIR_LENGTH_BY_BLOCKS 5
#define FREE_BITMAP_LENGTH_BY_BLOCK 5
#define FREE_BITMAP_LENGTH_PER_BLOCK 1024 //1 char for 1 block 0 if full | 1 if empty
#define INDIRECT_POINTER_NUM 22 //to be safe 30 blocks per file left open so a lot of free space left
#define INDIRECT_POINTER_SIZE 92 //bytes
/*#define INDIRECT_COUNT 80
#define INDIRECT_TOTAL_BYTES 7360*/
#define DIRECT_POINTER_NUM 12 //default
//->filesize 34000 max can be created this way

// You can add more into this file.

//variables
//------
typedef struct{
    int magic;
    int block_size; //by bytes
    int file_system_size; //by block
    int i_node_table_length; //by block
    int root_dir; // root directory i_node -> 0
}super_block;

typedef struct{
    int mode;
    int link_cnt; //1 if full | 0 if empty
    int size;   
    int direct_pointers[12];
    int indirect_pointer;
}i_node; // size -> 64 bytes

typedef struct{

    int size;
    int* ptr; //22 indirect entries to data blocks

}indirect_block;

typedef struct{
    char filename[MAX_FILENAME]; // 56
    int is_active; //0 if empty | 1 if full
    int inode_num; // respective i_node number
}dir_entry; //28 + 4 => 32 bytes
//64 * 79 files -> 5 blocks half of last block is empty

typedef struct{
    char is_empty; //1 if empty | 0 if full
}bitmap_entry;


typedef struct{
    int inode_num;
    int rwpointer; //read & write pointer
    int is_being_used; //if it is actively being used -> 1 | otherwise 0
}file_descriptor_entry;


void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, const char*, int);

int sfs_fread(int, char*, int);

int sfs_fseek(int, int);

int sfs_remove(char*);

#endif
