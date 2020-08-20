/* controlmapping.c: Control Mapping for OpenDingux. Widget functions.
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

#include "widget_internals.h"
#include "controlmapping/controlmapping.h"

#ifdef GCWZERO
int widget_control_mapping_finish( widget_finish_state finished )
{
  widget_options_finish( finished );

  /* If we exited normally, actually set the options */
  if( finished == WIDGET_FINISHED_OK ) {
    controlmapping_check_settings_changed( &settings_current );

    /* Activate/Deactivate menu options */
    ui_menu_activate(UI_MENU_ITEM_JOYSTICKS_CONTROL_MAPPING, settings_current.control_mapping_per_game ? 1 : 0);
  } /* WIDGET_FINISHED_OK */

  return 0;
}
#endif
