//EGE GUNEY FILE SYSTEM
#include "sfs_api.h"
#include "disk_emu.h"


//local variables
char cur_filename[] = "Ege_file_sys";

//in memory & on disk
super_block cur_sb;                 //initialize super block
i_node cur_ic[I_NODE_TABLE_LENGTH]; //initialize i cache
dir_entry dir_table[MAX_FILE_NUM];  //initialize directory table
bitmap_entry free_bitmap[NUM_OF_BLOCKS]; //has all blocks in it

//in-memory only
file_descriptor_entry fdt[MAX_FILE_NUM];// 79 files and every one of them has a descriptor
int cur_file = 0;
//same index with directory table

//HELPER FUNCTIONS
void create_super_block(){
    cur_sb.magic = 0xACBD0005;
    cur_sb.block_size = BLOCK_SIZE;
    cur_sb.file_system_size = NUM_OF_BLOCKS;
    cur_sb.i_node_table_length = I_NODE_TABLE_LENGTH_BLOCKS;
    cur_sb.root_dir = ROOT_DIR;
}//create superblock

void create_inode_table(){
    cur_ic[0].link_cnt = 1;
    for(int i = 1; i < I_NODE_TABLE_LENGTH; i++){
        cur_ic[i].link_cnt = 0;
        cur_ic[i].size = 0;
    }
} //create inode cache

void create_directory_table(){
    for(int i = 0; i < MAX_FILE_NUM;i++){
        dir_table[i].is_active = 0; //all is empty
    }
}//create directory table

void create_free_bitmap(){
    //0 is for full | 1 is for empty
    for(int i = 0; i < NUM_OF_BLOCKS; i++){
        if(i < I_NODE_TABLE_LENGTH_BLOCKS + 1){//1 is size of superblock + INODE BLOCKS
            free_bitmap[i].is_empty = '0'; //block is full
        }
        else if(i >= NUM_OF_BLOCKS - 10){ //last 5 bitmap itself | before it 5 blocks for directory
            free_bitmap[i].is_empty = '0'; //bitmap and directory table
        }
        else{
            free_bitmap[i].is_empty = '1'; //block is empty and a data block
        }
    }
}//wow a bitmap amazing

void write_inode_table(){
    char* ic_buffer = malloc(BLOCK_SIZE * I_NODE_TABLE_LENGTH_BLOCKS);
    memcpy(ic_buffer, &cur_ic, sizeof(cur_ic));
    write_blocks(1, I_NODE_TABLE_LENGTH_BLOCKS,ic_buffer);
    free(ic_buffer);
    //Write inode table
}

void write_directory_table(){
    char* dir_buffer = malloc(BLOCK_SIZE * DIR_LENGTH_BY_BLOCKS);
    memcpy(dir_buffer, &dir_table, sizeof(dir_table));
    write_blocks(NUM_OF_BLOCKS - DIR_LENGTH_BY_BLOCKS - FREE_BITMAP_LENGTH_BY_BLOCK, DIR_LENGTH_BY_BLOCKS, dir_buffer); //wooo magic
    free(dir_buffer);
    //Write directory table
}

void write_free_bitmap(){
    char* bitmap_buffer = malloc(BLOCK_SIZE * FREE_BITMAP_LENGTH_BY_BLOCK);
    memcpy(bitmap_buffer, &free_bitmap, sizeof(free_bitmap));
    write_blocks(NUM_OF_BLOCKS - FREE_BITMAP_LENGTH_BY_BLOCK, FREE_BITMAP_LENGTH_BY_BLOCK, bitmap_buffer); //even more magic
    //DEBUG COMMENT printf("+ Done Creating free bitmap blocks...\n");
    free(bitmap_buffer);
    //write free bitmap blocks
}

