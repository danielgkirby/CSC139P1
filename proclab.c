#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_CHILDREN 20
#define ZOMBIE_TIMEOUT_MS 2000
#define POLL_INTERVAL_MS 5
#define WORKER_PATH "./proclab-worker"
#define WORKER_NAME "proclab-worker"

typedef struct {
  pid_t pid;
  int expected_exit;
  int zombie_observed;
} Child;

// Provided argument-handling code. Read and understand it before continuing.
static void print_usage(FILE *out, const char *program) {
  fprintf(out, "Usage: %s <N>\n", program);
}

// Stores the int for child count in *count_out
// returns 1 if successful, 0 if failed
static int parse_child_count(const char *text, int *count_out) {
  errno = 0;
  char *end = NULL;
  long value = strtol(text, &end, 10);

  if (errno != 0 || end == text || *end != '\0' || value < 0 ||
      value > MAX_CHILDREN) {
    return 0;
  }

  *count_out = (int)value;
  return 1;
}

int main(int argc, char **argv) {
  // Keep parent output in deterministic order, even when redirected to a file.
  setvbuf(stdout, NULL, _IONBF, 0);

  int child_count;
  if (argc != 2 || !parse_child_count(argv[1], &child_count)) {
    print_usage(stderr, argv[0]);
    return 1;
  }

  if (child_count == 0) {
    printf("DONE spawned=0 reaped=0\n");
    return 0;
  }

  /*
   * TODO: Implement the process lifecycle described in the project prompt.
   *
   * 1. Allocate storage for child records.
   * 2. Fork child_count children.
   * 3. In each child, exec WORKER_PATH with exit code 10 + index.
   * 4. In the parent, store and report each child in index order.
   * 5. Observe every child as a WORKER_NAME zombie through /proc.
   * 6. Reap every child with waitpid and report its decoded exit status.
   * 7. Print DONE and release all allocated memory.
   * 8. On any failure, report the error and reap every child already created.
   *
   * Choose your own helper functions and decomposition. The tutorial reference
   * in the prompt explains each required system interface.
   */

  // 1. Allocate storage for child records.
  Child *children = malloc((size_t)child_count * sizeof(*children));

  if (children == NULL){
    perror("malloc");
    return 1;
  }

  // 2. Fork child_count children.
  int spawnedChildren = 0;

  for(int i=0; i<child_count; i++){
    int expected_exit = 10 + i;
        
    pid_t pid = fork();

    if (pid < 0) {
      perror("fork");

      // Reap any children that were already created
      for(int j=0; j<spawnedChildren; j++){
        waitpid(children[j].pid, NULL, 0);
      }

      free(children);
      return 1;
    }

    if (pid == 0) {
      // This is the child process
      char exit_text[16];
      snprintf(exit_text, sizeof(exit_text), "%d", expected_exit);
      execl(WORKER_PATH, WORKER_NAME, exit_text, (char *)NULL);

      // execl() only returns if it fails
      perror("execl");
      _exit(127);
    }

    // Only the parent can reach this section and run this code
    children[i].pid = pid;
    children[i].expected_exit = expected_exit;
    spawnedChildren++;

    printf("SPAWN index =%d pid=%d expected_exit=%d\n",
           i, pid, expected_exit);
  }

  for (int i = 0; i < spawnedChildren; i++) {
    waitpid(children[i].pid, NULL, 0);
  }

  free(children);
  return 0;
}
