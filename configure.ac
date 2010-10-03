#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([libfm], [0.1.14], [http://libfm.sf.net/])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MMAP
AC_CHECK_LIB(m, pow)

# Large file support
AC_ARG_ENABLE([largefile],
    AS_HELP_STRING([--enable-largefile],
        [enable Large file support (default: yes)]),
    largefile=$enableval, largefile="yes")

if test x"$largefile" = x"yes"; then
  CPPFLAGS="$CPPFLAGS -D_LARGEFILE_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64"
  AC_DEFINE(_LARGEFILE64_SOURCE, 1, [Whether to enable large file support])
  AC_DEFINE(_FILE_OFFSET_BITS, 64, [File offset bits])
fi


# intltool
IT_PROG_INTLTOOL([0.40.0])

pkg_modules="gtk+-2.0 >= 2.16.0"
PKG_CHECK_MODULES(GTK, [$pkg_modules])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

glib_modules="glib-2.0 >= 2.18.0"
PKG_CHECK_MODULES(GLIB, [$glib_modules])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

gio_modules="gthread-2.0 gio-unix-2.0 glib-2.0 >= 2.18.0"
PKG_CHECK_MODULES(GIO, [$gio_modules])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

gmodule_modules="gmodule-2.0"
PKG_CHECK_MODULES(GMODULE, [$gmodule_modules])
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

menu_modules="libmenu-cache >= 0.3.2"
PKG_CHECK_MODULES(MENU_CACHE, [$menu_modules])
AC_SUBST(MENU_CACHE_CFLAGS)
AC_SUBST(MENU_CACHE_LIBS)

AC_ARG_ENABLE(udisks,
    [AC_HELP_STRING([--enable-udisks],
        [build libfm with udisks support (Linux only) @<:@default=yes@:>@])],
    [enable_udisks="${enableval}"],
    [enable_udisks=no]
)
AM_CONDITIONAL(ENABLE_UDISKS, test x"$enable_udisks" = x"yes")
if test x"$enable_udisks" = x"yes"; then
    # turn on udisks support
    AC_DEFINE_UNQUOTED(USE_UDISKS, [1], [Enable UDisks support])

    # test for availability of dbus
    dbus_modules="dbus-glib-1"
    PKG_CHECK_MODULES(DBUS, [$dbus_modules])
    AC_SUBST(DBUS_CFLAGS)
    AC_SUBST(DBUS_LIBS)
fi

#check for gtk-doc
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

AC_ARG_ENABLE(demo,
    [AC_HELP_STRING([--enable-demo],
        [build libfm-demo file manager @<:@default=no@:>@])],
    [enable_demo="${enableval}"],
    [enable_demo=no]
)
AM_CONDITIONAL(ENABLE_DEMO, test x"$enable_demo" = x"yes")

AC_ARG_ENABLE(debug,
    [AC_HELP_STRING([--enable-debug],
        [build libfm with debug support @<:@default=no@:>@])],
    [enable_debug="${enableval}"],
    [enable_debug=no]
)
if test "$enable_debug" = "yes"; then
    # turn on debug and disable optimization
    CPPFLAGS="$CPPFLAGS -DG_ENABLE_DEBUG -O0 -g"
else
    # turn off glib debug checks
    CPPFLAGS="$CPPFLAGS -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
fi

# GIO extension module
giomodulesdir=$libdir/gio/modules
AC_SUBST(giomodulesdir)


GETTEXT_PACKAGE=libfm
AC_SUBST(GETTEXT_PACKAGE)

# Generate po/LINGUAS on the fly rather than relying on translators
# to maintain it manually. This also overcome the problem that Transifex
# cannot add a language to po/LINGUAS if a new po file is submitted.
rm -f $srcdir/po/LINGUAS
for po_file in `ls $srcdir/po/*.po | sort`;
do
    lang=`echo "$po_file" | sed "s|.*/po/\(.*\)\.po|\1|g"`
    echo $lang >> $srcdir/po/LINGUAS
done

AM_GLIB_GNU_GETTEXT
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    src/tests/Makefile
    data/Makefile
    data/ui/Makefile
    po/Makefile.in
    docs/Makefile
    docs/reference/Makefile
    docs/reference/libfm/Makefile
    libfm.pc
    libfm-gtk.pc
])

AC_OUTPUT

if test x"$sysconfdir" != x'/etc'; then
    echo
    echo
    echo 'Error: sysconfdir is not /etc.'
    echo 'Please consider passing --sysconfdir=/etc to configure.';
    echo 'Otherwise default config files will be installed to wrong place.'
    echo
    echo
fi
