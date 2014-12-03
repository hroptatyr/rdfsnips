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


/* the actual splitting */
static ssize_t
proc(const char *buf, size_t bsz)
{
	const char *sp = buf;
	const char *const UNUSED(ep) = buf + bsz;

next:
	/* overread whitespace */
	for (; *sp > '\0' && *sp <= ' '; sp++);
	/* statements are always concluded by . so look for that guy */
	for (const char *eo, *bp = sp;
	     (eo = strpbrk(bp, ".<\"")); bp = eo + 1U) {
		switch (*eo) {
		case '.':
			write(STDOUT_FILENO, sp, eo + 1U - sp);
			write(STDOUT_FILENO, "\n\n", 2U);
			sp = eo + 1U;
			goto next;
		case '<':
			/* find matching > */
			if (UNLIKELY((eo = strchr(eo, '>')) == NULL)) {
				goto out;
			}
			break;
		case '"':
			/* find matching " or """*/
			if (eo[1U] != '"') {
				eo = strchr(eo + 1U, '"');
			} else if (eo[2U] != '"') {
				if (UNLIKELY(eo[2U] == '\0')) {
					goto out;
				}
				/* otherwise point to second quote */
				eo++;
			} else {
				/* it's one of those """strings"""" */
				eo = strstr(eo + 3U, "\"\"\"");
			}
			if (UNLIKELY(eo == NULL)) {
				goto out;
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
		} else if (npr == 0) {
			/* need a bigger buffer */
			void *nub = resz(buf, bsz, bsz << 1U);

			if (UNLIKELY(nub == NULL)) {
				goto fuck;
			}
			/* otherwise ass buf and increase bsz */
			buf = nub;
			bsz <<= 1U;
		} else if ((bix -= npr) > 0) {
			/* memmove to the front */
			memmove(buf, buf + npr, bix);
		}
	}
	/* last try, we don't care how much gets processed */
	(void)proc(buf, bix);

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
