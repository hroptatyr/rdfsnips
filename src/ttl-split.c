/*** ttl-split.c -- split ttl files
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


/* helpers */
static __attribute__((const, pure)) size_t
next_2pow(size_t x)
{
	x--;
	x |= x >> 1U;
	x |= x >> 2U;
	x |= x >> 4U;
	x |= x >> 8U;
	x |= x >> 16U;
	return ++x;
}

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

static size_t
fl_stmt(const char *buf, size_t bsz)
{
	size_t tot = 0U;

	for (ssize_t nwr;
	     tot < bsz &&
		     (nwr = write(STDOUT_FILENO, buf + tot, bsz - tot)) > 0;
	     tot += nwr);
	return tot;
}

static void
wr_stmt(const char *s, size_t z)
{
	static char _buf[4096U];
	static char *buf = _buf;
	static size_t bsz = sizeof(_buf);
	static size_t bix = 0U;

#define fini_stmt()	wr_stmt(NULL, 0U)
	if (UNLIKELY(z == 0U)) {
		/* flushing instruction */
		fl_stmt(buf, bix);
		if (buf != _buf) {
			munmap(buf, bsz);
			buf = _buf;
			bsz = sizeof(_buf);
		}
		bix = 0U;
		return;
	}

	if (UNLIKELY(bix + z + 2U/*\n*/ > bsz)) {
		/* time to flush */
		fl_stmt(buf, bix);
		/* reset index pointer */
		bix = 0U;

		if (UNLIKELY(z + 2U/*\n*/ > bsz)) {
			/* resize :O */
			size_t nuz = next_2pow(z + 2U);
			void *nub = resz(buf, bsz, nuz);

			if (UNLIKELY(nub == NULL)) {
				return;
			}
			buf = nub;
			bsz = nuz;
		}
	}

	if (*s != '@') {
		buf[bix++] = '\n';
	} else {
		/* cache directives */
		;
	}
	/* copy beef */
	memcpy(buf + bix, s, z);
	bix += z;
	/* append newline */
	buf[bix++] = '\n';
	return;
}


/* the actual splitting */
static ssize_t
proc(const char *buf, size_t bsz)
{
	const char *sp = buf;
	const char *const UNUSED(ep) = buf + bsz;
	enum {
		FREE,
		IN_ANGLES,
		IN_QUOTES,
		IN_LONG_QUOTES,
	} st = FREE;

#define fini_proc()	proc(NULL, 0U)
	if (UNLIKELY(bsz == 0U)) {
		/* and finalise */
		fini_stmt();
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
			     eo = strpbrk(tp, ".<\"");
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
			wr_stmt(sp, eo - sp);
			sp = eo;
			goto next;
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
						fputs("LONG\n", stderr);
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
		default:
			break;
		}
	}
out:
	return sp - buf;
}

static int
split1(const char *fn)
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
	/* read into buf */
	bix = 0U;
	for (ssize_t nrd, npr;
	     (nrd = read(fd, buf + bix, bsz - bix - 1U/*\nul*/)) > 0;) {
		/* mark the end of the buffer */
		buf[bix += nrd] = '\0';
		if ((npr = proc(buf, bix)) < 0) {
			goto fuck;
		} else if (npr == 0 && bix + 1 >= bsz) {
			/* need a bigger buffer */
			void *nub = resz(buf, bsz, bsz << 1U);

			if (UNLIKELY(nub == NULL)) {
				goto fuck;
			}
			/* otherwise ass buf and increase bsz */
			buf = nub;
			bsz <<= 1U;
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
	(void)proc(buf, bix);
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


#include "ttl-split.yucc"

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
		rc -= split1(argi->args[i]);
	}

out:
	yuck_free(argi);
	return rc;
}

/* ttl-split.c ends here */
