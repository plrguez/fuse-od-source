/* memory.c: memory browser widget
   Copyright (c) 2004 Darren Salt

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

   E-mail: linux@youmustbejoking.demon.co.uk

*/

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "compat.h"
#include "debugger/debugger.h"
#include "pokefinder/pokefinder.h"
#include "ui/ui.h"
#include "widget.h"
#include "widget_internals.h"

static libspectrum_word memaddr = 0;

#define LC(X) ( (X)*8 - DISPLAY_BORDER_ASPECT_WIDTH )
#define LR(Y) ( (Y)*8 - DISPLAY_BORDER_HEIGHT )

int
widget_memory_draw( void *data )
{
  int x, y;
  char pbuf[36];

  widget_rectangle( LC(0), LR(0), 40 * 8, 16 * 8 + 4, 1 );
  widget_rectangle( LC(0), LR(16) + 2, 320, 1, 7 );

  for( y = 0; y < 16; ++y ) {
    libspectrum_word addr = memaddr + y * 8;

    sprintf( pbuf, "%04X:", addr );
    widget_printstring_right( LC(5) - 4, LR(y), 5, pbuf );

    for( x = 0; x < 8; ++x ) {
      libspectrum_byte b = readbyte_internal( addr + x );

      widget_printchar_fixed( LC(x + 29) / 8, LR(y) / 8, 7 - (y & 1), b );
      sprintf( pbuf + x * 3, "%02X ", b );
    }
    widget_printstring_fixed( LC(5) / 8, LR(y) / 8, 7 - (y & 1), pbuf );
  }

  widget_display_lines( LR(0) / 8, 17 );

  return 0;
}

void
widget_memory_keyhandler( input_key key )
{
  switch ( key ) {
#ifdef GCWZERO
  case INPUT_KEY_Home:
  case INPUT_KEY_End: /* RetroFW */
    widget_end_all( WIDGET_FINISHED_CANCEL );
    return;
#endif

#ifdef GCWZERO
  case INPUT_KEY_Alt_L:
#endif
  case INPUT_KEY_Escape:	/* Close widget */
    widget_end_widget( WIDGET_FINISHED_CANCEL );
    break;

#ifdef GCWZERO
  case INPUT_KEY_Control_L:
#endif
  case INPUT_KEY_Return:	/* Close widget */
  case INPUT_KEY_KP_Enter:
    widget_end_all( WIDGET_FINISHED_OK );
    break;

  /* Address selection */
  case INPUT_KEY_Up:
    memaddr -= 16;    widget_memory_draw( NULL ); break;
  case INPUT_KEY_Down:
    memaddr += 16;    widget_memory_draw( NULL ); break;
#ifdef GCWZERO
  case INPUT_KEY_Tab:
#else
  case INPUT_KEY_Page_Up:
#endif
    memaddr -= 128;   widget_memory_draw( NULL ); break;
#ifdef GCWZERO
  case INPUT_KEY_BackSpace:
#else
  case INPUT_KEY_Page_Down:
#endif
    memaddr += 128;   widget_memory_draw( NULL ); break;
#ifdef GCWZERO
  case INPUT_KEY_Page_Up:
#else
  case INPUT_KEY_Home:
#endif
    memaddr = 0;      widget_memory_draw( NULL ); break;
#ifdef GCWZERO
  case INPUT_KEY_Page_Down:
#else
  case INPUT_KEY_End:
#endif
    memaddr = 0xFF80; widget_memory_draw( NULL ); break;

  default:;
  }
}
