NULL=

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/base \
	-I$(srcdir)/job \
	-I$(srcdir)/gtk \
	-I$(srcdir)/demo \
	-DPACKAGE_DATA_DIR=\""$(datadir)/libfm"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/libfm/ui"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)/libfm"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(NULL)

base_SOURCES = \
	base/fm-config.c \
	base/fm-list.c \
	base/fm-path.c \
	base/fm-folder.c \
	base/fm-file-info.c \
	base/fm-mime-type.c \
	base/fm-utils.c \
	base/fm-file-launcher.c \
	base/fm-icon.c \
	base/fm-bookmarks.c \
	base/fm-nav-history.c \
	base/fm-monitor.c \
	base/fm-dummy-monitor.c \
	base/fm-archiver.c \
	base/fm-marshal.c \
	$(NULL)

job_SOURCES = \
	job/fm-job.c \
	job/fm-simple-job.c \
	job/fm-dir-list-job.c \
	job/fm-deep-count-job.c  \
	job/fm-file-ops-job.c \
	job/fm-file-info-job.c \
	job/fm-file-ops-job-xfer.c \
	job/fm-file-ops-job-delete.c \
	job/fm-file-ops-job-change-attr.c \
	$(NULL)

libfm_SOURCES = \
	fm.c \
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
	fm-gtk.c \
	$(exo_SOURCES) \
	gtk/fm-folder-view.c \
	gtk/fm-folder-model.c \
	gtk/fm-places-view.c \
	gtk/fm-app-menu-view.c \
	gtk/fm-app-chooser-dlg.c \
	gtk/fm-app-chooser-combo-box.c \
	gtk/fm-path-entry.c \
	gtk/fm-file-menu.c \
	gtk/fm-file-properties.c \
	gtk/fm-clipboard.c \
	gtk/fm-progress-dlg.c \
	gtk/fm-icon-pixbuf.c \
	gtk/fm-cell-renderer-text.c \
	gtk/fm-cell-renderer-pixbuf.c \
	gtk/fm-dnd-src.c \
	gtk/fm-dnd-dest.c \
	gtk/fm-gtk-utils.c \
	gtk/fm-thumbnail.c \
	gtk/fm-tab-label.c \
	gtk/fm-gtk-marshal.c \
	$(NULL)

libfmincludedir = $(includedir)/libfm/libfm
libfminclude_HEADERS = \
	fm.h \
	base/fm-config.h \
	base/fm-list.h \
	base/fm-path.h \
	base/fm-folder.h \
	base/fm-file-info.h \
	base/fm-mime-type.h \
	base/fm-utils.h \
	base/fm-file-launcher.h \
	base/fm-icon.h \
	base/fm-bookmarks.h \
	base/fm-nav-history.h \
	base/fm-monitor.h \
	base/fm-dummy-monitor.h \
	base/fm-archiver.h \
	base/fm-marshal.h \
	job/fm-job.h \
	job/fm-simple-job.h \
	job/fm-dir-list-job.h \
	job/fm-deep-count-job.h \
	job/fm-file-ops-job.h \
	job/fm-file-info-job.h \
	job/fm-file-ops-job-xfer.h \
	job/fm-file-ops-job-delete.h \
	job/fm-file-ops-job-change-attr.h \
	$(NULL)

libfmgtkincludedir = $(includedir)/libfm/libfm
libfmgtkinclude_HEADERS = \
	fm-gtk.h \
	gtk/fm-folder-view.h \
	gtk/fm-folder-model.h \
	gtk/fm-places-view.h \
	gtk/fm-app-menu-view.h \
	gtk/fm-app-chooser-dlg.h \
	gtk/fm-app-chooser-combo-box.h \
	gtk/fm-path-entry.h \
	gtk/fm-file-menu.h \
	gtk/fm-file-properties.h \
	gtk/fm-clipboard.h \
	gtk/fm-progress-dlg.h \
	gtk/fm-icon-pixbuf.h \
	gtk/fm-cell-renderer-text.h \
	gtk/fm-cell-renderer-pixbuf.h \
	gtk/fm-dnd-src.h \
	gtk/fm-dnd-dest.h \
	gtk/fm-gtk-utils.h \
	gtk/fm-thumbnail.h \
	gtk/fm-tab-label.h \
	gtk/fm-gtk-marshal.h \
	$(NULL)

