#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv)
{
	int pipe_fd = open("tmp", O_WRONLY);
	printf("Session open (pid: %d)!", getpid());

	int bytes_read = 0;
	char buffer[1024];

	bytes_read = read(0, buffer, 1024);
	write(pipe_fd, buffer, bytes_read);

	close(pipe_fd);
	return 0;
}
