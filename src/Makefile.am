SUBDIRS = lib tools

pkginclude_HEADERS = include/aff4.h

deb-pkg:	
	chmod 744 debian/rules
	dpkg-buildpackage -rfakeroot