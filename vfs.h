#ifndef vfs_H_
#define vfs_H_

//I can have a max of 50 open files, because I can have a max of 50 inodes
#define MAX_OPEN_FILES 50
#include <unistd.h>

typedef int vfd_t; //This is the inode idx

typedef struct vnode {
    int vnode_number;
    char name[150];
    struct vnode *parent;
    int permissions;
    int type;
    int num_children;
    /*struct unix_fs { //these structs need to be fleshed out by you
        int inode;//is this the idx of the inode? isn't all the above info also in the inode?
    } unixfs;*/
    int inode;
    int size;
    //actually a max of 522 children are possible
    vnode* child_ptrs[300];
} vnode_t;

typedef struct vstat {
  int st_type; // file or directory
  int st_inode; // inode corresponding to file
  int st_perms; // permission mode of the file
  int st_size; // filesize
  int st_blocks; // number of blocks given to the file
} vstat_t;

struct vdir_entry {
  // your data structure here
  int type; /*dir = type 0, file = 1*/
  int valid; /*valid = 1, invalid = 0*/
  char name[150];
  int inode_idx;
};

typedef struct open_file_entry {
  vnode_t *vn;       // Pointer to vnode
  int flags;         // Open flags (like O_WRONLY, O_RDWR, etc)
  int in_use;        // Simple flag to show if this entry is active
  int file_pos;
} open_file_entry_t;

vfd_t f_open(vnode_t *vn, const char *filename, int flags);
size_t f_read(vnode_t *vn, void *data, size_t size, int num, vfd_t fd);
//size_t f_write(vnode_t *vn, void *data, size_t size, vfd_t fd);
size_t f_write(vnode_t *vn, void *data, size_t size,int num, vfd_t fd);
int f_close(vnode_t *vn, vfd_t fd);
int f_seek(vnode_t *vn, vfd_t fd, off_t offset, int whence);
int f_stat(vnode_t *vn, vfd_t fd, vstat_t* stats);
int f_remove(vnode_t *vn, vfd_t fd);
vfd_t f_opendir(vnode_t *vn, const char* path);/*cd along a path through directories*/
vdir_entry* f_readdir(vnode_t *vn, vfd_t fd);
int f_closedir(vnode_t *vn, vfd_t fd); 
int f_mkdir(vnode_t *vn, const char* path); 
int f_rmdir(vnode_t *vn, const char* path); 
int f_mount(vnode_t *vn, const char* sourcefile, const char* path); 
int f_unmount(vnode_t *vn, const char* path); 

int init_fs();
int init_fs_shell();
vnode* f_find(vnode* parent_dir, const char* filename);
char* f_readdir_get_addr(vnode_t * vn, vdir_entry* table_entry, const char* name, int validity);
int save_fs(const char* filename);
int cleanup_fs();
vnode* move_on_path(vnode* vn, const char* path);
int f_move(vnode* vn, const char* path);
char* print_path(vnode* vn);
int change_permanent_permissions(char* flags_input, char* path);

extern vnode_t* fs_root;
extern int fs_errno;

#endif
