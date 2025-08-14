/*Alison Teske
shell version 2
3 Feb 2025
*/
#include "libparser.h"
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>

#include "vfs.h"
#include "format.h"//just included for max file size metric
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

// Helpful macros for working with color
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/*
To do:
- support parsing for >, and >>. >> appends text to a file
- change the pwd according to the current dir

make sure that current dir and refed_dir are open
Notes on shell functionality
- 
*/

vnode* root_vnode;/*This is the root*/
vfd_t root_fd;
vnode* current_dir;
vfd_t current_fd;
//this is the node on which the command is being executed
vnode* refed_node;
vfd_t refed_fd;
//if we are writing to a file, this says which one
vnode* out_node;
vfd_t out_fd;
vnode* nextnode;
int null_flag = 0;
int trunc_flag = 0;
int append_flag = 0;
char filename[150];//for when a new file or directory is created

char data_buffer[MAX_FILE_SIZE];
char out_buffer[MAX_FILE_SIZE];
off_t out_buffer_offset = 0;
size_t out_buffer_len;

char* make_prompt(char* pwd) {
    long unsigned int i = 0;
    pwd = get_current_dir_name();
    
    printf(ANSI_COLOR_GREEN "*" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_GREEN "*" ANSI_COLOR_RESET);
    
    for (i = 0; i < strlen(pwd); i++) {
      printf(ANSI_COLOR_BLUE);
      printf("%c" ,pwd[i]);
      printf(ANSI_COLOR_RESET);
    }

    printf(ANSI_COLOR_GREEN "$" ANSI_COLOR_RESET);
    printf(ANSI_COLOR_GREEN ">" ANSI_COLOR_RESET);

    return pwd;
}

