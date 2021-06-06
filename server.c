#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	mkfifo("tmp", 0666);
	int pipe_fd = open("tmp", O_RDONLY);
	printf("Server Online (pid: %d)!\n", getpid());

	int bytes_read = 0;
	char buffer[1024];
	while(1)
	{
		printf("Waiting for request...\n");
		while((bytes_read = read(pipe_fd, buffer, 1024)) > 0)
		{
			printf("Request received.");
		}
	}

	close(pipe_fd);
	unlink("tmp");
	printf("Server closed!");
	return 0;
}
