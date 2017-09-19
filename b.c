#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
int main () {
	pid_t parent, cmd_child, ch1, ch2;
	int status;

	cmd_child = fork(); 
	if(cmd_child == -1) {
		perror("why");
	}

	if (cmd_child == 0) {
		setsid();
		printf("cmd %d\n", getpgid(0));
		ch1 = fork();

		if (ch1 == 0) {
			printf("c1 %d\n", getpgid(0));
		}
		else {
			ch2 = fork();

			if (ch2 == 0) {
				printf("c2 %d\n", getpgid(0));
			}
		}
	} else {
		waitpid(cmd_child, &status, 0);
		printf("parent %d\n", getpgid(0));
	}
}
