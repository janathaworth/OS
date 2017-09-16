#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>

#define MODES S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH

char* trim(char* str);
void split(char* input, char** args);
int isSymbol(char* str);
static void sig_handler(int signo);

pid_t cpid;
char* symbol;

int main () {
	char str[2000];
	int status, length;
	char* input;
	char** args;
	// int file;

	// int pipefd[2];

	// if(pipe(pipefd) == -1) {
	// 	perror("pipe");
	// 	exit(EXIT_FAILURE);
	// }

	while(1){ 
		printf("# ");
		if(fgets(str, 2000, stdin) == NULL){
			//does not work for children (when written - child would not get here)
			raise(SIGINT);
		}

		input = trim(str);
		int length = strlen(input);
		args = malloc(sizeof(char*)*length*sizeof(char)*length);
		symbol = NULL;
		split(input, args);
	
		cpid = fork();
		if (cpid == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		//cpid = fork();
		if (cpid == 0) {


		
			if (symbol != NULL) {
				//symb = malloc(sizeof(char)*2); 
				char* symb = strdup(symbol);
				char** ptr = args; 

				//symbol is at symbol
				while(*ptr != symbol) {
					ptr++;
				}

				*ptr = NULL;


				if (*symb == '>') {
					int out = dup(1);
					int file = open(*(ptr + 1), O_CREAT| O_TRUNC| O_WRONLY, MODES);
					dup2(file, 1);
					//close(file);
					//dup2(stdout, 1)
				}
				else if(strcmp(symb, "2>") == 0) {
					int err = dup(2);
					int file = open(*(ptr + 1), O_CREAT| O_TRUNC| O_WRONLY, MODES);
					dup2(file, 2);
				}
			}
	
			// if( file = open(*(ptr + 1), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR) == -1) {
			// 	fprintf(stderr, "yash: cannot open file");
			// 	exit(EXIT_FAILURE);	
			// }


			execvp(*args, args);
			//need to break if input wrong and can print message in parent
			printf("yash: %s: command not found\n", *args);
			exit(EXIT_FAILURE);	
		} else {
			waitpid(cpid, &status, 0);
			free(args);
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

static void sig_handler(int signo) {
	switch(signo){
		case SIGINT:
			//change to -pid to send to group
		 	kill(cpid, SIGINT);	
			break;
	}
}
