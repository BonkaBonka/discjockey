#include <linux/cdrom.h>

void display_version()
{
	puts("$Id$");
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
	int fd, status;
	char *device;

	c = process_args(argc, argv);
	if(c < 1)
	{
		return c;
	}

	device = argv[c];

	return 0;
}
