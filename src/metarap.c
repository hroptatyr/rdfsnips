/*** metarap.c -- reify rdf
 *
 * Copyright (C) 2017-2018 Sebastian Freundt
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <raptor2.h>
#include "nifty.h"


static void*
recalloc(void *oldp, size_t oldz, size_t newz, size_t nmemb)
{
	char *p = realloc(oldp, newz * nmemb);
	if (LIKELY(newz >= oldz)) {
		memset(p + oldz * nmemb, 0, (newz - oldz) * sizeof(nmemb));
	}
	return p;
}


/* murmur3 */
#define HASHSIZE	(128U / 8U)

static inline __attribute__((pure, const)) uint64_t
rotl64(uint64_t x, int8_t r)
{
	return (x << r) | (x >> (64 - r));
}

static inline __attribute__((pure, const, always_inline)) uint64_t
fmix64(uint64_t k)
{
	k ^= k >> 33;
	k *= 0xff51afd7ed558ccdULL;
	k ^= k >> 33;
	k *= 0xc4ceb9fe1a85ec53ULL;
	k ^= k >> 33;

	return k;
}

static void
MurmurHash3_x64_128(const void *key, size_t len, uint8_t out[static HASHSIZE])
{
	const uint8_t *data = (const uint8_t*)key;
	const size_t nblocks = len / 16U;

	uint64_t h1 = 0U, h2 = 0U;

	const uint64_t c1 = 0x87c37b91114253d5ULL;
	const uint64_t c2 = 0x4cf5ad432745937fULL;

	//----------
	// body

	const uint64_t * blocks = (const uint64_t *)(data);

	for (size_t i = 0; i < nblocks; i++) {
		uint64_t k1 = blocks[i*2+0];
		uint64_t k2 = blocks[i*2+1];

		k1 *= c1; k1  = rotl64(k1,31); k1 *= c2; h1 ^= k1;

		h1 = rotl64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;

		k2 *= c2; k2  = rotl64(k2,33); k2 *= c1; h2 ^= k2;

		h2 = rotl64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
	}

	//----------
	// tail

	const uint8_t * tail = (const uint8_t*)(data + nblocks*16);

	uint64_t k1 = 0;
	uint64_t k2 = 0;

	switch(len & 15)
	{
	case 15: k2 ^= ((uint64_t)tail[14]) << 48;
	case 14: k2 ^= ((uint64_t)tail[13]) << 40;
	case 13: k2 ^= ((uint64_t)tail[12]) << 32;
	case 12: k2 ^= ((uint64_t)tail[11]) << 24;
	case 11: k2 ^= ((uint64_t)tail[10]) << 16;
	case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;
	case  9: k2 ^= ((uint64_t)tail[ 8]) << 0;
		k2 *= c2; k2  = rotl64(k2,33); k2 *= c1; h2 ^= k2;

	case  8: k1 ^= ((uint64_t)tail[ 7]) << 56;
	case  7: k1 ^= ((uint64_t)tail[ 6]) << 48;
	case  6: k1 ^= ((uint64_t)tail[ 5]) << 40;
	case  5: k1 ^= ((uint64_t)tail[ 4]) << 32;
	case  4: k1 ^= ((uint64_t)tail[ 3]) << 24;
	case  3: k1 ^= ((uint64_t)tail[ 2]) << 16;
	case  2: k1 ^= ((uint64_t)tail[ 1]) << 8;
	case  1: k1 ^= ((uint64_t)tail[ 0]) << 0;
		k1 *= c1; k1  = rotl64(k1,31); k1 *= c2; h1 ^= k1;
	};

	//----------
	// finalization

	h1 ^= len; h2 ^= len;

	h1 += h2;
	h2 += h1;

	h1 = fmix64(h1);
	h2 = fmix64(h2);

	h1 += h2;
	h2 += h1;

	((uint64_t*)out)[0] = h1;
	((uint64_t*)out)[1] = h2;
}

static inline __attribute__((pure, const)) unsigned char
c2h(unsigned int c)
{
	return (unsigned char)((c < 10U) ? (c ^ '0') : (c + 'W'));
}

static size_t
mmh3(unsigned char *restrict tgt, size_t tsz, const char *str, size_t len)
{
	uint8_t h[HASHSIZE];
	MurmurHash3_x64_128(str, len, h);
	/* print hash */
	for (size_t i = 0U; i < countof(h) && 2U * i + 1U < tsz; i++) {
		tgt[2U * i + 0U] = c2h((h[i] >> 0U) & 0b1111U);
		tgt[2U * i + 1U] = c2h((h[i] >> 4U) & 0b1111U);
	}
	return 2U * HASHSIZE;
}

