## Makefile.am -- Process this file with automake to produce Makefile.in

NULL =
# we use glib/gio/libfm only by default
AM_CPPFLAGS = \
	-I.. -I../base -I../job \
	-Werror-implicit-function-declaration \
	$(GIO_CFLAGS)

# tell libtool we compile module not library
AM_LDFLAGS = -no-undefined -module -avoid-version $(GIO_LIBS)

# the module should be linked only with libfm, not libfm-gtk or whatever
LIBS += $(top_builddir)/src/libfm.la

# change default and put modules below usual place
pkglibdir = $(libdir)/@PACKAGE@/modules

# modules to compile
pkglib_LTLIBRARIES = \
	vfs-menu.la \
	vfs-search.la \
	$(NULL)

if ENABLE_GTK
if HAVE_ACTIONS
pkglib_LTLIBRARIES += gtk-menu-actions.la
endif
pkglib_LTLIBRARIES += \
	gtk-fileprop-x-desktop.la \
	gtk-fileprop-x-shortcut.la \
	gtk-menu-trash.la \
	$(NULL)
endif

# install only .so files, we don't need .la files
install-exec-hook:
	rm -f $(DESTDIR)$(pkglibdir)/*.la
	rm -f $(DESTDIR)$(pkglibdir)/*.a

# module-specific parameters
vfs_menu_la_CFLAGS = $(MENU_CACHE_CFLAGS) -I../extra
vfs_menu_la_LIBADD = $(MENU_CACHE_LIBS) $(top_builddir)/src/libfm-extra.la

gtk_fileprop_x_desktop_la_CFLAGS = $(GTK_CFLAGS) -I../gtk
gtk_fileprop_x_desktop_la_LIBADD = $(GTK_LIBS)

gtk_fileprop_x_shortcut_la_CFLAGS = $(GTK_CFLAGS) -I../gtk
gtk_fileprop_x_shortcut_la_LIBADD = $(GTK_LIBS)

gtk_menu_actions_la_CFLAGS = -I../actions $(GTK_CFLAGS) -I../gtk
gtk_menu_actions_la_LIBADD = $(GTK_LIBS) $(top_builddir)/src/@LIBFM_GTK_LTLIBRARIES@

gtk_menu_trash_la_CFLAGS = $(GTK_CFLAGS) -I../gtk
gtk_menu_trash_la_LIBADD = $(GTK_LIBS) $(top_builddir)/src/@LIBFM_GTK_LTLIBRARIES@
