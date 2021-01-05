/* savestates.c: Control Mapping for OpenDingux
   Copyright (c) 2021 Pedro Luis Rodrí­guez González

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

#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <mntent.h>
#include <unistd.h>
#include <libspectrum.h>

#include "fuse.h"
#include "snapshot.h"
#include "compat.h"
#include "utils.h"
#include "settings.h"
#include "ui/ui.h"
#include "savestates/savestates.h"

#ifdef GCWZERO

static const char* re_expressions[] = {
    "(([[:space:]]|[-_])*)(([(]|[[])*[[:space:]]*)(disk|tape|side|part)(([[:space:]]|[[:punct:]])*)(([abcd1234])([[:space:]]*of[[:space:]]*[1234])*)([[:space:]]*([)]|[]])*)(([[:space:]]|[-_])*)",
    NULL };

static char*
quicksave_get_current_dir(void)
{
  const char* cfgdir;
  char buffer[ PATH_MAX ];
  char* filename;

  if ( !last_filename ) return NULL;

  /* Don't exist config path, no error but do nothing */
  cfgdir = compat_get_config_path(); if( !cfgdir ) return NULL;

  filename = quicksave_get_current_program();

  if (settings_current.od_quicksave_per_machine) {
      snprintf( buffer, PATH_MAX, "%s"FUSE_DIR_SEP_STR"%s"FUSE_DIR_SEP_STR"%s"FUSE_DIR_SEP_STR"%s",
                cfgdir, "savestates", libspectrum_machine_name( machine_current->machine ), filename );
  } else {
    snprintf( buffer, PATH_MAX, "%s"FUSE_DIR_SEP_STR"%s"FUSE_DIR_SEP_STR"%s",
              cfgdir, "savestates", filename );
  }

  libspectrum_free( filename );

  return utils_safe_strdup( buffer );
}

static int
quicksave_create_dir(void)
{
  char* savestate_dir;
  struct stat stat_info;

  /* Can not determine savestate_dir */
  savestate_dir = quicksave_get_current_dir();
  if( !savestate_dir ) return 1;

  /* Create if don't exist */
  if( stat( savestate_dir, &stat_info ) ) {
    if ( errno == ENOENT ) {
      if ( compat_createdir( savestate_dir ) == -1 ) {
        ui_error( UI_ERROR_ERROR, "error creating savestate directory '%s'", savestate_dir );
        return 1;
      }
    } else {
      ui_error( UI_ERROR_ERROR, "couldn't stat '%s': %s", savestate_dir, strerror( errno ) );
      return 1;
    }
  }

  libspectrum_free( savestate_dir );

  return 0;
}

char*
quicksave_get_current_program(void)
{
  if ( !last_filename ) return NULL;

  return compat_chop_expressions( re_expressions, utils_last_filename( last_filename, 1 ) );
}

char*
quicksave_get_label(void)
{
  char* current_program;
  char* filename;
  char* label;

  current_program = quicksave_get_current_program();
  if ( !current_program ) return NULL;

  filename = utils_last_filename( quicksave_get_filename(), 1 );
  if ( !filename ) {
    libspectrum_free(current_program);
    return NULL;
  }

  label = libspectrum_new(char, 20);
  snprintf(label,20,"%s: %s",filename,current_program);
  if ( strlen(current_program) > 15 )
    memcpy( &(label[18]), ">\0", 2 );

  libspectrum_free(filename);
  libspectrum_free(current_program);

  return label;
}

int
check_if_exist_current_savestate(void)
{
  char* filename;
  int exist = 0;

  filename = quicksave_get_filename();
  if (filename) {
    exist = compat_file_exists( filename ) ? 1 : 0;
    libspectrum_free(filename);
  }

  return exist;
}

int
check_if_savestate_possible(void)
{
  char* current_program;
  int possible = 0;

  current_program = quicksave_get_current_program();
  if (current_program) {
    possible = 1;
    libspectrum_free( current_program );
  }
  return possible;
}

char*
quicksave_get_filename(void)
{
  char *current_dir;
  char buffer[ PATH_MAX ];

  current_dir = quicksave_get_current_dir();
  if ( !current_dir ) return NULL;

  snprintf( buffer, PATH_MAX, "%s"FUSE_DIR_SEP_STR"%2d%s",
            current_dir, settings_current.od_quicksave_slot, settings_current.od_quicksave_format );

  libspectrum_free( current_dir );

  return utils_safe_strdup( buffer );
}

char*
get_savestate_last_chage(void) {
  compat_fd save_state_fd;
  char* last_change;

  if ( !check_if_exist_current_savestate() )
    return NULL;

  save_state_fd = compat_file_open( quicksave_get_filename(), 0 );
  if ( !save_state_fd )
    return NULL;

  last_change = compat_file_get_time_last_change( save_state_fd );
  compat_file_close( save_state_fd );

  /* Get rid of \n */
  if ( last_change )
    last_change[ strlen( last_change ) - 1 ] = '\0';

  return last_change;
}

int
quicksave_load(void)
{
  char* filename;
  char* slot;

  /* If don't exist savestate return but don't mark error */
  if (!check_if_exist_current_savestate()) return 0;

  fuse_emulation_pause();

  filename = quicksave_get_filename();
  if (!filename) { fuse_emulation_unpause(); return 1; }

  /*
   * Dirty hack for savesstates.
   * autoload is set to 9 for load from loadstates and avoid changing last
   * loaded filename and controlmapping files
   */
  int error = utils_open_file( filename, 9, NULL );

  slot = utils_last_filename( filename, 1 );
  if (error)
    ui_error( UI_ERROR_ERROR, "Error loading state from slot %s", slot );
  else
    ui_widget_show_msg_update_info( "Loaded slot %s (%s)", slot, get_savestate_last_chage() );

  libspectrum_free( filename );
  libspectrum_free( slot );

  display_refresh_all();

  fuse_emulation_unpause();

  return error;
}

int
quicksave_save(void)
{
  char* filename;
  char* slot;

  fuse_emulation_pause();

  filename = quicksave_get_filename();
  if (!filename) { fuse_emulation_unpause(); return 1; }

  if (quicksave_create_dir()) { fuse_emulation_unpause(); return 1; }

  int error = snapshot_write( filename );

  slot = utils_last_filename( filename, 1 );
  if (error)
    ui_error( UI_ERROR_ERROR, "Error saving state to slot %s", slot );
  else
    ui_widget_show_msg_update_info( "Saved to slot %s (%s)", slot, get_savestate_last_chage() );

  libspectrum_free( filename );
  libspectrum_free( slot );

  fuse_emulation_unpause();

  return error;
}
#endif /* #ifdef GCWZERO */
