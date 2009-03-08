bin_PROGRAMS            = unittests
noinst_LTLIBRARIES       = libfif.la liboo.la libmd5.la libsha1.la

liboo_la_SOURCES	= class.c talloc.c
liboo_la_CPPFLAGS 	= -DHAVE_VA_COPY

libmd5_la_SOURCES	= md5c.c
libsha1_la_SOURCES	= sha1.c

# specifics (bins)
unittests_LDADD           = libfif.la
unittests_SOURCES         = unittests.c

libfif_la_SOURCES       = misc.c zip.c stringio.c resolver.c blob.c image.c link.c map.c
libfif_la_LIBADD	= libsha1.la liboo.la libmd5.la
libfif_la_LDFLAGS       = -lz -luuid
