AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)/libfm-demo"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/libfm-demo/ui"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS = libfm-demo

libfm_SOURCES = \
	fm.c fm.h \
	fm-job.c fm-job.h \
	fm-dir-list-job.c fm-dir-list-job.h \
	fm-folder.c fm-folder.h \
	fm-file-info.c fm-file-info.h \
	fm-mime-type.c fm-mime-type.h \
	fm-utils.c fm-utils.h \
	fm-marshal.c fm-marshal.h

exo_SOURCES = \
	exo/exo-icon-view.c \
	exo/exo-private.c \
	exo/exo-tree-view.c \
	exo/exo-icon-view.h \
	exo/exo-private.h \
	exo/exo-tree-view.h \
	exo/exo-marshal.c \
	exo/exo-string.c \
	exo/exo-marshal.h \
	exo/exo-string.h


libfm_gtk_SOURCES = \
	$(exo_SOURCES) \
	fm-folder-view.c fm-folder-view.h \
	fm-folder-model.c fm-folder-model.h \
	fm-path-entry.c fm-path-entry.h \
	fm-file-menu.c fm-file-menu.h \
	fm-gtk-marshal.c fm-gtk-marshal.h

libfm_demo_SOURCES = \
	$(libfm_SOURCES) \
	$(libfm_gtk_SOURCES) \
	libfm-demo.c \
	main-win.c main-win.h

# GObject marshallers
fm-marshal.h: fm-marshal.list
	glib-genmarshal --header --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

fm-marshal.c: fm-marshal.list
	glib-genmarshal --body --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

fm-gtk-marshal.h: fm-gtk-marshal.list
	glib-genmarshal --header --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

fm-gtk-marshal.c: fm-gtk-marshal.list
	glib-genmarshal --body --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

# Workarounds to force the build
BUILT_SOURCES= \
	fm-marshal.h \
	fm-marshal.c \
	fm-gtk-marshal.h \
	fm-gtk-marshal.c

EXTRA_DIST = \
	fm-marshal.list \
	fm-gtk-marshal.list

CLEANFILES = \
	fm-marshal.h \
	fm-marshal.c \
	fm-gtk-marshal.h \
	fm-gtk-marshal.c

libfm_demo_CFLAGS = \
	@GTK_CFLAGS@ \
	-Werror-implicit-function-declaration

libfm_demo_LDADD = @GTK_LIBS@ $(INTLLIBS)


# A lib to be loaede with LD_PRELOADS to override GtkFileChooserWidget
#lib_LTLIBRARIES=libfm-chooser-hack.la

#libfm_chooser_hack_la_SOURCES=libfm-chooser-hack.c
#libfm_chooser_hack_la_CFLAGS=@GTK_CFLAGS@
#libfm_chooser_hack_la_LIBADD=@GMODULE_LIBS@
#libfm_chooser_hack_la_LDFLAGS= \
#        -no-undefined \
#        -export-symbols-regex gtk
