NULL=

# GtkBuilder UI definition files
uidir=$(datadir)/libfm/ui
ui_in_files= \
	file-prop.glade \
	progress.glade \
	ask-rename.glade \
	app-chooser.glade \
	preferred-apps.glade \
	exec-file.glade \
	$(NULL)
ui_DATA = $(ui_in_files:.glade=.ui)

EXTRA_DIST = \
	$(ui_in_files) \
	$(ui_DATA) \
	$(NULL)

# Purge GtkBuilder UI files
.glade.ui:
	test -f $(top_builddir)/src/xml-purge || \
		(cd $(top_builddir)/src && $(MAKE) xml-purge)
	cp $< $@
	$(top_builddir)/src/xml-purge $@
