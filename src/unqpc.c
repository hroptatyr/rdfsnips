/*** unq%.c -- unquote percent encoded character
 *
 * Copyright (C) 2023 Sebastian Freundt
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "nifty.h"


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


static char f1st = '0';

static int
haspc(const char *s, size_t z)
{
	size_t i = 0U;

more:
	for (; i < z && s[i] != '%'; i++);
	if (++i < z &&
	    (s[i] >= f1st && s[i] <= '9' ||
	     s[i] >= 'A' && s[i] <= 'F' ||
	     s[i] >= 'a' && s[i] <= 'f') &&
	    ++i < z &&
	    (s[i] >= '0' && s[i] <= '9' ||
	     s[i] >= 'A' && s[i] <= 'F' ||
	     s[i] >= 'a' && s[i] <= 'f') &&
	    /* make sure to disallow %FF and %FE in --only-printable */
	    (f1st == '0' ||
	     !((s[i-1] == 'F' || s[i-1] == 'f') &&
	       (s[i-0] == 'F' || s[i-0] == 'f')))) {
		return 1;
	} else if (i >= z) {
		return 0;
	}
	goto more;
}

static __attribute__((noinline)) ssize_t
kilpc(char *restrict s, size_t z)
{
	size_t i = 0U, k = 0U;
	char xh, xl;

	do {
		for (; i < z && s[i] != '%'; s[k++] = s[i++]);
		if (i + 2U < z &&
		    (s[i + 1U] >= f1st && s[i + 1U] <= '9' && (xh = s[i + 1U]) ||
		     s[i + 1U] >= 'A' && s[i + 1U] <= 'F' && (xh = s[i + 1U] - 0x7U) ||
		     s[i + 1U] >= 'a' && s[i + 1U] <= 'f' && (xh = s[i + 1U] - 0x27U)) &&
		    (s[i + 2U] >= '0' && s[i + 2U] <= '9' && (xl = s[i + 2U]) ||
		     s[i + 2U] >= 'A' && s[i + 2U] <= 'F' && (xl = s[i + 2U] - 0x7U) ||
		     s[i + 2U] >= 'a' && s[i + 2U] <= 'f' && (xl = s[i + 2U] - 0x27U))) {
			i += 3U;
			s[k++] = (char)((xh - 0x30U) << 4U ^ (xl - 0x30U));
		} else if (i < z) {
			s[k++] = s[i++];
		}
	} while (i < z);
	return k;
}

static int
fold1(FILE *fp)
{
	char *line = NULL;
	size_t llen = 0UL;

	for (ssize_t nrd; (nrd = getline(&line, &llen, fp)) > 0;) {
		nrd -= line[nrd - 1] == '\n';

		while (haspc(line, nrd)) {
			nrd = kilpc(line, nrd);
		}
		line[nrd++] = '\n';
		fwrite(line, 1, nrd, stdout);
	}
	return 0;
}


#include "unqpc.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}
	if (argi->only_printable_flag) {
		f1st = '2';
	}

	if (!argi->nargs) {
		rc = fold1(stdin) < 0;
	} else for (size_t i = 0U; i < argi->nargs; i++) {
		FILE *fp;

		if (UNLIKELY((fp = fopen(argi->args[i], "r")) == NULL)) {
			error("\
Error: cannot open file `%s'", argi->args[i]);
			rc = 1;
			continue;
		}
		rc |= fold1(fp) < 0;
		fclose(fp);
	}

out:
	yuck_free(argi);
	return rc;
}

/* unqpc.c ends here */
