#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
#define QUEUE_NAME "queue"

int readLn(int fd, char* buffer, int size);
char** parse(char* buffer, int size, char* delim);
void freearr(void** pointer, int size);
int myexec(int in_fd, int out_fd, char* bin_name, char** args);

void sigterm_handler(int signum)
{
	printf("Closing server...\n");
	//...
	unlink(QUEUE_NAME);
	printf("Server closed!\n");
}

int main(int argc, char **argv)
{
	signal(SIGTERM, sigterm_handler);

	char stdout_buffer[BUFFER_SIZE];
	setvbuf(stdout, stdout_buffer, _IOLBF, BUFFER_SIZE);

	mkfifo(QUEUE_NAME, 0666);
	int queue_fd = open(QUEUE_NAME, O_RDONLY);
	printf("Server Online (pid: %d)!\n", getpid());

	int bytes_read = 0, pid;
	char buffer[BUFFER_SIZE];
	char** parsed;
	while(1)
	{
		printf("Waiting for request... ");
		fflush(stdout);
		while((bytes_read = readLn(queue_fd, buffer, BUFFER_SIZE)) > 0)
		{
			//queue:
			//pid filenameoriginal filenamedestino filtro0 filtro1 ...\n
			parsed = parse(buffer, bytes_read, " ");
			printf("Request received (pid: %s).\n", parsed[0]);

			//pai -> Controller -> filho(ffmpeg)
			if((pid = fork()) == 0)
			{
				pid = myexec(stdin, stdout, "ffmpeg", &parsed[1]);
				kill(atol(parsed[0]), SIGUSR1); //processing
				wait(pid);
				kill(atol(parsed[0]), SIGUSR1); //finished
				_exit(0);
			}
			freearr(parsed, 0); //wrong btw
		}
	}
	return 0;
}

int readLn(int fd, char* buffer, int size)
{
	int bytes_read = 0;
	while((bytes_read += read(fd, buffer+bytes_read, 1)) > 0 && bytes_read < size)
	{
		if(buffer[bytes_read] == '\n') break;
	}
	buffer[bytes_read] = NULL;

	return bytes_read;
}

char** parse(char* buffer, int size, char* delim)
{
	char* token;
	char** parsed = calloc(size+1, sizeof(char));

	token = strtok(buffer, delim);
	for(int i = 0; ; i++)
	{
		parsed[i] = strdup(token);
		token = strtok(NULL, delim);
		if(!token)
		{
			parsed[i+1] = NULL;
			break;
		}
	}

	return parsed;
}

void freearr(void** pointer, int size)
{
	for(int i = 0; i < size; i++) free(pointer[i]);
	free(pointer);
	return;
}

int myexec(int in_fd, int out_fd, char* bin_name, char** args)
{
	int pid;
	if((pid = fork()) == 0)
	{
		//filho
		dup2(in_fd, stdin);
		dup2(out_fd, stdout);
		execvp(bin_name, args); //wrong ?
	}
	else return pid;
}


