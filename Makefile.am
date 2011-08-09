include $(top_srcdir)/Makefile.decl

NULL=

SUBDIRS=src po data docs

EXTRA_DIST += \
	libfm.pc.in \
	libfm-gtk.pc.in \
	po/libfm.pot \
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	libfm.pc \
	libfm-gtk.pc \
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
