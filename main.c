// Janat Haworth
// Operating Systems Project 1
// DUE: September 19, 2017

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define MODES S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH

char* trim(char* str);
void split(char* input, char** args);
int isSymbol(char* str);
int isPipe(char* str);
void redirect(char* filename);
void handleRedirection(char** ptr);
int getfd();
static void sig_int(int signo);
static void sig_tstp(int signo);


// yash_pid represents the shell. When a command is entered, 
// yash will create a command handler with pid cmds_pid which will
// create ch1 and ch2 if needed for the corresponding commands

// This model was adopted after discovering setsid() in ch1 would not let 
// ch2 join and would give error EPERM (even in sig_ex3.c) and not being able
// to figure out why setpgid(0,0) messed with the input/output of pipe neither 
// by myself nor with the help of Dr. Yeraballi. Thus, he suggested this method. 

pid_t ch1_pid, ch2_pid, yash_pid, cmds_pid;
char* symbol_ptr;
char* pipe_ptr; 

int main () {
	char str[2000];
	int status, length;
	char* input;
	char** args;
	char** ptr;
	int pipefd[2];
	int count = 0; 

	yash_pid = getpid(); 
	
	if (signal(SIGINT, sig_int) == SIG_ERR) {
		perror("signal(SIGINT) error");
	}
	if (signal(SIGTSTP, sig_tstp) == SIG_ERR) {
		perror("signal(SIGTSTP) error");
	}

	while(1){ 

		printf("# ");
		if(fgets(str, 2000, stdin) == NULL){
			printf("exit\n");
			if (cmds_pid > 0 && kill(-cmds_pid, 0) != -1) {
				kill(-cmds_pid, SIGTERM);
			}
			exit(EXIT_SUCCESS);
		}

		input = trim(str);
		length = strlen(input);
		args = malloc(sizeof(char*)*length*sizeof(char)*length);
		symbol_ptr = NULL;
		pipe_ptr = NULL;
		split(input, args);

		cmds_pid = fork();

		if 	(cmds_pid == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		else if (cmds_pid > 0) {
			//shell
			if (waitpid(cmds_pid, &status, 0) == -1) {
				perror("waitpid");
				exit(EXIT_FAILURE);
			}
		} else {
			//cmds 
			if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
				perror("signal(SIGINT) error");
			}
			if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
				perror("signal(SIGINT) error");
			}
			

			if (setsid() == -1) {
				perror("setsid");
			}

			//create pipe
			if(pipe_ptr != NULL){
				if(pipe(pipefd) == -1) {
					perror("pipe");
					exit(EXIT_FAILURE);
				}
			}

			ch1_pid = fork(); 

			if(ch1_pid == -1) {
				perror("fork");
				exit(EXIT_FAILURE);
			}

			else if (ch1_pid > 0) {
				//cmds

				if (pipe_ptr != NULL) {
					ch2_pid = fork(); 

					if(ch2_pid == -1) {
						perror("fork");
						exit(EXIT_FAILURE);
					} 
					else if (ch2_pid > 0) {
						//cmds
						close(pipefd[0]);
						close(pipefd[1]);

						if (waitpid(-1, &status, 0) == -1) {
							perror("waitpid");
							exit(EXIT_FAILURE);
						}
					} else {
						//child 2
						close(pipefd[1]); //close write end
						dup2(pipefd[0], STDIN_FILENO); // take input from STDIN
						close(pipefd[0]);

						while (*args != pipe_ptr) {
							args++;
						}
						args++;
						handleRedirection(args);
						execvp(*args, args);
						fprintf(stderr, "yash: %s: command/file not found\n", *args);
						exit(EXIT_FAILURE);	
					}
				}

				//cmds
				if (waitpid(-1, &status, 0) == -1) {
					perror("waitpid");
					exit(EXIT_FAILURE);
				}
				free(args);
				exit(EXIT_SUCCESS);
				
			} else {
				//child 1
				if (symbol_ptr != NULL) { 

					if(pipe_ptr != NULL) {
						close(pipefd[0]); //close read end
						dup2(pipefd[1], STDOUT_FILENO); //output to pipe
						close(pipefd[1]);
					}
					handleRedirection(args);
				}

				execvp(*args, args);
				fprintf(stderr, "yash: %s: command not found\n", *args);
				exit(EXIT_FAILURE);	

			}
		}
	}
}

char* trim(char* str) {
	char* end = str + strlen(str) - 1;
	while(isspace(*str))
		str++;
	
	while(end > str && isspace (*end))
		end--;

	*(end+1) = 0;
	return str;
} 

void split(char* input, char** args){
	*args = strtok(input, " ");
	while(*args != NULL){
		if(isSymbol(*args))
			symbol_ptr = *args;
		if(isPipe(*args))
			pipe_ptr = *args;
		args++;
		*args = strtok(NULL, " ");
	}
}

//do not call with null
int isSymbol(char* str){
	return (str != NULL && (strcmp(str, "<") == 0
		|| strcmp(str,">") == 0 || strcmp(str, "2>") == 0
		|| strcmp(str,"|") == 0)) ? 1 : 0;
}

int isPipe(char* str) {
	return (strcmp(str, "|") == 0) ? 1 : 0;
}

void handleRedirection(char** ptr) {
	while(!isSymbol(*ptr) && *ptr != NULL) {
		ptr++;
	} 

	while(*ptr != NULL && !isPipe(*ptr)) {
		symbol_ptr = *ptr;	
		*ptr = NULL;
		ptr++;
		redirect(*ptr);

		//go to next symbol if it exists
		while(!isSymbol(*ptr) && *ptr != NULL) {
			ptr++;
		}	
	}
	*ptr = NULL;
}

void redirect(char* filename) {
	int file = -1;  
	int fd = getfd();
	if(fd == STDIN_FILENO) 
		file  = open(filename, O_RDONLY, MODES);
	else if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
		file = open(filename, O_CREAT | O_TRUNC | O_WRONLY, MODES);

	if (file == -1) {
		fprintf(stderr,"yash: %s: %s\n", filename, strerror(errno));
		exit(EXIT_FAILURE);	
	}

	dup2(file, fd);
	close(file);
}

int getfd() {
	if (*symbol_ptr == '<') 
		return STDIN_FILENO;
	else if (*symbol_ptr == '>') 
		return STDOUT_FILENO;
	else if(strcmp(symbol_ptr, "2>") == 0)
		return STDERR_FILENO;
	else 
		return -1;
}

static void sig_int(int signo) {
	if (cmds_pid > 0 && kill(-cmds_pid,0) != -1) {
		kill(-cmds_pid, SIGINT);
		printf("\n");
	}
	else {
		//still waiting for input 
		printf("\n# ");
	}
	fflush(stdout);
}

static void sig_tstp(int signo) {
	if (cmds_pid > 0 && kill(-cmds_pid,0) != -1) {
		kill(-cmds_pid, SIGTSTP);
	}
}
