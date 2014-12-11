/*** ttl-wc.c -- count stuff in ttl files
 *
 * Copyright (C) 2012-2014 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of rdfsnips.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "nifty.h"

#if !defined MAP_ANON && defined MAP_ANONYMOUS
# define MAP_ANON	MAP_ANONYMOUS
#elif !defined MAP_ANON
# define MAP_ANON	(0x1000U)
#endif	/* !MAP_ANON */
#define PROT_RW		(PROT_READ | PROT_WRITE)
#define MAP_MEM		(MAP_PRIVATE | MAP_ANON)

#define assert(x...)

static size_t nsub;
static size_t npre;
static size_t nobj;


/* helpers */
static void*
resz(void *buf, size_t old, size_t new)
{
	void *nub = mmap(NULL, new, PROT_RW, MAP_MEM, -1, 0);

	if (UNLIKELY(nub == MAP_FAILED)) {
		return NULL;
	}
	(void)memcpy(nub, buf, old);
	return nub;
}

#define RESZ(_b, _oz, _nz)						\
	size_t _nuz_ = _nz;						\
	void *_nub_ = resz(_b, _oz, _nuz_);				\
	if (LIKELY(_nub_ != NULL)) {					\
		_b = _nub_;						\
		_oz = _nuz_;						\
	}

static inline bool
escapedp(const char *sp, const char *bp)
{
/* find out if sp is backslash-escaped
 * but backslash-escaped backslashes won't count,
 * so make sure the number of backslashes before sp is odd */
	int bksl = 0;

	while (sp-- > bp && *sp == '\\') {
		bksl ^= 1;
	}
	return bksl;
}


/* the actual counting */
static void
init_proc(void)
{
	nsub = 0U;
	npre = 0U;
	nobj = 0U;
	return;
}

static ssize_t
proc(const char *buf, size_t bsz, bool only_subs_p)
{
	const char *sp = buf;
	const char *const UNUSED(ep) = buf + bsz;
	enum {
		FREE,
		IN_ANGLES,
		IN_QUOTES,
		IN_LONG_QUOTES,
		IN_COMMENT,
	} st = FREE;

#define fini_proc()	proc(NULL, 0U, false)
	if (UNLIKELY(bsz == 0U)) {
		/* and finalise */
		st = FREE;
		return 0;
	}

next:
	/* overread whitespace */
	for (; *sp > '\0' && *sp <= ' '; sp++);

	/* statements are always concluded by . so look for that guy */
	for (const char *eo, *tp = sp;
	     ({
		     switch (st) {
		     case FREE:
			     if (only_subs_p) {
				     eo = strpbrk(tp, ".<\"#");
			     } else {
				     eo = strpbrk(tp, ".,;<\"#");
			     }
			     break;
		     case IN_ANGLES:
			     eo = strchr(tp, '>');
			     break;
		     case IN_QUOTES:
			     eo = strchr(tp, '"');
			     break;
		     case IN_LONG_QUOTES:
			     eo = strstr(tp, "\"\"\"");
			     break;
		     case IN_COMMENT:
			     eo = strchr(tp, '\n');
			     break;
		     default:
			     /* fuck */
			     eo = NULL;
			     break;
		     }
		     eo;
	     }) != NULL; tp = eo) {
		/* check character at point */
		switch (*eo++) {
		case '.':
			if (LIKELY(*sp != '@')) {
				/* don't count directives */
				nsub++;
				npre++;
				nobj++;
			}
			sp = eo;
			goto next;
		case ';':
			npre++;
			nobj++;
			break;
		case ',':
			nobj++;
			break;
		case '<':
			/* find matching > */
			st = IN_ANGLES;
			break;
		case '>':
			/* yay, go back to free scan */
			st = FREE;
			break;
		case '"':
			/* skip this occurrence if it's an escaped " */
			if (LIKELY(!escapedp(eo - 1, tp))) {
				switch (st) {
				case FREE:
					/* check if it's a long quote (""") */
					if (!eo[0U]) {
						goto out;
					} else if (eo[0U] != '"') {
						st = IN_QUOTES;
					} else if (!eo[1U]) {
						goto out;
					} else if (eo[1U] != '"') {
						/* oh brill, we're through
						 * already ... don't change
						 * the state */
						eo++;
					} else {
						st = IN_LONG_QUOTES;
						eo += 2U;
					}
					break;
				case IN_QUOTES:
					st = FREE;
					break;
				case IN_LONG_QUOTES:
					eo += 2U;
					st = FREE;
					break;
				default:
					/* huh? */
					break;
				}
			}
			break;
		case '#':
			assert(st == FREE);
			st = IN_COMMENT;
			break;
		case '\n':
			assert(st == IN_COMMENT);
			st = FREE;
			break;
		default:
			break;
		}
	}
out:
	return sp - buf;
}

