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
desktop_in_files= \
	libfm-pref-apps.desktop.in \
	$(NULL)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

libfmdatafilesdir = $(datadir)/libfm
libfmdatafiles_DATA = \
	archivers.list \
	$(NULL)

EXTRA_DIST= \
	$(libfmconf_DATA) \
	$(mime_DATA) \
	$(libfmdatafiles_DATA) \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(NULL)

