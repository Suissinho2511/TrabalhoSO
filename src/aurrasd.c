#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <wait.h>

#define BUFFER_SIZE 1024
#define QUEUE_NAME "queue"
#define STATUS_NAME "status"

int readLn(int fd, char* buffer, int size);
int parse(char** parsed, char* buffer, int size, char* delim);
void freearr(void** pointer, int size);
int myexec(int in_fd, int out_fd, char* bin_name, char** args);

typedef struct status {
	int pid_server;
	char* filtros;
	char* filtros_traduzidos;
	int* max;
	int* running;
	char** tasks;
} *STATUS;

void sigterm_handler(int signum)
{
	printf("Closing server...\n");
	unlink(STATUS_NAME);
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
	int status_fd = open(STATUS_NAME, O_RDWR | O_CREAT | O_TRUNC);
	int conf_fd = open(argv[1], O_RDONLY | O_EXCL);
	printf("Server Online (pid: %d)!\n", getpid());
	//TODO: Interpretar Config file

	int bytes_read = 0, size, pid, pid_cliente, wstatus;
	char buffer[BUFFER_SIZE];
	char** parsed = calloc(1, sizeof(char*)); //temporário
	while(1)
	{
		printf("Waiting for request... ");
		fflush(stdout);
		while((bytes_read = readLn(queue_fd, buffer, BUFFER_SIZE)) > 0)
		{
			//queue:
			//pid filenameoriginal filenamedestino filtro0 filtro1 ...\n
			size = parse(parsed, buffer, bytes_read, " ");
			printf("\rRequest received (pid: %s).\n", parsed[0]);

			//TODO: Limitar o numero de processos ativos simultaneamente
			//TODO: Update do status
			//server -> Controller -> filho(ffmpeg)
			if((pid = fork()) == 0)
			{
				pid_cliente = atol(parsed[0]);	
				pid = myexec(0, 1, "ffmpeg", parsed); //replaces parsed[0] with bin_name btw

				kill(pid_cliente, SIGUSR1); 		//processing
				waitpid(pid, &wstatus, 0);		//waits till ffmpeg finishes...
				if(WIFEXITED(wstatus))
					kill(pid_cliente, SIGUSR1); 	//finished
				else					//or
					kill(pid_cliente, SIGUSR2);	//error

				_exit(0);				//exits
			}
			freearr((void**) parsed, size); //not gonna stay here
		}
		freearr((void**) parsed, 1); //temporário
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
	buffer[bytes_read] = '\0';

	return bytes_read;
}

int parse(char** parsed, char* buffer, int size, char* delim)
{
	//char** parsed = calloc(size+1, sizeof(char));
	int i;
	char* token = strtok(buffer, delim);
	for(i = 0; ; i++)
	{
		parsed[i] = strdup(token);
		token = strtok(NULL, delim);
		if(!token)
		{
			parsed[i+1] = NULL;
			break;
		}
	}

	return i;
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
		args[0] = bin_name;
		dup2(in_fd, 0);
		dup2(out_fd, 1);
		execvp(bin_name, args);
	}
	return pid;
}