static char*
xmemmem(const char *hay, const size_t hayz, const char *ndl, const size_t ndlz)
{
	const char *const eoh = hay + hayz;
	const char *const eon = ndl + ndlz;
	const char *hp;
	const char *np;
	const char *cand;
	unsigned int hsum;
	unsigned int nsum;
	unsigned int eqp;

	/* trivial checks first
         * a 0-sized needle is defined to be found anywhere in haystack
         * then run strchr() to find a candidate in HAYSTACK (i.e. a portion
         * that happens to begin with *NEEDLE) */
	if (ndlz == 0UL) {
		return deconst(hay);
	} else if ((hay = memchr(hay, *ndl, hayz)) == NULL) {
		/* trivial */
		return NULL;
	}

	/* First characters of haystack and needle are the same now. Both are
	 * guaranteed to be at least one character long.  Now computes the sum
	 * of characters values of needle together with the sum of the first
	 * needle_len characters of haystack. */
	for (hp = hay + 1U, np = ndl + 1U, hsum = *hay, nsum = *hay, eqp = 1U;
	     hp < eoh && np < eon;
	     hsum ^= *hp, nsum ^= *np, eqp &= *hp == *np, hp++, np++);

	/* HP now references the (NZ + 1)-th character. */
	if (np < eon) {
		/* haystack is smaller than needle, :O */
		return NULL;
	} else if (eqp) {
		/* found a match */
		return deconst(hay);
	}

	/* now loop through the rest of haystack,
	 * updating the sum iteratively */
	for (cand = hay; hp < eoh; hp++) {
		hsum ^= *cand++;
		hsum ^= *hp;

		/* Since the sum of the characters is already known to be
		 * equal at that point, it is enough to check just NZ - 1
		 * characters for equality,
		 * also CAND is by design < HP, so no need for range checks */
		if (hsum == nsum && memcmp(cand, ndl, ndlz - 1U) == 0) {
			return deconst(cand);
		}
	}
	return NULL;
}


struct buf_s {
	char *s;
	size_t n;
	size_t z;
};

struct ctx_s {
	struct buf_s *b;
	raptor_serializer *shash;
	raptor_serializer *sfold;
};

static int
_init(void *user_data)
{
	struct buf_s *b = user_data;
	b->s = malloc(b->z = 256U);
	b->n = 0U;
	return 0;
}

static void
_finish(void *user_data)
{
	struct buf_s *b = user_data;
	free(b->s);
	return;
}

static int
_write_byte(void *user_data, const int byte)
{
	struct buf_s *b = user_data;
	if (b->n >= b->z) {
		b->s = realloc(b->s, b->z *= 2U);
	}
	b->s[b->n++] = (char)byte;
	return 1;
}

static int
_write_bytes(void *user_data, const void *ptr, size_t size, size_t nmemb)
{
	struct buf_s *b = user_data;
	if (b->n + size * nmemb >= b->z) {
		/* resize */
		while ((b->z *= 2U) < b->n + size * nmemb);
		b->s = realloc(b->s, b->z);
	}
	memcpy(b->s + b->n, ptr, size * nmemb);
	b->n += size * nmemb;
	return (int)(size * nmemb);
}

static __attribute__((unused)) void
_prnt(struct buf_s *b)
{
	fwrite(b->s, sizeof(*b->s), b->n, stdout);
	b->n = 0;
	return;
}

static size_t
_hash(unsigned char *restrict tgt, size_t tsz, struct buf_s *b)
{
	size_t r;
	b->n -= b->n && b->s[b->n - 1U] == '\n';
	r = mmh3(tgt, tsz, b->s, b->n);
	b->n = 0U;
	return r;
}

static const raptor_iostream_handler handler = {
	.version = 2,
	.init = _init,
	.finish = _finish,
	.write_byte = _write_byte,
	.write_bytes = _write_bytes,
};

static void
nmsp(void *user_data, raptor_namespace *ns)
{
	raptor_serializer *sfold = user_data;
	raptor_serializer_set_namespace_from_namespace(sfold, ns);
	return;
}


/* replacement of "@PREFIX" and <@PREFIX> */
static unsigned char *rplc;
static size_t nrplc;
static size_t zrplc;

static size_t **cord;
static size_t zcord;
static size_t *ncord;
static size_t *sufxs;
static raptor_term ***cake;

