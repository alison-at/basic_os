#include <stdlib.h>
#include "vfs.h"
#include "format.h"
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <errno.h>
#define PERMISSION_MASK (S_IRUSR | S_IWUSR)

//vnode_t* fs_root = NULL;/*root of the vnode tree*/
int fs_errno = 0;
char mem_buffer [1000000];/*Memory*/
open_file_entry_t open_file_table[MAX_OPEN_FILES];/*open file table*/

//use for vnode number
int next_vnode;
struct superblock s_block;
struct vnode *root_vnode = NULL;/*root of the vnode tree*/
vfd_t root_vn_fd = -1;


char buffer_block[BLOCK_SIZE];


//Vnode level
//find a child of a directory node - this is how you can move along the path
vnode* f_find(vnode* parent_dir, const char* filename) {
  if (strcmp(parent_dir->name, filename) == 0) {
    return parent_dir;
  }
  for (int i = 0; i < parent_dir->num_children;i++) {
    vnode *child = parent_dir->child_ptrs[i];
    //printf("child %s\n", child->name);
    if ((strcmp(child->name, filename) == 0) ) {
      return child;
    }
  }
  return NULL;
}

//Vnode & inode level
//either:
//new vnode is made, parent vnode and inode directory are updated
//existing vnode file position is set to end
//existing vnode is cleared, file position is set to the beginning
vfd_t f_open(vnode_t *vn, const char *filename, int flags) { 
  char* mem_ptr;
  
  if (vn->type == 0 && flags & O_CREAT) {
    //the filename was not amoung the child vnodes, it must be made
    struct vnode *new_vn = (vnode*)(malloc(sizeof(vnode)));
    struct inode new_in;
    inode parent_in;
    memset((void*)&parent_in, 0, sizeof(inode));

    //copy info of the next free inode to the new inode
    new_vn->inode = s_block.free_inode;
    
    mem_ptr = mem_buffer + INODE_OFFSET+  new_vn->inode*BLOCK_SIZE;
    memcpy((void*)&new_in, mem_ptr, sizeof(inode));

    //copy info to superblock
    s_block.free_inode = new_in.next_inode;
    //printf("next inode is %d\n", new_in.next_inode);
    mem_ptr = mem_buffer + BLOCK_SIZE;
    memcpy(mem_ptr, (void*)&s_block, sizeof(superblock));

    
    vnode* child = f_find(vn, (char*)filename);//check if the file is amoung the children of vn.EEXIST, EISDIR
    if (child != NULL) {
      if (child->type == 1) {
        fs_errno = EEXIST;
        return -1;
      } else {
        fs_errno = EISDIR;
        return -1;
      }
    }

    if (vn->type != 0) {//error check 
      fs_errno = ENOTDIR;
      return -1;
    }

    if (vn->num_children >= 300 || s_block.free_inode == 49) {
      fs_errno = ENOSPC;
      return -1;
    }

    if (strlen(filename) > 150) {
      fs_errno = ENAMETOOLONG;
      return -1;
    }

    //permission flag operations for the vnode, permanently
    int inputed_permissions = 0;
    int permanent_permissions = 0;
    inputed_permissions = flags & PERMISSION_MASK;//add the read bit or not and the write bit or not to the inputed permissions. If the flag say write, inputed permissions will say write ect
    permanent_permissions = permanent_permissions | inputed_permissions;// if the read bit is set in inputted permissions, set it in permanent permissions
   
    //add information to the new inode
    strcpy(new_in.name, (char*)filename);
    new_in.type = 1;
    new_in.permissions = permanent_permissions;
    new_in.size = 0;
    new_in.num_children = -1;
    
    //add information to the new vnode
    memset(&new_vn->child_ptrs, 0, sizeof(new_vn->child_ptrs));
    strncpy(new_vn->name, filename, sizeof(new_vn->name));
    new_vn->type = 1;
    new_vn->permissions = flags;
    new_vn->num_children = -1;
    new_vn->parent = vn;
    new_vn->vnode_number = next_vnode;
    new_vn->size = 0;
    next_vnode++;

    //copy inode into memory
    mem_ptr = + mem_buffer + (new_vn->inode)*BLOCK_SIZE + INODE_OFFSET ;
    memcpy(mem_ptr, (void*)&new_in, sizeof(inode));
    //printf("new file %s at %p inode idx %d\n", filename, new_vn, new_vn->inode);

    //update the parent vnode with information about the new file
    vdir_entry table_entry;
    table_entry.type = 1;
    table_entry.valid = 1;
    table_entry.inode_idx = new_vn->inode;
    strncpy(table_entry.name, filename, sizeof(table_entry.name));
    vdir_entry* new_table_entry = NULL;

    //replace a previous table entry, write to the dir if no table entry is invalid, fseek to the invalid entry, write the invalid entry
    int num_read = 0;
    int idx = 0;
    int found = 0;
    
    
    //Overwrite an invalid table entry between valid entries
    while (num_read < vn->num_children) {
      new_table_entry = f_readdir(vn, idx);//read through the parent for existing table entries
      //in this case
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        num_read++;
      }
      
      if (new_table_entry != NULL && new_table_entry->valid == 0) {
        //to make sure that size is the same, indicate that you are erasing/overwriting one vdir
        //this is where we want to write
        int data_offset = idx*sizeof(vdir_entry);
        vfd_t dir_fd = f_opendir(vn, (const char*)"");
        //set the directory to write to invalid entry
        f_seek(vn, dir_fd, data_offset, SEEK_SET);
        //printf("size was %d ", vn->size);
        //One needs to make the size smaller before overwriting
        mem_ptr = mem_buffer +INODE_OFFSET + vn->inode*BLOCK_SIZE;
        memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
        parent_in.size = parent_in.size - (int)(sizeof(vdir_entry));
        memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));
        vn->size = vn->size - (int)(sizeof(vdir_entry));
        //printf("size is now %d\n", vn->size);
        f_write(vn, (void*)&table_entry, sizeof(vdir_entry),1,dir_fd);

        f_closedir(vn, dir_fd);
        found = 1;
        free(new_table_entry);
        break;
      }
      free(new_table_entry);
      idx++;
    }
    
    //No overwriting an existing entry, writing a new entry. size increases
    if (found == 0) {
      vfd_t dir_fd = f_opendir(vn, (const char*)"");
      f_seek(vn, dir_fd,0, SEEK_END);//go to the end of the current data
      f_write(vn, (void*)&table_entry, sizeof(vdir_entry), 1, dir_fd);
      
      f_closedir(vn, dir_fd);
      
      found = 1;
      //free(new_table_entry);
    }
    
    //update parent vnode with children
    vn->child_ptrs[vn->num_children] = new_vn;
    vn->num_children = vn->num_children + 1;
    
    //update the number of children on the inode level 
    mem_ptr = mem_buffer +INODE_OFFSET + vn->inode*BLOCK_SIZE;
    memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
    parent_in.num_children = parent_in.num_children+1;
    memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));//copy back to mem

    //printf("%s has %d children now based on inode, %d based on vnode \n", parent_in.name, parent_in.num_children, vn->num_children);
    
    //update the open file table
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (!open_file_table[i].in_use ) {
          open_file_table[i].in_use = 1;
          open_file_table[i].vn = new_vn;
          //These are only temporary permissions for this open
          int temp_permissions = 0;
          temp_permissions = flags & PERMISSION_MASK;
          open_file_table[i].flags = temp_permissions;
          open_file_table[i].file_pos = 0;
          return i; // this is the file descriptor
      }
    }
    //if no open file fd returned there was an issue
    return -1;
  
  } else if (vn == NULL) {//error check: supposed to be a file node, file node does not exist
    fs_errno = ENOENT;
    return -1;
  //file exists, append to it
  } else if (vn->type == 1 && flags & O_APPEND) {
    unsigned int temp_permissions = 0; 

    temp_permissions = flags & PERMISSION_MASK;
    if ((vn->permissions & temp_permissions) != temp_permissions) {//check error: if temp permission override permanent file permission, error
      fs_errno = EACCES;
      return -1;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (open_file_table[i].vn == vn && open_file_table[i].in_use) {
          open_file_table[i].in_use = 1;
          open_file_table[i].vn = vn;
          //These are only temporary permissions for this open
          open_file_table[i].flags = temp_permissions;
          open_file_table[i].file_pos = 0;
          f_seek(vn ,i,0, SEEK_END);
          return i;
      }
      
    }
    //the file was not already open
    for (int j = 0; j < MAX_OPEN_FILES;j++) {
      if (!open_file_table[j].in_use) {
          open_file_table[j].in_use = 1;
          open_file_table[j].vn = vn;
          //These are only temporary permissions for this open
          open_file_table[j].flags = temp_permissions;
          open_file_table[j].file_pos = 0;
          f_seek(vn ,j,0, SEEK_END);
          return j; // this is the file descriptor
      }
    }
    //there was no free open slot
    //printf("failed append\n");
    fs_errno = ENFILE;
    return -1;
  //file exists, overwrite it
  } else if (vn->type == 1 && flags & O_TRUNC) {
      inode current_in;
      int temp_permissions = 0; 

      temp_permissions = flags & PERMISSION_MASK;
      if ((vn->permissions & temp_permissions) != temp_permissions) {//check error: if temp permission override permanent file permission, error
        fs_errno = EACCES;
        return -1;
      }
      //write the data of the existing files to be 0
      for (int j = 0; j < MAX_OPEN_FILES; j++) {
        if (open_file_table[j].vn == vn && open_file_table[j].in_use) {
          open_file_table[j].in_use = 1;
          open_file_table[j].vn = vn;
          //These are only temporary permissions
          open_file_table[j].flags = temp_permissions;
          open_file_table[j].file_pos = 0;

          size_t size_of_file = vn->size;
          f_seek(vn, j,0, SEEK_SET);
          //zero out data
          char *data = (char*)(malloc(size_of_file));
          memset(data, 0, size_of_file);
          f_write(vn, data, size_of_file, 1, j);
          free(data);
          f_seek(vn, j,0, SEEK_SET);

          //set size to 0
          mem_ptr = mem_buffer +INODE_OFFSET + vn->inode*BLOCK_SIZE;
          memcpy((void*)&current_in, mem_ptr, sizeof(inode));
          current_in.size = 0;
          memcpy(mem_ptr,(void*)&current_in, sizeof(inode));
          vn->size = 0;
          return j;
        }
      }
      for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_file_table[i].in_use) {
          open_file_table[i].in_use = 1;
          open_file_table[i].vn = vn;
          open_file_table[i].flags = temp_permissions;
          open_file_table[i].file_pos = 0;

          size_t size_of_file = vn->size;
          f_seek(vn, i,0, SEEK_SET);
          //zero out data
          char *data = (char*)(malloc(size_of_file));
          memset(data, 0, size_of_file);
          f_write(vn, data, size_of_file, 1, i);
          free(data);
          f_seek(vn, i,0, SEEK_SET);

          //set size to 0
          mem_ptr = mem_buffer +INODE_OFFSET + vn->inode*BLOCK_SIZE;
          memcpy((void*)&current_in, mem_ptr, sizeof(inode));
          current_in.size = 0;
          memcpy(mem_ptr,(void*)&current_in, sizeof(inode));
          vn->size = 0;
          
          return i; // this is the file descriptor
        }
      }
      //printf("failed trunc\n");
      fs_errno = ENFILE;
      return -1;
    
  }
  return -1;
}

