#include <assert.h>
#include <fcntl.h>
#include "vfs.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
//check the ability of a directory to hold files - simulate simple ls
int main()
{
    struct vnode* fs_root = (vnode*)(malloc(sizeof(vnode)));
    init_fs();
    f_mount(fs_root, "file_sys.bin", "./");

    unsigned int flags =  O_CREAT | S_IRUSR | S_IWUSR;
    vfd_t fd = f_open(fs_root, "newfile.txt", flags); 
    printf("child added %d\n",fs_root->num_children);

    vnode_t* newnode = f_find(fs_root, "newfile.txt");
    printf("here\n");
    if (newnode == NULL) {
        printf("child is missing\n");
        exit(0);
    }
    char data[64];
    char data2[64];
    strncpy(data, "It's a festival. Not a restival.", 64);
    size_t size = f_write(newnode, data, sizeof(char), 64, fd);
    printf("here now size %ld\n", size);
    assert(size == 64);

    f_seek(newnode, 0, 0, SEEK_SET);
    f_read(newnode, data2, 64, 1, fd);
    f_close(newnode, fd);
    printf("retrieved data: %s\n", data2);

    
    
    int num_read = 0;
    int idx = 0;
    vdir_entry* new_table_entry = NULL;
    
    while (num_read < fs_root->num_children && idx < 10) {
      new_table_entry = f_readdir(fs_root, idx);
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        printf("file %s\n", new_table_entry->name);
        num_read++;
      }
      free(new_table_entry);
      idx++;
    }
    cleanup_fs();
    save_fs("DISK-newfile");
    return 0;
}