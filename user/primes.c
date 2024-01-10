#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void 
solve(int p[]){
  int n;
  close(p[1]);
  if (read(p[0], &n, 4) == 0 ) {
    exit(0);
  }
  printf("prime %d\n", n);
  int p2[2];
  pipe(p2);
  if (fork() > 0){
    close(p2[0]);
    int x;
    while (read(p[0], &x, 4) > 0) {
      if (x%n != 0) {
        write(p2[1], &x, 4);
      }
    }
    close(p2[1]);
    wait((int *)0);
  } else {
    solve(p2);
  }
  exit(0);
}
int
main(int argc, char **argv)
{

  if(argc > 1){
    fprintf(2, "usage: primes\n");
    exit(1);
  }
  int p[2];
  pipe(p);
  int num;
  if (fork() > 0) {
    close(p[0]);
    for (num = 2; num <= 35; ++num) {
      write(p[1], &num, 4);
    }
    close(p[1]);
    wait((int *)0);
  } else {
    solve(p);
  }
  exit(0);
}
