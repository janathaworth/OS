#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>

char* trim(char* str);
void split(char* input, char* delim, char** args);

int main () {
	char str[2000];
	int status;
	char* input;
	char** args;
	pid_t cpid;
	while(1){ 
		printf("$");
		fgets(str, 2000, stdin); 
		input = trim(str);
		args = malloc(sizeof(char*)*strlen(input)*sizeof(char)*strlen(input));
		split(input, " ", args);
		cpid = fork();

		if (cpid == 0) {
			execvp(*args, args);
			//need to break if input wrong and can print message in parent
		}
		else {
			waitpid(cpid, &status, 0);
			free(args);
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
		ptr++;	
		*ptr = strtok(NULL, delim);	
	}	
}
