#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <raptor2.h>
#include "nifty.h"


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

static void
UNUSED(_prnt)(struct buf_s *b)
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


static raptor_world *world;
static raptor_term *rdfssub, *rdfspred, *rdfsobj;
static raptor_term *type;
static raptor_term *stmt;
static raptor_uri *rdfs;

static raptor_term **terms;
static size_t nterms;
static size_t zterms;
static raptor_term ***beefs;
static size_t *nbeefs;
static size_t *zbeefs;

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
			w, .subject = H, rdfssub, triple->subject});
	raptor_serializer_serialize_statement(
		ctx->sfold, &(raptor_statement){
			w, .subject = H, rdfspred, triple->predicate});
	raptor_serializer_serialize_statement(
		ctx->sfold, &(raptor_statement){
			w, .subject = H, rdfsobj, triple->object});

	if (nterms) {
		for (size_t j = 0U; j < nbeefs[i]; j += 2U) {
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
		beefs = realloc(beefs, zterms * sizeof(*beefs));
		nbeefs = realloc(nbeefs, zterms * sizeof(*nbeefs));
		zbeefs = realloc(zbeefs, zterms * sizeof(*zbeefs));
	}
	terms[i] = raptor_term_copy(triple->subject);
	nbeefs[i] = 0U;
	zbeefs[i] = 64U;
	beefs[i] = malloc(zbeefs[i] * sizeof(*beefs));
	nterms++;

yep:
	/* bang po to beefs */
	if (UNLIKELY(nbeefs[i] >= zbeefs[i])) {
		/* resize */
		zbeefs[i] *= 2U;
		beefs[i] = realloc(beefs[i], zbeefs[i] * sizeof(*beefs[i]));
	}
	beefs[i][nbeefs[i]++] = raptor_term_copy(triple->predicate);
	beefs[i][nbeefs[i]++] = raptor_term_copy(triple->object);
	return;
}

static void
fltr(const char *fn, raptor_serializer *sfold)
{
	raptor_parser *p = raptor_new_parser(world, "trig");
	raptor_uri *fu = raptor_new_uri_from_uri_or_file_string(
		world, NULL, (const unsigned char*)fn);

	if (LIKELY(!zterms)) {
		zterms = 64U;
		terms = malloc(zterms * sizeof(*terms));
		beefs = calloc(zterms, sizeof(*beefs));
		nbeefs = calloc(zterms, sizeof(*nbeefs));
		zbeefs = calloc(zterms, sizeof(*zbeefs));
	}

	raptor_parser_set_statement_handler(p, NULL, flts);
	raptor_parser_set_namespace_handler(p, sfold, nmsp);
	raptor_parser_parse_file(p, fu, NULL);

	raptor_free_uri(fu);
	raptor_free_parser(p);
	return;
}


#include "metarap.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	raptor_serializer *shash;
	raptor_serializer *sfold;
	raptor_iostream *iostream;
	raptor_uri *base;
	raptor_uri *x;
	struct buf_s buf;

	if (yuck_parse(argi, argc, argv) < 0) {
		goto out;
	}

	world = raptor_new_world();
	raptor_world_open(world);

	base = raptor_new_uri(world, " ");

	shash = raptor_new_serializer(world, "ntriples");
	sfold = raptor_new_serializer(world, "turtle");
	iostream = raptor_new_iostream_from_handler(world, &buf, &handler);
	raptor_serializer_start_to_iostream(shash, NULL, iostream);
	raptor_serializer_start_to_file_handle(sfold, NULL, stdout);

	rdfs = raptor_new_uri(world, "http://www.w3.org/2000/01/rdf-schema#");
	raptor_serializer_set_namespace(sfold, rdfs, "rdfs");

	if (argi->nargs) {
		fltr(*argi->args, sfold);
	}

	type = raptor_new_term_from_uri_string(
		world, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
	stmt = raptor_new_term_from_uri_string(
		world, "http://www.w3.org/1999/02/22-rdf-syntax-ns#Statement");

	x = raptor_new_uri_from_uri_local_name(world, rdfs, "subject");
	rdfssub = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	x = raptor_new_uri_from_uri_local_name(world, rdfs, "predicate");
	rdfspred = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	x = raptor_new_uri_from_uri_local_name(world, rdfs, "object");
	rdfsobj = raptor_new_term_from_uri(world, x);
	raptor_free_uri(x);

	with (raptor_parser *p = raptor_new_parser(world, "trig")) {
		raptor_parser_set_statement_handler(
			p, &(struct ctx_s){&buf, shash, sfold}, prnt);
		raptor_parser_parse_file_stream(p, stdin, NULL, base);
		raptor_free_parser(p);
	}

	raptor_serializer_serialize_end(shash);
	raptor_serializer_serialize_end(sfold);
	raptor_free_serializer(shash);
	raptor_free_serializer(sfold);

	raptor_free_term(rdfssub);
	raptor_free_term(rdfspred);
	raptor_free_term(rdfsobj);
	raptor_free_term(stmt);
	raptor_free_term(type);

	raptor_free_uri(base);
	raptor_free_uri(rdfs);

	raptor_free_iostream(iostream);

	/* unfilter */
	for (size_t i = 0U; i < nterms; i++) {
		raptor_free_term(terms[i]);
	}
	free(terms);
	for (size_t i = 0U; i < nterms; i++) {
		for (size_t j = 0U; j < nbeefs[i]; j++) {
			raptor_free_term(beefs[i][j]);
		}
		free(beefs[i]);
	}
	free(beefs);
	free(nbeefs);
	free(zbeefs);

	raptor_free_world(world);

out:
	yuck_free(argi);
	return 0;
}
