/* savestates.h: Control Mapping for OpenDingux
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


#ifndef FUSE_SAVESTATES_H
#define FUSE_SAVESTATES_H

#include <libspectrum.h>
#include "settings.h"

#define MAX_SAVESTATES 100

int quicksave_create_dir(void);
char* quicksave_get_filename(int slot);
char* savestate_get_screen_filename(int slot);
char* quicksave_get_current_program(void);
char* quicksave_get_current_dir(void);
char* quicksave_get_label(int slot);
int check_current_savestate_exist(int slot);
int check_current_savestate_exist_savename(char* savename);
int check_any_savestate_exist(void);
int check_if_savestate_possible(void);
int quicksave_load(void);
int quicksave_save(void);
char* get_savestate_last_change(int slot);
int savestate_write( const char *savestate );
int savestate_read( const char *savestate );

#endif /* FUSE_SAVESTATES_H */