static void
free_rplc(void)
{
	for (size_t i = 0U; i < zcord; i++) {
		free(cord[i]);
	}
	for (size_t i = 0U; i < zcord; i++) {
		for (size_t j = 0U; j < ncord[i]; j++) {
			raptor_free_term(cake[i][j]);
		}
		free(cake[i]);
	}
	free(cord);
	free(ncord);
	free(cake);
	free(sufxs);

	free(rplc);
	return;
}

static size_t
find_rplc(const unsigned char *str, size_t len)
{
	const char *p;
	size_t r;

	if (!(p = xmemmem((const char*)rplc, nrplc, (const char*)str, len))) {
		return -1ULL;
	} else if (p[-1] != '@' || p[len]) {
		return -1ULL;
	}
	r = ((const unsigned char*)p - rplc);
	return r / sizeof(r);
}

static size_t
add_rplc(const unsigned char *str, size_t len)
{
/* register prefix STR (sans the @) of size LEN
 * we align the strings on a sizeof(size_t) boundary */
	size_t r = nrplc;

	if (UNLIKELY(nrplc + len + sizeof(r) >= zrplc)) {
		while ((zrplc *= 2U) < nrplc + len + sizeof(r));
		rplc = realloc(rplc, zrplc);
	}
	rplc[nrplc++] = '@';
	memcpy(rplc + nrplc, str, len);
	nrplc += len;
	/* fast forward to next boundary */
	memset(rplc + nrplc, 0, sizeof(r) - (nrplc % sizeof(r)));
	nrplc += sizeof(r) - (nrplc % sizeof(r));
	return r / sizeof(r);
}

static void
add_cord(const size_t r, size_t i, size_t j, raptor_term *t, size_t sufx)
{
	if (UNLIKELY(r >= zcord)) {
		const size_t nuz = (zcord * 2U) ?: 64U;
		cord = recalloc(cord, zcord, nuz, sizeof(*cord));
		cake = recalloc(cake, zcord, nuz, sizeof(*cake));
		ncord = recalloc(ncord, zcord, nuz, sizeof(*ncord));
		sufxs = recalloc(sufxs, zcord, nuz, sizeof(*sufxs));
		zcord = nuz;
	}
	if (UNLIKELY(!(ncord[r] & (ncord[r] + 2U)))) {
		const size_t nuz = (ncord[r] + 2U) * 2U;
		cord[r] = realloc(cord[r], nuz * sizeof(*cord[r]));
		cake[r] = realloc(cake[r], nuz * sizeof(*cake[r]));
	}
	cake[r][ncord[r] / 2U] = raptor_term_copy(t);
	cord[r][ncord[r]++] = i;
	cord[r][ncord[r]++] = j;
	sufxs[r] = (sufx > sufxs[r]) ? sufx : sufxs[r];
	return;	
}


static raptor_world *world;
static raptor_uri *base;
static raptor_term *rdfsub, *rdfpred, *rdfobj;
static raptor_term *type;
static raptor_term *stmt;
static raptor_uri *rdf;

static raptor_term **terms;
static size_t nterms;
static size_t zterms;
static raptor_term ***beefs;
static size_t *nbeefs;

static void
free_terms(void)
{
	for (size_t i = 0U; i < nterms; i++) {
		for (size_t j = 0U; j < nbeefs[i]; j++) {
			raptor_free_term(beefs[i][j]);
		}
		free(beefs[i]);
	}
	free(beefs);
	free(nbeefs);

	for (size_t i = 0U; i < nterms; i++) {
		raptor_free_term(terms[i]);
	}
	free(terms);
	return;
}

