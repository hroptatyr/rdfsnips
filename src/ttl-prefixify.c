/*** ttl-prefixify.c -- split ttl files
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
#include <stddef.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include "nifty.h"

#if !defined MAP_ANON && defined MAP_ANONYMOUS
# define MAP_ANON	MAP_ANONYMOUS
#elif !defined MAP_ANON
# define MAP_ANON	(0x1000U)
#endif	/* !MAP_ANON */
#define PROT_RW		(PROT_READ | PROT_WRITE)
#define MAP_MEM		(MAP_PRIVATE | MAP_ANON)

#define assert(x...)

struct str_s {
	const char *str;
	size_t len;
};

struct prfx_s {
	struct str_s prfx;
	struct str_s puri;
};

#define LIT2STR(s)	{s, sizeof(s) - 1U}
static struct prfx_s dflt_pres[64U] = {
	{LIT2STR("foaf"), LIT2STR("http://xmlns.com/foaf/0.1/")},
	{LIT2STR("ldp"), LIT2STR("http://www.w3.org/ns/ldp#")},
	{LIT2STR("owl"), LIT2STR("http://www.w3.org/2002/07/owl#")},
	{LIT2STR("rdf"), LIT2STR("http://www.w3.org/1999/02/22-rdf-syntax-ns#")},
};

static struct prfx_s *pres = dflt_pres;
static size_t npres = 4U;
static size_t zpres = countof(dflt_pres);


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

#define RESZ(_b, _oz, _nz)						\
	size_t _nuz_ = _nz;						\
	void *_nub_ = resz(_b, _oz, _nuz_);				\
	if (LIKELY(_nub_ != NULL)) {					\
		_b = _nub_;						\
		_oz = _nuz_;						\
	}

#define RESZ_S(_b, _oz, _nz)						\
	size_t _nuz_ = _nz;						\
	void *_nub_ = resz(_b, _oz * sizeof(*_b), _nuz_ * sizeof(*_b));	\
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


/* prefix handling */
static size_t
subst(char *str, size_t len)
{
/* expects STR to be \nul-term'd */
	char *sp = str;
	char *cp = NULL;
	char *pp = NULL;

	for (char *tp; (tp = strchr(sp, '<')) != NULL; sp = tp) {
		tp++;
		for (size_t i = 0U; i < npres; i++) {
			char *ep;

			/* check if it's an URI we know of
			 * and find its end */
			if (memcmp(tp, pres[i].puri.str, pres[i].puri.len)) {
				continue;
			} else if (UNLIKELY((ep = strchr(tp, '>')) == NULL)) {
				/* big cluster fuck */
				return 0U;
			}
			/* great, memmove the whole shebang */
			if (cp) {
				memmove(pp, cp, tp - 1U - cp);
				pp += tp - 1U - cp;
			} else {
				pp = tp - 1U;
			}

			/* actually substitute for the prefix */
			memcpy(pp, pres[i].prfx.str, pres[i].prfx.len);
			pp += pres[i].prfx.len;
			*pp++ = ':';
			/* move value now */
			with (size_t pz = pres[i].puri.len) {
				memmove(pp, tp + pz, ep - (tp + pz));
				/* store annex point for future run */
				pp += ep - (tp + pz);
			}
			/* and set TP (to get SP) for the next round */
			cp = tp = ep + 1U/*>*/;
			break;
		}
	}
	/* final move */
	if (cp) {
		memmove(pp, cp, str + len - cp);
		pp += str + len - cp;
		len = pp - str;
	}
	return len;
}

static int
add_prefix(const char *str, size_t len)
{
/* add prefix in STR and return 0 if added, -1 on failure, and
 * 1 if prefix already has been registered */
	struct str_s p;
	struct str_s u;
	const char *const ep = str + len;
	const char *tp;
	static char _prb[4096U];
	static char *prb = _prb;
	static size_t prz = sizeof(_prb);
	static size_t pix;

#define fini_prefix()	add_prefix(NULL, 0U)
	if (UNLIKELY(len == 0U)) {
		if (pres != dflt_pres) {
			(void)munmap(pres, zpres * sizeof(*pres));
		}
		pres = dflt_pres;
		npres = 4U;
		if (prb != _prb) {
			munmap(prb, prz);
			prb = _prb;
			prz = sizeof(_prb);
		}
		return 0;
	}

	if (memcmp(str, "@prefix", 7U) &&
	    memcmp(str, "@PREFIX", 7U)) {
		return -1;
	} else if (!isspace(str[7U])) {
		return -1;
	}
	/* skip leading whitespace */
	for (p.str = str + 8U; p.str < ep && isspace(*p.str); p.str++);
	/* find end of prefix */
	if (UNLIKELY((tp = memchr(p.str, ':', ep - p.str)) == NULL)) {
		return -1;
	}
	/* rewind over trailing whitespace */
	for (p.len = tp - p.str; p.len && isspace(p.str[p.len - 1U]); p.len--);

	/* find the url bit */
	if (UNLIKELY((u.str = memchr(tp, '<', ep - tp)) == NULL)) {
		return -1;
	} else if (UNLIKELY((tp = memchr(u.str, '>', ep - u.str)) == NULL)) {
		return -1;
	}
	/* adjust */
	u.str++;
	u.len = tp - u.str;

	for (size_t i = 0U; i < npres; i++) {
		if (p.len != pres[i].prfx.len) {
			;
		} else if (memcmp(pres[i].prfx.str, p.str, p.len)) {
			;
		} else if (u.len != pres[i].puri.len) {
			/* FUCK, the prefixes are identical but
			 * the urls are different :/ */
			return 1;
		} else if (memcmp(pres[i].puri.str, u.str, u.len)) {
			/* FUCK, the prefixes are identical but
			 * the urls are different :/ */
			return 1;
		} else {
			return 1;
		}
	};
	/* check for room in the prefix buffer */
	if (UNLIKELY(pix + p.len + u.len > prz)) {
		/* resize */
		size_t nuz = pix + p.len + u.len;
		char *old = prb;
		ptrdiff_t dlt;

		RESZ(prb, prz, next_2pow(nuz))
		else {
			return -1;
		}
		dlt = prb - old;
		for (size_t i = 4U; i < npres; i++) {
			pres[i].prfx.str += dlt;
			pres[i].puri.str += dlt;
		}
	}
	/* add him to the list of pres */
	if (UNLIKELY(npres >= zpres)) {
		/* resize */
		RESZ_S(pres, zpres, zpres << 1U)
		else {
			return -1;
		}
	}

	/* copy details over to prefix buffer */
	memcpy(prb + pix, p.str, p.len);
	p.str = prb + pix;
	pix += p.len;
	memcpy(prb + pix, u.str, u.len);
	u.str = prb + pix;
	pix += u.len;

	/* and assign */
	pres[npres].prfx = p;
	pres[npres].puri = u;
	npres++;
	return 0;
}


