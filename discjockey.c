#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <linux/cdrom.h>

#define DEFAULT_CHILD_COMMAND "rip"

static int daemonize = 1;
static int killified = 0;
static int rescan_delay = 5;
static int max_children = 0;
static int *child_pids = NULL;
static char *child_cmd = NULL;
static char *pidfile = NULL;

int spawn_child(int index, char *device)
{
	char *args[4];
	args[0] = child_cmd;
	args[1] = device;
	args[2] = device + strlen(device) - 1;
	args[3] = NULL;

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
	puts("Version 2.0 - Compiled on " __DATE__);
}

void display_help()
{
	puts("Usage:");
	puts("  discjockey [options] <dev1> ... [devN]");
	puts("");
	puts("Options:");
	puts("  -d <delay>    the number of seconds between checks");
	puts("  -f            don't daemonize");
	puts("  -h            display this help");
	puts("  -p <pidfile>  the name of the pidfile");
	puts("  -r <command>  the command to run upon disc detection");
	puts("  -v            display the version of the code");
}

int process_args(int argc, char **argv)
{
	int c;

	while((c = getopt(argc, argv, "d:fhp:r:v")) != -1)
	{
		switch(c)
		{
			case 'd':
				rescan_delay = atoi(optarg);
				break;
			case 'f':
				daemonize = 0;
				break;
			case 'p':
				pidfile = strdup(optarg);
				if(pidfile == NULL)
				{
					perror("strdup");
					return -1;
				}
				break;
			case 'r':
				child_cmd = strdup(optarg);
				if(child_cmd == NULL)
				{
					perror("strdup");
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
			perror("strdup");
			return -1;
		}
	}

	return optind;
}

void releasemem()
{
	if(pidfile != NULL) free(pidfile);
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
		return abs(c);
	}

	max_children = argc - c;
	if((child_pids = (int *)calloc(max_children, sizeof(int))) == NULL)
	{
		perror("calloc");
		releasemem();
		return 1;
	}

	while(!killified)
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
					if(spawn_child(i, device) != 0)
					{
						return 1;
					}
				}
			}
		}

		sleep(rescan_delay);

		while((status = waitpid(-1, NULL, WNOHANG)) > 0)
		{
			for(i = 0; i < max_children; i++)
			{
				if(child_pids[i] == status)
				{
					child_pids[i] = 0;
					break;
				}
			}

			if(i >= max_children)
			{
				fprintf(stderr, "Notified about a child process (%d) ending that's not on my watch list\n", status);
				return 1;
			}
		}

		if(status < 0 && errno != ECHILD)
		{
			perror("waitpid");
			return 1;
		}
	}

	releasemem();

	return 0;
}
