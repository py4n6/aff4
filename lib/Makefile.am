bin_PROGRAMS            = unittests
noinst_LTLIBRARIES      = liboo.la
lib_LTLIBRARIES 	= libaff4.la

liboo_la_SOURCES	= class.c talloc.c
liboo_la_CPPFLAGS 	= -DHAVE_VA_COPY

# specifics (bins)
unittests_LDADD           = libaff4.la
unittests_SOURCES         = unittests.c

libaff4_la_SOURCES       = misc.c zip.c stringio.c resolver.c image.c link.c map.c dirvol.c libaff4.c identity.c encode.c
libaff4_la_LIBADD	= liboo.la
libaff4_la_LDFLAGS       = -lz -luuid

if HAVE_LIBCURL
libaff4_la_LDFLAGS       += -lcurl
libaff4_la_SOURCES       += http.c
endif

if HAVE_OPENSSL
libaff4_la_LDFLAGS       += -lssl
libaff4_la_SOURCES       += encrypt.c
endif

if HAVE_LIBAFFLIB
libaff4_la_LDFLAGS       += -lafflib
libaff4_la_SOURCES       += aff1.c
endif

if HAVE_LIBEWF
libaff4_la_LDFLAGS       += -lewf
libaff4_la_SOURCES       += ewfvolume.c
endif