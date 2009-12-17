SUBDIRS = lib tools python2.6

pkginclude_HEADERS = include/aff4.h

ACLOCAL_AMFLAGS = -I config

deb-pkg:	
	chmod 744 debian/rules
	dpkg-buildpackage -rfakeroot