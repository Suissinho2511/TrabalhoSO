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
#define QUEUE_NAME "tmp/queue"
#define STATUS_NAME "tmp/status"
#define MAX_FILTERS 32

int readLn(int fd, char *buffer, int size);

ssize_t readln(int fd, char *line, size_t size);

int parse(char **parsed, char *buffer, int size, char *delim);

void freearr(void **pointer, int size);

int findIndex(char **array, char *string, int size);

typedef struct status
{
	int pid_server, num_filters;
	int *max, *running;
	char **filters, **filtersT, **tasks;
} * STATUS;

int run = 1;

STATUS newStatus(char *conf_filepath);

STATUS readStatus(STATUS s, char *conf_filepath);

STATUS addTask(STATUS s, char **task, int task_number);

STATUS removeTask(STATUS s, char **task, int task_number);

int canRun(struct status s, char **task);

void writeStatus(int fd, STATUS s);

int myexec(int in_fd, int out_fd, char **args, int size,STATUS s);

void resetStatus(STATUS s);

STATUS status_clone(STATUS s);

void sigterm_handler(int signum)
{
	run = 0;
}

int main(int argc, char **argv)
{
	signal(SIGTERM, sigterm_handler); //Handling sigterm to slowly close server

	char stdout_buffer[BUFFER_SIZE];
	setvbuf(stdout, stdout_buffer, _IOLBF, BUFFER_SIZE); //stdout to Line Buffered

	//=============================================Files & Variables=================================================//
	
	int status_fd = open(STATUS_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);	
	STATUS s = newStatus(argv[1]);
	writeStatus(status_fd, s);	//escrever o status

	if (mkfifo(QUEUE_NAME, 0666) == -1)
	{ //create fifo queue
		perror("fifo");
		return -1;
	};
	int queue_fd = open(QUEUE_NAME, O_RDONLY); //open fifo queue

	int bytes_read = 0, size, pid, pid_cliente, wstatus, task_num = 0;
	char buffer[BUFFER_SIZE];
	char **parsed = calloc(s->num_filters + 4, sizeof(char *));

	printf("Server Online (pid: %d)!\n", getpid());

	//=====================================================Loop======================================================//

	while (run == 1)
	{
		while ((bytes_read = readln(queue_fd, buffer, BUFFER_SIZE)) > 0 && run == 1)
		{
			//pid transform filenameoriginal filenamedestino filtro0 filtro1 ...\n
			size = parse(parsed, buffer, s->num_filters, " ");
			pid_cliente = atoi(parsed[0]);
			printf("\rRequest received (pid: %s).\n",parsed[0]);
			kill(pid_cliente, SIGUSR1);    // pending
			//STATUS clone = status_clone(s);
			if (run == 0){//canRun((*clone), parsed) == 0){
				printf("\rRequest cannot be handled (pid: %d).\n", pid_cliente);
				kill(pid_cliente, SIGUSR2);
			}
			else
			{
				s = addTask(s, parsed, task_num); //update Status (add task)
				writeStatus(status_fd, s);				//write Status

				//server -> Controller -> filhos(1 para cada filtro) exemplo:guião5 ex5
				if ((pid = fork()) == 0)
				{
					int input_fd = open(parsed[2], O_RDWR | O_EXCL, 0666);
					int output_fd = open(parsed[3], O_RDWR | O_CREAT | O_TRUNC, 0666);
					kill(pid_cliente, SIGUSR1); //processing
					pid = myexec(input_fd, output_fd, parsed, size, s);

					
					waitpid(pid, &wstatus, 0);	//waits till ffmpeg finishes...
					if (WIFEXITED(wstatus))
						kill(pid_cliente, SIGUSR1); //finished
					else							//or
						kill(pid_cliente, SIGUSR2); //error

					//s = removeTask(s, parsed, task_num);		//update Status (remove task)
					_exit(0); //exits
				}

				task_num++;
				//freearr((void **)parsed, size);
				memset(parsed, 0, size * sizeof(char *));
			}
			//freeStatus(clone);	
		}
		resetStatus(s);
		writeStatus(status_fd, s);
	}
	printf("Closing server...\n");
	unlink(STATUS_NAME);
	close(queue_fd);
	unlink(QUEUE_NAME);
	printf("Server closed!\n");

	return 0;
}

/*
 * AUX FUNCTIONS:
 * (need to be reviewed)
 */

