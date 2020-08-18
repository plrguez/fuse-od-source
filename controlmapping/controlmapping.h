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

extern char *mapfile;

int controlmapping_load_mapfile( const char *filename );
int controlmapping_save_mapfile( const char *filename );
int controlmapping_init( void );
void controlmapping_register_startup( void );
const char* controlmapping_get_filename( void );

#endif /* FUSE_CONTROLMAPPING_H */
