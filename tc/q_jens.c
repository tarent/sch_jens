/* part of sch_jens (fork of sch_fq_codel), Deutsche Telekom LLCTO */
/* Copyright © 2021, 2022 mirabilos <t.glaser@tarent.de> */

/*
 * Fair Queue Codel
 *
 *  Copyright (C) 2012,2015 Eric Dumazet <edumazet@google.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... jens"
		"	[ limit PACKETS ] [ memory_limit BYTES ] [ drop_batch SIZE ]"
		"\n\t	[ flows NUMBER ] [ quantum BYTES ] [ interval TIME ]"
		"\n\t	[ target TIME ] [ markfree TIME ] [ markfull TIME ]"
		"\n\t	[ subbufs NUMBER ] [ nouseport ] [ fragcache NUMBER ]"
		"\n");
}

static int jens_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			      struct nlmsghdr *n, const char *dev)
{
	unsigned int drop_batch = 0;
	unsigned int limit = 0;
	unsigned int flows = 0;
	unsigned int target = 0;
	unsigned int markfree = ~0U;
	unsigned int markfull = ~0U;
	unsigned int subbufs = ~0U;
	unsigned int fragcache = ~0U;
	unsigned int interval = 0;
	unsigned int quantum = 0;
	unsigned int memory = ~0U;
	unsigned char nouseport = 0;
	struct rtattr *tail;

	while (argc > 0) {
		if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_unsigned(&limit, *argv, 0)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "flows") == 0) {
			NEXT_ARG();
			if (get_unsigned(&flows, *argv, 0)) {
				fprintf(stderr, "Illegal \"flows\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "quantum") == 0) {
			NEXT_ARG();
			if (get_unsigned(&quantum, *argv, 0)) {
				fprintf(stderr, "Illegal \"quantum\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "drop_batch") == 0) {
			NEXT_ARG();
			if (get_unsigned(&drop_batch, *argv, 0)) {
				fprintf(stderr, "Illegal \"drop_batch\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "target") == 0) {
			NEXT_ARG();
			if (get_time(&target, *argv)) {
				fprintf(stderr, "Illegal \"target\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "markfree") == 0) {
			NEXT_ARG();
			if (get_time(&markfree, *argv)) {
				fprintf(stderr, "Illegal \"markfree\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "markfull") == 0) {
			NEXT_ARG();
			if (get_time(&markfull, *argv)) {
				fprintf(stderr, "Illegal \"markfull\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "subbufs") == 0) {
			NEXT_ARG();
			if (get_unsigned(&subbufs, *argv, 0)) {
				fprintf(stderr, "Illegal \"subbufs\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "fragcache") == 0) {
			NEXT_ARG();
			if (get_unsigned(&fragcache, *argv, 0)) {
				fprintf(stderr, "Illegal \"fragcache\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "memory_limit") == 0) {
			NEXT_ARG();
			if (get_size(&memory, *argv)) {
				fprintf(stderr, "Illegal \"memory_limit\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "interval") == 0) {
			NEXT_ARG();
			if (get_time(&interval, *argv)) {
				fprintf(stderr, "Illegal \"interval\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "nouseport") == 0) {
			nouseport = 1;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--; argv++;
	}

	tail = addattr_nest(n, 1024, TCA_OPTIONS);
	if (nouseport)
		addattr_l(n, 1024, TCA_JENS_NOUSEPORT, NULL, 0);
	if (limit)
		addattr_l(n, 1024, TCA_JENS_LIMIT, &limit, sizeof(limit));
	if (flows)
		addattr_l(n, 1024, TCA_JENS_FLOWS, &flows, sizeof(flows));
	if (quantum)
		addattr_l(n, 1024, TCA_JENS_QUANTUM, &quantum, sizeof(quantum));
	if (interval)
		addattr_l(n, 1024, TCA_JENS_INTERVAL, &interval, sizeof(interval));
	if (target)
		addattr_l(n, 1024, TCA_JENS_TARGET, &target, sizeof(target));
	if (markfree != ~0U)
		addattr_l(n, 1024, TCA_JENS_MARKFREE, &markfree, sizeof(markfree));
	if (markfull != ~0U)
		addattr_l(n, 1024, TCA_JENS_MARKFULL, &markfull, sizeof(markfull));
	if (subbufs != ~0U)
		addattr_l(n, 1024, TCA_JENS_SUBBUFS, &subbufs, sizeof(subbufs));
	if (fragcache != ~0U)
		addattr_l(n, 1024, TCA_JENS_FRAGCACHE, &fragcache, sizeof(fragcache));
	if (memory != ~0U)
		addattr_l(n, 1024, TCA_JENS_MEMORY_LIMIT,
			  &memory, sizeof(memory));
	if (drop_batch)
		addattr_l(n, 1024, TCA_JENS_DROP_BATCH_SIZE, &drop_batch, sizeof(drop_batch));

	addattr_nest_end(n, tail);
	return 0;
}

static int jens_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_JENS_MAX + 1];
	unsigned int limit;
	unsigned int flows;
	unsigned int interval;
	unsigned int target;
	unsigned int markfree;
	unsigned int markfull;
	unsigned int subbufs;
	unsigned int fragcache;
	unsigned int quantum;
	unsigned int memory_limit;
	unsigned int drop_batch;

	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_JENS_MAX, opt);

	if (tb[TCA_JENS_LIMIT] &&
	    RTA_PAYLOAD(tb[TCA_JENS_LIMIT]) >= sizeof(__u32)) {
		limit = rta_getattr_u32(tb[TCA_JENS_LIMIT]);
		print_uint(PRINT_ANY, "limit", "limit %u ", limit);
	}
	if (tb[TCA_JENS_MEMORY_LIMIT] &&
	    RTA_PAYLOAD(tb[TCA_JENS_MEMORY_LIMIT]) >= sizeof(__u32)) {
		memory_limit = rta_getattr_u32(tb[TCA_JENS_MEMORY_LIMIT]);
		print_uint(PRINT_JSON, "memory_limit", NULL, memory_limit);
		print_string(PRINT_FP, NULL, "memory_limit %s ",
			     sprint_size(memory_limit, b1));
	}
	if (tb[TCA_JENS_DROP_BATCH_SIZE] &&
	    RTA_PAYLOAD(tb[TCA_JENS_DROP_BATCH_SIZE]) >= sizeof(__u32)) {
		drop_batch = rta_getattr_u32(tb[TCA_JENS_DROP_BATCH_SIZE]);
		if (drop_batch)
			print_uint(PRINT_ANY, "drop_batch", "drop_batch %u ", drop_batch);
	}
	if (tb[TCA_JENS_FLOWS] &&
	    RTA_PAYLOAD(tb[TCA_JENS_FLOWS]) >= sizeof(__u32)) {
		flows = rta_getattr_u32(tb[TCA_JENS_FLOWS]);
		print_uint(PRINT_ANY, "flows", "flows %u ", flows);
	}
	if (tb[TCA_JENS_QUANTUM] &&
	    RTA_PAYLOAD(tb[TCA_JENS_QUANTUM]) >= sizeof(__u32)) {
		quantum = rta_getattr_u32(tb[TCA_JENS_QUANTUM]);
		print_uint(PRINT_ANY, "quantum", "quantum %u ", quantum);
	}
	if (tb[TCA_JENS_INTERVAL] &&
	    RTA_PAYLOAD(tb[TCA_JENS_INTERVAL]) >= sizeof(__u32)) {
		interval = rta_getattr_u32(tb[TCA_JENS_INTERVAL]);
		print_uint(PRINT_JSON, "interval", NULL, interval);
		print_string(PRINT_FP, NULL, "interval %s ",
			     sprint_time(interval, b1));
	}
	if (tb[TCA_JENS_TARGET] &&
	    RTA_PAYLOAD(tb[TCA_JENS_TARGET]) >= sizeof(__u32)) {
		target = rta_getattr_u32(tb[TCA_JENS_TARGET]);
		print_uint(PRINT_JSON, "target", NULL, target);
		print_string(PRINT_FP, NULL, "target %s ",
			     sprint_time(target, b1));
	}
	if (tb[TCA_JENS_MARKFREE] &&
	    RTA_PAYLOAD(tb[TCA_JENS_MARKFREE]) >= sizeof(__u32)) {
		markfree = rta_getattr_u32(tb[TCA_JENS_MARKFREE]);
		print_uint(PRINT_JSON, "markfree", NULL, markfree);
		print_string(PRINT_FP, NULL, "markfree %s ",
			     sprint_time(markfree, b1));
	}
	if (tb[TCA_JENS_MARKFULL] &&
	    RTA_PAYLOAD(tb[TCA_JENS_MARKFULL]) >= sizeof(__u32)) {
		markfull = rta_getattr_u32(tb[TCA_JENS_MARKFULL]);
		print_uint(PRINT_JSON, "markfull", NULL, markfull);
		print_string(PRINT_FP, NULL, "markfull %s ",
			     sprint_time(markfull, b1));
	}
	if (tb[TCA_JENS_SUBBUFS] &&
	    RTA_PAYLOAD(tb[TCA_JENS_SUBBUFS]) >= sizeof(__u32)) {
		subbufs = rta_getattr_u32(tb[TCA_JENS_SUBBUFS]);
		print_uint(PRINT_ANY, "subbufs", "subbufs %u ", subbufs);
	}
	if (tb[TCA_JENS_FRAGCACHE] &&
	    RTA_PAYLOAD(tb[TCA_JENS_FRAGCACHE]) >= sizeof(__u32)) {
		fragcache = rta_getattr_u32(tb[TCA_JENS_FRAGCACHE]);
		print_uint(PRINT_ANY, "fragcache", "fragcache %u ", fragcache);
	}
	if (tb[TCA_JENS_NOUSEPORT]) {
		print_bool(PRINT_ANY, "nouseport", "nouseport ", true);
	}

	return 0;
}

static int jens_print_xstats(struct qdisc_util *qu, FILE *f,
				 struct rtattr *xstats)
{
	struct tc_jens_xstats _st = {}, *st;

	SPRINT_BUF(b1);

	if (xstats == NULL)
		return 0;

	st = RTA_DATA(xstats);
	if (RTA_PAYLOAD(xstats) < sizeof(*st)) {
		memcpy(&_st, st, RTA_PAYLOAD(xstats));
		st = &_st;
	}
	if (st->type == TCA_JENS_XSTATS_QDISC) {
		print_uint(PRINT_ANY, "maxpacket", "  maxpacket %u",
			st->qdisc_stats.maxpacket);
		print_uint(PRINT_ANY, "drop_overlimit", " drop_overlimit %u",
			st->qdisc_stats.drop_overlimit);
		print_uint(PRINT_ANY, "new_flow_count", " new_flow_count %u",
			st->qdisc_stats.new_flow_count);
		print_uint(PRINT_ANY, "ecn_mark", " ecn_mark %u",
			st->qdisc_stats.ecn_mark);
		if (st->qdisc_stats.ce_mark)
			print_uint(PRINT_ANY, "ce_mark", " ce_mark %u",
				st->qdisc_stats.ce_mark);
		if (st->qdisc_stats.memory_usage)
			print_uint(PRINT_ANY, "memory_used", " memory_used %u",
				st->qdisc_stats.memory_usage);
		if (st->qdisc_stats.drop_overmemory)
			print_uint(PRINT_ANY, "drop_overmemory", " drop_overmemory %u",
				st->qdisc_stats.drop_overmemory);
		print_nl();
		print_uint(PRINT_ANY, "new_flows_len", "  new_flows_len %u",
			st->qdisc_stats.new_flows_len);
		print_uint(PRINT_ANY, "old_flows_len", " old_flows_len %u",
			st->qdisc_stats.old_flows_len);
	}
	if (st->type == TCA_JENS_XSTATS_CLASS) {
		print_int(PRINT_ANY, "deficit", "  deficit %d",
			st->class_stats.deficit);
		print_uint(PRINT_ANY, "count", " count %u",
			st->class_stats.count);
		print_uint(PRINT_ANY, "lastcount", " lastcount %u",
			st->class_stats.lastcount);
		print_uint(PRINT_JSON, "ldelay", NULL,
			st->class_stats.ldelay);
		print_string(PRINT_FP, NULL, " ldelay %s",
			sprint_time(st->class_stats.ldelay, b1));
		if (st->class_stats.dropping) {
			print_bool(PRINT_ANY, "dropping", " dropping", true);
			print_int(PRINT_JSON, "drop_next", NULL,
				  st->class_stats.drop_next);
			if (st->class_stats.drop_next < 0)
				print_string(PRINT_FP, NULL, " drop_next -%s",
					sprint_time(-st->class_stats.drop_next, b1));
			else {
				print_string(PRINT_FP, NULL, " drop_next %s",
					sprint_time(st->class_stats.drop_next, b1));
			}
		}
	}
	return 0;

}

struct qdisc_util jens_qdisc_util = {
	.id		= "jens",
	.parse_qopt	= jens_parse_opt,
	.print_qopt	= jens_print_opt,
	.print_xstats	= jens_print_xstats,
};
