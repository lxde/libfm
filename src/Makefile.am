# for use in libtool -version-info
ABI_VERSION=4:2:1

NULL=

# current dir needs to be built before tests
SUBDIRS= \
	$(ACTIONS_SUBDIR) \
	. \
	tests \
	$(NULL)

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/base \
	-I$(srcdir)/actions \
	-I$(srcdir)/job \
	-I$(srcdir)/gtk \
	-I$(srcdir)/demo \
	-DPACKAGE_DATA_DIR=\""$(datadir)/libfm"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/libfm/ui"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)/libfm"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(NULL)

glib_compat_SOURCES= \
	glib-compat.c \
	glib-compat.h \
	$(NULL)

gtk_compat_SOURCES= \
	$(glib_compat_SOURCES) \
	gtk-compat.c \
	gtk-compat.h \
	$(NULL)

if ENABLE_UDISKS
udisks_SOURCES= \
	udisks/fm-udisks.c \
	udisks/fm-udisks.h \
	udisks/g-udisks-volume-monitor.c \
	udisks/g-udisks-volume-monitor.h \
	udisks/g-udisks-device.c \
	udisks/g-udisks-device.h \
	udisks/g-udisks-volume.c \
	udisks/g-udisks-volume.h \
	udisks/g-udisks-drive.c \
	udisks/g-udisks-drive.h \
	udisks/g-udisks-mount.c \
	udisks/g-udisks-mount.h \
	udisks/dbus-utils.c \
	udisks/dbus-utils.h \
	$(NULL)
else
udisks_SOURCES=
endif

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
	base/fm-app-info.c \
	base/fm-thumbnailer.c \
	base/fm-file.c \
	base/fm-marshal.c \
	vfs/fm-vfs-menu.c \
	vfs/fm-vfs-search.c \
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
	$(glib_compat_SOURCES) \
	$(base_SOURCES) \
	$(job_SOURCES) \
	$(udisks_SOURCES) \
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
	$(gtk_compat_SOURCES) \
	$(exo_SOURCES) \
	gtk/fm-folder-view.c \
	gtk/fm-standard-view.c \
	gtk/fm-folder-model.c \
	gtk/fm-places-view.c \
	gtk/fm-places-model.c \
	gtk/fm-dir-tree-view.c \
	gtk/fm-dir-tree-model.c \
	gtk/fm-side-pane.c \
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
	gtk/fm-dnd-auto-scroll.c \
	gtk/fm-gtk-utils.c \
	gtk/fm-gtk-file-launcher.c \
	gtk/fm-thumbnail.c \
	gtk/fm-tab-label.c \
	gtk/fm-path-bar.c \
	gtk/fm-gtk-marshal.c \
	$(NULL)

libfmincludedir = $(includedir)/@PACKAGE@-@FMLIBVER@
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
	base/fm-app-info.h \
	base/fm-thumbnailer.h \
	base/fm-file.h \
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

libfmgtkincludedir = $(includedir)/@PACKAGE@-@FMLIBVER@
libfmgtkinclude_HEADERS = \
	fm-gtk.h \
	gtk/fm-folder-view.h \
	gtk/fm-standard-view.h \
	gtk/fm-folder-model.h \
	gtk/fm-places-view.h \
	gtk/fm-places-model.h \
	gtk/fm-dir-tree-view.h \
	gtk/fm-dir-tree-model.h \
	gtk/fm-side-pane.h \
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
	gtk/fm-dnd-auto-scroll.h \
	gtk/fm-gtk-utils.h \
	gtk/fm-gtk-file-launcher.h \
	gtk/fm-thumbnail.h \
	gtk/fm-tab-label.h \
	gtk/fm-path-bar.h \
	gtk/fm-sortable.h \
	gtk/fm-gtk-marshal.h \
	$(NULL)

pkginclude_HEADERS = \
	$(LIBFM_INCLUDES) \
	$(LIBFM_GTK_INCLUDES) \
	$(NULL)

EXTRA_LTLIBRARIES = libfm-gtk.la libfm-gtk3.la

lib_LTLIBRARIES = libfm.la @LIBFM_GTK_LTLIBRARIES@
libfm_la_SOURCES = \
	$(libfm_SOURCES) \
	$(NULL)

