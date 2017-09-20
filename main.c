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
static void sig_handler(int signo);

typedef struct job {
	int jobnum;
	struct job* next;
	struct job* prev;
	pid_t pid;
	char status[8]; 
	char command[2000];
} job;

// yash_pid represents the shell. When a command is entered, 
// yash will create a command handler with pid cmds_pid which will
// create ch1 and ch2 if needed for the corresponding commands

// This model was adopted after discovering setsid() in ch1 would not let 
// ch2 join and would give error EPERM (even in sig_ex3.c) and not being able
// to figure out why setpgid(0,0) messed with the input/output of pipe neither 
// by myself nor with the help of Dr. Yeraballi. Thus, he suggested this method. 

pid_t fg;
char* symbol_ptr;
char* pipe_ptr; 

job* first_job = NULL;
job* last_job = NULL;
job* current_job = NULL;

int main () {
	pid_t ch1_pid, ch2_pid, yash_pid, cmd_pid;
	char str[2000];
	int status, length;
	char* input;
	char** args, ptr, cmds;
	int pipefd[2];
	int pid;
	int count = 0;
	yash_pid = getpid(); 

	while(1){ 

		if (signal(SIGINT, sig_handler) == SIG_ERR) {
			perror("signal(SIGINT) error");
		}
		if (signal(SIGTSTP, sig_handler) == SIG_ERR) {
			perror("signal(SIGTSTP) error");
		}
		if (signal(SIGCHLD, sig_handler) == SIG_ERR) {
			perror("signal(SIGTSTP) error");
		}

		printf("# ");
		fflush(stdout);
		if(fgets(str, 2000, stdin) == NULL){
			printf("exit\n");
			while (first_job != NULL) {
				if (first_job->pid > 0 && kill(-(first_job->pid), 0) != -1) {
					kill(-(first_job->pid), SIGTERM);
				}
				first_job = first_job->next;
			}
			exit(EXIT_SUCCESS);
		}

		input = trim(str);

		if(strcmp(input,"jobs") == 0) {
			job* ptr = first_job;
			while(ptr->next != NULL) {
				printf("[%d] %s %s        %s\n", ptr->jobnum, "-", ptr->status, ptr->command);
				ptr = ptr->next;
			}
			printf("[%d] %s %s        %s\n", ptr->jobnum, "+", ptr->status, ptr->command);
		}

		else if(strcmp(input, "fg") == 0 ) {
			if (last_job != NULL) {
				//TAKE OUT OF LIST?????
				pid = last_job->pid;
				if (pid > 0 && kill(pid, 0) != -1) {
					fg = pid;
					strcpy(last_job->status, "Running");
					printf("%s\n", last_job->command);
					kill(-pid, SIGCONT);
				}
				int wait = waitpid(pid, &status, WUNTRACED);
				if (wait == -1) {
					perror("waitpid");
					exit(EXIT_FAILURE);
				}


				if (WIFEXITED(status)) {
					if (WEXITSTATUS(status) <= 0) {
						// current_job = NULL;
						if (last_job != NULL && fg == last_job->pid) { 
							count--; 
							free(last_job);
							if (count == 0) {
								last_job = NULL;
							} else {
								last_job = last_job->prev; 
								last_job->next = NULL;
							}
							fg = 0;
						}
					}
				} else if (WIFSIGNALED(status)) {
					if(WTERMSIG(status) == SIGINT) {
						if (last_job != NULL && fg == last_job->pid) { 
							count--; 
							free(last_job);
							if (count == 0) {
								last_job = NULL;
							} else {
								last_job = last_job->prev; 
								last_job->next = NULL;
							}
							fg = 0;
						}
					}
				} else if (WIFSTOPPED(status)) {
					strcpy(last_job->status, "Stopped");
					fg = 0; 
				}
			}
		}
		else if(strcmp(input, "bg") == 0) {
			if(last_job != NULL) {
				job* ptr = last_job;
				while (ptr != NULL && !strcmp(ptr->status, "Running")) {
					ptr = ptr->prev;
				}
				if(ptr != NULL && kill(ptr->pid, 0) != -1) {
					printf("[%d] %s %s\n", ptr->jobnum, "+", ptr->command);
					kill(-(ptr->pid), SIGCONT);
					strcpy(ptr->status, "Running");
				}
			}
		}
	
		else {
			int bg = (*(input + strlen(input) -1) == '&');
			char* cmd = strdup(input);
			if(bg) {
				*(input + strlen(input) - 2) = '\0';
			}

			length = strlen(input);
			args = malloc(sizeof(char*)*length*sizeof(char)*length);
			symbol_ptr = NULL;
			pipe_ptr = NULL;
			split(input, args);

			cmd_pid = fork();

			if 	(cmd_pid == -1) {
				perror("fork");
				exit(EXIT_FAILURE);
			}

			if (cmd_pid > 0) {
				//shell
				if (bg) {
					current_job = malloc(sizeof(job));
					current_job->jobnum = count + 1; 
					current_job->pid = cmd_pid;
					strcpy(current_job->command, cmd);
					strcpy(current_job->status, "Running");
					current_job->next = NULL;
					current_job->prev = last_job;

					if (count == 0) {
						first_job = current_job;
						last_job = current_job;
					}
					else {
						last_job->next = current_job;
						last_job = current_job;
					}
					count++;
				}
				else {
					fg = cmd_pid;

					pid = waitpid(cmd_pid, &status, WUNTRACED);
	
					if (pid == -1) {
						perror("waitpid");
						exit(EXIT_FAILURE);
					}

					if (WIFSTOPPED(status)) {
						current_job = malloc(sizeof(job));
						current_job->jobnum = count + 1; 
						current_job->pid = cmd_pid;
						strcpy(current_job->command, input);
						strcpy(current_job->status, "Stopped");
						current_job->next = NULL;
						current_job->prev = last_job;
						if (count == 0) {
							first_job = current_job;
							last_job = current_job;
						}
						else {
							last_job->next = current_job;
							last_job = current_job;
						}
						count++;
					}
				}
			} else {
				//cmd

				if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
					perror("signal(SIGINT) error");
				}
				if (signal(SIGTSTP, SIG_DFL) == SIG_ERR) {
					perror("signal(SIGTSTP) error");
				}
				if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
					perror("signal(SIGTSTP) error");
				}
			
				if (setpgrp()== -1) {
					perror("setpgrp");
					exit(EXIT_FAILURE);
				}
				// signal(SIGTTOU, SIG_IGN);
				// tcsetpgrp(open("/dev/tty", O_RDONLY, MODES), getpid());

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
					//cmd

					if (pipe_ptr != NULL) {
						ch2_pid = fork(); 

						if(ch2_pid == -1) {
							perror("fork");
							exit(EXIT_FAILURE);
						} 
						else if (ch2_pid > 0) {
							//cmd
							close(pipefd[0]);
							close(pipefd[1]);

						
							if(!bg) {
								pid = waitpid(0, &status, WUNTRACED) ;
								if (pid == -1){
									perror("waitpid");
									exit(EXIT_FAILURE);
								}
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

					//cmd
					if (!bg) {
						int pid = waitpid(0, &status, 0) ;
						if (pid == -1){
							perror("waitpid");
							exit(EXIT_FAILURE);
						}
						free(args);
						exit(EXIT_SUCCESS);
					}
					
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

static void sig_handler(int signo) {
	switch (signo) {
		int dedpid; 
		case SIGINT:
			if (fg > 0 && kill(-fg,0) != -1) {
				kill(-fg, SIGINT);
				fflush(stdout);
				printf("\n");
			}
			else {
				//still waiting for input 
				printf("\n# ");
			}
			fflush(stdout);
			break;

		case SIGTSTP:
		// fg = tcgetpgrp(open("/dev/tty", O_RDONLY, MODES));
			if (fg > 0 && kill(-fg, 0) != -1) {
				kill(-fg, SIGTSTP);
			}
			printf("\n");
			fflush(stdout);
			break;

		case SIGCHLD:
			dedpid = waitpid(-1, 0, WNOHANG);
			
			break;

	}
}