## Process this file with automake to produce Makefile.in
## Copyright (c) 2020 Pedro Luis Rodríguez González

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along
## with this program; if not, write to the Free Software Foundation, Inc.,
## 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
##
## Author contact information:
##
## E-mail: pl.rguez@gmail.com

if BUILD_GCWZERO

fuse_SOURCES += \
                controlmapping/controlmapping.c \
                controlmapping/controlmappingsettings.c

noinst_HEADERS += \
                  controlmapping/controlmapping.h \
                  controlmapping/controlmappingsettings.h

EXTRA_DIST += controlmapping/settings.pl \
	      controlmapping/settings-header.pl

BUILT_SOURCES += $(control_mapping_build_sources)

endif

CLEANFILES += $(control_mapping_build_sources)

control_mapping_build_sources = controlmapping/controlmappingsettings.c \
                                controlmapping/controlmappingsettings.h

controlmapping/controlmappingsettings.c: $(srcdir)/controlmapping/settings.pl $(srcdir)/settings.dat
	$(AM_V_GEN)$(PERL) -I$(srcdir)/perl $(srcdir)/controlmapping/settings.pl $(srcdir)/settings.dat > $@.tmp && mv $@.tmp $@

controlmapping/controlmappingsettings.h: $(srcdir)/controlmapping/settings-header.pl $(srcdir)/settings.dat
	$(AM_V_GEN)$(PERL) -I$(srcdir)/perl $(srcdir)/controlmapping/settings-header.pl $(srcdir)/settings.dat > $@.tmp && mv $@.tmp $@