int find_file_dir_index(char* filename){ //return -1 if there is no file | directory table index if there is one 
    
    //iterate dir table to find if the filename matches to any file inside dir
    for(int i = 0; i < MAX_FILE_NUM; i++){
        //DEBUG COMMENT printf("This dir entry is -> %d\n",dir_table[i].is_active);
        if(dir_table[i].is_active != 0){
            //DEBUG COMMENT printf("real name -> %s , our name -> %s\n",filename, dir_table[i].filename);
            if(strcmp(filename, dir_table[i].filename) == 0){
                //DEBUG COMMENT printf("Directory index found for file %s is %d\n",filename, i);
                return i;
            }
        }
    }
    return -1;

}

int find_available_inode(){ //return avaliable inode number
    //find an inode number that is not being used
    for(int i = 0; i < I_NODE_TABLE_LENGTH;i++){
        if(cur_ic[i].link_cnt == 0){
            return i;
        }
    }
    return -1;//if there is no available inode left !! BIG PROBLEM
}

int find_available_directory_index(){
    //find a directory entry that is not being used
    for(int i = 0; i < MAX_FILE_NUM; i++){
        if(dir_table[i].is_active == 0){
            return i;
        }
    }
    return -1;
}

int find_available_file_descriptor(){
    //find a file descriptor entry that is not being used
    for(int i = 0; i < MAX_FILE_NUM; i++){
        if(fdt[i].is_being_used == 0){
            return i;
        }
    }
    return -1;
}

int find_file_descriptor_index(int i_node_index){
    //find the file descriptor index given an inode number
    int fdt_index = -1;
    for(int i = 0; i < MAX_FILE_NUM; i++){
        
        if(fdt[i].is_being_used == 1){
            
            if(fdt[i].inode_num == i_node_index){

                fdt_index = i;
                break;
            }
            
        }
    }
    return fdt_index;
}

int block_count(int size){
    //blocks needed to write or read the bytes given
    int res = 0;

    for(int i = 0; i < size; i += BLOCK_SIZE){
        res++;
    }

    return res;
}

int find_available_data_block(){

    for(int i = 0; i < NUM_OF_BLOCKS; i++){
        if(free_bitmap[i].is_empty == '1'){ //is empty return the number of block
            return i;
        }
    }
    return -1;
}

int find_block_num_through_rw(int rw, int inode, indirect_block* ind){
    
    if(rw > DIRECT_POINTER_NUM * BLOCK_SIZE){ //in indirect
        int block_num = ((int) rw / BLOCK_SIZE) - 12; 
        return ind->ptr[block_num];
    }
    else{//in direct pointers
        int block_num= (int) rw / BLOCK_SIZE; //number of block rw belongs to
        
        return cur_ic[inode].direct_pointers[block_num];
    }
}

void initialize_indirect_pointer(indirect_block* ind){
    ind->ptr = (int*)calloc(INDIRECT_POINTER_NUM, sizeof(int));
    ind->size = 0;
    //so size of indirect pointers will stay INDIRECT POINTER SIZE as constant when created
}

void read_indirect_block(indirect_block *ind, int i_node_num){ //read indirect block data from disk
    //check for errors turn to pointer logic if there are errors !!!!
    char* indirect_buffer = malloc(BLOCK_SIZE);
    //DEBUG COMMENT printf("Read from: %d\n", cur_ic[i_node_num].indirect_pointer);
    read_blocks(cur_ic[i_node_num].indirect_pointer, 1, indirect_buffer);
    memcpy(ind, indirect_buffer, sizeof(int)); //size copied
    memcpy(ind->ptr, indirect_buffer + sizeof(int), INDIRECT_POINTER_NUM * sizeof(int)); //copied pointers
    //DEBUG COMMENT printf("Indirect block size: %d\n", ind->size);
    free(indirect_buffer);
    //DEBUG COMMENT finish reading indirect block
}

int get_dir_size(){
    int res = 0;
    for(int i = 0; i < MAX_FILE_NUM; i++){
        if(dir_table[i].is_active == 1){
            res++;
        }
    }
    return res;
}
//END OF HELPER FUNCTIONS


