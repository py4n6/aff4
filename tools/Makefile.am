bin_PROGRAMS          = aff4imager raid_reconstructor

if HAVE_LIBFUSE
bin_PROGRAMS          += affuse

affuse_SOURCES        = affuse.c
affuse_LDFLAGS        = -lfuse
affuse_LDADD          = ../lib/libaff4.la
endif

# Disabled for the moment
#if HAVE_TSK
#bin_PROGRAMS          += fsbuilder

#fsbuilder_SOURCES     = fsbuilder.c common.c
#fsbuilder_LDADD       = ../lib/libaff4.la
#fsbuilder_LDFLAGS     = -ltsk3
#endif

aff4imager_SOURCES    = aff4imager.c common.c
aff4imager_LDFLAGS    = -lssl
aff4imager_LDADD      = ../lib/libaff4.la

raid_reconstructor_SOURCES = raid_reconstructor.c common.c
raid_reconstructor_LDADD = ../lib/libaff4.la