//Inode level
//Read through indirect blocks of a given file
void* read_from_indirect_blocks(int starting_table_idx, int* bytes_read, int end_bytes, inode current_in, void* data) {
  char* mem_ptr;
  int data_block_idx;
  int diff;
  //At this point, I need to start allocating new blocks for tables and data. loop through the tables
  for (int x = starting_table_idx; x < N_IBLOCKS && *bytes_read < end_bytes; x++) {

    //loop through the data blocks in each table
    for (int y = 0; y < (BLOCK_SIZE/4) &&  *bytes_read < end_bytes; y++) {
      //get the top of the table block
      //printf("reading from table %d\n", current_in.iblocks[x]);
      mem_ptr = mem_buffer+DATA_OFFSET+ current_in.iblocks[x]*BLOCK_SIZE;

      //get the address in the table
      mem_ptr = mem_ptr + y*4;

      //get the data block address in the table
      memcpy((void*)&data_block_idx, mem_ptr, sizeof(int));
      //printf("reading from block %d\n", data_block_idx);

      //go to the data block 
      mem_ptr = mem_buffer+ DATA_OFFSET+ data_block_idx*BLOCK_SIZE;

      if (BLOCK_SIZE > (end_bytes - *bytes_read) ) {
        diff = end_bytes - *bytes_read;
      } else {
        diff = BLOCK_SIZE;
      }

      //read to data from memory of indirect data block
      memcpy(data, mem_ptr, diff);
      data = (char*)data + diff;
      *bytes_read = *bytes_read + diff;
    }
  }
  return data;
}

// Mostly inode level
//read data of given vnode into data buffer
size_t f_read(vnode_t *vn, void *data, size_t size, int num, vfd_t fd)
{
  int bytes_read = 0;
  int end_bytes = size*num;

  if (fd < 0 || fd > 49 || open_file_table[fd].in_use == 0) {//error check: bad file fd
    fs_errno = EBADF;
    return -1;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return -1;
  }

  if ((vn->permissions & S_IRUSR) == 0 || (open_file_table[fd].flags & S_IRUSR) == 0) {
    fs_errno = EPERM;
    //printf("permissions error\n");
    return -1;
  }

  //get the inode of the vn
  char* mem_ptr = mem_buffer + INODE_OFFSET+ vn->inode*BLOCK_SIZE;
  struct inode current_in;
  memcpy((void*)&current_in, mem_ptr, sizeof(inode));

  int starting_block_idx = open_file_table[fd].file_pos/BLOCK_SIZE;
  int starting_block = current_in.dblocks[starting_block_idx];
  
  //if you start reading in the direct blocks
  if (starting_block_idx < N_DBLOCKS) {
    //printf("start reading at %d openfile at %d\n", starting_block, ((open_file_table[fd].file_pos)/BLOCK_SIZE));
    //read any uneven offset
    int starting_offset = (open_file_table[fd].file_pos)%BLOCK_SIZE;
    mem_ptr = mem_buffer + DATA_OFFSET+ starting_block*BLOCK_SIZE + starting_offset;

    //this is the uneven offset to be read
    int diff  = BLOCK_SIZE - starting_offset;
    if (diff > (end_bytes - bytes_read)) {
      diff = end_bytes - bytes_read;
    }

    //read from memory to data
    memcpy(data, mem_ptr, diff);
    data = (char*)data+ diff;
    bytes_read = bytes_read + diff;
    //printf("read offset at block %d\n", starting_block);
    //printf("bytes read %d contents: %s\n", bytes_read, ((char*)data - diff));

    if (bytes_read < end_bytes) {
      //read the rest of the direct blocks
      for (int i = (starting_block_idx +1); i < N_DBLOCKS && bytes_read < end_bytes; i++) {
        mem_ptr = mem_buffer + DATA_OFFSET+ current_in.dblocks[i]*BLOCK_SIZE;
        int diff  = BLOCK_SIZE;
        if (diff > (end_bytes - bytes_read)) {
          diff = end_bytes - bytes_read;
        }
        //printf("am reading at %p, data block %d\n", mem_ptr,current_in.dblocks[i]);
        memcpy(data, mem_ptr, diff);
        data = (char*)data+ diff;
        bytes_read = bytes_read + diff;
      }
    }
    
    //move to the indirect blocks. We need to alloc each indirect table and data block as we go
    if (bytes_read < end_bytes) {
      read_from_indirect_blocks(0, &bytes_read, end_bytes, current_in, data);
    }

  //read starting in the indirect blocks. At least one indirect block has been alloced.
  } else {
    //                          indirect bytes                        /  bytes stored per indirect table    
    int indirect_table_idx = ((open_file_table[fd].file_pos) - N_DBLOCKS*BLOCK_SIZE) / ((BLOCK_SIZE/4)*BLOCK_SIZE);
    //                      indirect bytes                      % bytes stored per indirect table 
    int offset_in_table = ((open_file_table[fd].file_pos) - N_DBLOCKS*BLOCK_SIZE) % ((BLOCK_SIZE/4)*BLOCK_SIZE);
    //all bytes alloced over indirect table / bytes alloced per block
    int table_idx = (offset_in_table)/ BLOCK_SIZE;
    int offset_in_data = offset_in_table%BLOCK_SIZE;
    int indirect_table_block = current_in.iblocks[indirect_table_idx];
    int data_block_idx;

    //printf("left off at %d table block index %d, %d data block in table, offset in data %d, position %d compared to size %d\n", indirect_table_idx,indirect_table_block, table_idx, offset_in_data, (open_file_table[fd].file_pos), current_in.size);
    
    //if there is a partially filled data block
    if (offset_in_data != 0) {
      int diff = BLOCK_SIZE - offset_in_data;

      //in case we dont need to move to a new block and can read only current block
      if (diff > (end_bytes - bytes_read)) {
        diff = end_bytes - bytes_read;
      }

      //get the top of the table block
      char* mem_ptr = mem_buffer+DATA_OFFSET+ indirect_table_block*BLOCK_SIZE;

      //get the address in the table
      mem_ptr = mem_ptr + table_idx*4;

      //get the data block address in the table
      memcpy((void*)&data_block_idx, mem_ptr, sizeof(int));
      //printf("here, end with %d, read %d bytes, data block idx %d offset %d, diff %d\n", end_bytes, bytes_read, data_block_idx, offset_in_data,diff);
      
      //go to the current offset in the data block 
      mem_ptr = mem_buffer+ DATA_OFFSET+ data_block_idx*BLOCK_SIZE + offset_in_data;

      //read to data from memory of paritally alloced indirect data block
      memcpy(data, mem_ptr, diff);
      data = (char*) data + diff;
      //printf("data: %p\n", data);
      bytes_read = bytes_read + diff;
    } else {
      table_idx = table_idx -1;
    }
    
    //if we need to read more indirect blocks in the same table
    for (int j = table_idx+1; j < (BLOCK_SIZE/4) && bytes_read < end_bytes; j++) {
      //get the address of the data block in the indirect table
      mem_ptr = mem_buffer+ DATA_OFFSET+ current_in.iblocks[indirect_table_idx]*BLOCK_SIZE;
      mem_ptr = mem_ptr+ 4*j;

      //go to the data block
      memcpy((void*)&data_block_idx, mem_ptr, sizeof(int));
      mem_ptr = mem_buffer+ DATA_OFFSET+ data_block_idx*BLOCK_SIZE;

      //read to the data block
      if (end_bytes - bytes_read < BLOCK_SIZE) {
        memcpy(data, mem_ptr,(end_bytes - bytes_read));
        data = (char*) data + (end_bytes - bytes_read);
        bytes_read = bytes_read + (end_bytes - bytes_read);
      } else {
        memcpy(data, mem_ptr,BLOCK_SIZE);
        data = (char*) data + BLOCK_SIZE;
        bytes_read = bytes_read + BLOCK_SIZE;
      }
    }
    
    //if we need to read from whole new tables
    if (bytes_read < end_bytes) {
      read_from_indirect_blocks(indirect_table_idx+1, &bytes_read, end_bytes , current_in, data);
    }
  }
  //printf("bytes read %d contents: %s\n", bytes_read, ((char*)data - bytes_read));
  return bytes_read;
}

