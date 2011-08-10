#! /bin/sh
AC_VERSION=

AUTOMAKE=${AUTOMAKE:-automake}
AM_INSTALLED_VERSION=$($AUTOMAKE --version | sed -e '2,$ d' -e 's/.* \([0-9]*\.[0-9]*\).*/\1/')

if [ "$AM_INSTALLED_VERSION" != "1.10" \
    -a "$AM_INSTALLED_VERSION" != "1.11" ];then
    echo
    echo "You must have automake > 1.10 or 1.11 installed"
    echo "Install the appropriate package for your distribution,"
    echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
    exit 1
fi

if [ "x${ACLOCAL_DIR}" != "x" ]; then
    ACLOCAL_ARG=-I ${ACLOCAL_DIR}
fi

if gtkdocize; then
    echo "Files needed by gtk-doc are generated."
else
    echo "You need gtk-doc to build this package."
    echo "http://www.gtk.org/gtk-doc/"
    exit 1
fi

echo
echo "If you are going to 'make dist', please add configure option --enable-gtk-doc."
echo "Otherwise, API documents for libfm won't be correctly built by gtk-doc."
echo

set -x

${ACLOCAL:-aclocal$AM_VERSION} ${ACLOCAL_ARG}
${AUTOHEADER:-autoheader$AC_VERSION} --force
AUTOMAKE=$AUTOMAKE libtoolize -c --automake --force
AUTOMAKE=$AUTOMAKE intltoolize -c --automake --force
$AUTOMAKE --add-missing --copy --include-deps
${AUTOCONF:-autoconf$AC_VERSION}

rm -rf autom4te.cache
