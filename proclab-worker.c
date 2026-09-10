#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    dprintf(STDERR_FILENO, "Usage: %s <exit_code>\n", argv[0]);
    return 2;
  }

  errno = 0;
  char *end = NULL;
  long exit_code = strtol(argv[1], &end, 10);
  if (errno != 0 || end == argv[1] || *end != '\0' || exit_code < 0 ||
      exit_code > 255) {
    dprintf(STDERR_FILENO, "Invalid exit code: %s\n", argv[1]);
    return 2;
  }

  _exit((int)exit_code);
}