//Inode level
//Write the indirect blocks of a file
void* write_to_indirect_blocks(int starting_table_idx, int* bytes_written, int end_bytes, inode* current_in, void* data) {
  char* mem_ptr;
  int diff;
  int next_alloc_block;
  int alloc_block;

  //At this point, I need to start allocating new blocks for tables and data
  for (int x = starting_table_idx; x < N_IBLOCKS && *bytes_written < end_bytes; x++) {
    //get the next free block
    alloc_block = s_block.free_block;
    if (alloc_block == -1) {
      //printf("run out of data blocks");
      return data;
    }
    //printf("alloced %d as table block\n", alloc_block);
    
    //get the next free block and make it the head of the list
    mem_ptr = mem_buffer + DATA_OFFSET+alloc_block*BLOCK_SIZE;
    memcpy((void*)&next_alloc_block, mem_ptr, sizeof(int));
    s_block.free_block = next_alloc_block;
    //printf("next free block is %d\n", next_alloc_block);
    
    //write to the indirect block of the current inode
    current_in->iblocks[x] = alloc_block;
    
    //alloc a new block as a data block
    for (int y = 0; y < (BLOCK_SIZE/4) &&  *bytes_written < end_bytes; y++) {
      alloc_block = s_block.free_block;
      //printf("still writing to same table %d, with data block %d\n", current_in->iblocks[x],alloc_block);
      if (alloc_block == -1) {
        //printf("run out of data blocks");
        return data;
      }

      ///update the just alloced indirect table with the alloced data block
      mem_ptr = mem_buffer+ DATA_OFFSET+ current_in->iblocks[x]*BLOCK_SIZE;
      mem_ptr = mem_ptr+ 4*y;
      memcpy(mem_ptr, (void*)&alloc_block, sizeof(int));
      
      //update the sblock
      mem_ptr = mem_buffer + DATA_OFFSET+alloc_block*BLOCK_SIZE;
      memcpy((void*)&next_alloc_block, mem_ptr, sizeof(int));
      s_block.free_block = next_alloc_block;

      diff = BLOCK_SIZE;
      if (diff > (end_bytes - *bytes_written)) {
        diff = end_bytes - *bytes_written;
      }
      
      mem_ptr = mem_buffer + DATA_OFFSET+ alloc_block*BLOCK_SIZE;
      memcpy(mem_ptr, data, diff);
      data = (char*)data + diff;
      *bytes_written = *bytes_written + diff;
      if(*bytes_written >= end_bytes) {
        break;
      }
    }
  }

  //write modified sblock to memory
  mem_ptr = mem_buffer+BLOCK_SIZE;
  memcpy(mem_ptr, (void*)&s_block, sizeof(superblock));

  return data;
}


// Inode and vnode level
//Write from data buffer to file
size_t f_write(vnode_t *vn, void *data, size_t size,int num, vfd_t fd)
{
  char verify_buf[BLOCK_SIZE*N_DBLOCKS + 1000] = {0}; 
  int bytes_written = 0;
  int end_bytes = size*num;


  if (fd < 0 || fd > 49 || open_file_table[fd].in_use == 0) {//error check: bad file fd
    fs_errno = EBADF;
    return -1;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return -1;
  }

  if (open_file_table[fd].file_pos + size*num > MAX_FILE_SIZE) {
    fs_errno = EFBIG;
    return -1;
  }

  if ((vn->permissions & S_IWUSR) == 0 || (open_file_table[fd].flags & S_IWUSR) == 0) {
    fs_errno = EPERM;
    return -1;
  }

  //get the inode of the vn
  char* mem_ptr = mem_buffer + INODE_OFFSET+ vn->inode*BLOCK_SIZE;
  struct inode current_in;
  memcpy((void*)&current_in, mem_ptr, sizeof(inode));
  //printf("writing blocks to %s at %d, fd is %d, inode %d first block %d\n", current_in.name, (open_file_table[fd].file_pos), fd, vn->inode, current_in.dblocks[0]);
  
  int starting_block_idx = ((open_file_table[fd].file_pos))/BLOCK_SIZE;
  int starting_block = current_in.dblocks[starting_block_idx];

  //starting in the direct blocks
  if (starting_block_idx < N_DBLOCKS) {
    //printf("starting block is %d\n", starting_block);
    //write to fill any uneven offset
    int starting_offset = (open_file_table[fd].file_pos)%BLOCK_SIZE;
    mem_ptr = mem_buffer + DATA_OFFSET+ starting_block*BLOCK_SIZE + starting_offset;

    int diff  = BLOCK_SIZE - starting_offset;
    if (diff > (end_bytes - bytes_written)) {
      diff = end_bytes - bytes_written;
    }

    //write from data to memory
    memcpy(mem_ptr, data, diff);
    data = (char*)data + (long unsigned int)diff;
    bytes_written = bytes_written + diff;
    //printf("done here\n");
    for (int i = (starting_block_idx +1); i < N_DBLOCKS && bytes_written < end_bytes; i++) {
      //printf("writing at block %d, data is %p, end of data is %p to data block %d\n", i, data, ((char*)data + size*num), current_in.dblocks[i]);
      diff = BLOCK_SIZE;
      if (diff > (end_bytes - bytes_written)) {
        diff = end_bytes - bytes_written;
      }
      mem_ptr = mem_buffer + DATA_OFFSET+ current_in.dblocks[i]*BLOCK_SIZE;
      memcpy(mem_ptr, data, diff);
      data = (char*)data + diff;
      bytes_written = bytes_written + diff;
    }

    //move to the indirect blocks. We need to alloc each indirect table and data block as we go
    if (bytes_written < end_bytes) {
      write_to_indirect_blocks(0, &bytes_written, end_bytes, &current_in, data);
    }
  // we write starting in the indirect blocks. At least one indirect block has been alloced.
  } else {
    //                          indirect bytes                        /  bytes stored per indirect table    
    int indirect_table_idx = ((open_file_table[fd].file_pos) - N_DBLOCKS*BLOCK_SIZE) / ((BLOCK_SIZE/4)*BLOCK_SIZE);
    //                      indirect bytes                      % bytes stored per indirect table 
    int offset_in_table = ((open_file_table[fd].file_pos) - N_DBLOCKS*BLOCK_SIZE) % ((BLOCK_SIZE/4)*BLOCK_SIZE);
    //if the table is not perfectly aligned
    //all bytes alloced over indirect table / bytes alloced per block
    int table_idx = (offset_in_table)/ BLOCK_SIZE;
    int offset_in_data = offset_in_table%BLOCK_SIZE;
    int indirect_table_block = current_in.iblocks[indirect_table_idx];
    int data_block_idx;
    
    //printf("left off at %d table block index %d, %d data block in table, offset in data %d, position %d compared to size %d\n", indirect_table_idx,indirect_table_block, table_idx, offset_in_data, (open_file_table[fd].file_pos), current_in.size);
    
    //if there is a partially filled data block
    if (offset_in_data != 0) {
      
      int diff = BLOCK_SIZE -offset_in_data;
      //in case we dont need to move to a new block and can fill existing block
      if (diff > (end_bytes - bytes_written)) {
        diff = end_bytes - bytes_written;
      }

      //get the top of the table block
      char* mem_ptr = mem_buffer+DATA_OFFSET+ indirect_table_block*BLOCK_SIZE;
      //get the address in the table
      mem_ptr = mem_ptr + table_idx*4;

      //get the address of the data block
      memcpy((void*)&data_block_idx, mem_ptr, sizeof(int));
      
      //printf("writing into offset at table %d block %d which is entry %d, offset %d\n", indirect_table_block, data_block_idx, table_idx, offset_in_data);
      //go to the partially filled data block
      mem_ptr = mem_buffer+ DATA_OFFSET+ data_block_idx*BLOCK_SIZE + offset_in_data;

      //fill the partially filled block last alloced from the last written to the end
      memcpy(mem_ptr, data, diff);
      data = (char*)data + diff;
      bytes_written = bytes_written + diff;

      //printf("beginning safety read\n");
      //this is to check my work - see how the string grows 
      
      f_seek(vn, fd, ((open_file_table[fd].file_pos)), SEEK_SET);
      f_seek(vn, fd, (-10), SEEK_END);    
      //printf("new open file pos %d\n", (open_file_table[fd].file_pos));       
      f_read(vn, verify_buf, diff+11, 1, fd);
    } else {
      table_idx = table_idx -1;
    }
    //int next_free_block;
    //if at least some data has already been alloced in this indirect table (I don't need to alloc a block for the table)
    if (table_idx != 0 && offset_in_data != 0) {
      //fill the rest of the slots in the table - for each slot, alloc a new data block

      for (int j = table_idx+1; j < (BLOCK_SIZE/4) &&  bytes_written < end_bytes; j++) {
        int alloc_block = s_block.free_block;
        //update the indirect table with the block
        mem_ptr = mem_buffer+ DATA_OFFSET+ current_in.iblocks[indirect_table_idx]*BLOCK_SIZE;
        mem_ptr = mem_ptr+ 4*j;
        memcpy(mem_ptr, (void*)&alloc_block, sizeof(int));

        
        //update super block with the next free block
        mem_ptr = mem_buffer + DATA_OFFSET+alloc_block*BLOCK_SIZE;
        memcpy((void*)&s_block.free_block, mem_ptr, sizeof(int));
        
        
        //zero out the new block before you write to it
        memset(mem_ptr, 0, BLOCK_SIZE);

        int diff = BLOCK_SIZE;
        if (diff > (end_bytes - bytes_written)) {
          diff = (end_bytes - bytes_written);
        }
        memcpy(mem_ptr, data, diff);
        data = (char*)data + diff;
        bytes_written = bytes_written+diff;

      }
    } else {
      //in the case that you start a new table block, you are allocating the current indirect block not just indirect_table_idx+1
      indirect_table_idx = indirect_table_idx -1;
    }
    
    //blocks get allocated for new tables pointing to new data
    if (bytes_written < end_bytes) {
      write_to_indirect_blocks(indirect_table_idx+1, &bytes_written, end_bytes , &current_in, data);
    }
  }

  open_file_table[fd].file_pos = open_file_table[fd].file_pos+bytes_written;
  //update size
  current_in.size = current_in.size + bytes_written;
  vn->size = vn->size + bytes_written;
  //write the inode to memory
  mem_ptr = mem_buffer + INODE_OFFSET + vn->inode*BLOCK_SIZE;
  memcpy(mem_ptr, (void*)&current_in, sizeof(inode));

  //copy s_block , with the final updated free block, to the disk just in case free block changed NOTE: this design requires no simultaneous processes on the disk!!
  mem_ptr = mem_buffer + BLOCK_SIZE;
  memcpy(mem_ptr, (void*)&s_block, sizeof(superblock));
  //printf("wrote %d bytes\n",bytes_written);

  return bytes_written;
}

