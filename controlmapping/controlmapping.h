/* controlmapping.h: Control Mapping for OpenDingux
   Copyright (c) 2020 Pedro Luis Rodríguez González

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: pl.rguez@gmail.com

*/


#ifndef FUSE_CONTROLMAPPING_H
#define FUSE_CONTROLMAPPING_H

#include <libspectrum.h>
#include "settings.h"

extern char *mapfile;
extern libspectrum_class_t mapfile_class;
extern char *defaultmapfile;
extern settings_info  settings_old;

int controlmapping_load_default_mapfile( );
int controlmapping_save_default_mapfile( );
int controlmapping_eject_mapfile( libspectrum_class_t class );
int controlmapping_load_mapfile( const char *filename, libspectrum_class_t class, int is_autoload );
int controlmapping_load_mapfile_with_class( const char *filename, libspectrum_class_t class, int is_autoload );
int controlmapping_save_current_mapfile( void );
int controlmapping_save_mapfile( const char *filename );
int controlmapping_load_from_file( const char *filename, int current );
int controlmapping_save_to_file( const char *filename );
void controlmapping_register_startup( void );
const char* controlmapping_get_filename( void );
void controlmapping_set_defaults( settings_info *settings );
void controlmapping_reset_to_defaults( settings_info *settings );
int controlmapping_something_changed( settings_info *settings );
int controlmapping_something_changed_defaults( settings_info *settings );
int controlmapping_different_from_defaults( settings_info *settings );
void controlmapping_check_settings_changed( settings_info *settings );

#endif /* FUSE_CONTROLMAPPING_H */
