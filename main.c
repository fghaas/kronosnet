#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "cfg.h"
#include "vty.h"
#include "utils.h"

#define LOCKFILE_NAME RUNDIR PACKAGE ".pid"

#define OPTION_STRING "hdfVc:b:p:"

static int daemonize = 1;

struct knet_cfg_top knet_cfg_head;

extern int utils_debug;

static void print_usage(void)
{
	printf("Usage:\n\n");
	printf(PACKAGE " [options]\n\n");
	printf("Options:\n\n");
	printf("  -b <ip_addr> Bind management VTY to ip_addr (default: all)\n");
	printf("  -p <port>    Bind management VTY to port (default %d)\n",
		KNET_VTY_DEFAULT_PORT);
	printf("  -c <file>    Use config file (default "DEFAULT_CONFIG_FILE")\n");
	printf("  -f           Do not fork in background\n");
	printf("  -d           Enable debugging output\n");
	printf("  -h           This help\n");
	printf("  -V           Print program version information\n");
	return;
}

static int read_arguments(int argc, char **argv)
{
	int cont = 1;
	int optchar;
	int int_port;

	while (cont) {
		optchar = getopt(argc, argv, OPTION_STRING);

		switch (optchar) {

		case 'b':
			knet_cfg_head.vty_ip = strdup(optarg);
			if (!knet_cfg_head.vty_ip)
				return -1;
			break;

		case 'p':
			int_port = atoi(optarg);
			if ((int_port < 0) || (int_port > 65535)) {
				errno = EINVAL;
				return -1;
			}
			knet_cfg_head.vty_port = strdup(optarg);
			if (!knet_cfg_head.vty_port)
				return -1;
			break;

		case 'c':
			knet_cfg_head.conffile = strdup(optarg);
			if (!knet_cfg_head.conffile)
				return -1;
			break;

		case 'd':
			utils_debug = 1;
			break;

		case 'f':
			daemonize = 0;
			break;

		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
			break;

		case 'V':
			printf(PACKAGE " " PACKAGE_VERSION " (built " __DATE__
			       " " __TIME__ ")\n");
			exit(EXIT_SUCCESS);
			break;

		case EOF:
			cont = 0;
			break;

		default:
			fprintf(stderr, "unknown option: %c\n", optchar);
			print_usage();
			exit(EXIT_FAILURE);
			break;

		}
	}
	return 0;
}

static int set_scheduler(void)
{
	struct sched_param sched_param;
	int err;

	err = sched_get_priority_max(SCHED_RR);
	if (err < 0) {
		log_error("Could not get maximum scheduler priority");
		return err;
	}

	sched_param.sched_priority = err;
	err = sched_setscheduler(0, SCHED_RR, &sched_param);
	if (err < 0)
		log_error("could not set SCHED_RR priority %d",
			   sched_param.sched_priority);

	return err;
}

static void remove_lockfile(void)
{
	unlink(LOCKFILE_NAME);
}

static int create_lockfile(const char *lockfile)
{
	int fd, value;
	size_t bufferlen;
	ssize_t write_out;
	struct flock lock;
	char buffer[50];

	if ((fd = open(lockfile, O_CREAT | O_WRONLY,
		       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))) < 0) {
		fprintf(stderr, "Cannot open lockfile [%s], error was [%s]\n",
			lockfile, strerror(errno));
		return -1;
	}

	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

retry_fcntl:

	if (fcntl(fd, F_SETLK, &lock) < 0) {
		switch (errno) {
		case EINTR:
			goto retry_fcntl;
			break;
		case EACCES:
		case EAGAIN:
			fprintf(stderr, "Cannot lock lockfile [%s], error was [%s]\n",
				lockfile, strerror(errno));
			break;
		default:
			fprintf(stderr, "process is already running\n");
		}

		goto fail_close;
	}

	if (ftruncate(fd, 0) < 0) {
		fprintf(stderr, "Cannot truncate pidfile [%s], error was [%s]\n",
			lockfile, strerror(errno));

		goto fail_close_unlink;
	}

	memset(buffer, 0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer)-1, "%u\n", getpid());

	bufferlen = strlen(buffer);
	write_out = write(fd, buffer, bufferlen);

	if ((write_out < 0) || (write_out == 0 && errno)) {
		fprintf(stderr, "Cannot write pid to pidfile [%s], error was [%s]\n",
			lockfile, strerror(errno));

		goto fail_close_unlink;
	}

	if ((write_out == 0) || (write_out < bufferlen)) {
		fprintf(stderr, "Cannot write pid to pidfile [%s], shortwrite of"
				"[%zu] bytes, expected [%zu]\n",
				lockfile, write_out, bufferlen);

		goto fail_close_unlink;
	}

	if ((value = fcntl(fd, F_GETFD, 0)) < 0) {
		fprintf(stderr, "Cannot get close-on-exec flag from pidfile [%s], "
				"error was [%s]\n", lockfile, strerror(errno));

		goto fail_close_unlink;
	}
	value |= FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, value) < 0) {
		fprintf(stderr, "Cannot set close-on-exec flag from pidfile [%s], "
				"error was [%s]\n", lockfile, strerror(errno));

		goto fail_close_unlink;
	}

	atexit(remove_lockfile);

	return 0;

fail_close_unlink:
	if (unlink(lockfile))
		fprintf(stderr, "Unable to unlink %s\n", lockfile);

fail_close:
	if (close(fd))
		fprintf(stderr, "Unable to close %s file descriptor\n", lockfile);
	return -1;
}

static void set_cfg_defaults(void)
{
	if (!knet_cfg_head.conffile)
		knet_cfg_head.conffile = strdup(DEFAULT_CONFIG_FILE);
	if (!knet_cfg_head.conffile) {
		fprintf(stderr, "Unable to allocate memory for config file");
		exit(EXIT_FAILURE);
	}

	if (!knet_cfg_head.vty_ip)
		knet_cfg_head.vty_ip = strdup("::");
	if (!knet_cfg_head.vty_ip) {
		fprintf(stderr, "Unable to allocate memory for default ip address");
		exit(EXIT_FAILURE);
	}

	if (!knet_cfg_head.vty_port) {
		char portbuf[8];
		memset(&portbuf, 0, sizeof(portbuf));
		snprintf(portbuf, sizeof(portbuf), "%d", KNET_VTY_DEFAULT_PORT);
		knet_cfg_head.vty_port = strdup(portbuf);
	}
	if (!knet_cfg_head.vty_port) {
		fprintf(stderr, "Unable to allocate memory for default port address");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	int err;

	memset(&knet_cfg_head, 0, sizeof(struct knet_cfg_top));

	utils_syslog=1;

	if (create_lockfile(LOCKFILE_NAME) < 0) {
		fprintf(stderr, "Unable to create lockfile");
		exit(EXIT_FAILURE);
	}

	if (read_arguments(argc, argv) < 0) {
		fprintf(stderr, "Unable to parse options");
		exit(EXIT_FAILURE);
	}

	set_cfg_defaults();

	if (daemonize) {
		if (daemon(0, 0) < 0) {
			perror("Unable to daemonize");
			exit(EXIT_FAILURE);
		}
	}

	log_info(PACKAGE " version " VERSION);

	err = set_scheduler();
	if (err < 0)
		goto out;

	if (knet_vty_main_loop() < 0)
		log_error("Detected fatal error in main loop");

out:
	if (knet_cfg_head.conffile)
		free(knet_cfg_head.conffile);
	if (knet_cfg_head.vty_ip)
		free(knet_cfg_head.vty_ip);
	if (knet_cfg_head.vty_port)
		free(knet_cfg_head.vty_port);

	return 0;
}
