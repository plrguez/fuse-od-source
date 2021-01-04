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

char* quicksave_get_filename(void);
char* quicksave_get_current_program(void);
char* quicksave_get_label(void);
int check_if_exist_current_savestate(void);
int check_if_savestate_possible(void);
int quicksave_load(void);
int quicksave_save(void);

#endif /* FUSE_SAVESTATES_H */

