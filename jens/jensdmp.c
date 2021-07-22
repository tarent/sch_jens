/*-
 * Copyright © 2021
 *	mirabilos <t.glaser@tarent.de>
 * Licensor: Deutsche Telekom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/types.h>
#include "../sch_jens/jens_uapi.h"

#ifndef INFTIM
#define INFTIM (-1)
#endif

#define BIT(n) (1U << (n))

struct tc_jens_relay rbuf[TC_JENS_RELAY_NRECORDS];
#define cbuf ((char *)rbuf)

static void consume(size_t);

static volatile sig_atomic_t do_exit = 0;

/* should be /var/empty but Debian doesn’t have that */
#define CHROOT_TARGETDIR	"/root"
/* nobody user, or a dedicated one */
#define PRIVDROP_UID		65534
#define PRIVDROP_GID		65534

static const gid_t privdrop_gid = PRIVDROP_GID;

static void
drop_privs(void)
{
	/* drop privileges so we stop running as root */
	if (chroot(CHROOT_TARGETDIR))
		err(1, "chroot");
	if (chdir("/"))
		err(1, "chdir");
	if (setgroups(1, &privdrop_gid))
		err(1, "setgroups");
	if (setresgid(PRIVDROP_GID, PRIVDROP_GID, PRIVDROP_GID))
		err(1, "setresgid");
	if (setresuid(PRIVDROP_UID, PRIVDROP_UID, PRIVDROP_UID))
		err(1, "setresuid");
}

static void
sighandler(int signo)
{
	do_exit = signo;
}

static const struct sigaction sa = {
	.sa_handler = &sighandler,
};

int
main(int argc, char *argv[])
{
	int fd;
	size_t n, off = 0;
	struct pollfd pfd;
	int pres;

	printf("<pkt ts=\"1\" time=\"1\" chance=\"0.123456789\" ecn-in=\"00\" ecn-out=\"00\"/>\n");
	printf("<pkt ts=\"20\" time=\"3FFF\" chance=\"1\" slow=\"1\" mark=\"1\" drop=\"1\" ecn-valid=\"1\" ecn-in=\"01\" ecn-out=\"11\"/>\n");
	printf("<qsz ts=\"FFFFFFFFFFFFFFFF\" len=\"FFFFFFFF\"/>\n");
	if (argc > 0) exit(0);

	if (argc != 2)
		errx(255, "Usage: %s /path/to/debugfs/sch_jens/nnnn:0",
		    argc > 0 ? argv[0] : "jensdmp");

#define setup_signal(sig) \
	if (sigaction((sig), &sa, NULL)) \
		err(1, "sigaction: %s", #sig)
	setup_signal(SIGHUP);
	setup_signal(SIGINT);
	setup_signal(SIGPIPE);
	setup_signal(SIGTERM);
#undef setup_signal

	if ((fd = open(argv[1], O_RDONLY)) == -1)
		err(1, "open");

	/* switch away from running as root */
	drop_privs();

 loop:
	if (do_exit)
		goto do_exit;
	pfd.fd = fd;
	pfd.events = POLLIN;
	pres = poll(&pfd, 1, INFTIM);
	if (do_exit)
		goto do_exit;
	if (pres == -1) {
		if (errno == EINTR)
			goto loop;
		err(2, "poll");
	}
	/* pres == 1, (pfd.revents & (POLLIN | POLLRDNORM)) is true */
	if ((n = read(fd, cbuf + off, sizeof(rbuf) - off)) == (size_t)-1) {
		if (errno == EINTR)
			goto loop;
		err(2, "read");
	}
	if (n == 0)
		goto eof;
	off += n;
	printf(" read(%04zX)", n);
	if (off < sizeof(struct tc_jens_relay))
		goto loop;
	printf(" consume(%04zX)\n", off);

	n = 0;
	while ((n + sizeof(struct tc_jens_relay)) <= off) {
		consume(n / sizeof(struct tc_jens_relay));
		n += sizeof(struct tc_jens_relay);
		if (do_exit)
			goto do_exit;
	}
	if (n < off)
		memcpy(cbuf, cbuf + n, off - n);
	off -= n;
	goto loop;

 eof:
	errx(3, "end of input reached");
 do_exit:
	warnx("exiting on signal %d", do_exit);
	close(fd);

	return (0);
}

static void
consume(size_t idx)
{
	printf("%03zX [%llu.%09u] ", idx + 1U,
	    (unsigned long long)(rbuf[idx].ts / 1000000000),
	    (unsigned int)(rbuf[idx].ts % 1000000000));
	switch (rbuf[idx].type) {
	case TC_JENS_RELAY_INVALID:
	default:
		printf("INVALID(%02X)", rbuf[idx].type);
 dump:
		printf(" f(%02X) e(%04X) d(%08X)\n", rbuf[idx].f8,
		    rbuf[idx].e16, rbuf[idx].d32);
		break;

	case TC_JENS_RELAY_PADDING:
		printf("padding");
		goto dump;

	case TC_JENS_RELAY_SOJOURN:
		printf("sojourn");
		printf(" e(%04X) d(%08X)", rbuf[idx].e16, rbuf[idx].d32);/*XXX*/
		printf(" (ECN %d%d→%d%d %svalid%s%s%s)\n",
		    !!(rbuf[idx].f8 & BIT(1)), !!(rbuf[idx].f8 & BIT(0)),
		    !!(rbuf[idx].f8 & BIT(4)), !!(rbuf[idx].f8 & BIT(3)),
		    (rbuf[idx].f8 & BIT(2)) ? "" : "IN",
		    (rbuf[idx].f8 & TC_JENS_RELAY_SOJOURN_SLOW) ? ", slow" : "",
		    (rbuf[idx].f8 & TC_JENS_RELAY_SOJOURN_MARK) ? ", marked" : "",
		    (rbuf[idx].f8 & TC_JENS_RELAY_SOJOURN_DROP) ? ", dropped" : "");
		break;
	}
}