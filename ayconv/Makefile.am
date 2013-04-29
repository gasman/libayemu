bin_PROGRAMS		= ayconv
ayconv_SOURCES		= ayconv.c

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: dist

EXTRA_DIST = bootstrap

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpm -ta $(PACKAGE)-$(VERSION).tar.gz
