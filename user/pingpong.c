#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char **argv)
{

  if(argc > 1){
    fprintf(2, "usage: pingpong\n");
    exit(1);
  }
  int p[2];
  pipe(p);
  char buf[64];
  if (fork() > 0) {
    write(p[1], "ping", 4);
    wait((int *)0);
    read(p[0], buf, 4);
    printf("%d: received %s\n", getpid(), buf);
  } else {
    read(p[0], buf, 4);
    printf("%d: received %s\n", getpid(), buf);
    write(p[1], "pong", 4);
    exit(0);
  }
  exit(0);
}
