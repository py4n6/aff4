bin_PROGRAMS            = unittests
noinst_LTLIBRARIES       = libaff2.la liboo.la

liboo_la_SOURCES	= class.c talloc.c
liboo_la_CPPFLAGS 	= -DHAVE_VA_COPY

# specifics (bins)
unittests_LDADD           = libaff2.la
unittests_SOURCES         = unittests.c

libaff2_la_SOURCES       = misc.c zip.c stringio.c resolver.c blob.c image.c link.c map.c dirvol.c
libaff2_la_LIBADD	= liboo.la
libaff2_la_LDFLAGS       = -lz -luuid

if HAVE_LIBAFFLIB
libaff2_la_LDFLAGS       += -lcurl
libaff2_la_SOURCES       += http.c
endif

if HAVE_OPENSSL
libaff2_la_LDFLAGS       += -lssl
libaff2_la_SOURCES       += encrypt.c
endif