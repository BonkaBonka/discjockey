#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/cdrom.h>

static int rescan_delay = 5;
static int quiet = 0;
static char* pidfile = NULL;

void term_handler(int signal)
{
	if(pidfile && unlink(pidfile) == -1)
	{
		perror(pidfile);
	}

        exit(2);
}

int write_pidfile(char *filename, int pid)
{
	FILE* out;

	if((out = fopen(filename, "w")) == NULL)
	{
		perror(filename);
		return -1;
	}

	fprintf(out, "%d\n", pid);

	fclose(out);

	return 0;
}

void display_version()
{
	puts("Version 1.0 - Compiled on " __DATE__);
}

void display_help()
{
	puts("Usage:");
	puts("  discwait [options] <dev>");
	puts("");
	puts("Options:");
	puts("  -d <delay>  the number of seconds between checks");
	puts("  -h          display this help");
	puts("  -p <name>   write process ID to the given filename");
	puts("  -q          don't display disc type");
	puts("  -v          display the version of the code");
}

int process_args(int argc, char **argv)
{
	int c;

	while((c = getopt(argc, argv, "d:hp:qv")) != -1)
	{
		switch(c)
		{
			case 'd':
				rescan_delay = atoi(optarg);
				break;
			case 'p':
				pidfile = optarg;
				break;
			case 'q':
				quiet = 1;
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

	if(argc > optind + 1)
	{
		fputs("Too many devices specified\n\n", stderr);
		display_help();
		return -1;
	}

	return optind;
}

int main(int argc, char **argv)
{
	int c, fd, status;
	char *device;
        struct sigaction action;

	c = process_args(argc, argv);
	if(c < 1)
	{
		return c;
	}

	device = argv[c];

	if((fd = open(device, O_RDONLY | O_NONBLOCK)) < 0)
	{
		perror(device);
		return 1;
	}

	if(pidfile && write_pidfile(pidfile, getpid()) == -1)
	{
		return 1;
	}

        action.sa_flags = 0;
        action.sa_handler = term_handler;
        sigemptyset(&action.sa_mask);
        sigaction(SIGTERM, &action, NULL);

	while(ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT) != CDS_DISC_OK)
	{
		sleep(rescan_delay);
	}

	if(!quiet)
	{
		status = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);
		switch(status)
		{
			case CDS_AUDIO:
				puts("audio");
				break;
			case CDS_DATA_1:
			case CDS_DATA_2:
				puts("data");
				break;
			case CDS_MIXED:
				puts("mixed");
				break;
			default:
				puts("unknown");
		}
	}

	close(fd);

	return 0;
}
