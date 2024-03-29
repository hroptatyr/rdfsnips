### Makefile.am
include $(top_builddir)/version.mk

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=201001L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
AM_LDFLAGS = $(XCCLDFLAGS)

bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
noinst_LIBRARIES =
noinst_LTLIBRARIES =
pkglib_LTLIBRARIES =
noinst_HEADERS =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES = 
SUFFIXES =

CLEANFILES += version.c
EXTRA_DIST += version.c.in

noinst_LIBRARIES += libttl.a
libttl_a_SOURCES = version.c version.h
libttl_a_SOURCES += nifty.h

bin_PROGRAMS += ttl-split
ttl_split_SOURCES = ttl-split.c ttl-split.yuck
ttl_split_CPPFLAGS = $(AM_CPPFLAGS)
ttl_split_LDFLAGS = $(AM_LDFLAGS)
ttl_split_LDADD = libttl.a
BUILT_SOURCES += ttl-split.yucc

bin_PROGRAMS += ttl-wc
ttl_wc_SOURCES = ttl-wc.c ttl-wc.yuck
ttl_wc_CPPFLAGS = $(AM_CPPFLAGS)
ttl_wc_LDFLAGS = $(AM_LDFLAGS)
ttl_wc_LDADD = libttl.a
BUILT_SOURCES += ttl-wc.yucc

bin_PROGRAMS += ttl-prefixify
ttl_prefixify_SOURCES = ttl-prefixify.c ttl-prefixify.yuck
ttl_prefixify_CPPFLAGS = $(AM_CPPFLAGS)
ttl_prefixify_LDFLAGS = $(AM_LDFLAGS)
ttl_prefixify_LDADD = libttl.a
BUILT_SOURCES += ttl-prefixify.yucc

bin_PROGRAMS += hashl
hashl_SOURCES = hashl.c hashl.yuck
hashl_CPPFLAGS = $(AM_CPPFLAGS)
hashl_LDFLAGS = $(AM_LDFLAGS)
BUILT_SOURCES += hashl.yucc

bin_PROGRAMS += unqpc
unqpc_SOURCES = unqpc.c unqpc.yuck
unqpc_CPPFLAGS = $(AM_CPPFLAGS)
unqpc_LDFLAGS = $(AM_LDFLAGS)
BUILT_SOURCES += unqpc.yucc

bin_PROGRAMS += hashf
hashf_SOURCES = hashf.c hashf.yuck
hashf_CFLAGS = -mavx2 -fast
hashf_CPPFLAGS = $(AM_CPPFLAGS)
hashf_LDFLAGS = $(AM_LDFLAGS)
BUILT_SOURCES += hashf.yucc

bin_PROGRAMS += metarap
metarap_SOURCES = metarap.c metarap.yuck
metarap_CPPFLAGS = $(shell pkg-config --cflags raptor2)
metarap_LDFLAGS = $(shell pkg-config --libs raptor2)
metarap_LDADD = libttl.a
BUILT_SOURCES += metarap.yucc

## version rules
version.c: version.c.in $(top_builddir)/.version
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver --ignore-noscm --force -o $@ \
			--use-reference --reference $(top_builddir)/.version $<

## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
