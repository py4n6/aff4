bin_PROGRAMS          = aff2imager

if HAVE_LIBFUSE
bin_PROGRAMS          += affuse
endif

if HAVE_TSK
bin_PROGRAMS          += fsbuilder
endif

aff2imager_SOURCES    = aff2imager.c common.c
aff2imager_LDFLAGS    = -lssl
aff2imager_LDADD      = ../lib/libaff4.la

affuse_SOURCES        = affuse.c
affuse_LDFLAGS        = -lfuse
affuse_LDADD          = ../lib/libaff4.la

fsbuilder_SOURCES     = fsbuilder.c common.c
fsbuilder_LDADD       = ../lib/libaff4.la
fsbuilder_LDFLAGS     = -ltsk3