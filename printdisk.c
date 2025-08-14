#include "inode.h"
#include <stdio.h>
#include <stdlib.h>

int block_size;
int inode_offset;
int data_offset;
int swap_offset;
int free_inode;
int free_block; 

/*
Notes
indoe header if != def
convert to use fread
*/
int main(int argc, char *argv[])
{
  struct superblock s_block;
  struct inode current_inode;
  int inode_size = sizeof(current_inode);
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <disk file>\n", argv[0]);
    exit(1);
  }

  FILE *disk = fopen(argv[1], "rb");
  if (disk == NULL) {
    perror("Error opening disk image");
    exit(1);
  }

  //move beyond boot block to super block
  int fs = fseek(disk,512 , SEEK_SET);

  int fr = fread((void*)(&s_block), sizeof(s_block),1, disk );
  if (fr != 1) {
    perror("Error reading on disk");
    exit(1);
  }

  block_size = s_block.size;
  data_offset = s_block.data_offset;
  inode_offset = s_block.inode_offset;
  swap_offset = s_block.swap_offset;
  free_inode = s_block.free_inode;
  free_block= s_block.free_block;
  

  printf("size %d\n inode offset %d\n data offset %d\n swap offset %d\n free inode %d\n free block %d\n\n",block_size, inode_offset, data_offset, swap_offset, free_inode, free_block);
  
  long int inode_location = 1024 + inode_offset* block_size;//equal to pointer size
  long int data_location = 1024 + data_offset* block_size;//equal to pointer size
  fs =  fseek(disk, inode_location, SEEK_SET);
  if (fs != 0) {
    perror("Error moving on disk");
    exit(1);
  }
  //printf("start iterating %ld inode location %ld data location\n", inode_location,data_location);
  
  fr = fread((void*)(&current_inode), inode_size,1, disk );
  if (fr != 1) {
    perror("Error reading on disk");
    exit(1);
  }
  //data is the next section of the disk after inodes
  while (inode_location < data_location ) {
    if ((inode_location + sizeof(inode)) > data_location) {
      break;
    }
    printf("\n\nNew Inode \n 0\nnext inode %d\nprotect %d\nnlink %d\nsize %d\nuid %d\ngid %d\nctime %d\nmtime %d\natime %d\n",
      current_inode.next_inode, current_inode.protect, current_inode.nlink, current_inode.size,
      current_inode.uid, current_inode.gid, current_inode.ctime, current_inode.mtime, current_inode.atime);
    int data_left_to_print = current_inode.size;
    printf("direct datablocks\n");
    for (int i = 0; i < N_DBLOCKS; i++) {
        printf("%d: %d\n", i, current_inode.dblocks[i]);
    }
    data_left_to_print = data_left_to_print - N_DBLOCKS;
    
    // Print single indirect blocks
    printf("single indirect\n");
    if (data_left_to_print > 0) {
      for (int i = 0; i < N_IBLOCKS; i++) {
        printf("%d: %d\n", i, current_inode.iblocks[i]);
      }
    }
   
    data_left_to_print = data_left_to_print - N_IBLOCKS;

    // Print doubly indirect block
    printf("double indirect\n%d\n", current_inode.i2block);
   
    // Print triply indirect block
    printf("triple indirect\n%d\n", current_inode.i3block);
    
    
    //current_inode = current_inode + inode_size;
    inode_location = inode_location + inode_size;
    //printf("next location %ld\n", inode_location);
    fs =  fseek(disk, inode_location, SEEK_SET);
    if (fs != 0) {
      perror("Error moving on disk");
      exit(1);
    }
    
    
    fr = fread((void*)(&current_inode), inode_size,1, disk );
    if (fr != 1) {
      perror("Error reading on disk");
      exit(1);
    }
  }
  fclose(disk);
}

