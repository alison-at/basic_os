Alison Teske
4/27/25

Format Metrics:

- Blocksize = 512
- Memory size = 10000000
- 50 inodes (50 files and dirs)
- default data storage = 2,560 bytes per file, 5 blocks per file automatically
- 128,000 data bytes are automatically alloced, 250 blocks
- 870976 bytes of data space remain free, 1701 data blocks are free to be alloced
- Leaves 34 blocks per file if evenly distributed (unlikely)
- Max file size = 5*512 + 4*(512/4)*512 = 264,704 bytes = 517 blocks + 4 overhead blocks

To change format metrics, change Inode and Vnode offset and change ENOSPC warnings in vfs.c 


Functionality & Testing Notes:

- init_fs is a seperate call from f_mount(root). init_fs before mounting a root.
- If f_read prints looped data, your appending data buffer for write or your buffer for f_read may be too small for the actual message size.
- Shell opens copies in and saves to DISK-newfile.
- Every /test* file copys in DISK, made by format.c, and writes out to DISK-newfile
  The exceptions is test7.c
- To run a shell on an empty system , delete DISK-newfile, run format, run test0.c, and then run shell.
- The order to write to a file is 
    f_open the file
    f_find the file
    f_seek the location 
    f_read the data
- Make sure that the file or directory has been closed before it is removed
- Make sure that a directory has beeen closed bfore it is removed


Logical Structures:

Inode
- By default, each inode has 5 data blocks assigned. This is to reduce overhead for assignement time.
- Directory inodes are unaware of their own directory (no "." entry). 
  This may be implemented later by refactoring format and mkdir to automatically write a vdir entry of name ".", inode index being itsself

Vnode
- vnodes are given space in the open file table
- The name of a file or directory is its own name, not the entire path. The path is in the parent relationship from the root node to the given vnode.

Directories
- Can hold a max of 300 sub files/directories.
- A file and a directory cannot share a name.


VFS Method Comments:

open_file_table
- It is possible to open a file or directory several times without closing it. 
  In these cases, the open file table modifys and returns the active index in the open file table.

clean_fs
- clears any part of the vnode tree which has not already been freed. Prevents memory leaks

f_mount
- Limited functionality to mount an initial file system
  TO IMPLEMENT: add ability to mount file system below and existing file system.
  Possible implementation is to create additional mem buffers representing lower order memory drives,
  build the vnode tree from the inodes in there, 
  and for each vnode/ inode in the lower order drive copy it to an inode location in the higher order memory buffer.
  unclear how this would work with unmount

f_unmount
- To IMPLEMENT: save and partial distruction of the existing file system

fread   
- One can read from a file or directory - EISDIR does not apply as an error. 
- There is no limit to stop you from reading beyon a file data block or memory altogether

fseek 
- You can seek through a directory, this is how I read inodes table entries, EISDIR error does not apply
- The location given is the logical offset from the start of data, not the physical offset

f_write
- One can write to a directory - EISDIR does not apply as an error. This is so that I can write table entries to directory tables in directory inodes 
- There is a check when making indirect table blocks that the there are more data blocks to alloc. 
  TO IMPLEMENT: add check for allocing blocks in an existing indirect table block

f_opendir
- In the shell, this should be performed on the root directory after mount is called
- If the directory is not already open, open the directory. 

f_closedir
- One can remove with a bad fd, this is to ensure that overwriten or lost fds do not mean that a file stays open forever.
    
f_remove
- f_remove does not make use of the fd parameter. closing a file is a seperate step that must be undertaken beforehand. 
  This is so that one can close and open files and close them again easily, and then after the last close, even having overwritten or lost the fd, remove the file.
- Works at the vnode level and inode level
- zeros out data in direct blocks
- sets indirect blocks back on the free list
- frees the vnode
- This can be called on a directory when rmdir is called, this is how the directory data blocks get freed and zerod. EISDIR does not apply
- DO NOT call f_remove on root vnode. This is not allowd on shell.

f_readdir
- One may read un-opened directories. Readdir does the work of opening the directory in order to read it. EBADF does not apply.
- In this implementation, fd is an index of a table entry in a inode directory table.
- f_readdir returns one table entry at the given index of the table

change_permanent_permissions
- implements chmod
- You cannot call chmod on a directory. All directories are read/write.
- If it is later desired to implement chmod for directories, decide whether every subdirectory and sub file must also have their permission changed.
  This would lock/unlock a subtree of the file tree. Otherwise, only the directory node on the file tree is locked/unlocked.


Shell:
- "echo" is the only non-system command implemented.
- Shell can only support 3 spaced string command, either seperated by spaces or in quote marks, and 2 ouput parameters: an output direction arrow, and an output path
  example: <echo "hi there" > hi.txt > is legal, < chmod 0200 a.txt b.txt > will only impact a.txt
- "cat" DOES NOT create a new file by default
  This could implemented by adding an fopen for unrecognized files, but for the simple shell this functionality was kept to "touch"
- Right now, there are no checks in place to ensure that you are calling the command on the correct type: either file or dir
- By default, "touch" puts files in read/write mode. 
- Shell automatically mounts DISK-newfile
- The root directory is opened after mount
- The data buffer and output buffer are equal to the max file data size
- It is not possible to create a directory or file of the same name as another directory or file in the same parent directory
  TO IMPLEMENT: Recheck name uniqueness on moving a directory
- "chmod" means that the file is closed, before the new permissions are applied
  In the current implementation, a file with only S_IRUSR cannot be concatenated to the screen.
- Only 1 set of permanent and 1 set of temporary permissions exist for each file, there is no user-group-world format. The same permissions are applied at every scope.
- "mv" does not let you edit file names.
  