//Vnode level
//Remove a file from the open file table
int f_close(vnode_t *vn, vfd_t fd)
{
  if (vn->type == 0) {
    fs_errno = EISDIR;
    return -1;
  } 
  if (fd < 0 || fd > 49 || open_file_table[fd].in_use == 0) {//error check: bad file fd
    fs_errno = EBADF;
    return -1;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return -1;
  }

  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (open_file_table[i].vn == vn && open_file_table[i].in_use) {
        open_file_table[i].in_use = 0;
        return 0; 
    }
  }
  return -1;
}

//Vnode level
//set the position in a given file
int f_seek(vnode_t *vn, vfd_t fd, off_t offset, int whence)
{
  if (fd < 0 || fd > 49 || open_file_table[fd].in_use == 0) {//error check: bad file fd
    fs_errno = EBADF;
    return -1;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return -1;
  }

  inode current_in;
  //printf("seeking in vn %s %p %p fd is %d\n", vn->name, vn, open_file_table[fd].vn, fd);
 
  //get the inode
  char* mem_ptr = mem_buffer + vn->inode*BLOCK_SIZE + INODE_OFFSET;
  memcpy((void*)&current_in, (void*)mem_ptr, sizeof(inode));

  int new_location = -1;
  //if (open_file_table[fd].in_use && vn == open_file_table[fd].vn) {
  if (open_file_table[fd].vn == vn) {
    if (whence == SEEK_SET) {
      //set the fd to the address (offset) from the start of the file
      new_location = offset;
    } else if (whence == SEEK_CUR) {
      new_location = open_file_table[fd].file_pos+offset;
    } if (whence == SEEK_END) {
      new_location = current_in.size + offset;
    }

    open_file_table[fd].file_pos = new_location;
  }

  if (new_location > MAX_FILE_SIZE) {//error check: to large a seek
    fs_errno = ENXIO;
    return -1;
  }
  
  return new_location;
}

//Vnode level
//Get summary of information about a given file or directory
//fd is not important
int f_stat(vnode_t *vn, vfd_t fd, vstat_t* stats)
{
  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return -1;
  }

  //fill out vstats
  inode current_in;
  memset((void*)&current_in, 0, sizeof(inode));
  char* mem_ptr = mem_buffer + INODE_OFFSET +  vn->inode*BLOCK_SIZE ;
  memcpy((void*)&current_in, mem_ptr, sizeof(inode));

  stats->st_type = vn->type;
  stats->st_inode = vn->inode;
  stats->st_perms = vn->permissions;
  stats->st_size = current_in.size;

  int starting_block_idx = vn->size/BLOCK_SIZE;
  if (starting_block_idx < N_DBLOCKS) {
    stats->st_blocks = N_DBLOCKS;
  } else  {
    int indirect_table_idx = (vn->size - N_DBLOCKS*BLOCK_SIZE)/((BLOCK_SIZE/4)*BLOCK_SIZE);
    int offset_in_table = (vn->size - N_DBLOCKS*BLOCK_SIZE)%((BLOCK_SIZE/4)*BLOCK_SIZE);
    int table_idx = (offset_in_table)/ BLOCK_SIZE;//in the table, the last table entry alloced
    // num blocks  =            the filled indirect tables   + the blocks used in the unfilled table + the overhead blocks of table blocks
    stats->st_blocks = (indirect_table_idx*(BLOCK_SIZE/(sizeof(int)))) + (table_idx + 1)  + (indirect_table_idx+1) ;
  }
  /*if (current_in.size <= N_DBLOCKS*BLOCK_SIZE) {
    stats->st_blocks = N_DBLOCKS;
  } else if (current_in.size <= (N_DBLOCKS*BLOCK_SIZE + 1*(BLOCK_SIZE/4)*BLOCK_SIZE)) {
    stats->st_blocks = N_DBLOCKS + 1 + ((current_in.size - N_DBLOCKS*BLOCK_SIZE)/BLOCK_SIZE);
  } else if (current_in.size <= (N_DBLOCKS*BLOCK_SIZE + 2*(BLOCK_SIZE/4)*BLOCK_SIZE)) {
    stats->st_blocks = N_DBLOCKS + 2 + ((current_in.size - N_DBLOCKS*BLOCK_SIZE)/BLOCK_SIZE);
  } else if (current_in.size <= (N_DBLOCKS*BLOCK_SIZE + 3*(BLOCK_SIZE/4)*BLOCK_SIZE)) {
    stats->st_blocks = N_DBLOCKS + 3 + ((current_in.size - N_DBLOCKS*BLOCK_SIZE)/BLOCK_SIZE);
  } else if (current_in.size <= (N_DBLOCKS*BLOCK_SIZE + 4*(BLOCK_SIZE/4)*BLOCK_SIZE)) {
    stats->st_blocks = N_DBLOCKS + 4 + ((current_in.size - N_DBLOCKS*BLOCK_SIZE)/BLOCK_SIZE);
  } else {
    stats->st_blocks = -1;
  }*/
  
  return 0;
}