// Essential functions 
//TODO: WRITE TABLE METHODS | WRITE AFTER EVERY MODIFICATION
void mksfs(int fresh){
    if(fresh == 1){//create new file system from scratch
        //printf("- Creating file system...\n");

        //initialize disk file 
        int error_check =init_fresh_disk(FILE_SYSTEM_NAME, BLOCK_SIZE, NUM_OF_BLOCKS);
        if(error_check == -1){
            printf("Internal Error: mksfs function|couldn't allocate disk space, external function error!\n");
        }
        //super block
        ////printf("- Creating super block...\n");
        create_super_block();
         //super block is 1 block so write super block onto disk
        char* sb_buffer = malloc(BLOCK_SIZE); //a buffer that is the size of a block
        memcpy(sb_buffer, &cur_sb, sizeof(super_block)); // copy current_super block to a buffer
        write_blocks(0,1, sb_buffer);
        ////printf("+ Done Creating super block...\n");
        free(sb_buffer);
        //!!SHOULD BE WORKING CHECK BUFFER

        //i_node table
        ////printf("- Creating inode table blocks...\n");
        create_inode_table();
        write_inode_table();
        ///directory
        ////printf("- Creating directory table blocks...\n");
        create_directory_table();
        write_directory_table();
        //free bitmap
        ////printf("- Creating free bitmap blocks...\n");
        create_free_bitmap();
        write_free_bitmap();
        
        //finito
        //printf("+ Done creating file system.\n");
    }
    else{ //there is an existing file system in place

        //printf("- Reading file system from disk...\n");

        //read super block from first block
        char* sb_buffer = malloc(BLOCK_SIZE);
        read_blocks(0,1, sb_buffer);
        memcpy(&cur_sb, sb_buffer ,sizeof(super_block));
        free(sb_buffer);
        //read inode table
        char* ic_buffer = malloc(BLOCK_SIZE * I_NODE_TABLE_LENGTH_BLOCKS);
        read_blocks(1, I_NODE_TABLE_LENGTH_BLOCKS, ic_buffer);
        memcpy(&cur_ic, ic_buffer, sizeof(cur_ic));
        free(ic_buffer);
        //read directory
        char* dir_buffer = malloc(BLOCK_SIZE * DIR_LENGTH_BY_BLOCKS);;
        read_blocks(NUM_OF_BLOCKS - DIR_LENGTH_BY_BLOCKS - FREE_BITMAP_LENGTH_BY_BLOCK, DIR_LENGTH_BY_BLOCKS, dir_buffer);
        memcpy(&dir_table, dir_buffer, sizeof(dir_table));
        free(dir_buffer);

        //read bitmap
        char* bitmap_buffer =  malloc(BLOCK_SIZE * FREE_BITMAP_LENGTH_BY_BLOCK);
        read_blocks(NUM_OF_BLOCKS - FREE_BITMAP_LENGTH_BY_BLOCK, FREE_BITMAP_LENGTH_BY_BLOCK, bitmap_buffer);
        memcpy(&free_bitmap, bitmap_buffer, sizeof(free_bitmap));
        free(bitmap_buffer);   
        //printf("+ Done reading file system from disk.\n");
    }
}

int sfs_getnextfilename(char* fname){
    //get the next file's name and if there is another file return 1
    int dir_size = get_dir_size();
    //get size of directory and compare it with our global variable cur_file which holds the index of the file we are on
    if(cur_file >= dir_size){
        return 0; //no file left in directory
    }
    else{
        strcpy(fname,dir_table[cur_file].filename);
        cur_file++;
        return 1; //still file left |  can be called again
    }
}

int sfs_getfilesize(const char* path){
    //filesize is stored in inodes

    char* temp = malloc(MAX_FILENAME);

    strcpy(temp, path);
    int file_index = find_file_dir_index(temp);
    //find file index in directory table

    if(file_index != -1){
        return cur_ic[dir_table[file_index].inode_num].size;
        //if there is a file return the size attribute from inode struct
    }
    else{
        return -1;
    }

}

