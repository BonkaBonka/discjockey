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
#define DEFAULT_OUTPUT_FILE "/dev/null"

static int daemonize = 1;
static int killified = 0;
static int rescan_delay = 5;
static int max_children = 0;
static int *child_pids = NULL;
static char *child_cmd = NULL;
static char *pidfile = NULL;
static char *outfile = DEFAULT_OUTPUT_FILE;

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

void passthrough_handler(int signal)
{
	int i;

	for(i = 0; i < max_children; i++)
	{
		kill(child_pids[i], signal);
	}

	killified = 1;
}

void install_signal_handler()
{
        struct sigaction action;

	/* What about SIGQUIT or SIGHUP? */

        action.sa_flags = 0;
        action.sa_handler = passthrough_handler;
        sigemptyset(&action.sa_mask);
        sigaction(SIGTERM, &action, NULL);

        action.sa_flags = 0;
        action.sa_handler = passthrough_handler;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, NULL);
}

int write_pidfile(char *filename, int pid)
{
	FILE *out;

	if(filename != NULL)
	{
		if((out = fopen(filename, "w+")) == NULL)
		{
			perror("fopen");
			return -1;
		}

		fprintf(out, "%d\n", pid);

		fclose(out);
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

	if(daemonize)
	{
		if(fork() != 0)
		{
			releasemem();
			return 0;
		}

		if(!write_pidfile(pidfile, getpid()))
		{
			releasemem();
			return 1;
		}

		install_signal_handler();

		setsid();

		chdir("/");

		close(0);

		fd = open(outfile, O_WRONLY | O_CREAT);
		if(fd < 0)
		{
			perror("open outfile");
			return -1;
		}

		dup2(fd, 1);
		dup2(fd, 2);

		close(fd);
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
