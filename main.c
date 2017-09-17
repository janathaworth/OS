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
void redirect(char* filename);
int getfd();
static void sig_handler(int signo);

pid_t ch1_pid, ch2_pid, pid;
char* symbol;

int main () {
	char str[2000];
	int status, length;
	char* input;
	char** args;
	char** ptr;
	int pipefd[2];
	int count = 0; 

	pid = getpid();

	while(1){ 
		printf("# ");
		if(fgets(str, 2000, stdin) == NULL){
			//does not work for children (when written - child would not get here)
			raise(SIGINT);
		}

		input = trim(str);
		length = strlen(input);
		args = malloc(sizeof(char*)*length*sizeof(char)*length);
		symbol = NULL;
		split(input, args);

		if(symbol != NULL && *symbol == '|'){
			if(pipe(pipefd) == -1) {
				perror("pipe");
				exit(EXIT_FAILURE);
			}
		}


		// fprintf(stderr, "fork 1\n");
		ch1_pid = fork();
		if 	(ch1_pid == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
 
		if (ch1_pid == 0) {

			if (symbol != NULL) { 
				ptr = args; 

				while(!isSymbol(*ptr)) {
					// fprintf(stderr, *ptr);
					ptr++;
				}
				symbol = *ptr; 
				// fprintf(stderr, symbol);

				while(*ptr != NULL) {
					symbol = *ptr;	
					*ptr = NULL;
					ptr++;

					if (*symbol == '|') {
						close(pipefd[0]); //close read end
						dup2(pipefd[1], STDOUT_FILENO); //output to pipe
						break;
					}
					else {
						redirect(*ptr);
					}

					//go to next symbol if it exists
					while(!isSymbol(*ptr) && *ptr != NULL) {
						ptr++;
					}	
				}
			}

			execvp(*args, args);
			fprintf(stderr, "yash: %s: command not found\n", *args);
			exit(EXIT_FAILURE);	
		} else {
			//fprintf(stderr, symbol);

			if(symbol != NULL && *symbol == '|') {
				//code here
				// fprintf(stderr,"fork 2\n");
				ch2_pid = fork();
				if (ch2_pid == 0) {
					close(pipefd[1]); //close write end
					dup2(pipefd[0], STDIN_FILENO); // take input from STDIN
					while (*args != symbol) {
						args++;
					}
					args++;

					ptr = args;
					while(!isSymbol(*ptr) && *ptr != NULL) {
						ptr++;
					}
					
					while(*ptr != NULL) {
						symbol = *ptr;	
						*ptr = NULL;
						ptr++;
						//fprintf(stderr, symbol);
						redirect(*ptr);

						//go to next symbol if it exists
						while(!isSymbol(*ptr) && *ptr != NULL) {
							ptr++;
						}	
					}

					execvp(*args, args);
					fprintf(stderr, "yash: %s: command not found\n", *args);
					exit(EXIT_FAILURE);	
				//execvp
				}
				else {
					close(pipefd[0]);
					close(pipefd[1]);

			
					if (waitpid(-1, &status, 0) == -1) {
						perror("waitpid");
						exit(EXIT_FAILURE);
					}
				}
			}

			if(getpid() == pid){ 
				//parent
				// fprintf(stderr, "parent\n");

				if (waitpid(-1, &status, 0) == -1) {
					perror("waitpid");
					exit(EXIT_FAILURE);
				}
				free(args);
			}
			
//			if(WIFEXITED(status)) {
//				printf("child done");
//			} else if(WIFSIGNALED(status)){
//				printf("signaled");
//			} else if(WIFSTOPPED(status)){
//				printf("stopped");
//			}
		}
	}
	return 0;
	
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
			symbol = *args;
		args++;
		*args = strtok(NULL, " ");
	}
}

//do not call with null
int isSymbol(char* str){
	return (str != NULL && (strcmp(str, "<") == 0
		|| strcmp(str,">") == 0 || strcmp(str, "2>") == 0
		|| strcmp(str, "|") == 0)) ? 1 : 0;
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
}

int getfd() {
	//strcmp??
	if (*symbol == '<') 
		return STDIN_FILENO;
	else if (*symbol == '>') 
		return STDOUT_FILENO;
	else if(strcmp(symbol, "2>") == 0)
		return STDERR_FILENO;
	else 
		return -1;
}

static void sig_handler(int signo) {
	switch(signo){
		case SIGINT:
			//change to -pid to send to group
		 	kill(ch1_pid, SIGINT);	
			break;
	}
}
