#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "usage: | xargs commonds...\n");
    exit(1);
  }
  --argc;
  char buf[DIRSIZ+1];
  char *p=buf;
  char *argvs[MAXARG+1]={0};
  memmove(argvs, argv+1, sizeof(char *)*argc);
  while(read(0, p, 1) > 0){
    if(*p == '\n') {
      *p = 0;
      p = buf;
      if (argc > MAXARG) {
        fprintf(2, "xargs: too many arguments.\n");
        exit(1);
      }
      argvs[argc++] = buf;
    }else{
      p++;
    }
  }
  if(fork() == 0){
    exec(argv[1], argvs);
    exit(0);
  }
  wait((int *)0);
  exit(0);
}
