/* $begin shellmain */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <fcntl.h>

#define MAXARGS 128
#define MAXLINE 8192 /* Max text line length */

typedef enum { IS_SIMPLE, IS_PIPE, IS_INPUT_REDIR, IS_OUTPUT_REDIR, IS_INPUT_OUTPUT_REDIR, IS_SEQ, IS_ANDIF} Mode; /* simple command, |, >, <, ;, && */
typedef struct { 
    char *argv[MAXARGS]; /* Argument list */
    int argc; /* Number of args */
    int bg; /* Background job? */
    Mode mode; /* Handle special cases | > < ; */
} parsed_args; 

extern char **environ; /* Defined by libc */

/* Function prototypes */
void eval(char *cmdline);
parsed_args parseline(char *buf);
int builtin_command(char **argv, pid_t pid, int status);
void signal_handler(int sig);
int exec_cmd(char** argv, posix_spawn_file_actions_t *actions, pid_t *pid, int *status, int bg);
int find_index(char** argv, char* target); 

void isPipe(parsed_args parsed_line);
void isOutRedir(parsed_args parsed_line);
void isInRedir(parsed_args parsed_line);
void isInOutRedir(parsed_args parsed_line);
void isSeq(parsed_args parsed_line);
void isAndIf(parsed_args parsed_line);

void unix_error(char *msg) /* Unix-style error */
{
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(EXIT_FAILURE);
}

int main() {
  char cmdline[MAXLINE]; /* Command line */
  /* TODO: register signal handlers */

  while (1) {
    char *result;
    /* Read */
    printf(">"); /* TODO: correct the prompt */
    result = fgets(cmdline, MAXLINE, stdin);
    if (result == NULL && ferror(stdin)) {
      fprintf(stderr, "fatal fgets error\n");
      exit(EXIT_FAILURE);
    }

    if (feof(stdin)) exit(EXIT_SUCCESS);

    /* Evaluate */
    eval(cmdline);
  }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) {
  char buf[MAXLINE];   /* Holds modified command line */
  pid_t pid;           /* Process id */
  int status;          /* Process status */
  posix_spawn_file_actions_t actions; /* used in performing spawn operations */
  posix_spawn_file_actions_init(&actions); 

  strcpy(buf, cmdline);
  parsed_args parsed_line = parseline(buf);
  if (parsed_line.argv[0] == NULL) return; /* Ignore empty lines */

  /* Not a bultin command */
  if (!builtin_command(parsed_line.argv, pid, status)) {
	  

	  
    switch (parsed_line.mode) {
    case IS_SIMPLE: /* cmd argv1 argv2 ... */
        if (!exec_cmd(parsed_line.argv, &actions, &pid, &status, parsed_line.bg)) return;
        break;
		
    case IS_PIPE: /* command1 args | command2 args */
		isPipe(parsed_line);
        break;
		
    case IS_OUTPUT_REDIR: /* command args > output_redirection */
		isOutRedir(parsed_line);
        break;
		
    case IS_INPUT_REDIR: /* command args < input_redirection */
		isInRedir(parsed_line);
        break;
		
    case IS_INPUT_OUTPUT_REDIR: /* command args < input_redirection > output_redirection */
		isInOutRedir(parsed_line);
        break;
		
    case IS_SEQ: /* command1 args ; command2 args */
		isSeq(parsed_line);
        break;
		
    case IS_ANDIF: /* command1 args && command2 args */
		isAndIf(parsed_line);
        break;
    }
    if (parsed_line.bg)
      printf("%d %s", pid, cmdline);
    
  }
  posix_spawn_file_actions_destroy(&actions);
  return;
}

