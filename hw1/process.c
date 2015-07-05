#include "process.h"
#include "shell.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>

/**
 * Executes the process p.
 * If the shell is in interactive mode and the process is a foreground process,
 * then p should take control of the terminal.
 */
void launch_process(process *p)
{
  char *paths = getenv("PATH");
  char *paths_copied = (char *)malloc(strlen(paths) + 1); /* can not call strtok on paths directly. */
  char *path;
  int needSearchPath = 1;
  int didExecuted = 0;
  char **t = p->argv;

  if (setpgid(p->pid, p->pid) < 0) {
    perror("Couldn't put the child process in its own process group");
    return;
  }

  /* IO rediction */
  dup2(p->stdin, STDIN_FILENO);
  dup2(p->stdout, STDOUT_FILENO);

  /* Path resolution */
  if (t[0][0] == '/' || (strlen(t[0]) > 2 && strncmp(t[0], "./", 2) == 0))
    needSearchPath = 0;
  strcpy(paths_copied, paths);
  path = strtok(paths_copied, ":");
  while (path && needSearchPath) {
    char *fname = (char *)malloc(strlen(path) + strlen(t[0]) + 2);
    if (!fname)
      break;
    memset(fname, 0, strlen(fname));
    strcpy(fname, path); strcat(fname, "/"); strcat(fname, t[0]);
    if (access(fname, F_OK) != -1) {  /* the executable file exist */
      execv(fname, &t[0]);
      didExecuted = 1;
      free(fname);
      break;
    }
    free(fname);
    path = strtok(NULL, ":");
  }

  /* Execute directly */
  if (!didExecuted) {
    if (access(t[0], F_OK) != -1) {
      execv(t[0], &t[0]);
    }
    else {
      printf("Could not find the executable file: %s\n", t[0]);
    }
  }

  free(paths_copied);
}

/*
 * http://www.gnu.org/software/libc/manual/html_node/Foreground-and-Background.html
*/
void put_process_in_foreground(process *p, int cont) {
  int status;
  /* Put the job into the foreground */
  tcsetpgrp(STDIN_FILENO, p->pid);

  /* Send the job a continue signal, if necessary. */
  if (cont) {
    tcsetattr(STDIN_FILENO, TCSADRAIN, &p->tmodes);
    if (kill (- p->pid, SIGCONT) < 0)
      perror("kill (SIGCONT)");
  }

  /* Wait for it to report. */
  waitpid(WAIT_ANY, &status, WUNTRACED);

  /* Put the shell back in the foreground. */
  tcsetpgrp(STDIN_FILENO, first_process->pid);

  /* Restore the shell's terminal modes. */
  //tcgetattr(STDIN_FILENO, &p->tmodes);
  //tcsetattr(shell_terminal, TCSADRAIN, &first_process->tmodes);
}

void put_process_in_background(process *p, int cont) {
  if (cont)
    if (kill (-p->pid, SIGCONT) < 0)
      perror("kill (SIGCONT)");
}

int mark_process_status(pid_t pid, int status)
{
  process *p;
  if (pid > 0) {
    for (p = first_process; p; p = p->next)
      if (p->pid == pid) {
        p->status = status;
        if (WIFSTOPPED(status))
          p->stopped = 1;
        else {
          p->completed = 1;
          if (WIFSIGNALED(status))
            fprintf(stderr, "%dd: Terminated by signal %d.\n", (int) pid, WTERMSIG(p->status));
        }
        return 0;
      }
  }
  return -1;
}

int background_processes_completed() {
  process *p;
  for (p=first_process; p; p=p->next)
    if ((!p->completed) && p->background)
      return 0;
  return 1;
}