int sfs_fopen(char* fname){
    //DEBUG COMMENT printf("-Opening file...\n");
    if(strlen(fname) > MAX_FILENAME){
        printf("Tried to create a file with limit exceeding file name.\n");
        return -1;
    }
    
    int file_index = find_file_dir_index(fname); //if there is no file create one
    if(file_index == -1){ //create file
        //DEBUG COMMENT printf("-Creating new file...\n");
        int i_node_allocation_num = find_available_inode();
        
        if(i_node_allocation_num != -1){
            //add entry to i_node
            cur_ic[i_node_allocation_num].link_cnt = 1;

            //add entry to file descriptor table
            int fdt_index = find_available_file_descriptor();
            fdt[fdt_index].inode_num = i_node_allocation_num;
            fdt[fdt_index].is_being_used = 1; //is active
            fdt[fdt_index].rwpointer = 0;
            
            //add entry to directory table (change active state of entry)
            int dir_table_index = find_available_directory_index();
            dir_table[dir_table_index].is_active = 1;
            
            
            strcpy(dir_table[dir_table_index].filename, fname);
            //DEBUG COMMENTprintf("Created file name %s and directory file name is %s\n", fname, dir_table[dir_table_index].filename);
            dir_table[dir_table_index].inode_num = i_node_allocation_num;

            write_inode_table();
            write_directory_table();
            //DEBUG COMMENTprintf("DIR INDEX => %d\n", dir_table[0].is_active);
            return fdt_index;

        }
        else{
            printf("Internal Error: I-NODE limit exceeded!\n");
            return -1;
        }
    }
    else{ //open file if there exists one

        int i_node_index = dir_table[file_index].inode_num;

        //check if it is open
        int old_fdt_index = find_file_descriptor_index(i_node_index);
        if(old_fdt_index == -1){
            int fdt_index = find_available_file_descriptor();
            fdt[fdt_index].inode_num = i_node_index;
            fdt[fdt_index].is_being_used = 1; //is active
            fdt[fdt_index].rwpointer = cur_ic[i_node_index].size;
            //DEBUG COMMENTprintf("+Opened file succesfuly.\n");
            return fdt_index;
        }
        else{//if file is already open -1
            printf("File already open\n");
            return -1;
        }
    }
    
    
}

int sfs_fclose(int fileID){

    //printf("-Closing file...%d\n", fileID);
    int fdt_index = fileID;
    if(fdt_index >= MAX_FILE_NUM){
        printf("Trying to read from an unavailable file number!\n");
        return -1;
    }
    else if(fdt[fdt_index].is_being_used == 0){//file already closed
        printf("File already closed.\n");
        return -1;
    }
    else{
        fdt[fdt_index].is_being_used = 0;
        return 0;
    }
}

