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

int status;

void progress_signal(int signum)
{
	status++;
	if (status == 1) {printf("processing "); pause();}
	if (status == 2) {printf("finished"); alarm(1);}
}
void error_signal(int signum)
{
	printf("Signal %d received. Server error.", signum);
}

int main(int argc, char **argv)
{
	signal(SIGUSR1, progress_signal);
	signal(SIGUSR2, error_signal);
	signal(SIGALRM, SIG_IGN);

	char stdout_buffer[BUFFER_SIZE];
	setvbuf(stdout, stdout_buffer, _IONBF, BUFFER_SIZE);

	int queue_fd = open(QUEUE_NAME, O_WRONLY);
	printf("Session open (pid: %d)!", getpid());

	//argv[1] ficheiro original
	//argv[2] ficheiro destino
	//argv[3-] filtros

	int size = 0;
	char buffer[1024];

	size += sprintf(buffer, "%d ", getpid());
	for ( int i = 1; i < argc; i++)
	{
		size += sprintf(&buffer[size],"%s ", argv[i]);
	}
	buffer[size] = '\n';
	write(queue_fd, buffer, size+1);

	printf("pending ");
	pause();

	close(queue_fd);
	return 0;
}
