/* controlmapping.c: Control Mapping for OpenDingux
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

#include <config.h>

#include <string.h>
#include <limits.h>
#include <libspectrum.h>

#include "compat.h"
#include "utils.h"
#include "settings.h"
#include "controlmapping/controlmapping.h"
#include "controlmapping/controlmappingsettings.h"

#ifdef GCWZERO
char *mapfile = NULL;              /* Path of a .fcm file to load */

/* Automatic search after the load of snapshots or tapes */
static char* get_mapping_filename( const char* filename );

static char*
get_mapping_filename( const char* filename )
{
  const char* cfgdir;
  char buffer[ PATH_MAX ];
  char* filaneme_test;

  if ( !filename ) return NULL;

  /* Don't exist config path, no error but do nothing */
  cfgdir = compat_get_config_path(); if( !cfgdir ) return NULL;

  filaneme_test = utils_last_filename( filename, 1 );

  snprintf( buffer, PATH_MAX, "%s"FUSE_DIR_SEP_STR"%s"FUSE_DIR_SEP_STR"%s%s",
            cfgdir, "mappings", filaneme_test, ".fcm" );

  return utils_safe_strdup( buffer );
}

/* Called on emulator startup */
int
controlmapping_init( void )
{
  int error;

  control_mapping_defaults( &control_mapping_default );
  control_mapping_defaults( &control_mapping_current );

  error = control_mapping_read_config_file( &control_mapping_current );
  if( error ) return error;

  return 0;
}

void
controlmapping_register_startup( void )
{
  control_mapping_register_startup();
}

int
controlmapping_load_mapfile( const char *filename )
{
  mapfile = get_mapping_filename( filename );
  if ( !mapfile ) return 1;

  if ( control_mapping_read_config_file( &control_mapping_current ) ) return 1;

  control_mapping_copy_to_settings( &settings_current, &control_mapping_current );
  return 0;
}

int
controlmapping_save_mapfile( const char *filename )
{
  mapfile = get_mapping_filename( filename );
  if ( !mapfile ) return 1;

  control_mapping_copy_from_settings( &control_mapping_current, &settings_current );
  return control_mapping_write_config( &control_mapping_current );
}

const char*
controlmapping_get_filename( void )
{
  if ( last_filename )
    return last_filename;
  else
    return NULL;
}
#endif /* GCWZERO */