int sfs_fwrite(int fileID , const char* buf , int length){
    //printf("Size will write: %d\n", length);

    if(fileID >= MAX_FILE_NUM){ //if there is no file (have to check this first because it will get an error otherwise)
        return -1;
    }
    else if(fdt[fileID].is_being_used == 0){ //if file is not open we cannot write to it
        printf("File not open!\n");
        return -1;
    }
    //ALL POTENTIAL ERRORS AVOIDED MOVE ON//
    int inode = fdt[fileID].inode_num;
    // DEBUG COMMENT printf("Starting to write to file with inode = %d\n",inode);
    int num_bytes_written = 0; //in one write cycle set and return at the end
    indirect_block* ind = 0; //specify that there is an indirect block

    if(fdt[fileID].rwpointer > 0){ //if file is not empty
        // DEBUG COMMENTprintf("file not empty: %d\n", fdt[fileID].rwpointer);
        // DEBUG COMMENTprintf("File is not empty\n");
        int block_index = (int) fdt[fileID].rwpointer / BLOCK_SIZE; //index in inode
        int last_point = fdt[fileID].rwpointer % BLOCK_SIZE; //pointer number in last block
        if(block_index >= DIRECT_POINTER_NUM){ //there is an indirection block
            // DEBUG COMMENTprintf("Reading indirect block...\n");
            ind = malloc(BLOCK_SIZE);
            initialize_indirect_pointer(ind);
            read_indirect_block(ind,inode); //update ind block
            //we set indirect block if there is one
        }
        
        if(fdt[fileID].rwpointer % BLOCK_SIZE != 0){ //there is a half written block
            
            int block_num = find_block_num_through_rw(fdt[fileID].rwpointer, inode, ind); //find block number in disk
            // DEBUG COMMENT printf("write half block to : %d\n", block_num);
            int num_bytes_to_write = 0;
            if(last_point + length > BLOCK_SIZE){//we are filling the block
                num_bytes_to_write = BLOCK_SIZE - last_point; 
            }
            else{//block still has space left after write command
                num_bytes_to_write = length;
            }

            // DEBUG COMMENTprintf("num bytes to write: %d\n", num_bytes_to_write);
            //read the last block written
            char* fill_buffer = malloc(BLOCK_SIZE);
            read_blocks(block_num, 1, fill_buffer);
            // DEBUG COMMENTprintf("what did we read: %s\n", fill_buffer);
            // DEBUG COMMENTprintf("last point: %d\n", last_point);
            memcpy(fill_buffer + last_point, buf, num_bytes_to_write);
            // DEBUG COMMENTprintf("what did we want to write: %s\n", buf);
            // DEBUG COMMENTprintf("what did we write: %s\n", fill_buffer + last_point);
            write_blocks(block_num,1, fill_buffer);
            
            free(fill_buffer);


            //update values so we can start allocating blocks if we have to
            buf += num_bytes_to_write;
            num_bytes_written += num_bytes_to_write;
            fdt[fileID].rwpointer += num_bytes_to_write; //if filling the block it should be dividable by block size
            length -= num_bytes_to_write;
            cur_ic[inode].size += num_bytes_to_write;
        }
    }
    if(length > 0){ //there is still data to write SO WRITE TO NEW BLOCK
            
        //fill the data block
        int required_blocks = block_count(length);
        int temp_length = length;

        for(int i = 0; i < required_blocks; i++){ //go through the same process for every block

            //write to new block
            int allocation_num = find_available_data_block();
            free_bitmap[allocation_num].is_empty = '0';
            char* write_buf = malloc(BLOCK_SIZE);
            int num_bytes_to_write;
            if(temp_length > BLOCK_SIZE){ //check the given length if smaller then block size write only the given length
                num_bytes_to_write = BLOCK_SIZE;
            }
            else{
                num_bytes_to_write = temp_length;
            }
            
            memcpy(write_buf, buf, num_bytes_to_write);//copy the remaining part
            //DEBUG COMMENT printf("what did we write to new block: %s\n", write_buf);
            write_blocks(allocation_num, 1, write_buf);
            free(write_buf);

            char* fill_buffer = malloc(BLOCK_SIZE);
            read_blocks(allocation_num, 1, fill_buffer);
            // DEBUG COMMENTprintf("what did we read after we write: %s\n", fill_buffer);
            free(fill_buffer);
            //FINISH WRITING TO DATA BLOCK

            int pointer_num = (int) (cur_ic[inode].size / BLOCK_SIZE);
            //printf("INODE NUM = %d\n",inode);
            if(pointer_num >= DIRECT_POINTER_NUM){
                //if there is a need for indirect pointers look if there exists and indirect pointer
                if(ind == 0){ //if not create one
                    //CREATION OF INDIRECT NODE
                    
                    ind = malloc(BLOCK_SIZE);
                    initialize_indirect_pointer(ind);
                    int ind_block_num = find_available_data_block();
                    
                    free_bitmap[ind_block_num].is_empty = '0';
                    cur_ic[inode].indirect_pointer = ind_block_num;
                    // DEBUG COMMENTprintf("Creating");
                    // DEBUG COMMENTprintf("INDIRECT SIZE AFTER INIT = %d\n", ind->size);

                    //WRITE INDIRECT BLOCK TO DISK
                    char *ind_buffer = malloc(BLOCK_SIZE);
                    memcpy(ind_buffer, ind, sizeof(int)); //DIFFERENT PROCESS TO WRITE BECAUSE IT IS A STRUCT POINTER!!!!
                    memcpy(ind_buffer + sizeof(int), ind->ptr, INDIRECT_POINTER_NUM * sizeof(int));
                    // DEBUG COMMENTprintf("Writing an indirect block in: %d\n", cur_ic[inode].indirect_pointer);
                    write_blocks(cur_ic[inode].indirect_pointer, 1, ind_buffer);
                    free(ind_buffer);
                    //WRITTEN TO INDIRECT BLOCK SLOT
                }

                if(ind->size >= INDIRECT_POINTER_NUM){//if the size of indirect blocks exceed limit
                    printf("Error: SIZE OF INDIRECT EXCEEDS LIMIT\n");
                    return -1;
                }
                else{ //if indirect blocks do not exceed limit
                    //DEBUG COMMENT printf("File written to indirect block num %d\n", ind->size);
                    ind->ptr[ind->size] = allocation_num;
                    ind->size += 1;
                }
            }
            else{ //ONLY DIRECT POINTER
                cur_ic[inode].direct_pointers[pointer_num] = allocation_num;
                //printf("Written to file-> %d\n", allocation_num);
            }

            //update all necessary variables regarding write data for the loop
            temp_length -= num_bytes_to_write;
            buf += num_bytes_to_write;
            fdt[fileID].rwpointer += num_bytes_to_write;
            num_bytes_written += num_bytes_to_write;
            cur_ic[inode].size += num_bytes_to_write;
        }
            //loop finished

    }

    if(ind != 0){
            //!!!if there is an indirection block update and write it again
            //printf("Writing updated indirect block to: %d\n",cur_ic[inode].indirect_pointer);
            char *ind_buffer = malloc(BLOCK_SIZE);
            memcpy(ind_buffer, ind, sizeof(int)); //DIFFERENT PROCESS TO WRITE BECAUSE IT IS A STRUCT POINTER!!!!
            memcpy(ind_buffer + sizeof(int), ind->ptr, INDIRECT_POINTER_NUM * sizeof(int));
            write_blocks(cur_ic[inode].indirect_pointer, 1, ind_buffer);
            //printf("Written ind size: %d\n", ind->size);

            free(ind_buffer);
            free(ind->ptr);
            free(ind);
            
            //free just to be safe
    }

    //********TODO: WRITE ALL TABLES AFTER MODIFICATION ******
    //VERY IMPORTANT
    // DEBUG COMMENT printf("NUM BYTES WRITTEN:\n%d\n", num_bytes_written);
    write_inode_table();
    write_directory_table();
    write_free_bitmap();
    return num_bytes_written;
}