//Vnode and inode levels
//remove the vn from the inode of the parent dir
//fd is not relevant, the file should previously have been closed
int f_remove(vnode_t *vn, vfd_t fd)
{

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return -1;
  }

  struct inode current_in;
  struct vnode* parent_vn = vn->parent;

  char* mem_ptr = mem_buffer + INODE_OFFSET + (vn->inode)*BLOCK_SIZE; //get the inode
  memcpy((void*)&current_in, mem_ptr, sizeof(inode));
  //printf("deleting inode %d of size %d\n", vn->inode, current_in.size);

  //zero out the direct blocks of the file
  for (int i = 0; i < N_DBLOCKS; i++) {
    mem_ptr = mem_buffer + DATA_OFFSET + current_in.dblocks[i]*BLOCK_SIZE;
    memset(mem_ptr, 0, (BLOCK_SIZE));//REVIEW: may be problem place, note that the next address in the block has been overwritten
  }

  //free each block the indirect table points to, then free the indirect table block, then delete each data block and indirect block;
  if (current_in.size > (N_DBLOCKS*BLOCK_SIZE)) {
    
    //the last indirect table alloced
    int indirect_table_idx = (current_in.size - N_DBLOCKS*BLOCK_SIZE)/((BLOCK_SIZE/4)*BLOCK_SIZE);
    int offset_in_table = (current_in.size - N_DBLOCKS*BLOCK_SIZE)%((BLOCK_SIZE/4)*BLOCK_SIZE);
    //in the table, the last table entry alloced
    int table_idx = (offset_in_table)/ BLOCK_SIZE;
    
    //only loop through the alloced blocks of indirect tables
    //for (int j = 0; j < (indirect_table_idx+1); j++) {
    for (int j = (indirect_table_idx); j >= 0; j--) {
      char* table_ptr = mem_buffer+DATA_OFFSET+(current_in.iblocks[j]*BLOCK_SIZE);
      char* table_end = table_ptr + BLOCK_SIZE;
      int block_to_free;
      //for the last table, probably not every block was alloced in table
      if (j == indirect_table_idx) {
        table_end = table_ptr + sizeof(int)*table_idx;
      }

      /*while (table_ptr < table_end) {
        //get the alloced block
        memcpy((void*)&block_to_free, table_ptr, sizeof(int));

        //find the alloced block in memory
        mem_ptr = mem_buffer + DATA_OFFSET + block_to_free*BLOCK_SIZE;

        //update the head of the free blocks in superblock
        int next_free = s_block.free_block;
        s_block.free_block = block_to_free;

        //write the address of the previous free block head to the freed block
        memcpy(mem_ptr, (void*)&next_free, sizeof(int));

        table_ptr = table_ptr + sizeof(4);
      }*/
      while (table_end >= table_ptr) {
        //get the alloced block
        memcpy((void*)&block_to_free, table_end, sizeof(int));

        //find the alloced block in memory
        mem_ptr = mem_buffer + DATA_OFFSET + block_to_free*BLOCK_SIZE;

        //update the head of the free blocks in superblock
        int next_free = s_block.free_block;
        s_block.free_block = block_to_free;

        //write the address of the previous free block head to the freed block
        memcpy(mem_ptr, (void*)&next_free, sizeof(int));

        table_end = table_end - sizeof(4);
      }

      //add the table overhead block to the free list
      int next_free = s_block.free_block;
      s_block.free_block = current_in.iblocks[j];
      memcpy(table_ptr, (void*)&next_free, sizeof(int));
      
    }
    
  }
  
  //REVIEW:the newly freed inode is not zeroed out
  
  //get the current head of the free inode list
  int next_free_in = s_block.free_inode;
  //set the next free inode to the head of the inode list
  //printf("free block %d\n", s_block.free_block);
  current_in.next_inode = next_free_in;
  //set the just freed inode to be the head of free inode list
  s_block.free_inode = vn->inode;

  //copy the freed inode to memory
  mem_ptr = mem_buffer + INODE_OFFSET + (vn->inode)*BLOCK_SIZE;
  memcpy(mem_ptr, (void*)&current_in, sizeof(inode));
  
  //write the sblock to memory with updated free inode head and updated free data block head
  mem_ptr = mem_buffer + BLOCK_SIZE;
  memcpy(mem_ptr, (void*)&s_block, sizeof(superblock));
  
  //get rid of the pointer in the parent directory
  if (parent_vn != NULL) {
    int i = 0;
    
    while(i < parent_vn->num_children &&parent_vn->child_ptrs[i] != vn ) {
      i++;
    }
    //i now equals vn
    while (i < (parent_vn->num_children-1)) {
      parent_vn->child_ptrs[i] = parent_vn->child_ptrs[i+1];
      i++;
    }
    
  }

  // for (int j = 0; j < parent_vn->num_children -1; j++) {
  //   printf("pointer %p to %s ", parent_vn->child_ptrs[j], parent_vn->child_ptrs[j]->name);
  // }

  vdir_entry* new_table_entry;
  int num_read = 0;
  int idx = 0;

  //set the vdir entry within the parent directory to be invalid
  while (num_read < parent_vn->num_children) {
    new_table_entry = f_readdir(parent_vn, idx);
    // if (new_table_entry != NULL) {
    //   printf("table entry %s idx %d valid %d\n", new_table_entry->name, idx, new_table_entry->valid);
    // }
    //found a valid child entry
    if (new_table_entry != NULL && new_table_entry->valid == 1) {
      num_read++;
    }
    
    //overwrite the searched for child entry
    if (new_table_entry != NULL && strcmp(new_table_entry->name, vn->name) == 0) {
      //set the flag to invalid
      new_table_entry->valid =0;
      off_t offset = idx*sizeof(vdir_entry);
      vfd_t dir_fd = f_opendir(parent_vn, (const char*)"");
      //seek the location of the invalid entry
      f_seek(parent_vn, dir_fd, offset, SEEK_SET);
      //delete from size to handle f_write adding to size, this is an overwrite operation
      parent_vn->size = parent_vn->size - (int)(sizeof(vdir_entry));
      //overwrite the nextry
      f_write(parent_vn, new_table_entry, sizeof(vdir_entry),1, dir_fd);
      f_closedir(parent_vn, dir_fd);
      free(new_table_entry);
      break;
    }
    free(new_table_entry);
    idx++;
  }

  //update the size and num children in the parent inode
  inode parent_in;
  mem_ptr = mem_buffer +INODE_OFFSET + parent_vn->inode*BLOCK_SIZE;
  memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
  parent_in.num_children = parent_in.num_children-1;
  //fwrite will have added to inode size, to make it clear it is an overwrite delete from size here
  parent_in.size = parent_in.size - (int)(sizeof(vdir_entry));

  //REVIEW: possible issue causer
  //if this was the only child or the last child in the directory, reduce size instead of just keeping size even
  if (parent_vn->num_children == 1  || num_read  == parent_vn->num_children  ) {
    parent_in.size = parent_in.size - (int)(sizeof(vdir_entry));
    parent_vn->size = parent_vn->size - (int)(sizeof(vdir_entry));
  }
  memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));
  //update num children in the parent vnode
  parent_vn->num_children = parent_vn->num_children -1;

  //set the vdir entry to invalid
  free(vn);

  return 0;
}

// Vnode level
//Puts dir file in open file table
//All directories are opened in read/write mode
vfd_t f_opendir(vnode_t *vn, const char* path)
{
  if (vn->type != 0) {
    fs_errno = ENOTDIR;
    return -1;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return -1;
  }
  int temp_permissions = 0;
  temp_permissions = temp_permissions | S_IWUSR;
  temp_permissions = temp_permissions | S_IRUSR;

  //REVIEW: note that this requires that files are closed before they are removed
  //first check if the file is in open files. If yes, return that
  for (int j = 0; j < MAX_OPEN_FILES; j++) {
    if (open_file_table[j].vn == vn &&open_file_table[j].in_use ==1 ) {
      return j;
    }
  }
  //if the file was not already open, then find a not in use slot and alloc it
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    
    if (!open_file_table[i].in_use) {
        open_file_table[i].in_use = 1;
        open_file_table[i].vn = vn;
        open_file_table[i].flags = temp_permissions;
        open_file_table[i].file_pos = 0;
        return i;
    }
  }

  fs_errno = ENFILE;//no open space in open file table
  return -1;
}

//Vnode level
//check on open file table
void print_open_file_table() {
  printf("=== Open File Table ===\n");
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (open_file_table[i].in_use) {
          vnode_t *vn = open_file_table[i].vn;
          printf("FD %d: vnode=%p name=%s file pos %d valid %d permissions %d\n",i, (void*)vn, vn->name,open_file_table[i].file_pos, open_file_table[i].in_use, open_file_table[i].flags);
      }
  }
  printf("=======================\n");
}