void isAndIf(parsed_args parsed_line) {
	char* cmd1[100];
	char* cmd2[100];
	int index = 0;
	
	// Get individual commands
	while(strcmp(parsed_line.argv[index], "&&") != 0) {
		cmd1[index] = parsed_line.argv[index];
		index++;
	}
	index++;
	cmd1[index] = NULL;
	
	int index2 = 0;
	while(parsed_line.argv[index] != NULL) {
		cmd2[index2] = parsed_line.argv[index];
		index++;
		index2++;
	}
	index2++;
	cmd2[index2] = NULL;
	posix_spawn_file_actions_t actions; /* used in performing spawn operations */
	posix_spawn_file_actions_init(&actions);
	int pid, pid2 = 0, status;
	if (exec_cmd(cmd1, &actions, &pid, &status, parsed_line.bg) != -1){
		exec_cmd(cmd2, &actions, &pid2, &status, parsed_line.bg);
	}
}


void isSeq(parsed_args parsed_line) {
	
	char* cmd1[100];
	char* cmd2[100];
	int index = 0;
	
	// Get individual commands
	while(strcmp(parsed_line.argv[index], ";") != 0) {
		cmd1[index] = parsed_line.argv[index];
		index++;
	}
	index++;
	cmd1[index] = NULL;
	
	int index2 = 0;
	while(parsed_line.argv[index] != NULL) {
		cmd2[index2] = parsed_line.argv[index];
		index++;
		index2++;
	}
	index2++;
	cmd2[index2] = NULL;
	posix_spawn_file_actions_t actions; /* used in performing spawn operations */
	posix_spawn_file_actions_init(&actions);
	int pid, pid2 = 0, status;
	exec_cmd(cmd1, &actions, &pid, &status, parsed_line.bg);
	exec_cmd(cmd2, &actions, &pid2, &status, parsed_line.bg);
}


void isInOutRedir(parsed_args parsed_line) {
	char* cmd1[100];
	char* inFileName;
	char* outFileName;
	int index = 0;
	
	// Get individual commands
	while(strcmp(parsed_line.argv[index], "<") != 0) {
		cmd1[index] = parsed_line.argv[index];
		index++;
	}
	index++;
	cmd1[index] = NULL;
	inFileName = parsed_line.argv[index];
	outFileName = parsed_line.argv[index + 2];
	
	// Modified FROM LAB
	int child_status;
	posix_spawn_file_actions_t actions1;
	int pid1;


	posix_spawn_file_actions_init(&actions1);
	int fileNum = 24;
	int fileNum2 = 25;
	
	posix_spawn_file_actions_addopen(&actions1, fileNum, inFileName, O_RDONLY, S_IWOTH|S_IROTH|S_IXOTH|S_IWUSR|S_IRUSR|S_IXUSR|S_IWGRP|S_IRGRP|S_IXGRP);
	posix_spawn_file_actions_addopen(&actions1, fileNum2, outFileName, O_WRONLY|O_CREAT,  S_IWOTH|S_IROTH|S_IXOTH|S_IWUSR|S_IRUSR|S_IXUSR|S_IWGRP|S_IRGRP|S_IXGRP);
	
	// Add duplication action of copying the write end of the pipe to the 
	// standard out file descriptor
	
	posix_spawn_file_actions_adddup2(&actions1, fileNum, STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions1, fileNum2, STDOUT_FILENO);
	

	// Add action of closing the read end of the pipe
	//posix_spawn_file_actions_addclose(&actions1, STDOUT_FILENO/*blank_2*/);

	  // Create the first child process 
	if (0 != posix_spawnp(&pid1, cmd1[0], &actions1/*blank_5*/, NULL, cmd1, environ)) {
		perror("spawn failed");
		exit(1);
	}

	close(fileNum);

	// Close the write end in the parent process
	close(pid1);

	// Wait for the first child to complete
	waitpid(pid1, &child_status, 0);
}