int parse_cmd() {
    char *input = NULL;
  //pid_t cpid;
 //int status = 0;
  struct Cmd current_cmd;
  char *pwd = NULL;
  int i = 0;
  //char path[100] = {0};
  char outpath[100] = {0};
  memset(out_buffer,0, sizeof(MAX_FILE_SIZE));
  memset(data_buffer,0, sizeof(MAX_FILE_SIZE));
  //int out_file;
  //char *token;

  
  //int flags = O_CREAT | O_TRUNC | O_WRONLY;
  //int perms = 0644;
  printf(ANSI_COLOR_GREEN "To switch directories, use /<relativepath> syntax!\n" ANSI_COLOR_RESET);

  //inifinite loop
  while (1) {
    //make prompt
    null_flag = 0;
    trunc_flag = 0;
    append_flag = 0;
    pwd = make_prompt(pwd);
    out_buffer_offset = 0;
    memset(out_buffer, 0, MAX_FILE_SIZE);
    memset(data_buffer, 0,MAX_FILE_SIZE);
    
    input = readline(">");
    //printf("%s\n",input);
    if (strcmp(input, "") == 0) {
      free(pwd);
      free(input);
      continue;
    }

    add_history(input);

    //quit
    if (strcmp(input, "quit") == 0) {
      save_fs("DISK-newfile");
      cleanup_fs();
      free(pwd);
      free(input);
      return 0;
    }
    
    //command parsed, memory is malloced
    get_command(input, &current_cmd);

    if (1) {
      //TODO: check that the fds are null terminated
      //> to output file
      if (current_cmd.cmd1_fds[0] != NULL) {
        strcpy(outpath, current_cmd.cmd1_fds[0]);
      }
      //>> to output file
      if (current_cmd.cmd1_fds[1] != NULL) {
        strcpy(outpath, current_cmd.cmd1_fds[1]);
      }
      
      //there is a command
      if (current_cmd.cmd1_argv[0] != NULL) {
        //printf("in %s\n", current_dir->name);

        //this is the path to the file/directory on which to perform the command
        refed_node = current_dir;
        if (current_cmd.cmd1_argv[1] != NULL) {
          char *path_for_split = strdup(current_cmd.cmd1_argv[1]); 
          refed_node = move_on_path(current_dir,current_cmd.cmd1_argv[1]);
          if (refed_node == NULL) {
            //printf("null dir %s\n", path_for_split);
            
            char *last_slash = strrchr(path_for_split, 47);
            if (last_slash != NULL) {
                //printf("complex path\n");
                *last_slash = '\0';// Remove the last component
                strcpy(filename, last_slash + 1);
                
            } else {
              strcpy(filename, path_for_split);
              path_for_split[0] = '\0';// No slash found, path becomes empty
            }
            refed_node = move_on_path(current_dir,path_for_split);
            if (refed_node == NULL ) {
              refed_node = current_dir;
            }
            //printf("new name: %s\n", filename);
            null_flag = 1;//this indicates that the file was not found on the path, commands can be handled accordingly
          }

          //printf("referenced dir: %s size  %d\n", refed_node->name, refed_node->size);
          
          if (refed_node->type == 1 && refed_node != current_dir) {
            refed_fd = f_open(refed_node, refed_node->name, (O_APPEND | S_IRUSR |S_IWUSR ));
          } else if (refed_node != current_dir) {
            refed_fd = f_opendir(refed_node, (const char*)"" );
          }
          //printf("refed_fd %d %d\n", refed_fd, fs_errno);
          free(path_for_split);
        }

        //find output place
        if (current_cmd.cmd1_fds[0] != NULL || current_cmd.cmd1_fds[1] != NULL) {

          if (current_cmd.cmd1_fds[0] != NULL) {//this says there is a >
            trunc_flag = 1;//truncate the given file
            out_node = move_on_path(current_dir,current_cmd.cmd1_fds[0]);
            if (out_node == NULL) {
              printf("invalid output\n");
              continue;
            }
            //printf("outnode name: %s size: %d\n", out_node->name, out_node->size);
          } else if (current_cmd.cmd1_fds[1] != NULL) {//this says there is a >>
            append_flag = 1;//append to the given file
            out_node = move_on_path(current_dir,current_cmd.cmd1_fds[1]);
            if (out_node == NULL) {
              printf("invalid output\n");
              continue;
            } 
            //printf("outnode name: %s size: %d\n", out_node->name, out_node->size);
          }
        }
        
        
        /*fill out command specific actions*/
        if (strcmp(current_cmd.cmd1_argv[0], "ls") == 0) {
          
          out_buffer_offset += snprintf(out_buffer + out_buffer_offset, sizeof(out_buffer) - out_buffer_offset, "directory has %d children\n", refed_node->num_children);

          for (int i = 0; i < refed_node->num_children; i++) {
              vnode *child = refed_node->child_ptrs[i];
              out_buffer_offset += snprintf(out_buffer + out_buffer_offset, sizeof(out_buffer) - out_buffer_offset, "%s  ", child->name);
          }

          out_buffer_offset += snprintf(out_buffer + out_buffer_offset, sizeof(out_buffer) - out_buffer_offset, "\n");
          out_buffer_len = out_buffer_offset;//this is to say how much should actually get written
          

        } else if (strcmp(current_cmd.cmd1_argv[0], "chmod") == 0) {
          //either give octal 0200 to hard reset to S_IWUSR, 0400 to hard reset to S_IRUSR, 0600 for both 
          //to modify permanent permissions, use = to hard reset, + to add a bit, - to zero one out. use w to change write permission, r to change read permission. example : chmod +rw no_permissions.txt
          change_permanent_permissions(current_cmd.cmd1_argv[1], current_cmd.cmd1_argv[2]);
        } else if (strcmp(current_cmd.cmd1_argv[0], "mkdir") == 0) {//mkdir within an existing dir acts strange
          f_mkdir(refed_node, (const char*)filename);

          if (refed_node != current_dir) {
            f_closedir(refed_node, refed_fd);
          }
        } else if (strcmp(current_cmd.cmd1_argv[0], "rmdir") == 0) {
          if (refed_node == current_dir || refed_node->type == 1 || refed_node == root_vnode) {
            printf("illegal remove statement\n");
          } else {
            f_closedir(refed_node, refed_fd);
            f_rmdir(refed_node, (const char*)"");
          }
        } else if (strcmp(current_cmd.cmd1_argv[0], "cd") == 0) {
          //closes the current dir
          if (refed_node != current_dir) {
            f_closedir(current_dir, current_fd);
          }
        
          current_dir = refed_node;
          current_fd = refed_fd;
          //keep refed_node, the new current dir, open
        } else if (strcmp(current_cmd.cmd1_argv[0], "pwd") == 0) {
          char* path = print_path(refed_node);
          out_buffer_offset += snprintf(out_buffer + out_buffer_offset, sizeof(out_buffer) - out_buffer_offset, "%s", path);
          out_buffer_len = out_buffer_offset;
          free(path);
        } else if (strcmp(current_cmd.cmd1_argv[0], "cat") == 0) {
          memset(data_buffer, 0, sizeof(MAX_FILE_SIZE));
          memset(out_buffer, 0, sizeof(MAX_FILE_SIZE));
          //close the opened node
          if (refed_node->type == 0) {
            printf("invalid command\n");
          } else {
            f_close(refed_node, refed_fd);
            refed_fd = f_open(refed_node, refed_node->name, (O_APPEND | S_IRUSR));//reopen only in read mode
            f_seek(refed_node, refed_fd, 0, SEEK_SET);
            f_read(refed_node, data_buffer, refed_node->size, 1, refed_fd);
            //printf("just read: %s\n", data_buffer);
            out_buffer_offset += snprintf(out_buffer + out_buffer_offset, sizeof(out_buffer) - out_buffer_offset, "%s", data_buffer);
            out_buffer_len = out_buffer_offset;
          }
        } else if (strcmp(current_cmd.cmd1_argv[0], "touch") == 0) {
          unsigned int flags =  O_CREAT | S_IRUSR | S_IWUSR;
          vfd_t new_file = f_open(refed_node, filename, flags);
          vnode* filenode = f_find(refed_node,filename);
          f_close(filenode, new_file);
          
          if (refed_node != current_dir) {//close a dir
            f_closedir(refed_node, refed_fd);
          }
        } else if (strcmp(current_cmd.cmd1_argv[0], "rm") == 0) {
            if (refed_node == current_dir || refed_node->type == 0) {
              printf("illegal remove statement\n");
            } else {
              f_close(refed_node, refed_fd);
              f_remove(refed_node, refed_fd);
            }
          
        } else if (strcmp(current_cmd.cmd1_argv[0], "mv") == 0) {
          //printf("move to %s\n", current_cmd.cmd1_argv[2]);
          f_move(refed_node, current_cmd.cmd1_argv[2]);
          if (refed_node != current_dir) {//close a dir
            f_closedir(refed_node, refed_fd);
          }
        } else if (strcmp(current_cmd.cmd1_argv[0], "stat") == 0) {
            //printf("not implemented\n");
            vstat* node_stats = (vstat*)(malloc(sizeof(vstat))); 
            f_stat(refed_node, refed_fd, node_stats);
            out_buffer_offset += snprintf(out_buffer + out_buffer_offset, sizeof(out_buffer) - out_buffer_offset, "data: \ntype %d\n inode %d\n permissions %d\n size %d\n size in vnode %d\nblocks %d\n",node_stats->st_type, node_stats->st_inode, node_stats->st_perms, node_stats->st_size, refed_node->size, node_stats->st_blocks);
            out_buffer_len = out_buffer_offset;
        }else if (strcmp(current_cmd.cmd1_argv[0], "mount") == 0) {
          
          printf("not implemented\n");
        } else if (strcmp(current_cmd.cmd1_argv[0], "mount") == 0) {
          
          printf("not implemented\n");
        } else if (strcmp(current_cmd.cmd1_argv[0], "unmount") == 0) {
          
          printf("not implemented\n");
        } else if (strcmp(current_cmd.cmd1_argv[0], "echo") == 0) {
          
          out_buffer_offset += snprintf(out_buffer + out_buffer_offset, sizeof(out_buffer) - out_buffer_offset, "%s", current_cmd.cmd1_argv[1]);
          out_buffer_len = out_buffer_offset;
        } else {
          printf("command not found\n");
        }

        if (out_buffer_offset != 0 && trunc_flag == 1  ) {
          if (out_node->type == 0) {
            printf("invalid destination\n");
          }
          vfd_t out_fd = f_open(out_node, out_node->name, (O_TRUNC  | S_IWUSR ));
          //printf("out_fd %d %d\n", out_fd, fs_errno);
          f_write(out_node, out_buffer, out_buffer_len, 1, out_fd);
          f_close(out_node, out_fd);
          //printf("trunc out buffer len %ld file size %d\n", out_buffer_len, out_node->size);
        } else if (out_buffer_offset != 0 && append_flag == 1  && out_node->type == 1) {
          if (out_node->type == 0) {
            printf("invalid destination\n");
          }
          vfd_t out_fd = f_open(out_node, out_node->name, (O_APPEND| S_IWUSR ));
          f_write(out_node, out_buffer, out_buffer_len, 1, out_fd);
          f_close(out_node, out_fd);
          //printf("append out buffer len %ld\n", out_buffer_len);
        } else {
          
          //printf("printing from outbuffer: %s \n", out_buffer);
          printf(" %s \n", out_buffer);
          
        }
        
        if (i == -1) {
          perror("error with execv");
        }
      }
    }
    

    free(pwd);
    free(current_cmd.cmd1_argv);
    free(current_cmd.cmd2_argv);
    free(input);
  }
    
  return 0;
}

int main() {
  struct vnode* fs_root = (vnode*)(malloc(sizeof(vnode)));
  init_fs_shell();
  f_mount(fs_root, "DISK-newfile", "./");
  root_vnode = fs_root;
  refed_node = fs_root;
  current_dir = fs_root;
  root_fd = f_opendir(fs_root, (const char*) "");//manually open root file
  current_fd = root_fd; 
  parse_cmd();
}
