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

// something has gone wrong, local commit is totally different from the repo

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

/* Path Resolution. */
void exec_full_path(char **command) {
  // Add path resolution here
  char *path = getenv("PATH");
  char *token = strtok(path, ":");
  char *program = *command;
  // For slash and null terminator
  char *slash_program = (char *) malloc(strlen(program)+2);
  slash_program[0] = '/';
  slash_program[1] = '\0';
  strcat(slash_program, program);
  while(token != NULL) {
    char *path_resoluted_program = (char *) malloc(strlen(token)+strlen(slash_program)+1);
    path_resoluted_program[0] = '\0';
    strcat(path_resoluted_program, token);
    strcat(path_resoluted_program, slash_program);
    *command = (char *) malloc(strlen(path_resoluted_program)+1);
    *command[0] = '\0';
    strcat(*command, path_resoluted_program);
    execv(path_resoluted_program, command);
    token = strtok(NULL, ":");
  }
}

/* Handle STDIN. */
// pos is the index of the file_name.
// assuming redirection is null-terminated when passed in.
void exec_stdin(char **command, struct tokens *tokens, int pos) {
  pid_t pid;
  int status;
  int fd[2];
  pipe(fd);
  char* file_name = tokens_get_token(tokens, pos);
  char buffer[1024];
  pid = fork();
  if (pid == -1) {
    printf("fork fails.");
  } else if (pid == 0) {
    dup2(fd[0], 0);
    close(fd[1]);
    close(fd[0]);
    execv(*command, command);
    exec_full_path(command);
    exit(0);
  }
  int ffd = open(file_name, O_CREAT|O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
  while(read(ffd, buffer, sizeof(buffer)) != 0)
    write(fd[1], buffer, strlen(buffer));
  close(fd[1]);
  close(fd[0]);
  waitpid(-1, &status, 0);
  close(ffd);
  exit(0); //TODO
}

/* Handle STDOUT. */
// pos is the index of the file name.
// assuming redirection is null-terminated when passed in.
void exec_stdout(char **command, struct tokens *tokens, int pos) {
  pid_t pid;
  int status;
  int fd[2];
  pipe(fd);
  char* file_name = tokens_get_token(tokens, pos);
  char buffer[1024];
  pid = fork();
  if (pid == -1) {
    printf("fork fails.");
  } else if (pid == 0) {
    dup2(fd[1], 1);
    close(fd[1]);
    close(fd[0]);
    execv(*command, command);
    exec_full_path(command);
    exit(0);
  }
  int ffd = open(file_name, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
  close(fd[1]); // it has to come before the read
  while(read(fd[0], buffer, sizeof(buffer)) != 0)
    write(ffd, buffer, strlen(buffer));
  close(fd[0]);
  waitpid(-1, &status, 0);
  close(ffd);
  exit(0); //TODO
}

/* Handle PIPE. */
// pos is the index of the pipe location list.
// read everything until pipe_location[pos]
// assuming pipe is null-terminated when passed in.
// command is incremented everytime.
void exec_pipe(struct tokens *tokens, int pos) {

  int initial_pos = pos;

  while(true) {
    if (tokens_get_length(tokens) == pos || strcmp(tokens_get_token(tokens, pos), "|") == 0)
      break;
    pos += 1;
  }

  char **command = (char **) malloc(sizeof(char *)*(pos-initial_pos+1));
  for (int i = initial_pos; i <= pos; i++) {
    if (i == pos)
      *(command+i-initial_pos) = NULL;
    else
      *(command+i-initial_pos) = tokens_get_token(tokens, i);
  }

  pid_t pid;
  int fd[2];
  pipe(fd);
  pid = fork();
  if (pid == -1) {
    printf("fork fails.");
  } else if (pid == 0) {
    if (tokens_get_length(tokens) != pos)
      dup2(fd[1], STDOUT_FILENO);
    close(fd[1]);
    close(fd[0]);
    execv(*command, command);
    exec_full_path(command);
    exit(0);
  }
  dup2(fd[0], STDIN_FILENO);
  close(fd[1]);
  close(fd[0]);
  wait(&pid);
  for (int j = 0; j < pos-initial_pos+1; j++)
    free(*(command+j));
  free(command);
  if (tokens_get_length(tokens) != pos)
    exec_pipe(tokens, pos+1);
  exit(0); //TODO
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

      // Add redirection
      bool inward = false;
      bool outward = false;

      // Add pipe
      int number_of_pipe = 0;       

      // Tokenize outside such that the parent process understands the redirection
      int number_of_tokens = tokens_get_length(tokens);
      // To Be Determined: Do we need to make it smaller if redirection happens?
      char **command = (char **) malloc(sizeof(char*)*number_of_tokens+1);
      // pointer to the pipe location
      int *pipes_location = (int *) malloc(sizeof(int)*number_of_tokens);
      int i;
      for (i = 0; i < number_of_tokens; i++) {
        *(command+i) = tokens_get_token(tokens, i);
        if (strcmp(*(command+i), "<") == 0 && number_of_pipe == 0) {
          *(command+i) = NULL;
          exec_stdin(command, tokens, i+1);
          inward = true;
          break;
        } else if (strcmp(*(command+i), ">") == 0 && number_of_pipe == 0) {
          *(command+i) = NULL;
          exec_stdout(command, tokens, i+1);
          outward = true;
          break;
        } else if (strcmp(*(command+i), "|") == 0) {
          exec_pipe(tokens, 0);
          // Assumption is that there will not be mixed use of pipe and redirection
          // pipes_location[number_of_pipe] = i;
          // number_of_pipe += 1;
          // *(command+i) = NULL;
        }
      }
      *(command+i) = NULL;

      int j = 0;
      int pipes[2][2];
      for (int k = 0; k < 2; k++)
        pipe(pipes[k]);
      int ref = number_of_pipe;

      do {
        // Only create pipe when there is redirection
        int pipefd[2];
        if (inward || outward) {
          pipe(pipefd);
        }
        pid = fork();
        if (pid == -1) {
          printf("Fork fails\n");
          exit(-1);
        } else if (pid == 0) { //child
          // write side of the pipe is the same as stdout
          // such that child process writes to stdout is the same as writing to the pipe
          // Redirection Fun
          if (outward) {
            dup2(pipefd[1], 1);
            close(pipefd[1]);
          } else if (inward) {
            close(pipefd[1]);
            dup2(pipefd[0], 0);
            close(pipefd[0]);
          } else if (number_of_pipe > 0) {
            // Even the first child process would not use the read pipe, it should be fine?
            // Could be buggy here.
            dup2(pipes[0][0], 0);
            dup2(pipes[0][1], 1);
            close(pipes[0][1]);
            close(pipes[0][0]);
            /*dup2(pipefd[1], 1);
            close(pipefd[1]);
            close(pipefd[0]);*/
          } else if (ref > 0 && number_of_pipe == 0) {
            dup2(pipes[1][0], 0);
            close(pipes[1][0]);
            close(pipes[1][1]);
          } 
          
          // Assuming this is full path if this succeeds.
          execv(*command, command);
          // Add path resolution here
          exec_full_path(command); 
          exit(0);
        } else { // parent
          // Redirection Fun
          char buffer[1024];
          char *file_name = tokens_get_token(tokens, ++i);
          int fd;
          if (outward) {
            // create file descriptor
            fd = open(file_name, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
            // Has to close parent's own write port so that read would not be blocked
            close(pipefd[1]);
            while (read(pipefd[0], buffer, sizeof(buffer)) != 0) {
              write(fd, buffer, strlen(buffer));
            }
            close(pipefd[0]);
          } else if (inward) {
            // create file descriptor
            fd = open(file_name, O_CREAT|O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
            close(pipefd[0]);
            while(read(fd, buffer, sizeof(buffer)) != 0) {
              write(pipefd[1], buffer, strlen(buffer));
            }
            close(pipefd[1]);
          } else if (number_of_pipe > 0) {
            close(pipes[0][1]);
            while(read(pipes[0][0], buffer, sizeof(buffer)) != 0) {
              write(pipes[1][1], buffer, strlen(buffer));
            }
            //close(pipes[1][1]);
            close(pipes[0][0]);
            /*dup2(pipefd[0], 0);
            close(pipefd[0]);
            close(pipefd[1]);*/ // make sure all write is clean; otherwise, it will stuck
            // increment command ptr
            command = command+pipes_location[j]+1; // if the index is 1 with wc shell.c | cat; the result will have stdin
            j += 1;
          } else if (ref > 0 && number_of_pipe == 0) {
            close(pipes[1][1]);
          }
          close(fd);
          waitpid(-1, &status, 0);
          
          if (inward || outward)
            break;
        }
        // run two times for one pipe, etc.
        if (number_of_pipe > -1)
          number_of_pipe -= 1;
      } while(number_of_pipe != -1);

      //free(command);
      free(pipes_location);
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
