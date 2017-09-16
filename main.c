#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

char* trim(char* str);
void split(char* input, char* delim, char** args);
int isToken(char* str);
static void sig_handler(int signo);

pid_t cpid;
char* tok;

int main () {
	char str[2000];
	int status;
	char* input;
	char** args;

	while(1){ 
		printf("# ");
		if(fgets(str, 2000, stdin) == NULL){
			//does not work for children (when written - child would not get here)
			raise(SIGINT);
		}

		input = trim(str);
		args = malloc(sizeof(char*)*strlen(input)*sizeof(char)*strlen(input));
		split(input, " ", args);
		printf(tok);
		cpid = fork();
		
		if (cpid == 0) {
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

void split(char* input, char* delim, char** args){
	char** ptr = args;	
	*ptr = strtok(input, delim);
	while(*ptr != NULL){
		if (isToken(*ptr))
			tok = *ptr;
		ptr++;	
		*ptr = strtok(NULL, delim);
	}	
}

int isToken(char* str){
	if (strcmp(str, "<") == 0 || strcmp(str,">") == 0 || strcmp(str, "2>") == 0 || strcmp(str, "|") == 0)
		return 1;
	else 
		return 0; 
}

static void sig_handler(int signo) {
	switch(signo){
		case SIGINT:
			//change to -pid to send to group
		 	kill(cpid, SIGINT);	
			break;
	}
}
