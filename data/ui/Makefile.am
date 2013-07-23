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
	filesearch.glade \
	$(NULL)
ui_DATA = $(ui_in_files:.glade=.ui) choose-icon.ui

EXTRA_DIST = \
	$(ui_in_files) \
	$(ui_DATA) \
	choose-icon.ui \
	$(NULL)

# Purge GtkBuilder UI files
.glade.ui:
	sed 's/^ *</</' < $< > $@
