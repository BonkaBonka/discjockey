#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stropts.h>
#include <unistd.h>
#include <linux/cdrom.h>

void display_version()
{
	puts("Version 1.0 - Compiled on " __DATE__);
}

void display_help()
{
	puts("Usage:");
	puts("  disctype [options] <dev>");
	puts("");
	puts("Options:");
	puts("  -h  display this help");
	puts("  -v  display the version of the code");
}

int process_args(int argc, char **argv)
{
	int c;

	while((c = getopt(argc, argv, "hv")) != -1)
	{
		switch(c)
		{
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

	status = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);

	close(fd);

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

	return 0;
}
