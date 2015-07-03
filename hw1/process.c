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
  char *paths_copied = (char *)malloc(strlen(paths) + 1);
  char *path;
  int needSearchPath = 1;
  int didExecuted = 0;
  char **t = p->argv;

  if (setpgid(p->pid, p->pid) < 0) {
    perror("Couldn't put the child process in its own process group");
    return;
  }

  dup2(p->stdin, STDIN_FILENO);
  dup2(p->stdout, STDOUT_FILENO);
  if (t[0][0] == '/' || (strlen(t[0]) > 2 && strncmp(t[0], "./", 2) == 0))
    needSearchPath = 0;

  strcpy(paths_copied, paths);
  path = strtok(paths_copied, ":");

  while (path && needSearchPath) {
    char *fname = (char *)malloc(strlen(path) + strlen(t[0]) + 2);
    if (!fname) {
      break;
    }
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
  tcsetpgrp(STDIN_FILENO, p->pid);
  //tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes);
}

void pr_exit(int status) {
  if (WIFEXITED(status))
    printf("normal termination, exit status = %d\n",
            WEXITSTATUS(status));
  else if (WIFSIGNALED(status))
    printf("abnormal termination, signal number = %d%s\n",
            WTERMSIG(status),
  #ifdef WCOREDUMP
    WCOREDUMP(status) ? " (core file generated)" : "");
  #else
    "");
  #endif
  else if (WIFSTOPPED(status))
    printf("child stopped, signal number = %d\n", WSTOPSIG(status));
}
