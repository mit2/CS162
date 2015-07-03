#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);

int cmd_cd(tok_t arg[]) {
  chdir(arg[0]);
}

/* Command Lookup table */
typedef int cmd_fun_t (tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
  {cmd_cd, "cd", "change current working directory"}
};

int cmd_help(tok_t arg[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

void init_shell()
{
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){

    /* force into foreground */
    while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
      kill( - shell_pgid, SIGTTIN);

    shell_pgid = getpid();
    /* Put shell in its own process group */
    if(setpgid(shell_pgid, shell_pgid) < 0){
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

/**
 * Add a process to our process list
 */
void add_process(process* p)
{
  /** YOUR CODE HERE */
}

/**
 * Creates a process given the inputString from stdin
 */
process* create_process(char* inputString)
{
  tok_t *t = getToks(inputString);
  // TODO: procInfo is not freeed
  process* procInfo = (process *)malloc(sizeof(process));
  int i = 0;

  if (t == NULL || t[0] == NULL || strlen(t[0]) == 0)
    return NULL;

  /* support for input and output redirection */
  /* Now only support the < and > sign in the last part of a command */
  procInfo->stdin = STDIN_FILENO;
  procInfo->stdout = STDOUT_FILENO;
  for (i = MAXTOKS - 2; i > 0; i--) {
    if (t[i] && t[i + 1] && strcmp(t[i], "<") == 0) {
      FILE *inputFile;
      if (inputFile = fopen(t[i + 1], "r")) {
        int j = 0;
        procInfo->stdin = fileno(inputFile);
        for (j = i; j < MAXTOKS - 2; j++)
          t[j] = t[j + 2];
        t[j] = NULL;
        break;
      }
    }
  }

  for (i = MAXTOKS - 2; i > 0; i--) {
    if (t[i] && t[i + 1] && strcmp(t[i], ">") == 0) {
      FILE *outputFile;
      if (outputFile = fopen(t[i + 1], "w")) {
        int j = 0;
        procInfo->stdout = fileno(outputFile);
        for (j = i; j < MAXTOKS - 2; j++)
          t[j] = t[j + 2];
        t[j] = NULL;
        break;
      }
    }
  }

  procInfo->argv = t;
  procInfo->argc = totalToks(t);
  procInfo->completed = 0;
  procInfo->stopped = 0;
  procInfo->background = 0;
  procInfo->status = 0;
  //procInfo->tmodes = ;
  //procInfo->stderr = ;
  //procInfo->next = ;
  //procInfo->prev = ;

  return procInfo;
}

int shell (int argc, char *argv[]) {
  char *s = malloc(INPUT_STRING_SIZE+1);			/* user input string */
  char *cpwd;
  tok_t *t;			/* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  pid_t pid = getpid();		/* get current processes PID */
  pid_t ppid = getppid();	/* get parents PID */
  pid_t cpid, tcpid, cpgid;

  init_shell();

  printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

  lineNum=0;
  cpwd = get_current_dir_name();
  fprintf(stdout, "%d %s: ", lineNum, cpwd);
  free(cpwd);
  while ((s = freadln(stdin))){
    char *s_copied = malloc(INPUT_STRING_SIZE+1);
    strcpy(s_copied, s);
    t = getToks(s); /* break the line into tokens */
    fundex = lookup(t[0]); /* Is first token a shell literal */
    if(fundex >= 0) cmd_table[fundex].fun(&t[1]);
    else {
      process *p = create_process(s_copied);
      if (p == NULL) {
        continue;
      }

      pid_t pid = fork();

      if (pid > 0) {  /* parent process */
        struct sigaction new_sa;
        struct sigaction old_sa;
        int child_status;

        p->pid = pid;
        setpgid(pid, pid);

        sigfillset(&new_sa.sa_mask);
        new_sa.sa_handler = SIG_IGN;
        new_sa.sa_flags = 0;

        sigaction(SIGTSTP, &new_sa, &old_sa);
        sigaction(SIGINT, &new_sa, NULL);
        sigaction(SIGTTOU, &new_sa, NULL);

        tcsetpgrp(STDIN_FILENO, pid);
        waitpid(WAIT_ANY, &child_status, WUNTRACED);
        tcsetpgrp(STDIN_FILENO, getpid());
        pr_exit(child_status);

        new_sa.sa_handler = SIG_DFL;
        sigaction(SIGTSTP, &new_sa, NULL);
        sigaction(SIGINT, &new_sa, NULL);
        sigaction(SIGTTOU, &new_sa, NULL);
      }
      else if (pid == 0) {  /* child process */
        if (p != NULL) {
          p->pid = getpid();
          launch_process(p);
        }
        else
          exit(-1);
      }
      else {
        perror("Couldn't spawn a new process");
      }
    }
    cpwd = get_current_dir_name();
    fprintf(stdout, "%d %s: ", ++lineNum, cpwd);
    free(cpwd);
    free(s_copied);
  }
  return 0;
}
