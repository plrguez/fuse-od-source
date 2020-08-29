/* paths.c: Directory-related compatibility routines for OpenDingux (linux-uclibc)
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

#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <mntent.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>

#include "compat.h"
#include "utils.h"
#include "ui/ui.h" /* For launch error messages */

#ifdef GCWZERO
static int   create_config_path( const char *config_path );

/*
 * Create config path if it not exists
 *
 *  1 - Error creting path supplied
 *  0 - Path created or already exist
 */
int create_config_path( const char *config_path )
{
  struct stat stat_info;
  
  if( stat( config_path, &stat_info ) ) {
    if ( errno == ENOENT ) {
      if ( compat_createdir( config_path ) == -1 ) {
        ui_error( UI_ERROR_ERROR, "error creating config directory '%s'", config_path );
        return 1;
      }
    } else {
      ui_error( UI_ERROR_ERROR, "couldn't stat '%s': %s", config_path, strerror( errno ) );
      return 1;
    }
  }

  return 0;
}

/*
 *  Create config paths used for OpenDingux/RetroFW
 *
 *  1 - Failed to create all paths
 *  0 - All paths created or already exists
 */
int
compat_create_config_paths( const char *config_path ) {
  char next_path[ PATH_MAX ];

  if ( create_config_path( config_path ) )
    return 1;

  snprintf( next_path, PATH_MAX, "%s" FUSE_DIR_SEP_STR "%s", config_path, "roms" );
  if ( create_config_path( next_path ) )
    return 1;

  snprintf( next_path, PATH_MAX, "%s" FUSE_DIR_SEP_STR "%s", config_path, "mappings" );
  if ( create_config_path( next_path ) )
    return 1;

  return 0;
}
#endif /* GCWZERO */

