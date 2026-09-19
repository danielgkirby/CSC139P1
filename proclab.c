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

// helper function
static int read_process_status(pid_t pid, char *name_out, size_t name_size,
                               char *state_out) {
  char path[64];

  // written is the length of the path
  // snprintf saves the path to path.
  int written = snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);

  // checks to make sure written isnt negative or too big
  if (written < 0 || (size_t)written >= sizeof(path)) {
    return 0;
  }

  FILE *file = fopen(path, "r");

  if (file == NULL) {
    return 0;
  }

  char line[1024];

  // reads /proc/<pid>/stat into line
  if (fgets(line, sizeof(line), file) == NULL) {
    fclose(file);
    return 0;
  }

  fclose(file);

  //find first (
  char *open_paren = strchr(line, '(');
  //find LAST )
  char *close_paren = strrchr(line, ')');

  if (open_paren == NULL || close_paren == NULL ||
      close_paren <= open_paren) {
    return 0;
  }

  size_t name_length = (size_t)(close_paren - open_paren - 1);

  if (name_length >= name_size) {
    return 0;
  }

  memcpy(name_out, open_paren + 1, name_length);
  name_out[name_length] = '\0';

  if (sscanf(close_paren + 1, " %c", state_out) != 1) {
    return 0;
  }

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
  int spawned_children = 0;

  for(int i=0; i<child_count; i++){
    int expected_exit = 10 + i;
        
    pid_t pid = fork();

    if (pid < 0) {
      perror("fork");

      // Reap any children that were already created
      for(int j=0; j<spawned_children; j++){
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
    spawned_children++;

    printf("SPAWN index =%d pid=%d expected_exit=%d\n",
           i, pid, expected_exit);
  }

  //Poll every child until they are all zombies
  int zombie_count = 0;
  struct timespec start_time;

  // error checking for clock error
  if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1){
    perror("clock_gettime");

    for (int i = 0; i < spawned_children; i++) {
    waitpid(children[i].pid, NULL, 0);
    }

    free(children);
    return 1;
  }

  while(zombie_count < spawned_children){
    for(int i = 0 ; i < spawned_children){
      // if zombie is already observed, skip this iteration and continue the loop
      if (children[1].zombie_observed){
        continue;
      }
      // zombie has not yet been observed
      char process_name[64];
      char process_state;

      if (read_process_status(children[i].pid, process_name, sizeof(process_name), &process_state) && strcmp(process_name, WORKER_NAME) == 0 && process_state == 'Z'){
        children[i].zombie_observed = 1;
        zombie_count++;

        // Prints new zombie that is observed
        printf("ZOMBIE index=%d pid=%ld name=%s state=%c\n",
             i, (long)children[i].pid, process_name, process_state);
      }
    }

    if (zombie_count == spawned_children) {
      break;
    }

    struct timespec current_time;

    if (clock_gettime(CLOCK_MONOTONIC, &current_time) == -1) {
      perror("clock_gettime");

      for (int i = 0; i < spawned_children; i++) {
        waitpid(children[i].pid, NULL, 0);
      }

      free(children);
      return 1;
    }

    long elapsed_ms =
      (current_time.tv_sec - start_time.tv_sec) * 1000L +
      (current_time.tv_nsec - start_time.tv_nsec) / 1000000L;

    //timeout condition
    if (elapsed_ms >= ZOMBIE_TIMEOUT_MS) {
      fprintf(stderr, "Timed out waiting for children to become zombies\n");
      
      for (int i = 0; i < spawned_children; i++) {
        waitpid(children[i].pid, NULL, 0);
      }

      free(children);
      return 1;
    }
    
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = POLL_INTERVAL_MS * 1000000L;

    nanosleep(&delay, NULL);

  }



  

  
}
