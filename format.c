/*
2000 bytes

2048 bytes


t=make a disk of size 1,000,000 bytes
512 boot block
512 super block

2048 inodes - get the sizeof my inode


look for file called disk,
if it exists, mount it to a big buffer
issue is sblock is bigger than it should be
 Add a "-s #" flag to allow the creation of different-sized disks, where # is a number in megabytes.
*/
#include "format.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/stat.h>  // For S_IRUSR and S_IWUSR

int main()
{
    FILE *file_sys;
    struct superblock s_block;
    char block_buffer[BLOCK_SIZE];
    memset(block_buffer, 0, BLOCK_SIZE);

    file_sys = fopen("DISK", "w+b");

    //zero out the boot block
    fwrite(block_buffer,  BLOCK_SIZE, 1, file_sys);

    //move beyond the boot block
    fseek(file_sys, BLOCK_SIZE, SEEK_SET);
    //zero out the super block
    fwrite(block_buffer, BLOCK_SIZE, 1,file_sys);

    s_block.inode_offset = INODE_OFFSET;
    s_block.data_offset = DATA_OFFSET;
    s_block.size = BLOCK_SIZE;
    s_block.free_block = DATA_OFFSET;
    s_block.free_inode = INODE_OFFSET;
    s_block.current_dir = 0;//start at root directory

    //initialize a root directory, direct blocks of all inodes
   
    struct inode current_in;
    struct inode root_in;
    char name[] = "root dir";
    memset(&root_in.name, 0, sizeof(root_in.name));
    strcpy(root_in.name,name );
    
    root_in.type = 0;
    root_in.size = 0;
    root_in.num_children = 0;
    root_in.permissions = (0 | S_IRUSR | S_IWUSR);

    int next_free_block = 0;
    int next_free_inode = 2;// start 1 up from 1st non-root inodew
    for (int j = 0; j < N_DBLOCKS; j++) {
      root_in.dblocks[j] = next_free_block;
      next_free_block++;
    }
    int mem_ptr = INODE_OFFSET;
    fseek(file_sys, mem_ptr, SEEK_SET);
    fwrite((void*)&root_in,  sizeof(inode), 1, file_sys);

    fseek(file_sys, mem_ptr, SEEK_SET);
    memset(&root_in, 0, sizeof(struct inode));
    fread((void*)&root_in, sizeof(inode), 1, file_sys );
    printf("name is %s last idx %d\n",root_in.name,root_in.dblocks[9]);
    printf("inode size is %ld now more inodes\n", sizeof(inode));
    printf("number of direct blocks %d\n", N_DBLOCKS);

    size_t num_inodes = ((char*)DATA_OFFSET - (char*)INODE_OFFSET) / sizeof(struct inode);

    int num_blocks = (1000000 - DATA_OFFSET)/BLOCK_SIZE;
    for (size_t i = 1; i < num_inodes; i++) {
        printf("formatted inode idx %ld\n", i);
        memset(&current_in, 0, sizeof(struct inode));
        for (int j = 0; j < N_DBLOCKS; j++) {
            current_in.dblocks[j] = next_free_block;
            printf("data block %d\n", next_free_block);
            next_free_block++;
        }
        current_in.next_inode = next_free_inode;
        next_free_inode++;
        mem_ptr = INODE_OFFSET + i*BLOCK_SIZE;
        fseek(file_sys, mem_ptr, SEEK_SET);
        fwrite((void*)&current_in, sizeof(inode), 1, file_sys);
    }

    //now write actual data to the superblock
    s_block.free_block = next_free_block;
    
    s_block.free_inode = 1;
    printf("sblock size %ld\n", sizeof(struct superblock));

    //in blocks, write the address of the next free block at the top
    for (int j = 0; j < num_blocks-1; j++) {
      mem_ptr = DATA_OFFSET + j*BLOCK_SIZE;
      fseek(file_sys, mem_ptr, SEEK_SET);
      int next = j+1;
      fwrite((void*)&next, sizeof(next), 1,  file_sys);
      //printf("block %d points to %d", j, j+1);
    }
    mem_ptr = DATA_OFFSET + (num_blocks-1)*BLOCK_SIZE;
    fseek(file_sys, mem_ptr, SEEK_SET);
    int next = -1;
    fwrite((void*)&next, sizeof(next), 1,  file_sys);


    fseek(file_sys, BLOCK_SIZE, SEEK_SET);
    //write the updated superblock to memory
    fwrite((void*)&s_block,  sizeof(superblock), 1, file_sys);
  
    /*
    //All this was just to check that root inode saved properly
    memset(&current_in, 0, sizeof(struct inode));
    fw = fseek(file_sys, INODE_OFFSET, SEEK_SET);
    if (fw != 0) {
      printf("fs error %d\n", fw);
    }
    fw = fread((void*)&current_in, sizeof(inode), 1, file_sys );
    if (fw != 1) {
      printf("fr erro %d\n", fw);
    }
    printf("name is %s last idx %d\n",current_in.name,current_in.dblocks[8]);*/

    fclose(file_sys);
    printf("done formatting\n");
    return 0;
}

