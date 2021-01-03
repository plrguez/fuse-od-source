/* roms.c: select ROMs widget
   Copyright (c) 2003-2014 Philip Kendall
   Copyright (c) 2015 Stuart Brady

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

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fuse.h"
#include "settings.h"
#include "widget_internals.h"

static settings_info *widget_settings;
static widget_roms_info *info;

static size_t first_rom, rom_count;
int is_peripheral;

#ifdef GCWZERO
static size_t highlight_line = 0;
static void set_default_rom( input_key key );
#endif

static void print_rom( int which );

int
widget_roms_draw( void *data )
{
  int i;
  char buffer[32];
  char key[] = "\x0A ";
  if( data ) info = data;

  /* Get a copy of the current settings */
  if( !info->initialised ) {

    widget_settings = malloc( sizeof( settings_info ) );
    if( !widget_settings ) {
      ui_error( UI_ERROR_ERROR, "out of memory at %s:%d", __FILE__, __LINE__ );
      return 1;
    }

    memset( widget_settings, 0, sizeof( settings_info ) );
    settings_copy( widget_settings, &settings_current );

    info->initialised = 1;
  }

  first_rom = info->start;
  rom_count = info->count;
  is_peripheral = info->is_peripheral;

  /* Blank the main display area */
#ifdef GCWZERO
  widget_dialog_with_border( 1, 2, 30, rom_count + 5 );
#else
  widget_dialog_with_border( 1, 2, 30, rom_count + 2 );
#endif

  widget_printstring( 10, 16, WIDGET_COLOUR_TITLE, info->title );
#ifdef GCWZERO
  widget_display_lines( 2, rom_count + 5 );
#else
  widget_display_lines( 2, rom_count + 2 );
#endif

  for( i=0; i < info->count; i++ ) {

    snprintf( buffer, sizeof( buffer ), "ROM %d:", i );
    key[1] = 'A' + i;
    widget_printstring_right( 24, i*8+24, WIDGET_COLOUR_FOREGROUND, key );
    widget_printstring( 28, i*8+24, WIDGET_COLOUR_FOREGROUND, buffer );

    print_rom( i );
  }

#ifdef GCWZERO
  i++;
  widget_printstring_right( 244, i*8+24, WIDGET_COLOUR_FOREGROUND,
                       "\012A\001 = confirm changes" );
  widget_printstring( 12, i*8+24, WIDGET_COLOUR_FOREGROUND,
				       "\012X\001 = change rom" );
  i++;
  widget_printstring( 12, i*8+24, WIDGET_COLOUR_FOREGROUND,
					   "\012Y\001 = default rom" );
  i--;
  widget_display_rasters(i*8+24,16);
#endif

  return 0;
}

static void
print_rom( int which )
{
#ifdef GCWZERO
  int colour = ( which == highlight_line ) ?  WIDGET_COLOUR_HIGHLIGHT : WIDGET_COLOUR_BACKGROUND;
#endif
  const char *setting;

  setting = *( settings_get_rom_setting( widget_settings,
					 which + first_rom, is_peripheral ) );
  while( widget_stringwidth( setting ) >= 232 - 68 )
    ++setting;

#ifdef GCWZERO
  widget_rectangle( 68, which * 8 + 24, 232 - 68, 8,
		    colour );
#else
  widget_rectangle( 68, which * 8 + 24, 232 - 68, 8,
		    WIDGET_COLOUR_BACKGROUND );
#endif
  widget_printstring (68, which * 8 + 24,
				   WIDGET_COLOUR_FOREGROUND, setting );
  widget_display_rasters( which * 8 + 24, 8 );
}

#ifdef GCWZERO
void
set_default_rom( input_key key )
{
  char **setting, **default_rom;
  static int default_init = 0;
  static settings_info default_roms;

  if (!default_init) {
    settings_defaults( &default_roms );
    default_init = 1;
  }

  key -= INPUT_KEY_a;

  setting = settings_get_rom_setting( widget_settings, key + first_rom,
					is_peripheral );
  default_rom = settings_get_rom_setting( &default_roms, key + first_rom,
					is_peripheral );
  settings_set_string( setting, *default_rom );

  print_rom( key );
}
#endif

