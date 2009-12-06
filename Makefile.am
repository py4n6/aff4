SUBDIRS = lib tools

pkginclude_HEADERS = include/aff4.h

ACLOCAL_AMFLAGS = -I m4

deb-pkg:	
	chmod 744 debian/rules
	dpkg-buildpackage -rfakeroot