#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "defines.h"


void printStrings(char* strs[32]) {
  for (int i = 0; i < 32; i++) printf("%s\n", strs[i]);
}

int main(int argc, char* argv[]) {

  char* newArgv[MAXARG];
  memmove(newArgv, argv + 1, (argc - 1) * sizeof argv[0]);
  // for (int i = 0; i < argc; i++) {
  //   // printf("%s\n", newArgv[i]);
  // }
  newArgv[argc] = 0;

  char c;
  char buf[512];
  int current = 0;
  while (read(STDIN, &c, sizeof c) > 0) {
    // printf("next char %c\n", c);
    if (c == '\n') {
      buf[current] = 0;
      newArgv[argc - 1] = buf;
      // printf("next arg %s\n", buf);
      int pid = fork();
      if (pid == 0) {
        // printf("about to run %s\n", newArgv[0]);
        exec(newArgv[0], newArgv);
        printf("failed to exec %s", newArgv[0]);
        exit(0);
      } else {
        wait(0);
        // printf("waited successfully, moving on...\n");
        current = 0;
      }
    } else {
      buf[current++] = c;
    }
  }
  exit(0);

}

