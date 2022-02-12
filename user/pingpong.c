#include "kernel/types.h"
#include "user/user.h"
#include "defines.h"


int
main(int argc, char* argv[]) {
  int p1[2];
  int p2[2];

  pipe(p1);
  pipe(p2);

  int pid = fork();
  int myPid = getpid();

  if (pid == 0) {
    // child process
    char x;
    read(p1[READ_END], &x, 1);
    fprintf(STDOUT, "%d: received ping\n", myPid);
    write(p2[WRITE_END], &x, 1);

  } else {
    //parent process
    char x = 'b';
    write(p1[WRITE_END], &x, 1);
    read(p2[READ_END], &x, 1);
    fprintf(STDOUT, "%d: received pong\n", myPid);

  }
  exit(0);
}