static void
nscp(void *user_data, raptor_namespace *ns)
{
	raptor_serializer *UNUSED(sfold) = user_data;
	unsigned char *uristr;
	size_t urilen;
	const unsigned char *prestr;
	size_t prelen;
	size_t r;

	prestr = raptor_namespace_get_counted_prefix(ns, &prelen);
	if (!~(r = find_rplc(prestr, prelen))) {
		return;
	}
	with (raptor_uri *uri = raptor_namespace_get_uri(ns)) {
		uristr = raptor_uri_as_counted_string(uri, &urilen);
	}
	/* a service for the suffixing later on */
	unsigned char tmp[urilen + sufxs[r]];
	if (sufxs[r]) {
		memcpy(tmp, uristr, urilen);
		uristr = tmp;
	}
	for (size_t k = 0U; k < ncord[r]; k += 2U) {
		/* replace terms */
		const size_t i = cord[r][k + 0U];
		const size_t j = cord[r][k + 1U];
		const raptor_term *proto = cake[r][k / 2U];
		size_t tmplen;

		switch (proto->type) {
			const unsigned char *prostr;
			size_t prolen;
			raptor_term *t;

		case RAPTOR_TERM_TYPE_URI:
			prostr = raptor_uri_as_counted_string(
				proto->value.uri, &prolen);
			goto sufxchck;

		case RAPTOR_TERM_TYPE_LITERAL:
			prostr = proto->value.literal.string;
			prolen = proto->value.literal.string_len;
			goto sufxchck;

		sufxchck:
			tmplen = urilen;
			if (prelen < prolen && prostr[prelen + 1U] == ':') {
				memcpy(uristr + urilen,
				       prostr + (prelen + 2U),
				       prolen - (prelen + 2U));
				tmplen += prolen - (prelen + 2U);
			}
			switch (proto->type) {
			case RAPTOR_TERM_TYPE_URI:
				t = raptor_new_term_from_counted_uri_string(
					world, uristr, tmplen);
				break;
			case RAPTOR_TERM_TYPE_LITERAL:
				t = raptor_new_term_from_counted_literal(
					world, uristr, tmplen,
					proto->value.literal.datatype,
					proto->value.literal.language,
					proto->value.literal.language_len);
				break;
			}
			/* actually do the replacing now */
			raptor_free_term(beefs[i][j]);
			beefs[i][j] = t;
		default:
			break;
		}
	}
	return;
}

static void
prnt(void *user_data, raptor_statement *triple)
{
	static unsigned char prfx[80U] = "http://data.ga-group.nl/meta/mmh3/";
	struct ctx_s *ctx = user_data;
	raptor_world *w = triple->world;
	size_t prfn;
	size_t i;

	if (!nterms) {
		goto yep;
	}
	for (i = 0U; i < nterms; i++) {
		/* have we got him? */
		if (raptor_term_equals(terms[i], triple->predicate)) {
			goto yep;
		}
	}
	/* not found */
	return;

yep:
	raptor_serializer_serialize_statement(ctx->shash, triple);
	prfn = 34U + _hash(prfx + 34U, sizeof(prfx) - 34U, ctx->b);

	raptor_term *H = raptor_new_term_from_counted_uri_string(w, prfx, prfn);

	raptor_serializer_serialize_statement(
		ctx->sfold, &(raptor_statement){w, .subject = H, type, stmt});
	raptor_serializer_serialize_statement(
		ctx->sfold, &(raptor_statement){
			w, .subject = H, rdfsub, triple->subject});
	raptor_serializer_serialize_statement(
		ctx->sfold, &(raptor_statement){
			w, .subject = H, rdfpred, triple->predicate});
	raptor_serializer_serialize_statement(
		ctx->sfold, &(raptor_statement){
			w, .subject = H, rdfobj, triple->object});

	if (nterms) {
		for (size_t j = 0U; j < nbeefs[i]; j += 2U) {
			if (UNLIKELY(!beefs[i][j + 1U]->type)) {
				continue;
			}
			raptor_serializer_serialize_statement(
				ctx->sfold, &(raptor_statement){
					w, .subject = H,
						beefs[i][j + 0U],
						beefs[i][j + 1U]});
		}
	}

	raptor_serializer_flush(ctx->sfold);
	raptor_free_term(H);
	return;
}

static void
flts(void *UNUSED(user_data), raptor_statement *triple)
{
	size_t i;

	for (i = 0U; i < nterms; i++) {
		/* have we got him? */
		if (raptor_term_equals(terms[i], triple->subject)) {
			goto yep;
		}
	}
	/* otherwise add */
	if (UNLIKELY(nterms >= zterms)) {
		zterms *= 2U;
		terms = realloc(terms, zterms * sizeof(*terms));
		beefs = recalloc(beefs, zterms / 2U, zterms, sizeof(*beefs));
		nbeefs = recalloc(nbeefs, zterms / 2U, zterms, sizeof(*nbeefs));
	}
	terms[i] = raptor_term_copy(triple->subject);
	nterms++;

yep:
	/* bang po to beefs */
	if (UNLIKELY(!(nbeefs[i] & (nbeefs[i] + 2U)))) {
		/* resize */
		const size_t nuz = (nbeefs[i] + 2U) * 2U;
		beefs[i] = realloc(beefs[i], nuz * sizeof(*beefs[i]));
	}
	beefs[i][nbeefs[i]++] = raptor_term_copy(triple->predicate);
	beefs[i][nbeefs[i]++] = raptor_term_copy(triple->object);

	/* see if triple->object contains @PREFIX semantics */
	switch (triple->object->type) {
		const unsigned char *str;
		const unsigned char *eos;
		size_t len;
		size_t r;

	case RAPTOR_TERM_TYPE_URI:
		str = raptor_uri_as_counted_string(
			triple->object->value.uri, &len);
		goto chck;
	case RAPTOR_TERM_TYPE_LITERAL:
		str = triple->object->value.literal.string;
		len = triple->object->value.literal.string_len;
		goto chck;

	chck:
		if (len--, *str++ != '@') {
			break;
		}
		eos = memchr(str, ':', len) ?: str + len;
		/* see if we've got him */
		if (!~(r = find_rplc(str, eos - str))) {
			/* nope, add him then */
			r = add_rplc(str, eos - str);
		}
		/* bang coords and keep original object term */
		{
			static raptor_term nul_term;
			const size_t j = nbeefs[i] - 1U;
			raptor_term *t = beefs[i][j];
			const size_t sufx = str + len - eos;
			add_cord(r, i, j, t, sufx);
			beefs[i][j] = &nul_term;
		}
		break;

	default:
		break;
	}
	return;
}