/* buffer handling */
static size_t
wr_buf(int fd, const char *buf, size_t bsz)
{
	size_t tot = 0U;

	for (ssize_t nwr;
	     tot < bsz &&
		     (nwr = write(fd, buf + tot, bsz - tot)) > 0;
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
	static const int cfd = STDOUT_FILENO;

#define fini_stmt()	wr_stmt(NULL, 0U)
	if (UNLIKELY(z == 0U)) {
		/* flushing instruction */
		if (LIKELY(cfd >= 0)) {
			wr_buf(cfd, buf, bix);
			close(cfd);
		}

		if (buf != _buf) {
			munmap(buf, bsz);
			buf = _buf;
			bsz = sizeof(_buf);
		}
		bix = 0U;

		fini_prefix();
		return;
	}

	if (UNLIKELY(bix == 0U)) {
		/* time to push our prefixes in */
		for (size_t i = 0U; i < npres; i++) {
			size_t adz = 8U/*@prefix*/ +
				pres[i].prfx.len + 1U/*:*/ + 1U/* */ +
				1U/*<*/ + pres[i].puri.len + 1U/*>*/ +
				1U/* */ + 1U/*.*/ + 1U/*\n*/;

			if (UNLIKELY(bix + adz > bsz)) {
				/* resize */
				RESZ(buf, bsz, next_2pow(adz))
				else {
					return;
				}
			}

			memcpy(buf + bix, "@prefix ", 8U);
			bix += 8U;
			memcpy(buf + bix, pres[i].prfx.str, pres[i].prfx.len);
			bix += pres[i].prfx.len;
			buf[bix++] = ':';
			buf[bix++] = ' ';
			buf[bix++] = '<';
			memcpy(buf + bix, pres[i].puri.str, pres[i].puri.len);
			bix += pres[i].puri.len;
			buf[bix++] = '>';
			buf[bix++] = ' ';
			buf[bix++] = '.';
			buf[bix++] = '\n';
		}
	}

	if (*s == '@') {
		/* check if we haven't got this directive already */
		if (add_prefix(s, z) > 0) {
			/* got him, skip */
			return;
		}
	}

	if (UNLIKELY(bix + z + 3U/*\n*/ > bsz)) {
		/* time to flush */
		wr_buf(cfd, buf, bix);
		/* reset index pointer */
		bix = 0U;

		if (UNLIKELY(z + 2U/*\n*/ > bsz)) {
			/* resize :O */
			RESZ(buf, bsz, next_2pow(z + 2U))
			else {
				return;
			}
		}
	}

	/* directives won't qualify as statements */
	if (*s != '@') {
		buf[bix++] = '\n';
	}
	/* copy */
	memcpy(buf + bix, s, z);
	/* finalise buffer */
	buf[bix + z] = '\0';
	/* and substitute, if it's not a @prefix */
	if (*s != '@') {
		z = subst(buf + bix, z);
	}
	/* append newline */
	buf[bix += z] = '\n';
	bix++;
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
		IN_COMMENT,
	} st = FREE;

#define fini_proc()	proc(NULL, 0U)
	if (UNLIKELY(bsz == 0U)) {
		/* and finalise */
		fini_stmt();
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
			     eo = strpbrk(tp, ".<\"#");
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


#include "ttl-prefixify.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	size_t i = 0U;
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}

	if (argi->nargs == 0U) {
		goto one;
	}
	for (; i < argi->nargs; i++) {
	one:
		rc -= split1(argi->args[i]);
	}

out:
	yuck_free(argi);
	return rc;
}

/* ttl-prefixify.c ends here */