//Vnode level
//readdir looks at the physical data tables in one data block
//given the entry number in the directory to read, readdir returns the table entry at that memory address
//in this case, fd is an index, not a file pointer!!
vdir_entry* f_readdir(vnode_t *vn, vfd_t fd)
{

  if (vn->type != 0) {
    fs_errno = ENOTDIR;
    return NULL;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {//error check: bad vnode, invalid inode
    fs_errno = EINVAL;
    return NULL;
  }

  char entry_buffer[sizeof(vdir_entry)];
  struct vdir_entry *table_entry = (vdir_entry*)malloc(sizeof(vdir_entry));
  vfd_t dir_fd = f_opendir(vn, (const char*)"");
  //print_open_file_table();
  off_t offset = ((off_t)fd)*sizeof(vdir_entry);//offset in data to read entry from
  //printf("look through %s for the %ld entry\n", vn->name, offset);
  f_seek(vn, dir_fd, offset, SEEK_SET);
  
  //printf("current position of the file: %d offset is %ld\n", open_file_table[dir_fd].file_pos,offset_ret);
  //read in the vdir entry
  int size = f_read(vn, entry_buffer, sizeof(vdir_entry),1, dir_fd);
  
  if (size != sizeof(vdir_entry)) {
    //printf("Error reading directory");
    return NULL;
  }

  //if it was copied correctly, return the entry
  memcpy((void*)table_entry, entry_buffer, sizeof(entry_buffer));
  f_closedir(vn, dir_fd);
  return table_entry;
}

//Vnode level
//remove a directory from the open file table
int f_closedir(vnode_t *vn, int dir_fd)
{
  
  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {
    fs_errno = EINVAL;
    return -1;
  }
  if (vn->type != 0) {
    fs_errno = ENOTDIR;
    return -1;
  }
  

  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (open_file_table[i].vn == vn && open_file_table[i].in_use) {
        open_file_table[i].in_use = 0;
        break;
    }
  }
  return 0;
}


// Vnode and inode level
//make a new directory under a given directory
//All directories are made to have read/write permissions
int f_mkdir(vnode_t *vn, const char* name)
{
  if (vn->type != 0) {
    fs_errno = ENOTDIR;
    return -1;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {
    fs_errno = EINVAL;
    return -1;
  }

  vnode* child = f_find(vn, (char*)name);//check if the file is amoung the children of vn.EEXIST, EISDIR
  if (child != NULL) {  
      fs_errno = EEXIST;
      return -1; 
  }

  if (strlen(name) > 150) {
    fs_errno = ENAMETOOLONG;
    return -1;
  }

  if (vn->num_children >= 300 || s_block.free_inode == 49) {
    fs_errno = ENOSPC;
    return -1;
  }

  int permanent_permissions = 0;
  permanent_permissions = permanent_permissions | PERMISSION_MASK;
  

  //make a given directory under the vnode
  char* mem_ptr;
  vnode *new_vn = (vnode*)(malloc(sizeof(vnode)));
  new_vn->type = 0;
  memset(&new_vn->name, 0, sizeof(new_vn->name));
  strncpy(new_vn->name, name, sizeof(new_vn->name));
  new_vn->num_children = 0;

  //make the new vnode
  new_vn->permissions = permanent_permissions;
  new_vn->parent = vn;
  new_vn->vnode_number = next_vnode;
  next_vnode++;
  new_vn->inode = s_block.free_inode;
  new_vn->size = 0;

  //move sblock inode up 1 on the list
  inode new_in;
  int next_free_in;
  mem_ptr = mem_buffer + INODE_OFFSET + new_vn->inode*BLOCK_SIZE;
  memcpy((void*)&new_in, mem_ptr, sizeof(inode));
  next_free_in = new_in.next_inode;

  //update the s_block
  s_block.free_inode = next_free_in;
  //printf("mkdir, next inode is %d, name is %s\n", next_free_in, name);
  mem_ptr = mem_buffer + BLOCK_SIZE;
  memcpy(mem_ptr, (void*)&s_block, sizeof(superblock));

  //initialize the new inode
  strncpy(new_in.name, name, sizeof(new_in.name)); 
  new_in.size = 0;
  new_in.type = 0;
  new_in.permissions = permanent_permissions;
  mem_ptr = mem_buffer + INODE_OFFSET + new_vn->inode*BLOCK_SIZE;
  memcpy(mem_ptr, (void*)&new_in, sizeof(inode));

  vdir_entry* new_table_entry = NULL;

  vdir_entry table_entry;
  table_entry.type = 0;
  table_entry.valid = 1;
  table_entry.inode_idx = new_vn->inode;
  strncpy(table_entry.name, name, sizeof(table_entry.name));

  int num_read = 0;
  int idx = 0;
  int found = 0;
  
  //Overwrite an existing invalid entry which is before valid entries
  while (num_read < vn->num_children) {
      new_table_entry = f_readdir(vn, idx);
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        num_read++;
      }
      
      if (new_table_entry != NULL && new_table_entry->valid == 0) {
        //this is where we want to overwrite
        int data_offset = idx*sizeof(vdir_entry);

        //set the directory to write to invalid entry
        vfd_t dir_fd = f_opendir(vn, (const char*) "");
        f_seek(vn, dir_fd, data_offset, SEEK_SET);

        //decrease the size of the parent dir before overwriting, write adds the size of fwrite
        //on the inode level
        struct inode parent_in;
        mem_ptr = mem_buffer +INODE_OFFSET + vn->inode*BLOCK_SIZE;
        memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
        parent_in.size = parent_in.size - (int)(sizeof(vdir_entry));
        memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));
        f_write(vn, (void*)&table_entry, sizeof(vdir_entry),1,dir_fd);
        //on the vnode level
        vn->size = vn->size - (int)(sizeof(vdir_entry));

        f_write(vn, (void*)&table_entry, sizeof(vdir_entry),1,dir_fd);
        
         f_closedir(vn, dir_fd);
        
        found = 1;
        free(new_table_entry);
        break;
      }
      free(new_table_entry);
      idx++;
  }

  if (found == 0) {
      vfd_t dir_fd = f_opendir(vn, (const char*) "");
      f_seek(vn, dir_fd,0, SEEK_END);//go to the end of the current data
      f_write(vn, (void*)&table_entry, sizeof(vdir_entry), 1, dir_fd);
      //printf("wrote %ld data to the inode\n", size);
      f_closedir(vn, dir_fd);
  }

  vn->child_ptrs[vn->num_children] = new_vn;
  vn->num_children = vn->num_children+1;

  //update the directory children on the inode level
  inode parent_in;
  mem_ptr = mem_buffer +INODE_OFFSET + vn->inode*BLOCK_SIZE;
  memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
  parent_in.num_children = parent_in.num_children+1;
  memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));

  return 0;
}

//Vnode level
//Recursively get rid of the contents of directories
int delete_vnode_tree(vnode* parent_vn, vnode* current_vn) {
  //printf("delete %d\n", current_vn->inode);
    
  //the inode is a file, set the inode to be free and return
  if (current_vn->type == 1) {
    
    f_remove(current_vn, 0);
    return 0;
  }

  //the inode is a dir inode, recurse
  for (int i = 0; i < current_vn->num_children; i++) {
    vnode* child_vn = current_vn->child_ptrs[i];
    delete_vnode_tree(current_vn, child_vn);
  }

  
  //printf("ready to remove parent dir\n");
  f_remove(current_vn, 0);
  return 0;
}
 
//Vnode level
//Get rid of all contents under a given directory
int f_rmdir(vnode_t *vn, const char* path)
{
  //struct vnode* to_remove = root_vnode;
  struct vnode* parent_vn = vn->parent;

  if (vn->type != 0) {
    fs_errno = ENOTDIR;
    return -1;
  }

  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {
    fs_errno = EINVAL;
    return -1;
  }

  delete_vnode_tree(parent_vn, vn);
  

  return 0;
}