int sfs_fread(int fileID, char* buf, int length){
    
    if(fileID >= MAX_FILE_NUM){ //if there is no file (have to check this first because it will get an error otherwise)
        return -1;
    }
    else if(fdt[fileID].is_being_used == 0){ //if file is not open we cannot read from it
        printf("File not open!\n");
        return -1;
    }
    else if(length < 0){
        printf("length < 0\n");
        return -1;
    }
    int num_bytes_read = 0;
    int num_bytes_left_to_read;
    int inode = fdt[fileID].inode_num;
    indirect_block* ind = 0;
    int data_block_place = (int) (fdt[fileID].rwpointer / BLOCK_SIZE);
    if(length > cur_ic[inode].size){ //if we are trying to read more than what is written
        //we can only read up to the size of data
        num_bytes_left_to_read = cur_ic[inode].size;
    }
    else{
        //under default circumstances
        num_bytes_left_to_read = length;
    }
    
    // DEBUG COMMENTprintf("Number of bytes wanted to read:\t%d\n", length);
    // DEBUG COMMENTprintf("Required blocks to read:\t%d\n", required_blocks);
    // DEBUG COMMENTprintf("Size read: %d\n", num_bytes_left_to_read);
    while(num_bytes_left_to_read > 0){//go through every block that we can read ._.
        int num_bytes_being_read = 0;
        int read_block_num;
        if(data_block_place >= DIRECT_POINTER_NUM){ //indirect pointers are needed
            ind = malloc(BLOCK_SIZE);
            initialize_indirect_pointer(ind);
            read_indirect_block(ind,inode);
            read_block_num = ind->ptr[data_block_place - DIRECT_POINTER_NUM]; //place in read block numbers
        }
        else{//use direct pointers

            read_block_num = cur_ic[inode].direct_pointers[data_block_place];
            //point to data block in direct pointer
        }
        if(fdt[fileID].rwpointer % BLOCK_SIZE + num_bytes_left_to_read > BLOCK_SIZE){
            num_bytes_being_read = BLOCK_SIZE - fdt[fileID].rwpointer % BLOCK_SIZE; //read whole block from pointer
        }
        else{
            num_bytes_being_read = num_bytes_left_to_read; //read what is left
        }

        //READ FILE BLOCK
        char* read_buffer = malloc(BLOCK_SIZE);
        read_blocks(read_block_num, 1, read_buffer);
        memcpy(buf + num_bytes_read, read_buffer + fdt[fileID].rwpointer % BLOCK_SIZE, num_bytes_being_read);
        free(read_buffer);
        //FINISH READING FILE BLOCK

        //update pointers and variables that hold read data in the next block
        num_bytes_left_to_read -= num_bytes_being_read;
        num_bytes_read += num_bytes_being_read;
        fdt[fileID].rwpointer += num_bytes_being_read;
        data_block_place = (int) (fdt[fileID].rwpointer / BLOCK_SIZE);

    }
    if(ind != 0) {//MEMORY PURPOSES
        free(ind->ptr);
        free(ind); //If there is an indirect block left in memory cleanse it.
    } 
    return num_bytes_read;

}

