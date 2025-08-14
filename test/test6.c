#include <assert.h>
#include <fcntl.h>
#include "vfs.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

//test larger text size 
int main()
{
    
  struct vnode* fs_root = (vnode*)(malloc(sizeof(vnode)));
  init_fs();
  f_mount(fs_root, "file_sys.bin", "./");

  unsigned int flags =  O_CREAT | S_IRUSR | S_IWUSR;
  vfd_t fd = f_open(fs_root, "newfile.txt", flags); 
  printf("child added %d %d\n",fs_root->num_children, fd);

  //this works
   vdir_entry* new_table_entry = NULL;
    int num_read = 0;
    int idx = 0;
    
    while (num_read < fs_root->num_children) {
      new_table_entry = f_readdir(fs_root, idx);
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        //to make sure that size is the same, indicate that you are erasing/overwriting one vdir
        //this is where we want to write
        printf("file %s\n", new_table_entry->name);
        break;
      }
      free(new_table_entry);
      idx++;
    }
  //assert(fd > 0);implement this along with the ability for several files to be open


  vnode_t* newnode = f_find(fs_root, "newfile.txt");
  if (newnode == NULL) {
    printf("child is missing\n");
    exit(0);
  }
  char data[2002];
  char data2[2002];
  strncpy(data, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 2000);
                 //aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
  size_t size = f_write(newnode, data, sizeof(char), 2000, fd);

  assert(size == 2000);

  f_seek(newnode, fd, 0, SEEK_SET);
  f_read(newnode, data2, 2000, 1, fd);
  f_close(newnode, fd);
  printf("retrieved data: %s\n", data2);
  printf("retrived data size: %ld\n", sizeof(data2));
  cleanup_fs();
  save_fs("DISK-newfile");

  return 0;

}