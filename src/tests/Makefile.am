include $(top_srcdir)/Makefile.decl

NULL=

AM_CPPFLAGS = \
	-I../ \
	-I../base \
	-I../actions \
	-I../job \
	$(GIO_CFLAGS) \
	-Werror-implicit-function-declaration \
	$(NULL)

noinst_PROGRAMS = $(TEST_PROGS) file-search-cli-demo

TEST_PROGS += fm-path
fm_path_SOURCES = test-fm-path.c
fm_path_LDADD= \
	../libfm.la \
	$(GIO_LIBS) \
	$(NULL)

file_search_cli_demo_SOURCES = \
	libfm-file-search-cli-demo.c \
	$(NULL)

file_search_cli_demo_CFLAGS = \
	-I../gtk \
	$(GTK_CFLAGS) \
	$(NULL)

file_search_cli_demo_LDADD = \
	$(GTK_LIBS) \
	$(INTLLIBS) \
	../libfm.la \
	../libfm-gtk.la \
	$(NULL)
