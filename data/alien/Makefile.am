## Process this file with automake to produce Makefile.in

SUBDIRS = nation

## Override automake so that "make install" puts these in proper place:
pkgdatadir = $(datadir)/$(PACKAGE)/alien

pkgdata_DATA =          \
	buildings.ruleset \
	cities.ruleset	\
	effects.ruleset	\
	nation_effects.ruleset \
	script.lua	\
	styles.ruleset	\
	game.ruleset	\
	governments.ruleset \
	nations.ruleset	\
	techs.ruleset	\
	terrain.ruleset	\
	units.ruleset

EXTRA_DIST = $(pkgdata_DATA)
