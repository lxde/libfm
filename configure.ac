#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([libfm], [1.0], [http://pcmanfm.sorceforge.net/])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_LIBTOOL

# automake 1.12 requires AM_PROG_AR.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

# Vala
AM_PROG_VALAC([0.13.0])

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

# modules required
pkg_modules="gtk+-2.0 >= 2.16.0"
PKG_CHECK_MODULES(GTK, [$pkg_modules])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

glib_modules="glib-2.0 >= 2.24.0 gobject-2.0 >= 2.26.0"
PKG_CHECK_MODULES(GLIB, [$glib_modules])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

gio_modules="gthread-2.0 gio-unix-2.0 >= 2.26.0 glib-2.0 >= 2.26.0 gobject-2.0"
PKG_CHECK_MODULES(GIO, [$gio_modules])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

pango_modules="pango >= 1.16.0"
PKG_CHECK_MODULES(PANGO, [$pango_modules])
AC_SUBST(PANGO_CFLAGS)
AC_SUBST(PANGO_LIBS)

gmodule_modules="gmodule-2.0"
PKG_CHECK_MODULES(GMODULE, [$gmodule_modules])
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

menu_modules="libmenu-cache >= 0.3.2"
PKG_CHECK_MODULES(MENU_CACHE, [$menu_modules])
AC_SUBST(MENU_CACHE_CFLAGS)
AC_SUBST(MENU_CACHE_LIBS)

# special checks for glib/gio 2.27 since it contains backward imcompatible changes.
# glib 2.26 uses G_DESKTOP_APP_INFO_LOOKUP_EXTENSION_POINT_NAME extension point while
# glib 2.27 uses x-scheme-handler/* mime-type to register handlers.
PKG_CHECK_MODULES(GLIB2_27, [glib-2.0 >= 2.27.0],
    [have_scheme_handler="yes"],
    [have_scheme_handler="no"])
AM_CONDITIONAL(HAVE_SCHEME_HANDLER, test "x$have_scheme_handler" = "xyes")
if test "x$have_scheme_handler" = "xyes"; then
    AC_DEFINE_UNQUOTED(HAVE_SCHEME_HANDLER, [1], [Have x-scheme/uri handler support])
fi

#optional features

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

AC_ARG_ENABLE([exif],
    AS_HELP_STRING([--disable-exif],
        [disable libexif which is used for faster thumbnail loading.]),
    [enable_exif="${enableval}"],
    [enable_exif=auto]
)
AM_CONDITIONAL(ENABLE_EXIF, test x"$enable_exif" != x"no")
AS_IF([test x"$enable_exif" != x"no"], [
    # test for availability of libexif
    exif_modules="libexif"
    PKG_CHECK_MODULES(EXIF, [$exif_modules],
        [# turn on libexif support
        enable_exif=yes
        AC_DEFINE_UNQUOTED(USE_EXIF, [1], [Enable libexif])
        AC_SUBST(EXIF_CFLAGS)
        AC_SUBST(EXIF_LIBS)],
        [AS_IF([test x"$enable_exif" = x"auto"], [enable_exif=no], [
            AC_ERROR([Package requirements (libexif) were not met:

$EXIF_PKG_ERRORS
])])])])

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
    case "$CC" in
    gcc*)
        CPPFLAGS="$CPPFLAGS -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
        ;;
    *)
        ;;
    esac
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

dnl Fix invalid sysconfdir when --prefix=/usr
if test `eval "echo $sysconfdir"` = /usr/etc
then
    sysconfdir=/etc
fi

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    src/actions/Makefile
    src/tests/Makefile
    data/Makefile
    data/libfm-pref-apps.1
    data/ui/Makefile
    po/Makefile.in
    docs/Makefile
    docs/reference/Makefile
    docs/reference/libfm/Makefile
    docs/reference/libfm/version.xml
    libfm.pc
    libfm-gtk.pc
])

AC_OUTPUT

echo
echo "Libfm $VERSION Configuration Summary:"
echo
echo "prefix:                                                 $prefix"
echo "sysconfdir:                                             $sysconfdir"
echo "Enable compiler flags and other support for debugging:  $enable_debug"
echo "Build udisks support (Linux only, experimental):        $enable_udisks"
echo "Build with libexif for faster thumbnail loading:        $enable_exif"
echo "Build demo program src/demo/libfm-demo:                 $enable_demo"
echo "Large file support:                                     $largefile"
if test x"$have_scheme_handler" = x"no"; then
echo "GIO module for preferred apps (for glib < 2.28 only):   $giomodulesdir";
else
echo "GIO module for preferred apps (for glib < 2.28 only):   not required";
fi
echo "Build API doc with gtk-doc (required for make dist):    $enable_gtk_doc"
echo
echo

if test x"$sysconfdir" != x'/etc'; then
    echo
    echo
    echo 'Warning: sysconfdir is not /etc.'
    echo 'Please consider passing --sysconfdir=/etc to configure.';
    echo 'Otherwise default config files will be installed to wrong place.'
    echo
    echo
fi
