NULL=

SUBDIRS=ui

mimedir = $(datadir)/mime/packages
mime_DATA = libfm.xml

libfmconfdir = $(sysconfdir)/xdg/libfm
libfmconf_DATA = \
	libfm.conf \
	pref-apps.conf \
	$(NULL)

desktopdir=$(datadir)/applications
desktop_DATA= \
	libfm-pref-apps.desktop \
	$(NULL)

libfmdatafilesdir = $(datadir)/libfm
libfmdatafiles_DATA = \
	archivers.list \
	$(NULL)

EXTRA_DIST= \
	$(libfmconf_DATA) \
	$(mime_DATA) \
	$(libfmdatafiles_DATA) \
	$(desktop_DATA) \
	$(NULL)