//From inode level to vnode level
//recursive logic to build the tree
int build_vnode_tree(vnode* parent_vn, vnode* child_vn, int child_in_idx) {
  struct vdir_entry *table_entry;
  struct vnode* current_vn = child_vn;
  struct inode current_in;

  //get the inode;
  char* mem_ptr = (mem_buffer + INODE_OFFSET) + (child_in_idx * BLOCK_SIZE);
  //printf("build tree %d in mem buffer %p at address %p\n", next_vnode, mem_buffer, mem_ptr);
  memcpy((void*)&current_in, mem_ptr, sizeof(inode));

  // for(int h = 0; h < N_DBLOCKS; h++) {
  //   printf("data block %d\n", current_in.dblocks[h]);
  // }
  
  //initialize the new vnode
  current_vn->parent = parent_vn; 
  memset(&current_vn->name, 0, sizeof(current_vn->name));
  strcpy(current_vn->name, current_in.name);
  current_vn->permissions = current_in.permissions;
  current_vn->vnode_number = next_vnode;
  next_vnode++;
  current_vn->inode = child_in_idx;
  current_vn->size = current_in.size;
  
  //the inode is a file, return
  if (current_in.type == 1) {
    current_vn->type = 1;
    current_vn->num_children = -1;
    return 0;
  }

  //the inode is a dir inode,recurse
  current_vn->type = 0;
  //current_vn->num_children = (current_in.size)/(sizeof(vdir_entry));
  current_vn->num_children = current_in.num_children;

  //zero out the child pointers
  memset(&current_vn->child_ptrs, 0, sizeof(current_vn->child_ptrs));
  //printf("dir %s has %d size %d children based on inode\n",current_in.name, current_in.size, current_in.num_children);

  int children_read = 0;
  int idx = 0;
  //look through entries in the directory. If valid, make a vnode
  while (children_read < current_vn->num_children) {
    table_entry = f_readdir(current_vn, idx);
    if (table_entry != NULL && table_entry->valid == 1) {
      //printf("found child %s\n", table_entry->name);
      vnode* new_child_vn = (vnode*)malloc(sizeof(vnode));
      //set the current 
      current_vn->child_ptrs[children_read] = new_child_vn;
      build_vnode_tree(current_vn, new_child_vn, table_entry->inode_idx);
      children_read++;
    }
    free(table_entry);
    idx++;
  }

  return 0;
}

//initialize the mem_buffer
int init_fs() {
  
  //memset(mem_buffer, 0, sizeof(1000000));
  memset(mem_buffer, 0, sizeof(mem_buffer));
  memset(buffer_block, 0, BLOCK_SIZE);
  
  FILE *disk= fopen("DISK", "rb");

  if (disk == NULL) {
      printf("DISK file not found. File system not loaded.\n Please format file system by issuing \"./format\" command\n");
  } else {
    fread(mem_buffer, 1, 1000000, disk);
    
  }
  fclose(disk);

  //move beyond boot block to superblock
  char* mem_ptr = mem_buffer+BLOCK_SIZE;
  memcpy((void*)&s_block, mem_ptr, sizeof(superblock));

  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    open_file_table[i].in_use = 0;
    open_file_table[i].vn = NULL;
    open_file_table[i].flags = 0;
    open_file_table[i].file_pos = 0;
  }
  
  return 1;
}

//initialize the mem_buffer for the shell
int init_fs_shell() {
  
  //memset(mem_buffer, 0, sizeof(1000000));
  memset(mem_buffer, 0, sizeof(mem_buffer));
  memset(buffer_block, 0, BLOCK_SIZE);
  
  FILE *disk= fopen("DISK-newfile", "rb");

  if (disk == NULL) {
      printf("DISK file not found. File system not loaded.\n Please format file system by issuing \"./format\" command and running a test to format DISK-newfile \n");
      return -1;
  } else {
    fread(mem_buffer, 1000000 - 1,1, disk);
      //f_mount();
  }
  fclose(disk);

  //move beyond boot block to superblock
  char* mem_ptr = mem_buffer+BLOCK_SIZE;
  memcpy((void*)&s_block, mem_ptr, sizeof(superblock));

  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    open_file_table[i].in_use = 0;
    open_file_table[i].vn = NULL;
    open_file_table[i].flags = 0;
    open_file_table[i].file_pos = 0;
  }
  
  return 1;
}

//From inode level, creates vnode level
//f_mount opens the root directory, creates the vnode tree
int f_mount(vnode_t *vn, const char* sourcefile, const char* name) {
  
  struct vnode *current_vn = vn;
  struct inode current_in;
  memset((void*)&current_in, 0, sizeof(inode));

  //not mounting onto existing file system
  if (root_vnode == NULL) {
    //get root inode
    char* mem_ptr = mem_buffer+INODE_OFFSET;
    memcpy((void*)&current_in, mem_ptr, sizeof(inode));

    //deal with making the root vnode. if the root vnode is null, initialize vnode tree root
    if (current_in.type == 0) {
      root_vnode = current_vn;
      
      //printf("root dir name is %s inode at %p in table %p inode offset %d\n",current_in.name, mem_ptr, mem_buffer, INODE_OFFSET);

      build_vnode_tree(NULL, current_vn, 0);
      return 0;
    } else {
      fs_errno = ENOTDIR;
      return -1;
    }
    //root_vn_fd = f_opendir(root_vnode, (const char*)"");
  }

  return -1;
}


int f_unmount(vnode_t *vn, const char* path)
{
  /*free part of the vnode tree*/
 
  return 0;
}

//Saves inode level
//Save the file system to the given filename
int save_fs(const char* filename) {
    FILE *fp = fopen(filename, "wb");

    fwrite(mem_buffer, sizeof(mem_buffer),1 ,  fp);
   
    fclose(fp);
    return 0;
}

int save_fs_zeroed(const char* filename) {
  FILE *fp = fopen(filename, "wb");

  fwrite(mem_buffer, 1000000,1 ,  fp);
 
  fclose(fp);
  return 0;
}

//Vnode level
//find a vnode deep in a given path
//returns null if this does not exist
vnode* move_on_path(vnode* vn, const char* path) {
  
  vnode* current_vn = vn;
  
  char* token = strtok((char*)path, "/");
  while (token != NULL) {
    //printf("token: %s\n", token);
    if (current_vn == NULL) {
      //printf("error invalid path");
    } if (strcmp(token , ".") == 0) {
      current_vn = current_vn;
    } else if (strcmp(token , "..") == 0) {
      current_vn = current_vn->parent;
    } else {
      current_vn = f_find(current_vn, token);
    }
    token = strtok(NULL, "/");
    
  }
  

  return current_vn;
}

