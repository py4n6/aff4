bin_PROGRAMS          = aff4imager

if HAVE_LIBFUSE
bin_PROGRAMS          += affuse
endif

if HAVE_TSK
bin_PROGRAMS          += fsbuilder
endif

aff4imager_SOURCES    = aff4imager.c common.c
aff4imager_LDFLAGS    = -lssl
aff4imager_LDADD      = ../lib/libaff4.la

affuse_SOURCES        = affuse.c
affuse_LDFLAGS        = -lfuse
affuse_LDADD          = ../lib/libaff4.la

fsbuilder_SOURCES     = fsbuilder.c common.c
fsbuilder_LDADD       = ../lib/libaff4.la
fsbuilder_LDFLAGS     = -ltsk3