static int
fltr(const char *fn, raptor_serializer *sfold)
{
	raptor_parser *p = raptor_new_parser(world, "trig");
	FILE *fp;
	int r;

	if (UNLIKELY(!(fp = fopen(fn, "r")))) {
		return -1;
	}

	if (LIKELY(!zterms)) {
		zterms = 64U;
		terms = malloc(zterms * sizeof(*terms));
		beefs = calloc(zterms, sizeof(*beefs));
		nbeefs = calloc(zterms, sizeof(*nbeefs));
	}

	rplc = malloc(zrplc = 256U);

	raptor_parser_set_statement_handler(p, NULL, flts);
	raptor_parser_set_namespace_handler(p, sfold, nmsp);
	r = raptor_parser_parse_file_stream(p, fp, NULL, base);

	raptor_free_parser(p);
	fclose(fp);
	return r;
}


#include "metarap.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	raptor_serializer *shash = NULL;
	raptor_serializer *sfold = NULL;
	raptor_iostream *iostream = NULL;
	raptor_uri *x;
	struct buf_s buf;
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		goto out;
	}

	world = raptor_new_world();
	raptor_world_open(world);

	base = raptor_new_uri(world, "-");

	if (!(shash = raptor_new_serializer(world, "ntriples"))) {
		rc = 1;
		goto err;
	}
	if (!(sfold = raptor_new_serializer(
		      world, argi->output_arg ?: "turtle"))) {
		rc = 1;
		goto err;
	}
	iostream = raptor_new_iostream_from_handler(world, &buf, &handler);
	raptor_serializer_start_to_iostream(shash, NULL, iostream);
	raptor_serializer_start_to_file_handle(sfold, NULL, stdout);

	rdf = raptor_new_uri(world, "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
	raptor_serializer_set_namespace(sfold, rdf, "rdf");

	if (argi->nargs) {
		if (fltr(*argi->args, sfold)) {
			rc = 1;
			goto err;
		}
	}

	x = raptor_new_uri_from_uri_local_name(world, rdf, "type");
	type = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	x = raptor_new_uri_from_uri_local_name(world, rdf, "Statement");
	stmt = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	x = raptor_new_uri_from_uri_local_name(world, rdf, "subject");
	rdfsub = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	x = raptor_new_uri_from_uri_local_name(world, rdf, "predicate");
	rdfpred = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	x = raptor_new_uri_from_uri_local_name(world, rdf, "object");
	rdfobj = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	with (raptor_parser *p = raptor_new_parser(
		      world, argi->input_arg ?: "trig")) {
		if (!p) {
			rc =1;
			goto err;
		}
		raptor_parser_set_statement_handler(
			p, &(struct ctx_s){&buf, shash, sfold}, prnt);
		raptor_parser_set_namespace_handler(p, sfold, nscp);
		raptor_parser_parse_file_stream(p, stdin, NULL, base);
		raptor_free_parser(p);
	}

	raptor_serializer_serialize_end(shash);
	raptor_serializer_serialize_end(sfold);

err:
	raptor_free_serializer(shash);
	raptor_free_serializer(sfold);

	raptor_free_term(rdfsub);
	raptor_free_term(rdfpred);
	raptor_free_term(rdfobj);
	raptor_free_term(stmt);
	raptor_free_term(type);

	raptor_free_uri(base);
	raptor_free_uri(rdf);

	raptor_free_iostream(iostream);

	/* unfilter */
	free_terms();
	free_rplc();

	raptor_free_world(world);

out:
	yuck_free(argi);
	return rc;
}
