#include <assert.h>
#include <fcntl.h>
#include "vfs.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
//check that temporary open permissions are temporary
int main()
{
    
  struct vnode* fs_root = (vnode*)(malloc(sizeof(vnode)));
  init_fs();
  f_mount(fs_root, "file_sys.bin", "./");
  f_opendir(fs_root, (const char*)"");
  //unsigned int flags = 0600 | O_CREAT | O_WRONLY | O_TRUNC;
  vfd_t fd0 = f_open(fs_root, "newfile.txt", (O_CREAT | S_IWUSR |S_IRUSR)); 
  printf("child added %d\n",fs_root->num_children);
  
  vnode_t* newnode = f_find(fs_root, "newfile.txt");
  f_close(newnode, fd0);
  vfd_t fd = f_open(newnode, "newfile.txt", (O_APPEND | S_IWUSR)); 
  f_seek(newnode, fd, 0, SEEK_SET);
  printf("here\n");
  
  char data[64];
  char data2[64];
  char data3[64];

  memset(data, 0 , sizeof(data));
  memset(data2, 0 , sizeof(data));
  memset(data3, 0 , sizeof(data));

  strncpy(data, "It's a festival. Not a restival.", 64);
  size_t size = f_write(newnode, data, 64, 1, fd);
  printf("here now size %ld\n", size);
  assert(size == 64);
  
  f_seek(newnode, fd, 0, SEEK_SET);
  f_read(newnode, data2, 64, 1, fd);
  
  f_close(newnode, fd);
  printf("retrieved data: %s\n", data2);

  vfd_t fd2 = f_open(newnode, "newfile.txt", (O_APPEND | S_IRUSR)); 
  f_seek(newnode, fd2, 0, SEEK_SET);
  f_read(newnode, data3, 64, 1, fd2);
  f_close(newnode, fd2);
  printf("retrieved data: %s\n", data3);
  
  save_fs("DISK-newfile");
  cleanup_fs();
  return 0;

}