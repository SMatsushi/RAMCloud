#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#if !defined(SCRIPT)
#error SCRIPT undefined.
#endif

int
main(int argc, char **argv)
{
  pid_t pid = fork();
  if (pid < 0) {
    return 1;
  }
  if (pid > 0) {
    wait(NULL);
    return 0;
  }
  setuid(0);
  if (execve(SCRIPT, argv, NULL) == -1) {
    return 1;
  }
  return 0;
}
