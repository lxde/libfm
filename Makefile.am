include $(top_srcdir)/Makefile.decl

NULL=

# a little trick against gtk-doc is here
@ENABLE_GTK_DOC_TRUE@SUBDIRS_DOCS = docs
@ENABLE_GTK_DOC_FALSE@EXTRA_DIST_DOCS = \
	gtk-doc.make \
	docs/Makefile.am \
	docs/Makefile.in \
	docs/reference/Makefile.am \
	docs/reference/Makefile.in \
	docs/reference/libfm/Makefile.am \
	docs/reference/libfm/Makefile.in \
	docs/reference/libfm/libfm-docs.xml \
	docs/reference/libfm/libfm-sections.txt \
	docs/reference/libfm/version.xml.in

SUBDIRS = src po data $(SUBDIRS_DOCS)

EXTRA_DIST += \
	libfm.pc.in \
	libfm-gtk.pc.in \
	libfm-gtk3.pc.in \
	po/libfm.pot \
	$(EXTRA_DIST_DOCS) \
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	libfm.pc \
	libfm-gtk.pc \
	libfm-gtk3.pc \
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
