#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "print current working directory"},
  {cmd_cd, "cd", "change directory to the destination"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Prints current working directory */
int cmd_pwd(unused struct tokens *tokens) {
  char *result = getcwd(NULL, 0);
  if (result == NULL)
    return -1;
  printf("%s\n", result);
  free(result);
  return 1;
}

/* Changes directory */
int cmd_cd(struct tokens *tokens) {
  char *result = tokens_get_token(tokens, 1);
  if (chdir(result) == -1)
    return -1;
  return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      // Time to fork a new process and run a new program provided in the command
      pid_t pid;
      int status;
      pid = fork();
      if (pid == -1) {
        printf("Fork fails\n");
        exit(-1);
      } else if (pid == 0) {
        int number_of_tokens = tokens_get_length(tokens);
        char **command = (char **) malloc(number_of_tokens+1);
        for (int i = 0; i < number_of_tokens; i++) {
          *(command+i) = tokens_get_token(tokens, i);
        }
        *(command+number_of_tokens) = NULL;

        // Add path resolution here
        char *path = getenv("PATH");
        printf("%s", path);
        char *token = strtok(path, ":"); 
        printf("%s", token);
        char *program = *command;
        // For slash and null terminator
        char *slash_program = (char *) malloc(strlen(*command)+2);
        slash_program[0] = '/';
        slash_program[1] = '\0';
        strcat(slash_program, program);
        while(token != NULL) {
          char *path_resoluted_program = (char *) malloc(strlen(token)+strlen(slash_program)+1);
          path_resoluted_program[0] = '\0';
          strcat(path_resoluted_program, token);
          strcat(path_resoluted_program, slash_program);
          *command = (char *) malloc(strlen(token)+strlen(*command)+2);
          strcat(*command, path_resoluted_program);
          execv(path_resoluted_program, command);
          token = strtok(NULL, ":");
        }
        
        // Assuming this is full path if this succeeds.
        execv(*command, command);
        fprintf(stdout, "This shell doesn't know how to run programs.\n");
        exit(0);
      } else {
        waitpid(pid, &status, 0);
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
