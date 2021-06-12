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
#define QUEUE_NAME "../tmp/queue"
#define STATUS_NAME "../tmp/status"

int readLn(int fd, char* buffer, int size);
int parse(char** parsed, char* buffer, int size, char* delim);
void freearr(void** pointer, int size);
int myexec(int in_fd, int out_fd, char* bin_name, char** args);

typedef struct status {
	int pid_server, num_filters;
	int *max, *running;
	char **filters, **filtersT, **tasks;
} *STATUS;

int run = 1;
STATUS newStatus();
STATUS readStatus(STATUS s, char* conf_filepath);
STATUS addTask(STATUS s, char** task, int task_number);
STATUS removeTask(STATUS s, char** task, int task_number);
int canRun(STATUS s, char** task);

void sigterm_handler(int signum)
{
	printf("Closing server...\n");
	unlink(STATUS_NAME);
	unlink(QUEUE_NAME);
	run = 0;
	printf("Server closed!\n");
}

int main(int argc, char **argv)
{
	signal(SIGTERM, sigterm_handler);				//Handling sigterm to slowly close server

	char stdout_buffer[BUFFER_SIZE];
	setvbuf(stdout, stdout_buffer, _IOLBF, BUFFER_SIZE);		//stdout to Line Buffered

	mkfifo(QUEUE_NAME, 0666);					//create fifo queue
	int queue_fd = open(QUEUE_NAME, O_RDONLY);			//open fifo queue
	int status_fd = open(STATUS_NAME, O_RDWR | O_CREAT | O_TRUNC);	//create status

	STATUS s = newStatus();
	s = readStatus(s, argv[1]);					//ler o status

	printf("Server Online (pid: %d)!\n", getpid());

	int bytes_read = 0, size, pid, pid_cliente, wstatus, task_num = 0;
	char buffer[BUFFER_SIZE];
	char** parsed = calloc(s->num_filters+4, sizeof(char*));
	while(run == 1)
	{
		printf("Waiting for request... ");
		fflush(stdout);
		while((bytes_read = readLn(queue_fd, buffer, BUFFER_SIZE)) > 0)
		{
			//pid transform filenameoriginal filenamedestino filtro0 filtro1 ...\n
			size = parse(parsed, buffer, s->num_filters, " ");
			printf("\rRequest received (pid: %s).\n", parsed[0]);

			if(canRun(s, parsed))
				addTask(s, parsed, task_num);		//update Status (add task)
			else
				sleep(1); //?

			//server -> Controller -> filho(ffmpeg)
			if((pid = fork()) == 0)
			{
				pid_cliente = atol(parsed[0]);
				//TODO: encontrar o indice do filtro. Não sei como aplicar o filtro
				pid = myexec(0, 1, "ffmpeg", parsed);

				kill(pid_cliente, SIGUSR1); 		//processing
				waitpid(pid, &wstatus, 0);		//waits till ffmpeg finishes...
				if(WIFEXITED(wstatus))
					kill(pid_cliente, SIGUSR1); 	//finished
				else					//or
					kill(pid_cliente, SIGUSR2);	//error

				removeTask(s, parsed, task_num);		//update Status (remove task)
				_exit(0);				//exits
			}

			task_num++;
		}
		memset(parsed, 0, s->num_filters * sizeof(char*)); //a lot of memory leaks xd
	}
	return 0;
}


/*
 * AUX FUNCTIONS:
 * (need to be reviewed)
 */

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


/*
 * STATUS FUNCTIONS:
 * (need to be reviewed)
 */

STATUS newStatus()
{
	STATUS r = calloc(1, sizeof(struct status));
	r -> pid_server = getpid();
	//TODO: alocar espaço para os arrays
	return r;
}

STATUS readStatus(STATUS s, char* conf_filepath)
{
	int conf_fd = open(conf_filepath, O_RDONLY | O_EXCL);	//open conf

	int bytes_read = 0;
	char buffer[BUFFER_SIZE], parsed[BUFFER_SIZE][BUFFER_SIZE];
	for(int i = 0; bytes_read > 0; i++)
	{
		bytes_read = readLn(conf_fd, buffer, BUFFER_SIZE);
		s -> num_filters ++;

		//conf:
		//filtro filtrotraduzido max
		parse(parsed, buffer, 1024, " ");
		s -> filters[i] = strdup(parsed[0]);
		s -> filtersT[i] = strdup(parsed[1]);
		s -> max[i] = atol(parsed[2]);

	}

	return s;
}

STATUS addTask(STATUS s, char** task, int task_number){
	for (int i=3; task[i] != NULL; i++){
		for(int j =0; j < s->num_filters; j++){
			if (strcmp(task[i], s ->filters[j])) {
				s->running[i]++;
			}
		}
	}
	char *c = malloc(BUFFER_SIZE);
	for (int i=1; task[i] != NULL; i++){
		strcat(c, task[i]);
	}
	s->tasks[task_number] = strdup(c);
	free(c);
	return s;
}

int canRun(STATUS s, char** task){
	for (int i=3; task[i] != NULL; i++){
		for(int j =0; j < s->num_filters; j++){
			if (strcmp(task[i], s ->filters[j]) && s->running[j] +1 > s->max[j]) return 0;
		}
				
	}
	return 1;
}

STATUS removeTask(STATUS s, char** task, int task_number){
	for (int i=3; task[i] != NULL; i++){
		for(int j =0; j < s->num_filters; j++){
			if (strcmp(task[i], s ->filters[j])) {
				s->running[i]--;
			}
		}
	}
	s->tasks[task_number] = NULL;
	return s;
}

