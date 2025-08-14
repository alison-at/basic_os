#include <assert.h>
#include <fcntl.h>
#include "vfs.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>

int main()
{
    
  init_fs();
  save_fs("DISK-newfile");
  cleanup_fs();
  return 0;

}