//Inode and vnode level
//Move a given file or directory from existing location in the file system to the given location node
int f_move(vnode* vn, const char* path) {
  char* mem_ptr;
  char filename[150];
  memset(filename, 0, sizeof(filename));
  int name_change = 0;

  char *path_for_split = strdup(path);
  vnode* new_parent_dir = move_on_path(root_vnode, path);
  
  if (new_parent_dir == NULL) {
    
    char *last_slash = strrchr(path_for_split, '/');
    if (last_slash != NULL) {
      //printf("complex path\n");
      *last_slash = '\0';// Remove the last component
      
      strcpy(filename, last_slash + 1);
      name_change = 1;
      new_parent_dir = move_on_path(root_vnode,path_for_split);
      if (new_parent_dir == NULL) {
        free(path_for_split);
        fs_errno = ENOENT;
        return -1;
      }
    }else  {
      free(path_for_split);
      fs_errno = ENOENT;
      return -1;
    }
  }
  free(path_for_split);
  vnode* parent_vn = vn->parent;
  
  //get rid of the old entry in vnode ptrs
  int i = 0;
  while(i < parent_vn->num_children &&parent_vn->child_ptrs[i] != vn ) {
        i++;
  }
  while (i < (parent_vn->num_children-1)) {
      parent_vn->child_ptrs[i] = parent_vn->child_ptrs[i+1];
      i++;
  }
  
  //set the vdir entry in the old parent directory to be invalid
  vdir_entry* new_table_entry;

  //recreate the original table entry to give to the new parent directory
  vdir_entry original_entry;
  if (name_change == 1 ) {
    printf("name is %s\n", filename);
    strcpy(original_entry.name, filename);
  } else {
    strcpy(original_entry.name, vn->name);
  }
  original_entry.type = vn->type;
  original_entry.valid = 1;
  original_entry.inode_idx = vn->inode;

  //printf("done parent is %s\n", parent_vn->name);
  int num_read = 0;
  int idx = 0;

  //remove the entry in the old parent inode directory
  while (num_read < parent_vn->num_children) {
      new_table_entry = f_readdir(parent_vn, idx);
      //found a valid child entry
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        num_read++;
      }
      
      //overwrite the searched-for child entry
      if (new_table_entry != NULL && strcmp(new_table_entry->name, vn->name) == 0) {
        
        //set the flag to invalid
        new_table_entry->valid =0;

        off_t offset = idx*sizeof(vdir_entry);
        vfd_t dir_fd = f_opendir(parent_vn, (const char*)"");

        //seek the location of the invalid entry
        f_seek(parent_vn, dir_fd, offset, SEEK_SET);

        //fwrite will have added to the size of the parent vnode, this makes it clear that the vdir entry was overwritten
        parent_vn->size = parent_vn->size - (int)(sizeof(vdir_entry));
        //overwrite the next entry
        f_write(parent_vn, new_table_entry, sizeof(vdir_entry),1, dir_fd);
        f_closedir(parent_vn, dir_fd);
        free(new_table_entry);
        //printf("found the entry to overwrite\n");
        break;
      }
      free(new_table_entry);
      idx++;
  }

  //name change on the vnode level may only occur after the overwrite loop
  if (name_change == 1) {
    strcpy(vn->name, filename);

    inode current_in;
    mem_ptr = mem_buffer + INODE_OFFSET + vn->inode*BLOCK_SIZE;
    memcpy((void*)&current_in, mem_ptr, sizeof(inode));
    strcpy(current_in.name, filename);
    memcpy(mem_ptr,(void*)&current_in, sizeof(inode));
  }
  
  //decrease num children by 1 for old parents
  parent_vn->num_children = parent_vn->num_children-1;
  
  //fwrite will have added to the size of the parent inode, deleting from size makes it clear that the vdir entry was overwritten
  inode parent_in;
  mem_ptr = mem_buffer +INODE_OFFSET + parent_vn->inode*BLOCK_SIZE;
  memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
  parent_in.num_children = parent_in.num_children-1;
  parent_in.size = parent_in.size - (int)(sizeof(vdir_entry));
  memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));
  
  vn->parent = new_parent_dir;

  //write to the inode of the new parent dir
  num_read = 0;
  idx = 0;
  memset((void*)&parent_in, 0 , sizeof(inode));//zero out the inode before reusing for the new_parent_dir
  int found = 0;

  //Overwrite an existing invalid entry which is before the last valid entry
  while (num_read < new_parent_dir->num_children) {
    new_table_entry = f_readdir(new_parent_dir, idx);
    if (new_table_entry != NULL && new_table_entry->valid == 1) {
      num_read++;
    }
    
    if (new_table_entry != NULL && new_table_entry->valid == 0) {
      int data_offset = idx*sizeof(vdir_entry);//this is where we want to overwrite

      vfd_t new_dir_fd = f_opendir(new_parent_dir, (const char*) "");
      f_seek(new_parent_dir, new_dir_fd, data_offset, SEEK_SET);//set the directory to write to invalid entry

      //decrease the size of the parent dir before overwriting, write adds the size of fwrite
      //on the inode level
      mem_ptr = mem_buffer +INODE_OFFSET + new_parent_dir->inode*BLOCK_SIZE;
      memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
      parent_in.size = parent_in.size - (int)(sizeof(vdir_entry));
      memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));

      f_write(new_parent_dir, (void*)&original_entry, sizeof(vdir_entry),1,new_dir_fd);
      //on the vnode level
      new_parent_dir->size = new_parent_dir->size - (int)(sizeof(vdir_entry));
      
      f_write(new_parent_dir, (void*)&original_entry, sizeof(vdir_entry),1,new_dir_fd);
      
      f_closedir(new_parent_dir, new_dir_fd);
      
      found = 1;
      free(new_table_entry);
      break;
    }
    free(new_table_entry);
    idx++;
  }

  //if you cannot overwrite, then write to the new parent. size of parent increases
  if (found == 0) {
    printf("move here\n");
   
    vfd_t new_dir_fd = f_opendir(new_parent_dir, (const char*) "");
    f_seek(new_parent_dir, new_dir_fd, 0, SEEK_END);//go to the end of the current data
    f_write(new_parent_dir, (void*)&original_entry, sizeof(vdir_entry), 1, new_dir_fd);
    f_closedir(new_parent_dir, new_dir_fd);
  }

  //write to the vnodes of the new parent dir to increase num children
  new_parent_dir->child_ptrs[new_parent_dir->num_children] = vn;
  new_parent_dir->num_children = new_parent_dir->num_children+1;
  

  mem_ptr = mem_buffer +INODE_OFFSET + new_parent_dir->inode*BLOCK_SIZE;
  memcpy((void*)&parent_in, mem_ptr, sizeof(inode));
  parent_in.num_children = parent_in.num_children + 1;
  memcpy(mem_ptr,(void*)&parent_in, sizeof(inode));


  return 0;
}

//Vnode level
//delete the vnode tree recursively
int delete_vnode_tree_only( vnode* current_vn) {
  //printf("delete %d\n", current_vn->inode);
    
  //the inode is a file, set the inode to be free and return
  if (current_vn->type == 1) {
    free(current_vn);
    return 0;
  }

  //the inode is a dir inode, recurse
  for (int i = 0; i < current_vn->num_children; i++) {
    vnode* child_vn = current_vn->child_ptrs[i];
    delete_vnode_tree_only(child_vn);
  }
  
  //printf("ready to remove parent dir\n");
  free(current_vn);
  return 0;
}

//Vnode level
//Prints the path from the root to the given location in the file system
char* print_path(vnode* vn) {
  char temp[150] = {0};
  char* pos = temp + 50 - 1;
  *pos = '\0';
  while (vn != NULL) {
    int len = strlen(vn->name);
    pos -= len;
    if (pos < temp) return NULL;  // overflow check
    memcpy(pos, vn->name, len);

    vn = vn->parent;
    if (vn != NULL) {
        pos--;
        if (pos < temp) return NULL;  // overflow check
        *pos = '/';
    }
  }
  size_t result_len = strlen(pos);
  char* result = (char*)malloc(result_len + 1);
  if (!result) return NULL;
  strcpy(result, pos);

  return result;
}

//implement chmod
int change_permanent_permissions(char* flags_input, char* path) {
  unsigned int flags = 0;
  vnode* vn = move_on_path(root_vnode, path);
  
  if (vn == NULL || (vn->inode < 0 || vn->inode > 49)) {
    fs_errno = EINVAL;
    return -1;
  }
  if (vn->type == 0) {
    fs_errno = EISDIR;
    return -1;
  }

  inode current_in;
  //printf("permissions were %d  ", vn->permissions);
  memset((void*)&current_in, 0, sizeof(inode));
  char* mem_ptr = mem_buffer + INODE_OFFSET + vn->inode*BLOCK_SIZE;
  memcpy((void*)&current_in, (void*)mem_ptr, sizeof(inode));

  //symbolic
  if (strstr(flags_input, "=") != NULL ) {
    if ((strstr(flags_input, "r")) != NULL && (strstr(flags_input, "w")) != NULL) {
      vn->permissions = flags | PERMISSION_MASK;
      current_in.permissions = flags | PERMISSION_MASK;
    } else if ((strstr(flags_input, "w")) != NULL) {
      vn->permissions = flags | S_IWUSR;
      current_in.permissions = flags | S_IWUSR;
    } else if ((strstr(flags_input, "r")) != NULL) {
      vn->permissions = flags | S_IRUSR;
      current_in.permissions = flags | S_IRUSR;
    }
  }
  else if (strstr(flags_input, "+") != NULL ) {
    if ((strstr(flags_input, "r")) != NULL && (strstr(flags_input, "w")) != NULL) {
      vn->permissions = vn->permissions | PERMISSION_MASK;
      current_in.permissions = current_in.permissions | PERMISSION_MASK;
    } else if ((strstr(flags_input, "w")) != NULL) {
      vn->permissions = vn->permissions | S_IWUSR;
      current_in.permissions = current_in.permissions | S_IWUSR;
    } else if ((strstr(flags_input, "r")) != NULL) {
      vn->permissions = vn->permissions | S_IRUSR;
      current_in.permissions = current_in.permissions| S_IRUSR;
    }
  }
  else if (strstr(flags_input, "-") != NULL ) {
    if ((strstr(flags_input, "r")) != NULL && (strstr(flags_input, "w")) != NULL) {
      vn->permissions = vn->permissions & ~PERMISSION_MASK;
      current_in.permissions = current_in.permissions & ~PERMISSION_MASK;
    } else if ((strstr(flags_input, "w")) != NULL) {
      vn->permissions = vn->permissions & ~S_IWUSR;
      current_in.permissions = current_in.permissions & ~S_IWUSR;
    } else if ((strstr(flags_input, "r")) != NULL) {
      vn->permissions = vn->permissions & ~S_IRUSR;
      current_in.permissions = current_in.permissions & ~S_IRUSR;
    }
  }
  
  //absolute
  if (strstr(flags_input, "0400") != NULL) {
    flags = flags | S_IRUSR;
    vn->permissions = (flags & PERMISSION_MASK);
    current_in.permissions = (flags & PERMISSION_MASK);
  }
  if (strstr(flags_input, "0200") != NULL) {
    flags = flags | S_IWUSR;
    vn->permissions = (flags & PERMISSION_MASK);
    current_in.permissions = (flags & PERMISSION_MASK);
  } if (strstr(flags_input, "0600") != NULL) {
    flags = flags | S_IWUSR | S_IRUSR;
    vn->permissions = (flags & PERMISSION_MASK);
    current_in.permissions = (flags & PERMISSION_MASK);
  }
  
  vfd_t new_fd = f_open(vn, vn->name, flags);
  f_close(vn, new_fd);//make sure that the file is closed

  
  //set inode permissions in memory
  memcpy(mem_ptr, (void*)&current_in, sizeof(inode));

  //printf("are now %d  \n", vn->permissions);
  return 0;
}

//Vnode level
//Removes every existing vnode under the root
int cleanup_fs() {
  if (root_vnode != NULL) {
    delete_vnode_tree_only(root_vnode);
  }
  
  memset(open_file_table, 0, sizeof(open_file_table));
  return 0;
}