int readLn(int fd, char *buffer, int size)
{
	int bytes_read = 0;
	while ((bytes_read += read(fd, buffer + bytes_read, 1)) > 0 && bytes_read < size)
	{
		if (buffer[bytes_read] == '\n')
			break;
	}
	buffer[bytes_read] = '\0';

	return bytes_read;
}

ssize_t readln(int fd, char *line, size_t size)
{
	ssize_t i = 0;
	while (i < size - 1)
	{
		ssize_t bytes_read = read(fd, &line[i], 1);
		if (bytes_read < 1)
			break;
		if (line[i++] == '\n')
			break;
	}
	line[i] = 0;
	return i;
}

int parse(char **parsed, char *buffer, int size, char *delim)
{
	int i;
	char *token = strtok(buffer, delim);
	for (i = 0;; i++)
	{
		parsed[i] = strdup(token);
		token = strtok(NULL, delim);
		if (token == NULL)
		{
			parsed[i + 1] = NULL;
			break;
		}
	}

	return i;
}

void freearr(void **pointer, int size)
{
	for (int i = 0; i < size; i++)
		free(pointer[i]);
	//free(pointer);
	return;
}

int myexec(int in_fd, int out_fd, char **args, int size,STATUS s)
{
	int pid;
	char bin_name[BUFFER_SIZE];

	if ((pid = fork()) == 0)
	{

		int x = 4, num = size-4, end, i;
		int pd[num - 1][2];
		if(size==5)
		{
			if((pid=fork()) == 0)
			{
				dup2(in_fd, 0);
				dup2(out_fd,1);
				sprintf(bin_name, "bin/aurrasd-filters/%s",s->filtersT[findIndex(s->filters, args[4], s->num_filters)]);
				execl(bin_name, bin_name, NULL);
			}
			return pid;
		}
		for (i = 0; i < num; i++)
		{
			if (i == 0)
			{
				pipe(pd[i]);
				if (fork() == 0)
				{
					dup2(pd[i][0], 0);
					close(pd[i][0]);
					dup2(in_fd, 0);
					dup2(pd[i][1], 1);
					close(pd[i][1]);
					sprintf(bin_name, "bin/aurrasd-filters/%s", s->filtersT[findIndex(s->filters, args[x], s->num_filters)]);
					execl(bin_name, bin_name, NULL);
					_exit(0);
				}
				else
					close(pd[i][1]);
			}

			else if (i == num - 1)
			{
				if (fork() == 0)
	
				{
					dup2(pd[i-1][1], 1);
					close(pd[i-1][1]);
					dup2(pd[i-1][0], 0);
					close(pd[i-1][0]);
					dup2(out_fd, 1);
					sprintf(bin_name, "bin/aurrasd-filters/%s", s->filtersT[findIndex(s->filters, args[x], s->num_filters)]);
					execl(bin_name, bin_name, NULL);
					_exit(0);
				}
				else
					close(pd[i][0]);
			}
			else
			{
				pipe(pd[i]);
				if (fork() == 0)
				{
					dup2(pd[i][1], 1);
					close(pd[i][1]);
					dup2(pd[i - 1][0], 0);
					close(pd[i - 1][0]);
					sprintf(bin_name, "bin/aurrasd-filters/%s", s->filtersT[findIndex(s->filters, args[x], s->num_filters)]);
					execl(bin_name, bin_name, NULL);
					_exit(0);
				}
				else
				{
					close(pd[i - 1][0]);
					close(pd[i][1]);
				}
			}
			x++;
		}
		for (end = 0; end < i; end++)
		{
			wait(NULL);
		}
	}
	return pid;
}

int findIndex(char **array, char *string, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		if (strcmp(array[i], string) == 0)
			return i;
	}
	return -1;
}

/*
 * STATUS FUNCTIONS:
 * (need to be reviewed)
 */

