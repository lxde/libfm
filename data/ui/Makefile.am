NULL=

# GtkBuilder UI definition files
uidir=$(datadir)/libfm/ui
ui_in_files= \
	file-prop.glade \
	progress.glade \
	ask-rename.glade \
	app-chooser.glade \
	preferred-apps.glade \
	$(NULL)
ui_DATA = $(ui_in_files:.glade=.ui)

EXTRA_DIST = \
	$(ui_DATA) \
	$(NULL)

CLEANFILES = \
	$(ui_DATA) \
	$(NULL)

# Purge GtkBuilder UI files
%.ui: %.glade
	cp $< $@
	$(top_builddir)/src/xml-purge $@
