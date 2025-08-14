#include <assert.h>
#include <fcntl.h>
#include "vfs.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
//Test moving nodes in the tree
int main()
{
    
  struct vnode* fs_root = (vnode*)(malloc(sizeof(vnode)));
  init_fs();
  f_mount(fs_root, "file_sys.bin", "./");

  unsigned int flags =  O_CREAT | S_IRUSR | S_IWUSR;
  vfd_t file_fd = f_open(fs_root, "newfile.txt", flags); 
  printf("child added %d\n",fs_root->num_children);
  vnode_t* newnode = f_find(fs_root, "newfile.txt");

  if (newnode == NULL) {
    printf("child is missing\n");
    exit(0);
  }

  char data[64];
  char data2[64];
  memset(data, 0 , sizeof(data));
  memset(data2, 0 , sizeof(data));
  strncpy(data, "It's a festival. Not a restival.", 64);

  size_t size = f_write(newnode, data, 64, 1, file_fd);
  printf("here now size %ld\n", size);
  assert(size == 64);
 
  f_seek(newnode, file_fd, 0, SEEK_SET);
  f_read(newnode, data2, 64, 1, file_fd);
  f_close(newnode, file_fd);
  printf("retrieved data: %s\n", data2);

  f_mkdir(fs_root, "Adir");
  
  printf("root children num %d\n", fs_root->num_children);

  vdir_entry* new_table_entry = NULL;
    int num_read = 0;
    int idx = 0;
    
  while (num_read < fs_root->num_children) {
      new_table_entry = f_readdir(fs_root, idx);
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        printf("file %s\n", new_table_entry->name);
        
        //break;
        num_read = num_read+1;
      }
      free(new_table_entry);
      idx++;
  }

   
    vnode_t* dirnode = f_find(fs_root, "Adir");
    //vfd_t dir_fd = f_opendir(dirnode, (const char*)"");
    vfd_t file_fd2 = f_open(dirnode, "A.txt", flags); 
    printf("A dir children num %d\n", dirnode->num_children);
    printf("child added %d\n",fs_root->num_children);
    
    vnode_t* filenode = f_find(dirnode, "A.txt");
    printf("inode for A.txt %d\n", filenode->inode);
    char data3[700];
    char data4[700];
    memset(data, 0 , sizeof(data));
    memset(data2, 0 , sizeof(data));
    const char* text2 = "Por cuanto por parte de vos, Miguel de Cervantes, nos fue fecha relación que habíades compuesto un libro intitulado El ingenioso hidalgo de la Mancha, el cual os había costado mucho trabajo y era muy útil y provechoso, nos pedistes y suplicastes os mandásemos dar licencia y facultad para le poder imprimir, y previlegio por el tiempo que fuésemos servidos, o como la nuestra merced fuese; lo cual visto por los del nuestro Consejo, por cuanto en el dicho libro se hicieron las diligencias que la premática últimamente por nos fecha sobre la impresión de los libros dispone, fue acordado que debíamos mandar dar esta nuestra cédula para vos, en la dicha razón; y nos tuvímoslo por bien.";
    size_t text2len = strlen(text2);
    strncpy(data3, text2, text2len);

    size_t size2 = f_write(filenode, data3, text2len, 1, file_fd2);
    printf("here now size %ld\n", size2);
    assert(size == 64);
  
    f_seek(filenode, file_fd2, 0, SEEK_SET);
    f_read(filenode, data4, text2len, 1, file_fd2);
    
    printf("retrieved data: %s\n", data4);
    f_close(filenode, file_fd2);

    num_read = 0;
    idx = 0;
    while (num_read < fs_root->num_children) {
      new_table_entry = f_readdir(fs_root, idx);
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        printf("file %s\n", new_table_entry->name);
        
        //break;
        num_read = num_read+1;
      }
      free(new_table_entry);
      idx++;
    }

    f_move(filenode, ".");
    printf("root children num %d\n", fs_root->num_children);

    num_read = 0;
    idx = 0;
    while (num_read < fs_root->num_children) {
      new_table_entry = f_readdir(fs_root, idx);
      if (new_table_entry != NULL && new_table_entry->valid == 1) {
        printf("file %s idx %d\n", new_table_entry->name, idx);
        num_read = num_read+1;
      }
      free(new_table_entry);
      idx++;
    }
    
  
  save_fs("DISK-newfile");
  cleanup_fs();
  return 0;

}