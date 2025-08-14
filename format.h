#define BLOCK_SIZE 512 // Typically 512 bytes per block
#define DEFAULT_DISK_SIZE_MB 1 // Default size of disk in MB
#define DATA_OFFSET 11264 //leaves 2048 Bytes for inodes
#define INODE_OFFSET 1024
#define N_DBLOCKS 5
#define N_IBLOCKS 4
#define MAX_FILE_SIZE 264704

#include "vfs.h"

struct superblock {
    int size; /* size of blocks in bytes */
    int inode_offset; /* offset of inode region in blocks */
    int data_offset; /* data region offset in blocks */
    int free_inode; /* head of free inode list, index, if disk is full, -1 */
    int free_block; /* head of free block list, index, if disk is full, -1 */
    int current_dir;/* index of the current file directory*/
} ;

struct inode {
    int next_inode; /* index of next free inode */
    int permissions; /*file permissions*/
    //int nlink; /* number of links to this file */
    int size; /* numer of bytes in file */
    int dblocks[N_DBLOCKS]; /* pointers to data blocks */
    int iblocks[N_IBLOCKS]; /* pointers to indirect blocks */
    int type;
    int num_children;
    char name[150];
};