libfm_la_CFLAGS = \
	$(GIO_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	$(DBUS_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

libfm_la_LIBADD = \
	$(GIO_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(DBUS_LIBS) \
	$(INTLLIBS) \
	$(LIBFMACTIONS) \
	$(NULL)

libfm_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^fm \
	-version-info $(ABI_VERSION) \
	$(NULL)

libfm_gtk_la_SOURCES = \
	$(libfm_gtk_SOURCES) \
	$(NULL)
libfm_gtk3_la_SOURCES = $(libfm_gtk_la_SOURCES)

libfm_gtk_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	$(EXIF_CFLAGS) \
	-I$(srcdir) \
	-I$(srcdir)/base \
	-I$(srcdir)/job \
	-Werror-implicit-function-declaration
libfm_gtk3_la_CFLAGS = $(libfm_gtk_la_CFLAGS)

libfm_gtk_la_LIBADD = \
	$(GTK_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(EXIF_LIBS) \
	$(INTLLIBS) \
	libfm.la \
	$(NULL)
libfm_gtk3_la_LIBADD = $(libfm_gtk_la_LIBADD)

libfm_gtk_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^fm \
	-version-info $(ABI_VERSION) \
	-rpath $(libdir) \
	$(NULL)
libfm_gtk3_la_LDFLAGS = $(libfm_gtk_la_LDFLAGS)

EXTRA_PROGRAMS = libfm-pref-apps

bin_PROGRAMS = @LIBFM_PREF_APPS@

libfm_pref_apps_SOURCES = \
	$(gtk_compat_SOURCES) \
	tools/libfm-pref-apps.c \
	$(NULL)

libfm_pref_apps_DEPENDENCIES = \
	libfm.la \
	@LIBFM_GTK_LTLIBRARIES@ \
	$(NULL)

libfm_pref_apps_CFLAGS = \
	$(GTK_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

libfm_pref_apps_LDADD = \
	$(GTK_LIBS) \
	$(MENU_CACHE_LIBS) \
	$(INTLLIBS) \
	libfm.la \
	@LIBFM_GTK_LTLIBRARIES@ \
	$(NULL)


if ENABLE_DEMO
bin_PROGRAMS += libfm-demo libfm-file-search-cli-demo
libfm_demo_SOURCES = \
	$(gtk_compat_SOURCES) \
	demo/libfm-demo.c \
	demo/main-win.c demo/main-win.h \
	$(NULL)

libfm_demo_DEPENDENCIES = \
	libfm.la \
	@LIBFM_GTK_LTLIBRARIES@ \
	$(NULL)

libfm_demo_CFLAGS = \
	$(GTK_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

libfm_demo_LDADD = \
	$(GTK_LIBS) \
	$(INTLLIBS) \
	libfm.la \
	@LIBFM_GTK_LTLIBRARIES@ \
	$(NULL)

libfm_file_search_cli_demo_SOURCES = \
	demo/libfm-file-search-cli-demo.c \
	$(NULL)

libfm_file_search_cli_demo_CFLAGS = \
	$(GTK_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

libfm_file_search_cli_demo_LDADD = \
	$(GTK_LIBS) \
	$(INTLLIBS) \
	libfm.la \
	libfm-gtk.la \
	$(NULL)
endif


# GIO module implementing some extension points
# This only works for glib < 2.27
# Glib >= 2.27 uses x-scheme-handler.
if !HAVE_SCHEME_HANDLER

giomodules_LTLIBRARIES = libgiofm.la

libgiofm_la_SOURCES= \
	$(glib_compat_SOURCES) \
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

endif


# GObject marshallers
base/fm-marshal.h: base/fm-marshal.list
	glib-genmarshal --header --prefix=fm_marshal $? > $@.tmp && mv $@.tmp $@

base/fm-marshal.c: base/fm-marshal.list
	glib-genmarshal --body --prefix=fm_marshal $? > $@.tmp && mv $@.tmp $@

gtk/fm-gtk-marshal.h: gtk/fm-gtk-marshal.list
	glib-genmarshal --header --prefix=fm_marshal $? > $@.tmp && mv $@.tmp $@

gtk/fm-gtk-marshal.c: gtk/fm-gtk-marshal.list
	glib-genmarshal --body --prefix=fm_marshal $? > $@.tmp && mv $@.tmp $@

# Workarounds to force the build
BUILT_SOURCES= \
	base/fm-marshal.h \
	base/fm-marshal.c \
	gtk/fm-gtk-marshal.h \
	gtk/fm-gtk-marshal.c \
	$(NULL)

# Vala vapi files
VAPI_FILES = \
	vapi/libfm.vapi \
	$(NULL)

EXTRA_DIST = \
	base/fm-marshal.list \
	gtk/fm-gtk-marshal.list \
	gtk/fm-file-search-ui.c \
	gtk/exo/README \
	demo/main-win-ui.c \
	udisks/gen-binding \
	udisks/udisks.h \
	udisks/udisks-device.h \
	vfs/fm-vfs-menu.h \
	vfs/fm-vfs-search.h \
	$(VAPI_FILES) \
	$(NULL)

CLEANFILES = \
	base/fm-marshal.h \
	base/fm-marshal.c \
	gtk/fm-gtk-marshal.h \
	gtk/fm-gtk-marshal.c \
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

install-data-local:
	@if test -e "$(DESTDIR)$(includedir)/@PACKAGE@"; then \
		echo rm -rf "$(DESTDIR)$(includedir)/@PACKAGE@"; \
		rm -rf "$(DESTDIR)$(includedir)/@PACKAGE@"; \
	fi
	test -z "$(includedir)" || $(MKDIR_P) "$(DESTDIR)$(includedir)"
	@LN_S@ @PACKAGE@-@FMLIBVER@ "$(DESTDIR)$(includedir)/@PACKAGE@"

uninstall-local:
	@if test -L "$(DESTDIR)$(includedir)/@PACKAGE@"; then \
		echo rm -f "$(DESTDIR)$(includedir)/@PACKAGE@"; \
		rm -f "$(DESTDIR)$(includedir)/@PACKAGE@"; \
	fi
	-rmdir "$(DESTDIR)$(libfmincludedir)"
