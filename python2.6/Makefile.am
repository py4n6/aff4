include $(top_srcdir)/config/Makefile.rules

noinst_LTLIBRARIES      = pyaff4.la
nodist_pkgpyexec_PYTHON	= pyaff4$(PYTHON_EXTENSION)

## Everything should be linked with libaff4
AM_LDFLAGS = -laff4

pyaff4_la_SOURCES 	= pyaff4.c
pyaff4_la_CPPFLAGS 	= $(PYTHON_CPPFLAGS) -I$(top_srcdir)/include
pyaff4_la_LIBADD  	= ../lib/libaff4.la
pyaff4_la_LDFLAGS     	= -module $(PYTHON_LDFLAGS)

## These are the interface files which are automatically bound in python
BOUND_FILES = ../include/aff4_constants.h ../include/aff4_rdf.h	\
              ../include/aff4_io.h ../include/aff4_resolver.h

## Automatically generate the bindings
pyaff4.c: class_parser.py $(BOUND_FILES)
	python class_parser.py $(BOUND_FILES) > pyaff4.c