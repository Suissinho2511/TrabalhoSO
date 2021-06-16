#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
#define QUEUE_NAME "tmp/queue"
#define STATUS_NAME "tmp/status"
int status;

void progress_signal(int signum)
{
	status++;
	if (status == 1)
	{
		printf("pending\n");
		pause();
	}
	if (status == 2)
	{
		printf("processing\n");
		pause();
	}
	if (status == 3)
	{
		printf("finished!\n");
		alarm(1);
	}
}
void error_signal(int signum)
{
	printf("Signal %d received. Server error.\n", signum);
}

int main(int argc, char **argv)
{

	if (strcmp(argv[1], "status") == 0)
	{
		//status
		int bytes_read, status_fd = open(STATUS_NAME, O_RDONLY);
		char buffer[BUFFER_SIZE];
		while ((bytes_read = read(status_fd, buffer, BUFFER_SIZE)) > 0)
		{
			write(1, buffer, bytes_read);
		}
		close(status_fd);
		return 0;
	}
	else if (strcmp(argv[1], "transform") == 0)
	{
		//transform
		signal(SIGUSR1, progress_signal);
		signal(SIGUSR2, error_signal);
		signal(SIGALRM, SIG_IGN);
		status = 0;

		char stdout_buffer[BUFFER_SIZE];
		setvbuf(stdout, stdout_buffer, _IOLBF, BUFFER_SIZE);

		int queue_fd = open(QUEUE_NAME, O_WRONLY);
		printf("Session open (pid: %d)!\n", getpid());

		//argv[1] transform
		//argv[2] ficheiro original
		//argv[3] ficheiro destino
		//argv[4-] filtros

		int size = 0;
		char buffer[1024];

		size += sprintf(buffer, "%d ", getpid());
		for (int i = 1; i < argc; i++)
		{
			size += sprintf(&buffer[size], "%s ", argv[i]);
		}
		buffer[size] = '\n';
		write(queue_fd, buffer, size + 1);

		pause();

		close(queue_fd);
		return 1;
	}
	else
	{
		printf("Operation not recognized: %s\n", argv[1]);
		return -1;
	}
}