void
widget_roms_keyhandler( input_key key )
{
#ifdef GCWZERO
  int new_highlight_line = 0;
  int cursor_pressed = 0;
#endif

  switch( key ) {

#if 0
  case INPUT_KEY_Resize:	/* Fake keypress used on window resize */
    widget_roms_draw( NULL );
    break;
#endif

#ifdef GCWZERO
  case INPUT_KEY_Home:  /* Power */
  case INPUT_KEY_End:   /* RetroFW */
    widget_end_all( WIDGET_FINISHED_CANCEL );
    return;
#endif

#ifdef GCWZERO
  case INPUT_KEY_Alt_L: /* B */
#else
  case INPUT_KEY_Escape:
#endif
    widget_end_widget( WIDGET_FINISHED_CANCEL );
    return;

#ifdef GCWZERO
  case INPUT_KEY_Control_L: /* A */
#else
  case INPUT_KEY_Return:
  case INPUT_KEY_KP_Enter:
#endif
    widget_end_all( WIDGET_FINISHED_OK );
    return;

#ifdef GCWZERO
  case INPUT_KEY_space: /* X */
    key = INPUT_KEY_a + highlight_line;
    break;

  case INPUT_KEY_Shift_L: /* Y */
    set_default_rom( INPUT_KEY_a + highlight_line );
    break;

  case INPUT_KEY_Up:
  case INPUT_JOYSTICK_UP:
    if ( highlight_line ) {
      new_highlight_line = highlight_line - 1;
      cursor_pressed = 1;
    } else {
      new_highlight_line = rom_count - 1;
      cursor_pressed = 1;
    }
    break;

  case INPUT_KEY_Down:
  case INPUT_JOYSTICK_DOWN:
    if ( highlight_line + 1 < rom_count ) {
      new_highlight_line = highlight_line + 1;
      cursor_pressed = 1;
    } else {
      new_highlight_line = 0;
      cursor_pressed = 1;
    }
    break;

  case INPUT_KEY_Tab: /* L1 */
    new_highlight_line = 0;
    cursor_pressed = 1;
    break;

  case INPUT_KEY_BackSpace: /* R1 */
    new_highlight_line = rom_count - 1;
    cursor_pressed = 1;
    break;

#endif

  default:	/* Keep gcc happy */
    break;

  }

#ifdef GCWZERO
  if( cursor_pressed ) {
    int old_highlight_line = highlight_line;
    highlight_line = new_highlight_line;
    print_rom( old_highlight_line );
    print_rom( highlight_line );
  }
#endif

  if( key >= INPUT_KEY_a && key <= INPUT_KEY_z &&
      key - INPUT_KEY_a < (ptrdiff_t)rom_count ) {
    char **setting;
    char buf[32];
    widget_filesel_data data;

    key -= INPUT_KEY_a;

    snprintf( buf, sizeof( buf ), "%s - ROM %d", info->title, key );

    data.exit_all_widgets = 0;
    data.title = buf;
#if GCWZERO
    ui_widget_set_file_filter_for_class( FILTER_CLASS_ROM, 0 );
#endif
    widget_do_fileselector( &data );
    if( !widget_filesel_name ) return;

    setting = settings_get_rom_setting( widget_settings, key + first_rom,
					is_peripheral );
    settings_set_string( setting, widget_filesel_name );

    print_rom( key );
  }
}

int
widget_roms_finish( widget_finish_state finished )
{
#ifdef GCWZERO
  highlight_line = 0;
#endif
  if( finished == WIDGET_FINISHED_OK ) {
    settings_copy( &settings_current, widget_settings );
  }

  settings_free( widget_settings ); free( widget_settings );
  return 0;
}