static int
count1(const char *fn, bool only_subs_p)
{
	static char _buf[4096U];
	char *buf = _buf;
	size_t bsz = sizeof(_buf);
	size_t bix;
	int fd;

	if (fn == NULL) {
		fd = STDIN_FILENO;
	} else if ((fd = open(fn, O_RDONLY)) < 0) {
		return -1;
	}
	/* initialise counters */
	init_proc();
	/* read into buf */
	bix = 0U;
	for (ssize_t nrd, npr;
	     (nrd = read(fd, buf + bix, bsz - bix - 1U/*\nul*/)) > 0;) {
		/* mark the end of the buffer */
		buf[bix += nrd] = '\0';
		if ((npr = proc(buf, bix, only_subs_p)) < 0) {
			goto fuck;
		} else if (npr == 0 && bix + 1 >= bsz) {
			/* need a bigger buffer */
			RESZ(buf, bsz, bsz << 1U)
			else {
				goto fuck;
			}
		} else if (npr == 0) {
			/* just read some more */
			;
		} else if ((bix -= npr) > 0) {
			/* memmove to the front */
			memmove(buf, buf + npr, bix);
		}
	}
	/* finalise buffer again, just in case */
	buf[bix] = '\0';
	/* last try, we don't care how much gets processed */
	(void)proc(buf, bix, only_subs_p);
	/* finalise processing */
	fini_proc();

fuck:
	/* resource freeing */
	close(fd);
	if (buf != _buf) {
		munmap(buf, bsz);
	}
	return 0;
}


#include "ttl-wc.yucc"

static void
pr_counts(yuck_t argi[static 1U], const char *fn)
{
	static const char tot[] = "total";
	static size_t tnsub;
	static size_t tnpre;
	static size_t tnobj;
	size_t x;

	if (fn == NULL && argi->nargs) {
		/* print a total value */
		fn = tot;
		nsub = tnsub;
		npre = tnpre;
		nobj = tnobj;
	}

	/* print counts */
	if (argi->subjects_flag) {
		x = nsub;
	} else if (argi->predicates_flag) {
		x = npre;
	} else if (argi->statements_flag) {
		x = nobj;
	} else {
		printf("% 5zu % 5zu % 5zu\t", nsub, npre, nobj);
		goto pr_fn;
	}
	printf("%zu\t", x);
pr_fn:
	if (fn == NULL) {
		puts("");
	} else {
		puts(fn);
	}
	/* sum them up for a summary */
	tnsub += nsub;
	tnpre += npre;
	tnobj += nobj;
	return;
}

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}

	if (argi->nargs == 0U) {
		goto one;
	}
	for (size_t i = 0U; i < argi->nargs; i++) {
	one:
		rc -= count1(argi->args[i], !!argi->subjects_flag);
		pr_counts(argi, argi->args[i]);
	}
	if (argi->nargs > 1U) {
		/* print summary as well */
		pr_counts(argi, NULL);
	}

out:
	yuck_free(argi);
	return rc;
}

/* ttl-wc.c ends here */
