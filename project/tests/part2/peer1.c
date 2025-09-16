#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>


#define MAX_COMMAND_SIZE 5520

void sender(char* pipe_name, char* argument){
	int pid = fork();
	if (pid == 0){
		//Child:
		int fd = open(pipe_name, O_RDONLY);
		if (fd == -1){
			printf("oof\n");
			exit(1);
		}
		//Redirect stdin to the named pipe
		if (dup2(fd, STDIN_FILENO) == -1){
			perror("Oof\n");
			exit(1);
		}
		close(fd);
		//Will use the named pipe.
		execlp("./btide", "./btide", "config1.cfg", NULL);
		perror("Failed to execute sender\n");
		exit(1);

	} else {
		//parent
		int fd = open(pipe_name, O_WRONLY);
		if (fd == -1){
			printf("Parent oof\n");
			return;
		}
		//Redirect stdout to the named pipe
		FILE* file = fopen(argument, "r");
		char buffer[MAX_COMMAND_SIZE] = {0};

		if (file){
			// printf("\nfile opened\n");
			while (fgets(buffer, MAX_COMMAND_SIZE, file)){
				// printf("read %s\n", buffer);
				size_t len = strlen(buffer);
				if (write(fd, buffer, len) != len){
					printf("error writing to pipe.\n");
				}
			}
			fclose(file);
			close(fd);
		} else {
			printf("Cannot open file\n");
			close(fd);
			kill(pid, SIGKILL);
			printf("Child killed!\n");
		}
		//Let the child process do what it can
		wait(NULL);
	}
}

void reciever(){
	int pid = fork();
	if (pid == 0){
		//Child:
		printf("Starting reciever\n");
		execlp("./btide", "./btide", "config2.cfg", NULL);
		perror("Failed to execute reciever\n");
		exit(1);
	}
}

int main(int argc, char** argv){
	char* name = "sender_pipe.pipe";
	mode_t perm = 0600;
	int ret = mkfifo(name, perm);

	if (-1 == ret){
		perror("Failed to create the pipe for sender.\n");
		return -1;
	}

	
	reciever();
	sleep(1);
	sender(name, argv[1]);

	// printf("Sender done\n");
	unlink(name);

	return 1;
}