STATUS newStatus(char* conf_filepath)
{
    int conf_fd = open(conf_filepath, O_RDONLY | O_EXCL);
    int i, bytes_read = 0, max_tasks, max_filter;
    char buffer[BUFFER_SIZE];
    char **parsed = malloc(sizeof(char *) * 3);

    STATUS r = calloc(1, sizeof(struct status));
    
    r -> pid_server =     getpid();
    r -> filters     =    malloc(sizeof(char *)     * MAX_FILTERS);
    r -> filtersT     =    malloc(sizeof(char *)     * MAX_FILTERS);
    r -> max         =    malloc(sizeof(int)         * MAX_FILTERS);
    r -> running     =    malloc(sizeof(int)         * MAX_FILTERS);

    for (i = 0;(bytes_read = readln(conf_fd, buffer, BUFFER_SIZE)) > 0; i++)
    {
        //filtro filtrotraduzido max
        parse(parsed, buffer, 3, " ");
        r -> filters[i] = strdup(parsed[0]);
        r -> filtersT[i] = strdup(parsed[1]);
        max_filter = atoi(parsed[2]);
        max_tasks += max_filter;
        r -> max[i] = max_filter;
        r -> running[i] = 0;
    }

    r -> num_filters = i;
    r -> tasks = malloc(sizeof(char *) * (max_tasks + 1));
    return r;
}

STATUS readStatus(STATUS s, char *conf_filepath)
{
	int conf_fd = open(conf_filepath, O_RDONLY | O_EXCL); //open conf

	int bytes_read = 0;
	char buffer[BUFFER_SIZE];
	char **parsed = malloc(sizeof(char *) * 20);
	for (int i = 0; (bytes_read = readln(conf_fd, buffer, BUFFER_SIZE)) > 0; i++)
	{
		s->num_filters++;

		//conf:
		//filtro filtrotraduzido max
		parse(parsed, buffer, 1024, " ");
		s->filters[i] = strdup(parsed[0]);
		s->filtersT[i] = strdup(parsed[1]);
		s->max[i] = atoi(parsed[2]);
		s->running[i] = 0;
	}

	return s;
}

STATUS addTask(STATUS s, char **task, int task_number)
{
	char c[BUFFER_SIZE] = "";
	for (int i = 1; task[i] != NULL; i++)
	{
		if (i > 3){
			int index_filtro = findIndex(s->filters, task[i], s->num_filters);
			if (index_filtro != -1) s->running[index_filtro]++;
		}
		sprintf(c, "%s %s", c, task[i]);
	}
	s->tasks[task_number] = strdup(c);
	return s;
}

int canRun(struct status s, char **task)
{
	for (int i = 4; task[i] != NULL; i++)
	{
		int index_filtro = findIndex(s.filters, task[i], s.num_filters);
		if (s.running[index_filtro]++ > s.max[index_filtro])
			return 0;
	}
	return 1;
}

STATUS removeTask(STATUS s, char **task, int task_number)
{
	for (int i = 4; task[i] != NULL; i++)
	{
		for (int j = 0; j < s->num_filters; j++)
		{
			if (strcmp(task[i], s->filters[j]) == 0)
			{
				s->running[j]--;
			}
		}
	}
	s->tasks[task_number] = " ";
	return s;
}

void writeStatus(int fd, STATUS s)
{
	lseek(fd, SEEK_SET, 0);
	char c[BUFFER_SIZE] = "";
	for (int i = 0; s->tasks[i] != NULL; i++)
	{
		sprintf(c, "%sTask #%d %s\n", c, i, s->tasks[i]);
	}
	for (int i = 0; i < s->num_filters; i++)
	{
		sprintf(c, "%sfilter %s: %d/%d (running/max)\n", c, s->filters[i], s->running[i], s->max[i]);
	}
	write(fd, c, strlen(c));
}

void resetStatus(STATUS s){
    for (int i = 0; i < s->num_filters; i++)
    {
        s -> running[i] = 0;
    }
    //memset(s->tasks, 0, s->num_filters * sizeof(char*));
}

STATUS status_clone(STATUS s)
{
	STATUS clone = malloc(sizeof(struct status));
	clone->tasks = malloc(sizeof(char *) * 25);
	clone->filters = malloc(sizeof(char *) * 6);
	clone->filtersT = malloc(sizeof(char *) * 6);
	clone->max = malloc(sizeof(int *));
	clone->running = malloc(sizeof(int *));
	for (int i = 0; i < s->num_filters; i++)
	{
		clone->filters[i] = strdup(s->filters[i]);
		clone->filtersT[i] = strdup(s->filtersT[i]);
		clone->max[i] = s->max[i];
		clone->running[i] = s->running[i];
		if (s->tasks[i] != NULL)
			clone->tasks[i] = strdup(s->tasks[i]);
	}
	return clone;
}