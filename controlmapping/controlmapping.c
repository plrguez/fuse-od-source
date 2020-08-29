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
#include "ui/ui.h"
#include "controlmapping/controlmapping.h"
#include "controlmapping/controlmappingsettings.h"
#include "peripherals/joystick.h"

#ifdef GCWZERO
char *mapfile = NULL;              /* Path of a .fcm file to load */
libspectrum_class_t mapfile_class;
settings_info settings_old;
static control_mapping_info control_mapping_tmp;

static void
controlmapping_enable_joystick_kempston( void )
{
  /* Enable Kempston joystick if there is some mapping to it.
     Kemposton joystick do not require restart and actually hot activate it
     is supported.
     Other peripherals, as fuller, need restart and the mapping control is
     not designed to change hardware. */
  if ( !settings_current.control_mapping_enable_kempston_joy ) return;
  if ( !settings_current.joy_kempston &&
        ( settings_current.joystick_1_output == JOYSTICK_TYPE_KEMPSTON ||
          settings_current.joystick_2_output == JOYSTICK_TYPE_KEMPSTON ||
          settings_current.joystick_keyboard_output == JOYSTICK_TYPE_KEMPSTON ) ) {
    settings_current.joy_kempston = 1;
    periph_update();
  }
}

/* Stablish current settings and control mapping defaults */
void
controlmapping_set_defaults( settings_info *settings ) {
  control_mapping_copy_from_settings( &control_mapping_default, settings );
}

/* Stablish defaults as control mapping settings */
void
controlmapping_reset_to_defaults( settings_info *settings ) {
  control_mapping_copy_to_settings( settings, &control_mapping_default );
}

void
controlmapping_register_startup( void )
{
  control_mapping_register_startup();
}

int
controlmapping_load_default_mapfile( )
{
  if ( !control_mapping_read_config_file( &control_mapping_default, defaultmapfile ) ) {
    control_mapping_copy( &control_mapping_default_old, &control_mapping_default );
    return 0;
  } else
    return 1;
}

int
controlmapping_save_default_mapfile( )
{
  if ( !control_mapping_write_config( &control_mapping_default, defaultmapfile ) ) {
    control_mapping_copy( &control_mapping_default_old, &control_mapping_default );
    return 0;
  } else
    return 1;
}

int
controlmapping_eject_mapfile( libspectrum_class_t class )
{
  /* Only unload if was the last mapfile class */
  if ( class == mapfile_class )
    return controlmapping_load_mapfile( NULL, LIBSPECTRUM_CLASS_UNKNOWN, 1 );
  else
    return 0;
}

int
controlmapping_load_mapfile( const char *filename, libspectrum_class_t class, int is_autoload )
{
  char *old_mapfile;
  int error;

  /* Mapping per game don't active o unset mapfile */
  if ( !settings_current.control_mapping_per_game ) {
    if ( mapfile && !filename ) libspectrum_free( mapfile );
    mapfile = NULL;
    return 0;
  }

  /* We are changing mapfile? Save previous */
  old_mapfile = mapfile;
  mapfile = get_mapping_filename( filename );
  mapfile_class = class;

  /* Different loads but same mapfile? (F.example SIDE B of tape
     Maintain current mapfilen */
  if ( old_mapfile && mapfile && !strcmp( old_mapfile,mapfile ) ) {
    /* Releoad of snapshot whithout joystick kempston configured */
    controlmapping_enable_joystick_kempston();
    return 0;
  }

  /* Autosave current mapfile preiovusly to load new mapfile */
  if ( old_mapfile ) {
    /* Auto-save, changed mapping and Something changed or don't yet created file? */;
    if ( settings_current.control_mapping_autosave )
      /* Media eject/cleared or load new */
      if ( ( !mapfile || strcmp( old_mapfile, mapfile ) ) &&
           ( control_mapping_something_changed( &control_mapping_current, &settings_current ) || !compat_file_exists( old_mapfile ) ) )
        controlmapping_save_to_file( old_mapfile );
    libspectrum_free( old_mapfile );
  }

  /* We are auto-loading */
  if ( is_autoload && !settings_current.control_mapping_autoload ) return 0;

  /* initialization with defaults if not checked last loaded as default */
  if ( !settings_current.control_mapping_not_detached_defaults )
    control_mapping_copy_to_settings( &settings_current, &control_mapping_default );

  error = controlmapping_load_from_file( mapfile, 1 );
  /* Activate kempston joystick if not conifigured even if not mapdile exist yet */
  if ( error && mapfile ) controlmapping_enable_joystick_kempston();

  return error;
}