void isInRedir(parsed_args parsed_line) {
	
	char* cmd1[100];
	char* inFileName;
	int index = 0;
	
	// Get individual commands
	while(strcmp(parsed_line.argv[index], "<") != 0) {
		cmd1[index] = parsed_line.argv[index];
		index++;
	}
	index++;
	cmd1[index] = NULL;
	inFileName = parsed_line.argv[index];
	
	// Modified FROM LAB
	int child_status;
	posix_spawn_file_actions_t actions1;
	int pid1;


	posix_spawn_file_actions_init(&actions1);
	int fileNum = 24;
	
	posix_spawn_file_actions_addopen(&actions1, fileNum, inFileName, O_RDONLY, S_IWOTH|S_IROTH|S_IXOTH|S_IWUSR|S_IRUSR|S_IXUSR|S_IWGRP|S_IRGRP|S_IXGRP);
	
	// Add duplication action of copying the write end of the pipe to the 
	// standard out file descriptor
	
	posix_spawn_file_actions_adddup2(&actions1, fileNum, STDIN_FILENO);

	// Add action of closing the read end of the pipe
	//posix_spawn_file_actions_addclose(&actions1, STDOUT_FILENO/*blank_2*/);

	  // Create the first child process 
	if (0 != posix_spawnp(&pid1, cmd1[0], &actions1/*blank_5*/, NULL, cmd1, environ)) {
		perror("spawn failed");
		exit(1);
	}

	close(fileNum);

	// Close the write end in the parent process
	close(pid1);

	// Wait for the first child to complete
	waitpid(pid1, &child_status, 0);
}


void isOutRedir(parsed_args parsed_line) {
	
	char* cmd1[100];
	char* outFileName;
	int index = 0;
	
	// Get individual commands
	while(strcmp(parsed_line.argv[index], ">") != 0) {
		cmd1[index] = parsed_line.argv[index];
		index++;
	}
	index++;
	cmd1[index] = NULL;
	outFileName = parsed_line.argv[index];
	
	// Modified FROM LAB
	int child_status;
	posix_spawn_file_actions_t actions1;
	int pid1;


	
	// Initialize spawn file actions object for both the processes
	posix_spawn_file_actions_init(&actions1);
	
	//Open the file
	int fileNum = 26;
	posix_spawn_file_actions_addopen(&actions1, fileNum, outFileName, O_WRONLY|O_CREAT,  S_IWOTH|S_IROTH|S_IXOTH|S_IWUSR|S_IRUSR|S_IXUSR|S_IWGRP|S_IRGRP|S_IXGRP);
	

	
	// Add duplication action of copying the write end of the pipe to the 
	// standard out file descriptor
	posix_spawn_file_actions_adddup2(&actions1, fileNum/* blank_1*/, STDOUT_FILENO);

	// Add action of closing the read end of the pipe

	  // Create the first child process 
	if (0 != posix_spawnp(&pid1, cmd1[0], &actions1/*blank_5*/, NULL, cmd1, environ)) {
		perror("spawn failed");
		exit(1);
	}

	// Close the read end in the parent process
	//close(cmdFileNum/*blank_6*/);
	
	close(fileNum);

	// Close the write end in the parent process
	close(pid1);

	// Wait for the first child to complete
	waitpid(pid1, &child_status, 0);
	
}


void isPipe(parsed_args parsed_line) {
	
	char* cmd1[100];
	char* cmd2[100];
	int index = 0;
	int index2 = 0;
	
	// Get individual commands
	while(strcmp(parsed_line.argv[index], "|") != 0) {
		cmd1[index] = parsed_line.argv[index];
		index++;
	}
	index++;
	cmd1[index] = NULL;
	
	while(parsed_line.argv[index] != NULL) {
		cmd2[index2] = parsed_line.argv[index];
		index++;
		index2++;
	}
	cmd2[index2 + 1] = NULL;
	
	
	// TAKEN FROM LAB
	int child_status;
	posix_spawn_file_actions_t actions1, actions2;
	int pipe_fds[2];
	int pid1, pid2;

	// Initialize spawn file actions object for both the processes
	posix_spawn_file_actions_init(&actions1);
	posix_spawn_file_actions_init(&actions2);

	// Create a unidirectional pipe for interprocess communication with a 
	// read and write end.
	pipe(pipe_fds);

	// Add duplication action of copying the write end of the pipe to the 
	// standard out file descriptor
	posix_spawn_file_actions_adddup2(&actions1, pipe_fds[1]/* blank_1*/, STDOUT_FILENO);

	// Add action of closing the read end of the pipe
	posix_spawn_file_actions_addclose(&actions1, pipe_fds[0]/*blank_2*/);

	// Add duplication action of copying the read end of the pipe to the 
	// standard in file descriptor
	posix_spawn_file_actions_adddup2(&actions2, pipe_fds[0], STDIN_FILENO/*blank_3*/);

	// Add the action of closing the write end of the pipe
	posix_spawn_file_actions_addclose(&actions2, pipe_fds[1]/*blank_4*/);

	  // Create the first child process 
	if (0 != posix_spawnp(&pid1, cmd1[0], &actions1/*blank_5*/, NULL, cmd1, environ)) {
		perror("spawn failed");
		exit(1);
	}

	//Create the second child process
	if (0 != posix_spawnp(&pid2, cmd2[0], &actions2, NULL, cmd2, environ)) {
		perror("spawn failed");
		exit(1);
	}

	// Close the read end in the parent process
	close(pipe_fds[0]/*blank_6*/);

	// Close the write end in the parent process
	close(pipe_fds[1]);

	// Wait for the first child to complete
	waitpid(pid1, &child_status, 0);

	// Wait for the second child to complete
	waitpid(pid2, &child_status, 0);
	
	
	
}


