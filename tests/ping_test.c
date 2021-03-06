#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <signal.h>
#include <arpa/inet.h>

#include "libknet.h"

static int knet_sock[2];
static knet_handle_t knet_h;

static in_port_t tok_inport(char *str)
{
	int value = atoi(str);

	if ((value < 0) || (value > UINT16_MAX))
		return 0;

	return (in_port_t) value;
}

static int tok_inaddrport(char *str, struct sockaddr_in *addr)
{
	char *strhost, *strport, *tmp = NULL;

	strhost = strtok_r(str, ":", &tmp);
	strport = strtok_r(NULL, ":", &tmp);

	if (strport == NULL)
		addr->sin_port = htons(KNET_RING_DEFPORT);
	else
		addr->sin_port = htons(tok_inport(strport));

	return inet_aton(strhost, &addr->sin_addr);
}

static void print_usage(char *name)
{
	printf("usage: %s <localip>[:<port>] <remoteip>[:port] [...]\n", name);
	printf("example: %s 0.0.0.0 192.168.0.2\n", name);
}

static void argv_to_hosts(int argc, char *argv[])
{
	int err, i;
	struct sockaddr_in *address;
	struct knet_host *host;
	struct knet_listener *listener;

	listener = malloc(sizeof(struct knet_listener));

	if (listener == NULL) {
		printf("Unable to create listener\n");
		exit(EXIT_FAILURE);
	}

	memset(listener, 0, sizeof(struct knet_listener));

	address = (struct sockaddr_in *) &listener->address;

	address->sin_family = AF_INET;
	err = tok_inaddrport(argv[1], address);

	if (err < 0) {
		printf("Unable to convert ip address: %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	err = knet_listener_add(knet_h, listener);

	if (err != 0) {
		printf("Unable to start knet listener\n");
		exit(EXIT_FAILURE);
	}

	for (i = 2; i < argc; i++) {
		if (knet_host_add(knet_h, i - 1) != 0) {
			printf("Unable to add new knet_host\n");
			exit(EXIT_FAILURE);
		}

		knet_host_get(knet_h, i - 1, &host);

		host->link[0].sock = listener->sock;
		host->link[0].address.ss_family = AF_INET;

		knet_link_timeout(&host->link[0], 1000, 5000, 2048);

		host->link[0].ready = 1;

		err = tok_inaddrport(argv[i],
				(struct sockaddr_in *) &host->link[0].address);

		if (err < 0) {
			printf("Unable to convert ip address: %s", argv[i]);
			exit(EXIT_FAILURE);
		}

		knet_host_release(knet_h, &host);
	}
}

/* Testing the latency/timeout:
 *   # tc qdisc add dev lo root handle 1:0 netem delay 1s limit 1000
 *   # tc -d qdisc show dev lo
 *   # tc qdisc del dev lo root
 */
static int print_link(knet_handle_t khandle, struct knet_host *host, struct knet_host_search *data)
{
	int i;

	for (i = 0; i < KNET_MAX_LINK; i++) {
		if (host->link[i].ready != 1) continue;

		printf("host %p, link %p latency is %llums, status: %s\n",
			host, &host->link[i], host->link[i].latency,
			(host->link[i].enabled == 0) ? "disabled" : "enabled");
	}

	return KNET_HOST_FOREACH_NEXT;
}

static void sigint_handler(int signum)
{
	int err;

	printf("Cleaning up...\n");

	if (knet_h != NULL) {
		err = knet_handle_free(knet_h);

		if (err != 0) {
			printf("Unable to cleanup before exit\n");
			exit(EXIT_FAILURE);
		}
	}

	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	char buff[1024];
	size_t len;
	fd_set rfds;
	struct timeval tv;
	struct knet_host_search print_search;

	if (argc < 3) {
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, IPPROTO_IP, knet_sock) != 0) {
		printf("Unable to create socket\n");
		exit(EXIT_FAILURE);
	}

	knet_h = NULL;

	if (signal(SIGINT, sigint_handler) == SIG_ERR) {
		printf("Unable to configure SIGINT handler\n");
		exit(EXIT_FAILURE);
	}

	if ((knet_h = knet_handle_new(knet_sock[0], 1)) == NULL) {
		printf("Unable to create new knet_handle_t\n");
		exit(EXIT_FAILURE);
	}
	
	argv_to_hosts(argc, argv);
	knet_handle_setfwd(knet_h, 1);	

	while (1) {
		knet_host_foreach(knet_h, print_link, &print_search);

		printf("Sending 'Hello World!' frame\n");
		write(knet_sock[1], "Hello World!", 13);

		tv.tv_sec = 5;
		tv.tv_usec = 0;

 select_loop:
		FD_ZERO(&rfds);
		FD_SET(knet_sock[1], &rfds);

		len = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);

		/* uncomment this to replicate the one-message problem */
		/* usleep(500000); */

		if (len < 0) {
			printf("Unable select over knet_handle_t\n");
			exit(EXIT_FAILURE);
		} else if (FD_ISSET(knet_sock[1], &rfds)) {
			len = read(knet_sock[1], buff, sizeof(buff));
			printf("Received data (%zu bytes): '%s'\n", len, buff);
		}

		if ((tv.tv_sec > 0) || (tv.tv_usec > 0))
			goto select_loop;
	}

	/* FIXME: allocated hosts should be free'd */

	return 0;
}