int
controlmapping_load_mapfile_with_class( const char *filename, libspectrum_class_t class, int is_autoload )
{
  switch ( class ) {
  case LIBSPECTRUM_CLASS_TAPE:
  case LIBSPECTRUM_CLASS_SNAPSHOT:
  case LIBSPECTRUM_CLASS_DISK_PLUS3:
  case LIBSPECTRUM_CLASS_DISK_DIDAKTIK:
  case LIBSPECTRUM_CLASS_DISK_PLUSD:
  case LIBSPECTRUM_CLASS_DISK_OPUS:
  case LIBSPECTRUM_CLASS_DISK_TRDOS:
  case LIBSPECTRUM_CLASS_DISK_GENERIC:
  case LIBSPECTRUM_CLASS_CARTRIDGE_IF2:
  case LIBSPECTRUM_CLASS_MICRODRIVE:
  case LIBSPECTRUM_CLASS_CARTRIDGE_TIMEX:
  case LIBSPECTRUM_CLASS_RECORDING:
    if ( filename )
      return controlmapping_load_mapfile( filename, class, 1 );
    else
      return controlmapping_eject_mapfile( class );

  default:
    return controlmapping_load_mapfile( NULL, class, 1 );
  }
}

int
controlmapping_load_from_file( const char *filename, int current )
{
  if ( !filename ) return 1;

  if ( control_mapping_read_config_file( &control_mapping_tmp, filename ) ) return 1;
  if ( current )
    control_mapping_copy( &control_mapping_current, &control_mapping_tmp );

  control_mapping_copy_to_settings( &settings_current, &control_mapping_tmp );
  if ( settings_current.control_mapping_not_detached_defaults )
    control_mapping_copy( &control_mapping_default, &control_mapping_tmp );
  controlmapping_enable_joystick_kempston();
  return 0;
}

int
controlmapping_save_current_mapfile( void )
{
  if ( !mapfile ) return 1;

  /* control mapping file exist but nothing changed */
  if( compat_file_exists( mapfile ) &&
      !control_mapping_something_changed( &control_mapping_current, &settings_current ) )
    return 1;

  return controlmapping_save_to_file( mapfile );
}

int
controlmapping_save_mapfile( const char *filename )
{
  mapfile = get_mapping_filename( filename );
  return controlmapping_save_current_mapfile();
}

int
controlmapping_save_to_file( const char *filename )
{
  if ( !filename ) return 1;

  control_mapping_copy_from_settings( &control_mapping_current, &settings_current );
  return control_mapping_write_config( &control_mapping_current, filename );
}


const char*
controlmapping_get_filename( void )
{
  return get_mapping_filename( mapfile );
}

int
controlmapping_something_changed( settings_info *settings )
{
  return control_mapping_something_changed( &control_mapping_current, settings );
}

int
controlmapping_something_changed_defaults( settings_info *settings )
{
  return control_mapping_something_changed( &control_mapping_default_old, settings );
}

int
controlmapping_different_from_defaults( settings_info *settings )
{
  return control_mapping_something_changed( &control_mapping_default, settings );
}

void
controlmapping_check_settings_changed( settings_info *settings ) {
  /* Control mapping per game: Enabling */
  if ( settings->control_mapping_per_game && !settings_old.control_mapping_per_game  ) {
    /* Initilize */
    control_mapping_init( NULL );
    /* Autoload control mapping files if some file is loaded */
    controlmapping_load_mapfile_with_class( last_filename, last_class, 1 );

  /* Enabled and not change */
  } else if ( settings->control_mapping_per_game && settings_old.control_mapping_per_game  ) {
    /* Not detached defaults: Disabling. Load current defaults, only if not yet loaded */
    if ( !settings->control_mapping_not_detached_defaults && settings_old.control_mapping_not_detached_defaults )
      controlmapping_load_default_mapfile();

    /* Not detached defaults: Enabling. Save previously current defaults */
    else if ( settings->control_mapping_not_detached_defaults && !settings_old.control_mapping_not_detached_defaults )
      controlmapping_save_default_mapfile();

  /* Control mapping per game: Disabling */
  } else if ( !settings->control_mapping_per_game && settings_old.control_mapping_per_game ) {
    /* Save current mapfile */
    controlmapping_save_current_mapfile();

    /* Not detached defaults: diabled. Save defaults and set current settings as new defaults */
    if ( !settings_old.control_mapping_not_detached_defaults ) {
      controlmapping_save_default_mapfile();
      controlmapping_reset_to_defaults( settings );
    }

    /* Unassign mapfile */
    controlmapping_load_mapfile_with_class( NULL, LIBSPECTRUM_CLASS_UNKNOWN, 1 );
  }

  /* Save actual configuration for detect changes */
  settings_copy( &settings_old, settings );
}
#endif /* GCWZERO */