/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv, pid_t pid, int status) {
  if (!strcmp(argv[0], "exit")) /* exit command */
    exit(EXIT_SUCCESS);
  if (!strcmp(argv[0], "&")) /* Ignore singleton & */
    return 1;
  // TODO: implement special command "?"

  return 0; /* Not a builtin command */
}
/* $end eval */

/* Run commands using posix_spawnp */
int exec_cmd(char** argv, posix_spawn_file_actions_t *actions, pid_t *pid, int *status, int bg) {
  //printf("simple command not yet implemented :(\n");
  // Lab 5 TODO: use posix_spawnp to execute commands
  posix_spawnattr_t attrp;
  posix_spawnattr_init(&attrp);
  if (posix_spawnp(pid,
				argv[0],		// const char *restrict path,
                actions,
                &attrp,		// const posix_spawnattr_t *restrict attrp,
                argv,		// char *const argv[restrict]
				environ		/* char *const envp[restrict]*/) != 0) {return -1;}
  // Lab 5 TODO: when posix_spawnp is ready, uncomment me
  if (getpid() != *pid)// if (!bg)
    if (waitpid(*pid, status, 0) < 0) unix_error("waitfg: waitpid error");
	
  return 1;
}
/* $end exec_cmd */

/* signal handler */
void signal_handler(int sig) {
  // TODO: handle SIGINT and SIGTSTP and SIGCHLD signals here
}

/* finds index of the matching target in the argumets */
int find_index(char** argv, char* target) {
  for (int i=0; argv[i] != NULL; i++)
    if (!strcmp(argv[i], target))
      return i;
  return 0;
}

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
parsed_args parseline(char *buf) {
  char *delim; /* Points to first space delimiter */
  parsed_args pa;

  buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* Ignore leading spaces */
    buf++;

  /* Build the argv list */
  pa.argc = 0;
  while ((delim = strchr(buf, ' '))) {
    pa.argv[pa.argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')){ /* Ignore spaces */
      buf++;
    }
  }
  pa.argv[pa.argc] = NULL;

  if (pa.argc == 0){ /* Ignore blank line */
    return pa;
  }

  /* Should the job run in the background? */
  if ((pa.bg = (*pa.argv[pa.argc - 1] == '&')) != 0) pa.argv[--pa.argc] = NULL;

  /* Detect various command modes */
  pa.mode = IS_SIMPLE;
  if (find_index(pa.argv, "|"))
    pa.mode = IS_PIPE;
  else if(find_index(pa.argv, ";")) 
    pa.mode = IS_SEQ; 
  else if(find_index(pa.argv, "&&"))
    pa.mode = IS_ANDIF;
  else {
    if(find_index(pa.argv, "<")) 
      pa.mode = IS_INPUT_REDIR;
    if(find_index(pa.argv, ">")){
      if (pa.mode == IS_INPUT_REDIR)
        pa.mode = IS_INPUT_OUTPUT_REDIR;
      else
        pa.mode = IS_OUTPUT_REDIR; 
    }
  }

  return pa;
}
/* $end parseline */
