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
#define PATH_STRING_SIZE 1024

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
  return 1;
}

int cmd_wait(tok_t arg[]) {
  pid_t pid;
  int status;
  do {
    pid = waitpid(WAIT_ANY, &status, WNOHANG | WUNTRACED);
    mark_process_status(pid, status);
  } while(!background_processes_completed());
  return 1;
}

int cmd_fg(tok_t arg[]) {
  pid_t pid;
  process *p;

  if (arg[0] != NULL)
    pid = atoi(arg[0]);
  else
    pid = -1;

  for(p = first_process; p->next; p = p->next)
    if (p->pid == pid)
      break;
  put_process_in_foreground(p, 0);
  return 1;
}

int cmd_bg(tok_t arg[]) {
  pid_t pid = atoi(arg[0]);
  process *p;

  if (arg[0] != NULL)
    pid = atoi(arg[0]);
  else
    pid = -1;

  for(p = first_process; p->next; p = p->next)
    if (p->pid == pid)
      break;
  put_process_in_background(p, 0);
  return 1;
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
  {cmd_cd, "cd", "change current working directory"},
  {cmd_wait, "wait", "wait all background finished"},
  {cmd_fg, "fg", "move the process with id pid to the foreground. If pid is not specified, then move the most recently launched process to the foreground."},
  {cmd_bg, "bg", "move the process with id pid to the background. If pid is not specified, then move the most recently launched process to the background."}
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
    struct sigaction new_sa;
    struct sigaction old_sa;

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

    /* ignore job control related signals */
    sigfillset(&new_sa.sa_mask);
    new_sa.sa_handler = SIG_IGN;
    new_sa.sa_flags = 0;

    sigaction(SIGTSTP, &new_sa, &old_sa);
    sigaction(SIGINT, &new_sa, NULL);
    sigaction(SIGTTOU, &new_sa, NULL);

    first_process = (process *)malloc(sizeof(process));
    first_process->pid = getpid();
    first_process->stopped = 0;
    first_process->completed = 0;
    first_process->background = 0;
    //tcgetattr(shell_terminal, &first_process->tmodes);
    first_process->stdin = STDIN_FILENO;
    first_process->stdout = STDOUT_FILENO;
    first_process->stderr = STDERR_FILENO;
    first_process->prev = first_process->next = NULL;
  }
}

/**
 * Add a process to our process list
 */
void add_process(process* p)
{
  process *q = first_process;;
  while (q->next) {
    q=q->next;
  }
  q->next = p;
  p->prev = q;
}

/**
 * Creates a process given the inputString from stdin
 */
process* create_process(tok_t *t)
{
  process* procInfo = (process *)malloc(sizeof(process));
  int i = 0;
  int total_toks = 0;

  if (t == NULL || t[0] == NULL || strlen(t[0]) == 0)
    return NULL;

  procInfo->stdin = STDIN_FILENO;
  procInfo->stdout = STDOUT_FILENO;
  procInfo->stderr = STDERR_FILENO;
  procInfo->argv = t;
  total_toks = totalToks(t);
  procInfo->completed = 0;
  procInfo->stopped = 0;
  procInfo->background = 0;
  procInfo->status = 0;
  //procInfo->tmodes = ;
  procInfo->next = NULL;
  procInfo->prev = NULL;

  /* support for input and output redirection */
  /* Now only support the < and > sign in the last part of a command */
  for (i = MAXTOKS - 2; i > 0; i--) {
    if (t[i] && t[i + 1] && strcmp(t[i], "<") == 0) {
      FILE *inputFile;
      if ((inputFile = fopen(t[i + 1], "r")) != NULL) {
        int j = 0;
        procInfo->stdin = fileno(inputFile);
        for (j = i; j < MAXTOKS - 2; j++)
          t[j] = t[j + 2];
        t[j] = NULL;
        total_toks-=2;
        break;
      }
    }
  }

  for (i = MAXTOKS - 2; i > 0; i--) {
    if (t[i] && t[i + 1] && strcmp(t[i], ">") == 0) {
      FILE *outputFile;
      if ((outputFile = fopen(t[i + 1], "w")) != NULL) {
        int j = 0;
        procInfo->stdout = fileno(outputFile);
        for (j = i; j < MAXTOKS - 2; j++)
          t[j] = t[j + 2];
        t[j] = NULL;
        total_toks-=2;
        break;
      }
    }
  }

  /* support for background sign */
  if (strcmp(t[total_toks - 1], "&") == 0) {
    procInfo->background = 1;
    t[total_toks - 1] = NULL;
    total_toks--;
  }
  else if (t[0][strlen(t[0]) - 1] == '&') {
    procInfo->background = 1;
    t[0][strlen(t[0]) - 1] = 0;
  }

  procInfo->argc = total_toks;

  return procInfo;
}

void update_status (void) {
  int status;
  pid_t pid;

  do {
    pid = waitpid(WAIT_ANY, &status, WUNTRACED|WNOHANG);
  } while (!mark_process_status(pid, status));
}

int shell (int argc, char *argv[]) {
  char *s = malloc(INPUT_STRING_SIZE+1);			/* user input string */
  char cpwd[PATH_STRING_SIZE];
  tok_t *t;			/* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  pid_t pid = getpid();		/* get current processes PID */
  pid_t ppid = getppid();	/* get parents PID */

  init_shell();

  printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

  lineNum=0;
  getcwd(cpwd, PATH_STRING_SIZE);
  fprintf(stdout, "%d %s: ", lineNum, cpwd);
  while ((s = freadln(stdin))){
    t = getToks(s); /* break the line into tokens */
    fundex = lookup(t[0]); /* Is first token a shell literal */
    if(fundex >= 0) cmd_table[fundex].fun(&t[1]);
    else {
      process *p = create_process(t);
      if (p != NULL) {
        add_process(p);
        pid_t pid = fork();
        if (pid > 0) {  /* parent process */
          p->pid = pid;
          setpgid(pid, pid);
          printf("Run process: %s on pid: %d\n", t[0], p->pid);
          if (!p->background) {
            put_process_in_foreground(p, 0);
          }
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
    }
    update_status();

    getcwd(cpwd, PATH_STRING_SIZE);
    fprintf(stdout, "%d %s: ", ++lineNum, cpwd);
  }
  return 0;
}
