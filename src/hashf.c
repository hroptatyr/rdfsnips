/*** hashf.c -- calculate file hashes
 *
 * Copyright (C) 2018-2021 Sebastian Freundt
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
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "nifty.h"
#define XXH_INLINE_ALL
#define XXH_PRIVATE_API
#define XXH_VECTOR XXH_AVX2
#include "xxhash.h"

#define KB	*(1U<<10)
#define MB	*(1U<<20)
#define GB	*(1U<<30)


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

static inline __attribute__((pure, const)) unsigned char
c2h(unsigned int c)
{
	return (unsigned char)((c < 10U) ? (c ^ '0') : (c + 'W'));
}

static int
hash1(int fd, const size_t fz, const size_t strd)
{
	XXH3_state_t st;
	unsigned char B[64 KB];
	XXH128_hash_t h;

	(void)XXH3_128bits_reset_withSeed(&st, fz);
	if (strd < 256U || fz < 256U) {
		for (ssize_t nrd; (nrd = read(fd, B, sizeof(B)) > 0);) {
			XXH3_128bits_update(&st, B, (size_t)nrd);
		}
	} else {
		for (size_t i = 0, stp, Z; i * strd < fz;) {
			ssize_t nrd;
			Z = 0U;
			do {
				for (stp = 0U;
				     stp < 256U && (nrd = read(fd, B + Z + stp, 256U - stp)) > 0;
				     stp += nrd);
				(void)lseek(fd, ++i * strd, SEEK_SET);
			} while ((Z += stp) < sizeof(B) && nrd > 0);
			XXH3_128bits_update(&st, B, Z);
		}
	}
	h = XXH3_128bits_digest(&st);

	/* print hash */
	B[0U] = c2h((h.high64 >> 60U) & 0b1111U);
	B[1U] = c2h((h.high64 >> 56U) & 0b1111U);
	B[2U] = c2h((h.high64 >> 52U) & 0b1111U);
	B[3U] = c2h((h.high64 >> 48U) & 0b1111U);
	B[4U] = c2h((h.high64 >> 44U) & 0b1111U);
	B[5U] = c2h((h.high64 >> 40U) & 0b1111U);
	B[6U] = c2h((h.high64 >> 36U) & 0b1111U);
	B[7U] = c2h((h.high64 >> 32U) & 0b1111U);
	B[8U] = c2h((h.high64 >> 28U) & 0b1111U);
	B[9U] = c2h((h.high64 >> 24U) & 0b1111U);
	B[10U] = c2h((h.high64 >> 20U) & 0b1111U);
	B[11U] = c2h((h.high64 >> 16U) & 0b1111U);
	B[12U] = c2h((h.high64 >> 12U) & 0b1111U);
	B[13U] = c2h((h.high64 >> 8U) & 0b1111U);
	B[14U] = c2h((h.high64 >> 4U) & 0b1111U);
	B[15U] = c2h((h.high64 >> 0U) & 0b1111U);

	B[16U] = c2h((h.low64 >> 60U) & 0b1111U);
	B[17U] = c2h((h.low64 >> 56U) & 0b1111U);
	B[18U] = c2h((h.low64 >> 52U) & 0b1111U);
	B[19U] = c2h((h.low64 >> 48U) & 0b1111U);
	B[20U] = c2h((h.low64 >> 44U) & 0b1111U);
	B[21U] = c2h((h.low64 >> 40U) & 0b1111U);
	B[22U] = c2h((h.low64 >> 36U) & 0b1111U);
	B[23U] = c2h((h.low64 >> 32U) & 0b1111U);
	B[24U] = c2h((h.low64 >> 28U) & 0b1111U);
	B[25U] = c2h((h.low64 >> 24U) & 0b1111U);
	B[26U] = c2h((h.low64 >> 20U) & 0b1111U);
	B[27U] = c2h((h.low64 >> 16U) & 0b1111U);
	B[28U] = c2h((h.low64 >> 12U) & 0b1111U);
	B[29U] = c2h((h.low64 >> 8U) & 0b1111U);
	B[30U] = c2h((h.low64 >> 4U) & 0b1111U);
	B[31U] = c2h((h.low64 >> 0U) & 0b1111U);
	fwrite(B, 1, 32U, stdout);
	return 0;
}

static size_t
fz(int fd)
{
	struct stat st;

	if (fstat(fd, &st) < 0) {
		return 0U;
	}
	return st.st_size;
}


#include "hashf.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	long unsigned int strd;
	bool strp = false;
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}

	/* read stride length */
	with (char *ep = NULL) {
		strd = argi->stride_arg ? strtoul(argi->stride_arg, &ep, 0) : 0U;
		if (!ep) {
			break;
		}
		switch (*ep) {
		case 't':
		case 'T':
			strd *= 1024U;
		case 'g':
		case 'G':
			strd *= 1024U;
		case 'm':
		case 'M':
			strd *= 1024U;
		case 'k':
		case 'K':
			strd *= 1024U;
		default:
			break;
		case '%':
			strp = true;
			break;
		}
	}

	if (!argi->nargs) {
		rc = hash1(STDIN_FILENO, 0U, 0U) < 0;
		fputc('\n', stdout);
	} else for (size_t i = 0U; i < argi->nargs; i++) {
		int fd;

		if (UNLIKELY((fd = open(argi->args[i], O_RDONLY)) < 0)) {
			error("\
Error: cannot open file `%s'", argi->args[i]);
			rc = 1;
			continue;
		}
		fputs(argi->args[i], stdout);
		fputc('\t', stdout);
		with (size_t thisfz = fz(fd)) {
			size_t thistrd = !strp ? strd : thisfz * 100U / strd;
			rc |= hash1(fd, thisfz, thistrd) < 0;
		}
		close(fd);
		fputc('\n', stdout);
	}

out:
	yuck_free(argi);
	return rc;
}

/* hashf.c ends here */
