# The icons

icondir = $(datadir)/icons/hicolor

install-data-local:
	i=pkg_maemo-mapper; \
	$(mkinstalldirs) $(DESTDIR)$(icondir)/26x26/hildon; \
	$(mkinstalldirs) $(DESTDIR)$(icondir)/40x40/hildon; \
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/hildon; \
	$(INSTALL_DATA) $(srcdir)/$$i.26.png $(DESTDIR)$(icondir)/26x26/hildon/$$i.png; \
	$(INSTALL_DATA) $(srcdir)/$$i.40.png $(DESTDIR)$(icondir)/40x40/hildon/$$i.png; \
	$(INSTALL_DATA) $(srcdir)/$$i.64.png $(DESTDIR)$(icondir)/scalable/hildon/$$i.png;

uninstall-local:
	i=pkg_maemo-mapper; \
	rm $(DESTDIR)$(icondir)/26x26/hildon/$$i.png; \
	rm $(DESTDIR)$(icondir)/40x40/hildon/$$i.png; \
	rm $(DESTDIR)$(icondir)/scalable/hildon/$$i.png;

# The program

bin_PROGRAMS = maemo-mapper

maemo_mapper_SOURCES = maemo-mapper.c
maemo_mapper_CFLAGS = $(GTK_CFLAGS) $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(GNOME_VFS_CFLAGS) $(GCONF_CFLAGS) $(LIBXML2_CFLAGS)
maemo_mapper_LDADD = $(GTK_LIBS) $(OSSO_LIBS) $(HILDON_LIBS) $(GNOME_VFS_LIBS) $(GCONF_LIBS) $(LIBXML2_LIBS) -lbluetooth

desktopdir = $(datadir)/applications/hildon
desktop_DATA = maemo-mapper.desktop

servicedir = /usr/share/dbus-1/services
service_DATA = com.nokia.maemo_mapper.service

EXTRA_DIST =					\
        autogen.sh				\
        debian/changelog			\
        debian/compat				\
        debian/copyright			\
        debian/control				\
        debian/rules				\
        debian/maemo-mapper.links		\
        maemo-mapper.desktop			\
        com.nokia.maemo_mapper.service	\
        pkg_maemo-mapper.26.png		\
        pkg_maemo-mapper.40.png		\
        pkg_maemo-mapper.64.png
