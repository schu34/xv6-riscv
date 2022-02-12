#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/fs.h"

#include "defines.h"


char* getPathEnd(char* fullPath) {
  char* ret = fullPath;
  char* itr = fullPath;

  while (*itr != 0) {
    if (*itr == '/') {
      ret = itr + 1;
    }
    itr++;
  }

  return ret;
}

void recursivelySearch(char* start, char* search) {
  int fd;
  char* p;

  struct stat startStat;
  struct dirent de;


  if ((fd = open(start, O_RDONLY)) < 0) {
    fprintf(2, "find: failed to open %s\n", start);
    return;
  }

  if (fstat(fd, &startStat) < 0) {
    fprintf(2, "find: cannot stat %s\n", start);
    return;
  }

  if (startStat.type == T_FILE) {
    if (strcmp(search, getPathEnd(start)) == 0) {
      printf("%s\n", start);
    }
  } else {
    char buf_reuse[512];

    //only recurse if we're sure the next path won't overflow our buffer
    if (strlen(start) + 1 + DIRSIZ + 1 <= sizeof buf_reuse) {
      strcpy(buf_reuse, start);
      p = buf_reuse + strlen(buf_reuse);
      *p++ = '/';

      while (read(fd, &de, sizeof de) == sizeof(de)) {
        if (
          de.inum == 0 ||
          strcmp(de.name, ".") == 0 ||
          strcmp(de.name, "..") == 0 ||
          strcmp(de.name, "console") == 0
          ) {
          continue;
        }
        char newbuf[512];

        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        strcpy(newbuf, buf_reuse);
        int newFd;
        if ((newFd = open(newbuf, O_RDONLY)) < 0) {
          fprintf(2, "find: failed to open (inner) %s\n", newbuf);
          return;
        }
        struct stat nextStat;
        if (fstat(newFd, &nextStat) < 0) {
          fprintf(2, "find: failed to stat %s\n", newbuf);
        }
        if (nextStat.type == T_FILE) {
          if (strcmp(de.name, search) == 0) {
            printf("%s\n", newbuf);
          }
        } else {
          recursivelySearch(newbuf, search);
        }
        close(newFd);
      }
    }
  }
  close(fd);
  return;

}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("usage: find [start] [search]\n");
    exit(1);
  }
  char* start = argv[1];
  char* search = argv[2];
  recursivelySearch(start, search);

  exit(0);
}
