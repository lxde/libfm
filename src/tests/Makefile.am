include $(top_srcdir)/Makefile.decl

NULL=

AM_CPPFLAGS = \
	-I../ \
	-I../base \
	-I../job \
	-I../gtk \
	$(GIO_CFLAGS) \
	$(MENU_CACHE_CFLAGS) \
	$(DBUS_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += fm-path
fm_path_SOURCES = test-fm-path.c
fm_path_LDADD= \
	$(GLIB_LIBS) \
	../libfm.la \
	$(NULL)
