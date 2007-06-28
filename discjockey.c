// #include <errno.h>
// #include <fcntl.h>
// #include <getopt.h>
// #include <signal.h>
// #include <stdarg.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <time.h>
// #include <unistd.h>
// #include <sys/stat.h>
// #include <sys/wait.h>

static int rescan_delay = 5;
static int max_children = 0;
static int *child_pids = NULL;
static char *child_cmd = NULL;

int spawn_child(int index, char *device)
{
	char *args[] = { child_cmd, device };

	if((child_pids[index] = fork()) == 0)
	{
		if(execvp(child_cmd, args) == -1)
		{
			perror(child_cmd);
			return 1;
		}
	}

	return 0;
}

void display_version()
{
	puts("$Id$");
}

void display_help()
{
	puts("Usage:");
	puts("  discjockey [options] <dev1> ... [devN]");
	puts("");
	puts("Options:");
	puts("  -d <delay>    the number of seconds between checks");
	puts("  -h            display this help");
	puts("  -r <command>  the command to run upon disc detection");
	puts("  -v            display the version of the code");
}

int process_args(int argc, char **argv)
{
	int c;

	while((c = getopt(argc, argv, "d:hr:v")) != -1)
	{
		switch(c)
		{
			case 'd':
				rescan_delay = atoi(optarg);
				break;
			case 'h':
				child_cmd = strdup(optarg);
				if(child_cmd == NULL)
				{
					perror(NULL);
					return -1;
				}
				break;
			case 'v':
				display_version();
				return 0;
			default:
				display_help();
				return -1;
		}
	}

	if(optind >= argc)
	{
		fputs("No devices specified\n\n", stderr);
		display_help();
		return -1;
	}

	if(rescan_delay < 1)
	{
		fputs("Invalid rescan delay\n", stderr);
		return -1;
	}

	if(child_cmd == NULL)
	{
		child_cmd = strdup(DEFAULT_CHILD_COMMAND);
		if(child_cmd == NULL)
		{
			perror(NULL);
			return -1;
		}
	}

	return optind;
}

void releasemem()
{
	if(child_cmd != NULL)  free(child_cmd);
	if(child_pids != NULL) free(child_pids);
}

int main(int argc, char **argv)
{
	int c, i, fd, status;
	char *device;

	c = process_args(argc, argv);
	if(c < 1)
	{
		releasemem();
		return c;
	}

	max_children = argc - c;
	if((child_pids = (int *)calloc(max_children, sizeof(int))) == NULL)
	{
		perror(NULL);
		releasemem();
		return 1;
	}

	while(1)
	{
		for(i = 0; i < max_children; i++)
		{
			if(child_pids[i] == 0)
			{
				device = argv[c + i];

				if((fd = open(device, O_RDONLY | O_NONBLOCK)) < 0)
				{
					perror(device);
					releasemem();
					return 1;
				}

				status = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);

				close(fd);

				if(status == CDS_DISC_OK)
				{
					spawn_child(i, device);
				}
			}
		}

		sleep(rescan_delay);

		while((status = waitpid(-1, NULL, WNOHANG)) != 0)
		{
			cleanup_child(status);
		}
	}

	releasemem();

	return 0;
}