pkginclude_HEADERS = \
	$(LIBFM_INCLUDES) \
	$(LIBFM_GTK_INCLUDES) \
	$(NULL)

lib_LTLIBRARIES = libfm.la libfm-gtk.la
libfm_la_SOURCES = \
	$(libfm_SOURCES) \
	$(NULL)

libfm_la_CFLAGS = \
	$(GIO_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

libfm_la_LIBADD = \
	$(GIO_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(INTLLIBS) \
	$(NULL)

libfm_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^fm \
	$(NULL)


libfm_gtk_la_SOURCES = \
	$(libfm_gtk_SOURCES) \
	$(NULL)

libfm_gtk_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	-I$(srcdir) \
	-I$(srcdir)/base \
	-I$(srcdir)/job \
	-Werror-implicit-function-declaration

libfm_gtk_la_LIBADD = \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(INTLLIBS) \
	libfm.la \
	$(NULL)

libfm_gtk_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^fm \
	$(NULL)

bin_PROGRAMS = libfm-pref-apps

libfm_pref_apps_SOURCES = \
	tools/libfm-pref-apps.c \
	$(NULL)

libfm_pref_apps_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

libfm_pref_apps_LDADD = \
	$(GTK_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(INTLLIBS) \
	libfm-gtk.la \
	$(NULL)


if ENABLE_DEMO
bin_PROGRAMS += libfm-demo
libfm_demo_SOURCES = \
	demo/libfm-demo.c \
	demo/main-win.c demo/main-win.h \
	$(NULL)

libfm_demo_CFLAGS = \
	$(GTK_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

libfm_demo_LDADD = \
	$(GTK_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(INTLLIBS) \
	libfm.la \
	libfm-gtk.la \
	$(NULL)
endif


# GIO module implementing some extension points

giomodules_LTLIBRARIES = libgiofm.la

libgiofm_la_SOURCES= \
	gio/module.c \
	gio/fm-app-lookup.c \
	gio/fm-app-lookup.h \
	$(NULL)

libgiofm_la_CFLAGS = \
	$(GIO_CFLAGS) \
	-DGIO_MODULE_DIR=\"$(giomodulesdir)\"	\
	-DGVFS_LOCALEDIR=\""$(localedir)"\"	\
	-DG_DISABLE_DEPRECATED \
	$(NULL)

libgiofm_la_LDFLAGS = 	\
	-export_dynamic \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^g_io_module_(load|unload|query)'
	$(NULL)

libgiofm_la_LIBADD = \
	$(GIO_LIBS) \
	$(INTLLIBS) \
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
	demo/main-win-ui.c \
	$(NULL)

CLEANFILES = \
	base/fm-marshal.h \
	base/fm-marshal.c \
	gtk/fm-gtk-marshal.h \
	gtk/fm-gtk-marshal.c \
	$(NULL)

# Wrapper program named gnome-terminal used to override the default terminal
# of glib/gio since it calls gnome-terminal when a term is needed, and this
# is hard coded in glib.
pkglib_PROGRAMS=gnome-terminal

gnome_terminal_SOURCES = \
	gnome-terminal-wrapper.c \
	$(NULL)

gnome_terminal_CFLAGS = \
	$(GLIB_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

gnome_terminal_LDADD = \
	$(GLIB_LIBS) \
	$(NULL)

# A lib to be loaede with LD_PRELOADS to override GtkFileChooserWidget
#lib_LTLIBRARIES=libfm-chooser-hack.la

#libfm_chooser_hack_la_SOURCES=libfm-chooser-hack.c
#libfm_chooser_hack_la_CFLAGS=$(GTK_CFLAGS)
#libfm_chooser_hack_la_LIBADD=$(GMODULE_LIBS)
#libfm_chooser_hack_la_LDFLAGS= \
#        -no-undefined \
#        -export-symbols-regex gtk

# Little program to optimize size of xml files
noinst_PROGRAMS=xml-purge
xml_purge_SOURCES=xml-purge.c
xml_purge_CFLAGS=$(GIO_CFLAGS)
xml_purge_LDADD=$(GIO_LIBS)
