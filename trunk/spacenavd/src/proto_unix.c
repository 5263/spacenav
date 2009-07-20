#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include "proto_unix.h"
#include "spnavd.h"

enum {
	UEV_TYPE_MOTION,
	UEV_TYPE_PRESS,
	UEV_TYPE_RELEASE
};

static int lsock;

int init_unix(void)
{
	int s;
	mode_t prev_umask;
	struct sockaddr_un addr;

	if(lsock) return 0;

	if((s = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("failed to create socket");
		return -1;
	}

	unlink(SOCK_NAME);	/* in case it already exists */

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_NAME);

	prev_umask = umask(0);

	if(bind(s, (struct sockaddr*)&addr, sizeof addr) == -1) {
		fprintf(stderr, "failed to bind unix socket: %s: %s\n", SOCK_NAME, strerror(errno));
		return -1;
	}

	umask(prev_umask);

	if(listen(s, 8) == -1) {
		perror("listen failed");
		return -1;
	}

	lsock = s;
	return 0;
}

int get_unix_socket(void)
{
	return lsock;
}

void send_uevent(spnav_event *ev, struct client *c)
{
	int i, data[8] = {0};
	float motion_mul;

	if(!lsock) return;

	switch(ev->type) {
	case EVENT_MOTION:
		data[0] = UEV_TYPE_MOTION;

		motion_mul = get_client_sensitivity(c);
		for(i=0; i<6; i++) {
			float val = (float)ev->motion.data[i] * motion_mul;
			data[i + 1] = (int)val;
		}
		data[7] = ev->motion.period;
		break;

	case EVENT_BUTTON:
		data[0] = ev->button.press ? UEV_TYPE_PRESS : UEV_TYPE_RELEASE;
		data[1] = ev->button.bnum;
		break;

	default:
		break;
	}

	while(write(get_client_socket(c), data, sizeof data) == -1 && errno == EINTR);
}

int handle_uevents(fd_set *rset)
{
	struct client *citer;

	if(lsock == -1) {
		return -1;
	}

	if(FD_ISSET(lsock, rset)) {
		/* got an incoming connection */
		int s;

		if((s = accept(lsock, 0, 0)) == -1) {
			perror("error while accepting connection on the UNIX socket");
		} else {
			if(!add_client(CLIENT_UNIX, &s)) {
				perror("failed to add client");
			}
		}
	}

	/* all the UNIX socket clients */
	citer = first_client();
	while(citer) {
		struct client *c = citer;
		citer = next_client();

		if(get_client_type(c) == CLIENT_UNIX) {
			int s = get_client_socket(c);

			if(FD_ISSET(s, rset)) {
				int rdbytes;
				float sens;

				/* got a request from a client, decode and execute it */
				/* XXX currently only sensitivity comes from clients */

				while((rdbytes = read(s, &sens, sizeof sens)) <= 0 && errno == EINTR);
				if(rdbytes <= 0) {	/* something went wrong... disconnect client */
					close(get_client_socket(c));
					remove_client(c);
					continue;
				}

				set_client_sensitivity(c, sens);
			}
		}
	}

	return 0;
}
