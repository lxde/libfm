NULL=

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/base \
	-I$(srcdir)/job \
	-I$(srcdir)/gtk \
	-I$(srcdir)/demo \
	-DPACKAGE_DATA_DIR=\""$(datadir)/libfm"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/libfm/ui"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS = libfm-demo

base_SOURCES = \
	base/fm-list.c base/fm-list.h \
	base/fm-path.c base/fm-path.h \
	base/fm-folder.c base/fm-folder.h \
	base/fm-file-info.c base/fm-file-info.h \
	base/fm-mime-type.c base/fm-mime-type.h \
	base/fm-utils.c base/fm-utils.h \
	base/fm-icon.c base/fm-icon.h \
	base/fm-marshal.c base/fm-marshal.h \
	$(NULL)

job_SOURCES = \
	job/fm-job.c job/fm-job.h \
	job/fm-dir-list-job.c job/fm-dir-list-job.h \
	job/fm-deep-count-job.c job/fm-deep-count-job.h \
	job/fm-file-ops-job.c job/fm-file-ops-job.h \
	job/fm-file-info-job.c job/fm-file-info-job.h \
	job/fm-file-ops-job-xfer.c job/fm-file-ops-job-xfer.h \
	job/fm-file-ops-job-delete.c job/fm-file-ops-job-delete.h \
	$(NULL)

libfm_SOURCES = \
	fm.c fm.h \
	$(base_SOURCES) \
	$(job_SOURCES) \
	$(NULL)

exo_SOURCES = \
	gtk/exo/exo-icon-view.c \
	gtk/exo/exo-private.c \
	gtk/exo/exo-tree-view.c \
	gtk/exo/exo-icon-view.h \
	gtk/exo/exo-private.h \
	gtk/exo/exo-tree-view.h \
	gtk/exo/exo-marshal.c \
	gtk/exo/exo-string.c \
	gtk/exo/exo-marshal.h \
	gtk/exo/exo-string.h \
	$(NULL)


libfm_gtk_SOURCES = \
	$(exo_SOURCES) \
	gtk/fm-folder-view.c gtk/fm-folder-view.h \
	gtk/fm-folder-model.c gtk/fm-folder-model.h \
	gtk/fm-path-entry.c gtk/fm-path-entry.h \
	gtk/fm-file-menu.c gtk/fm-file-menu.h \
	gtk/fm-file-properties.c gtk/fm-file-properties.h \
	gtk/fm-clipboard.c gtk/fm-clipboard.h \
	gtk/fm-progress-dlg.c gtk/fm-progress-dlg.h \
	gtk/fm-file-ops.c gtk/fm-file-ops.h \
	gtk/fm-icon-pixbuf.c gtk/fm-icon-pixbuf.h \
	gtk/fm-cell-renderer-text.c gtk/fm-cell-renderer-text.h \
	gtk/fm-dnd-src.c gtk/fm-dnd-src.h \
	gtk/fm-gtk-marshal.c gtk/fm-gtk-marshal.h \
	$(NULL)

libfm_demo_SOURCES = \
	$(libfm_SOURCES) \
	$(libfm_gtk_SOURCES) \
	demo/libfm-demo.c \
	demo/main-win.c demo/main-win.h \
	$(NULL)

# GObject marshallers
base/fm-marshal.h: base/fm-marshal.list
	glib-genmarshal --header --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

base/fm-marshal.c: base/fm-marshal.list
	glib-genmarshal --body --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

gtk/fm-gtk-marshal.h: gtk/fm-gtk-marshal.list
	glib-genmarshal --header --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

gtk/fm-gtk-marshal.c: gtk/fm-gtk-marshal.list
	glib-genmarshal --body --prefix=fm_marshal $^ > $@.tmp && mv $@.tmp $@

# Workarounds to force the build
BUILT_SOURCES= \
	base/fm-marshal.h \
	base/fm-marshal.c \
	gtk/fm-gtk-marshal.h \
	gtk/fm-gtk-marshal.c \
	$(NULL)

EXTRA_DIST = \
	base/fm-marshal.list \
	gtk/fm-gtk-marshal.list \
	$(NULL)

CLEANFILES = \
	base/fm-marshal.h \
	base/fm-marshal.c \
	gtk/fm-gtk-marshal.h \
	gtk/fm-gtk-marshal.c \
	$(NULL)

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