int sfs_fseek(int fileID, int loc){
    //straightforward pointer mover
    int fdt_index = fileID;
    if(fdt_index >= 0){
        fdt[fdt_index].rwpointer = loc; 
        return 0;
    }
    else{
        return -1;
    }

} //moves file pointer to location

int sfs_remove(char* file){ //we are not erasing what was inside file!! so it can be read later on accidentally
    //finding directory through file name
    //printf("Removing => %s\n", file);
    char* name_found = 0;
    int removal_index = -1; //directory index of the file that is going to be removed
    int inode;
    for(int i = 0; i < MAX_FILE_NUM; i++){
        if(dir_table[i].is_active == 1){ //if there is a file in that spot

            name_found = malloc(MAX_FILENAME);
            name_found = dir_table[i].filename;
            if(strcmp(name_found, file) == 0){
                removal_index = i;
                inode = dir_table[i].inode_num;
                break;
            }
            //find the directory index of file
        }
    }

    if(removal_index == -1){
        //there is no file
        printf("Internal Error: There is no file\n");
        return -1;
    }
    indirect_block* ind = 0;
    int pointer_num = (int) (cur_ic[inode].size / BLOCK_SIZE);
    for(int i = 0; i < pointer_num; i++){

        if(i >= DIRECT_POINTER_NUM){ //indirect removal
            if(ind == 0){ //read indirect block
                ind = malloc(BLOCK_SIZE);
                initialize_indirect_pointer(ind);
                read_indirect_block(ind,inode);
            }
            int deallocation_num = ind->ptr[i - DIRECT_POINTER_NUM];
            free_bitmap[deallocation_num].is_empty = '1'; //free the block in indirect pointer
        }
        else{ //direct pointer removal
            int deallocation_num = cur_ic[inode].direct_pointers[i];
            free_bitmap[deallocation_num].is_empty = '1'; //free the block in direct pointer
        }
    }
    cur_ic[inode].size = 0;
    cur_ic[inode].link_cnt = 0;
    
    int dir_size = get_dir_size();
    
    for(int i = 0 ; i < dir_size; i++){ //go through every file except root dir
        if(i >= removal_index){
            //need to change directory table so we do not have any space in between entries!!
            dir_table[i] = dir_table[i + 1];
        }

    }
    if(ind != 0){ //if there is an indirect block remove it as well 
        int deallocation_num = cur_ic[inode].indirect_pointer;
        free_bitmap[deallocation_num].is_empty = '1'; //free the block
        free(ind->ptr);
        free(ind);
        //free allocated memory
    }
    write_inode_table();
    write_directory_table();
    write_free_bitmap();
    return 0; //return 0 on success of removal
}
//END OF FILE



