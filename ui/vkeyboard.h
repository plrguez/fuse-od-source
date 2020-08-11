/* vkeyboard.h: Virtual Keyboard
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

#ifndef FUSE_VKEYBOARD_H
#define FUSE_VKEYBOARD_H

#include <config.h>

#if VKEYBOARD
#include <libspectrum.h>
#include "input.h"

extern int vkeyboard_enabled;
void uidisplay_putpixel_alpha( int x, int y, int colour );
void uidisplay_vkeyboard( void (*print_fn)(void), int position );
void uidisplay_vkeyboard_input( void (*input_fn)(input_key key), input_key key);
void uidisplay_vkeyboard_release( void (*release_fn)(input_key key), input_key key);
void uidisplay_vkeyboard_end( void);
#endif
#endif			/* #ifndef FUSE_VKEYBOARD_H */
