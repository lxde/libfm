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
pkglib_LTLIBRARIES += \
	gtk-fileprop-x-desktop.la \
	gtk-menu-actions.la \
	$(NULL)
endif

# module-specific parameters
vfs_menu_la_CFLAGS = $(MENU_CACHE_CFLAGS) -I../extra
vfs_menu_la_LIBADD = $(MENU_CACHE_LIBS) $(top_builddir)/src/libfm-extra.la

gtk_menu_actions_la_CFLAGS = -I../actions $(GTK_CFLAGS) -I../gtk
gtk_menu_actions_la_LIBADD = $(LIBFMACTIONS) $(GTK_LIBS)
gtk_menu_actions_la_DEPENDENCIES_EXTRA = $(LIBFMACTIONS)

gtk_fileprop_x_desktop_la_CFLAGS = $(GTK_CFLAGS) -I../gtk
gtk_fileprop_x_desktop_la_LIBADD = $(GTK_LIBS)
