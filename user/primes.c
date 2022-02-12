#include "kernel/types.h"
#include "user/user.h"
#include "defines.h"



void forkAndProcess(int* prevPipe) {
  int pid = fork();

  if (pid == 0) {
    close(prevPipe[WRITE_END]);
    int nextVal;
    read(prevPipe[READ_END], &nextVal, sizeof nextVal);
    int n = nextVal;
    if (nextVal == 0) exit(0);

    printf("prime %d\n", nextVal);

    char forked = 0;
    int nextPipe[2];

    //printf("pipeRet %d: %d\n", nextVal, pipeRet);


    int nbytes;
    while ((nbytes = read(prevPipe[READ_END], &nextVal, sizeof nextVal)) > 0) {
      //if(n ==31){
      //  printf("stage %d processing %d\n bytes: %d\n", n, nextVal, nbytes);
      //}

      // sleep(n);
      if (nextVal % n != 0) {
        if (forked == 0) {
          forked = 1;
          while (pipe(nextPipe) < 0) { sleep(1); }
          //printf("new pipe in %d writeEnd: %d readEnd: %d\n", n, nextPipe[WRITE_END], nextPipe[READ_END]);
          forkAndProcess(nextPipe);

        }

        write(nextPipe[WRITE_END], &nextVal, sizeof nextVal);
      }
    }
    //printf("pipeline stage %d finished reading from pipe bytes: %d\n", n, nbytes);
    close(prevPipe[READ_END]);
    close(nextPipe[WRITE_END]);
    wait(0);
    //printf(" stage %d chidren exited\n", n);
    exit(0);
  } else { return; }
}

int main(int argc, char* argv[]) {
  int firstPipe[2];

  pipe(firstPipe);
  //printf("firstPipe: %d %d\n", firstPipe[READ_END], firstPipe[WRITE_END]);

  for (int i = 2; i <= 35; i++) {
    //printf("writing %d to firstPipe\n", i);
    write(firstPipe[WRITE_END], &i, sizeof i);
  }

  close(firstPipe[WRITE_END]);
  forkAndProcess(firstPipe);
  close(firstPipe[READ_END]);

  wait(0);
  exit(0);

}
