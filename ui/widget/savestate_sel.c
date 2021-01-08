/* savestate_sel.c: Savestates selection dialog box
   Copyright (c) 2021 Pedro Luis Rodríguez González

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

#ifdef GCWZERO
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "settings.h"
#include "fuse.h"
#include "ui/ui.h"
#include "ui/uidisplay.h"
#include "utils.h"
#include "widget_internals.h"

#include "savestates/savestates.h"

#define DIALOG_WIDTH 24
#define DIALOG_HEIGHT 22
#define DIALOG_X_POSITION 4
#define DIALOG_Y_POSITION 2
#define ENTRIES_PER_SCREEN (DIALOG_HEIGHT - 5)

typedef struct widget_stateent {
  char *name;
  char *info;
  int size;
  int slot;
  int has_screenshot;
} widget_stateent;

static struct widget_stateent **widget_savestates; /* Savestates in the current slot */
size_t widget_numsavestates; /* The number of savestates for current program */

static const char *title;
static int is_saving;

/* The savestate to return */
static char* widget_savestate_name;

/* Should we exit all widgets when we're done with this selector? */
static int exit_all_widgets;

/* The number of the savestate in the top-left corner of the current
   display, that of the savestate which the `cursor' is on, and that
   which it will be on after this keypress */
static size_t top_savestate, current_savestate, new_current_savestate;
static size_t saved_top_savestate, saved_current_savestate;
static int saved_position;
static int showing_screenshot = 0;

static char* last_program = NULL;

static utils_file black_screen;
static int previous_black_screen = 0;

static void widget_savestate_init( void );
static void savestate_print_screenshot_back( struct widget_stateent *savestate );
static char *widget_get_savestate( const char *title, int saving );
static int widget_print_all_savestates( struct widget_stateent **savestates, int numsavestates,
				       int top, int current );
static inline void widget_get_position_detail( int position, int *x, int *y );
static int widget_print_savestate( struct widget_stateent *savestate, int position,
                                   int inverted );
static int widget_print_savestate_detail( struct widget_stateent *savestate, int position,
                                          int inverted );
static void widget_print_show_screenshot_label( struct widget_stateent *savestate );
static void widget_scan_savestates( void );
static int widget_scan_compare( const struct widget_stateent **a,
				                const struct widget_stateent **b );
static int widget_search_savestates( struct widget_stateent ***namelist );
static int widget_add_savestate( int *allocated, int index, int slot,
                                      struct widget_stateent ***namelist,
                                      const char *name );

static void
savestate_print_screenshot_back( struct widget_stateent *savestate )
{
  utils_file screen;
  char* screenshot;

  if ( savestate->has_screenshot ) {
    previous_black_screen = 0;
    screenshot = savestate_get_screen_filename( savestate->slot );
    if ( !compat_file_exists( screenshot ) ) {
      libspectrum_free( screenshot );
      return;
    }

    if( utils_read_screen( screenshot, &screen ) ) {
      libspectrum_free( screenshot );
      return;
    }

    libspectrum_free( screenshot );

  } else if ( previous_black_screen ) {
    return;

  } else {
    if ( !black_screen.buffer ) {
      black_screen.length = 6912;
      black_screen.buffer = libspectrum_new( unsigned char, 6912 );
      memset( black_screen.buffer, 0, black_screen.length );
    }
    screen = black_screen;
    previous_black_screen = 1;
  }

  uidisplay_spectrum_screen( screen.buffer, 0 );
  uidisplay_frame_end();

  if ( savestate->has_screenshot ) libspectrum_free( screen.buffer );
}

static char *
widget_get_savestate( const char *title, int saving )
{
  char *savestate = NULL;

  widget_filesel_data data;

  data.exit_all_widgets = 1;
  data.title = title;

  if( saving ) {
    widget_do_savestate_selector_save( &data );
  } else {
    widget_do_savestate_selector( &data );
  }
  if( widget_savestate_name )
    savestate = utils_safe_strdup( widget_savestate_name );

  return savestate;
}

char *
ui_get_open_savestate( const char *title )
{
  return widget_get_savestate( title, 0 );
}

char *
ui_get_save_savestate( const char *title )
{
  char* savestate = widget_get_savestate( title, 1 );
  if ( savestate && check_current_savestate_exist_savename( savestate ) ) {
      const char *filename1 = strrchr( savestate, FUSE_DIR_SEP_CHR );
      filename1 = filename1 ? filename1 + 1 : savestate;

      dont_refresh_display = 1;
      ui_confirm_save_t confirm = ui_confirm_save(
          "%s already exists.\n"
          "Do you want to overwrite it?",
          filename1
        );
      dont_refresh_display = 0;

      switch( confirm ) {
      case UI_CONFIRM_SAVE_SAVE: return savestate;
      case UI_CONFIRM_SAVE_DONTSAVE: return NULL;
      case UI_CONFIRM_SAVE_CANCEL: return NULL;
      default: return NULL;
      }
  }

  return savestate;
}

static int
widget_add_savestate( int *allocated, int index, int slot,
                      struct widget_stateent ***namelist,
                      const char *name )
{
  int i; size_t length;

  (*namelist)[index] = malloc( sizeof(***namelist) );
  if( !(*namelist)[index] ) {
    for( i = 0; i < index; i++ ) {
      free( (*namelist)[i]->name );
      free( (*namelist)[i]->info );
      free( (*namelist)[i] );
    }
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

  (*namelist)[index]->slot = slot;
  char* final_name = utils_last_filename( name, 0 );
  length = strlen( final_name ) + 1;
  if( length < 16 ) length = 16;

  (*namelist)[index]->name = malloc( length );
  if( !(*namelist)[index]->name ) {
    free( (*namelist)[index] );
    for( i = 0; i < index; i++ ) {
      free( (*namelist)[i]->name );
      free( (*namelist)[i]->info );
      free( (*namelist)[i] );
    }
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

  char* info = get_savestate_last_change( slot );
  if (!info) info = strdup( "Empty" );
  int length_info = strlen( info ) + 1;
  (*namelist)[index]->info = malloc( length_info );
  if( !(*namelist)[index]->info ) {
    free( (*namelist)[index] );
    for( i = 0; i < index; i++ ) {
      free( (*namelist)[i]->name );
      free( (*namelist)[i]->info );
      free( (*namelist)[i] );
    }
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

  memcpy( (*namelist)[index]->name, final_name, length - 1 );
  (*namelist)[index]->name[ length - 1 ] = 0;

  memcpy( (*namelist)[index]->info, info, length_info - 1 );
  (*namelist)[index]->info[ length_info - 1 ] = 0;

  char* screenshot = savestate_get_screen_filename( slot );
  if ( compat_file_exists( screenshot ) )
    (*namelist)[index]->has_screenshot = 1;
  else
    (*namelist)[index]->has_screenshot = 0;

  libspectrum_free( info );
  libspectrum_free( final_name );
  libspectrum_free( screenshot );

  return 0;
}

static int
widget_scan_compare( const struct widget_stateent **a,
				const struct widget_stateent **b )
{
  return strcmp( (*a)->name, (*b)->name );
}

static int
widget_search_savestates( struct widget_stateent ***namelist )
{
  int allocated, number;
  int index = 0;

  *namelist = malloc( MAX_SAVESTATES * sizeof(**namelist) );
  if( !*namelist ) return -1;

  allocated = MAX_SAVESTATES; 
  number = 0;
  index = 0;

  for ( number = 0; number < MAX_SAVESTATES; number++ ) {
    if ( !is_saving && !check_current_savestate_exist(number) )
      continue;

    char* name;
    name = quicksave_get_filename(number);
    if( widget_add_savestate( &allocated, index, number, namelist, name ) ) {
      if ( name ) libspectrum_free( name );
      return -1;
    }
    libspectrum_free(name);

    index++;
  }

  return index;
}

static void
widget_scan_savestates( void )
{
  size_t i;

  /* Free the memory belonging to the files in the previous directory */
  for( i=0; i<widget_numsavestates; i++ ) {
    free( widget_savestates[i]->name );
    free( widget_savestates[i]->info );
    free( widget_savestates[i] );
  }

  widget_numsavestates = widget_search_savestates( &widget_savestates );

  if( widget_numsavestates == (size_t)-1 ) return;

  qsort( widget_savestates, widget_numsavestates, sizeof(struct widget_stateent*),
	 (int(*)(const void*,const void*))widget_scan_compare );
}

static void
widget_savestate_init( void )
{
  char* program;

  program = quicksave_get_current_program();
  if (!last_program || strcmp( program,last_program ) ) {
    if (last_program) libspectrum_free( last_program );
    last_program = utils_safe_strdup( program );
    saved_position = 0;
    previous_black_screen = 0;
  }
  libspectrum_free( program );

  /* Restore position and savestate selected for save */
  if ( saved_position ) {
    saved_position = 0;
    top_savestate = saved_top_savestate;
    new_current_savestate = current_savestate = saved_current_savestate;
  } else {
    if ( is_saving ) {
      top_savestate = new_current_savestate = current_savestate = settings_current.od_quicksave_slot;
    } else {
      top_savestate = new_current_savestate = current_savestate = 0;
    }
  }
}

/* Savestate selection widget */

static int
widget_savestate_selector_draw( void *data )
{
  widget_filesel_data *filesel_data = data;
  int error;

  exit_all_widgets = filesel_data->exit_all_widgets;
  title = filesel_data->title;

  widget_savestate_init();

  widget_scan_savestates();

  /* Create the dialog box */
  error = widget_dialog_with_border( DIALOG_X_POSITION, DIALOG_Y_POSITION, DIALOG_WIDTH, DIALOG_HEIGHT );
  if( error ) {
    return error;
  }

  /* Show all the savestates */
  widget_print_all_savestates( widget_savestates, widget_numsavestates,
			                   top_savestate, current_savestate );

  return 0;
}

static int
widget_print_all_savestates( struct widget_stateent **savestates, int numsavestates,
				             int top, int current )
{
  int i;
  int error;

  if ( settings_current.od_quicksave_show_back_preview )
    savestate_print_screenshot_back( widget_savestates[ current ] );

  /* Give us a clean box to start with */
  error = widget_dialog_with_border( DIALOG_X_POSITION, DIALOG_Y_POSITION, DIALOG_WIDTH, DIALOG_HEIGHT );
  if( error ) return error;

  widget_printstring( DIALOG_X_POSITION * 8 + 2, DIALOG_Y_POSITION * 8, WIDGET_COLOUR_TITLE, title );

  if( top ) widget_up_arrow( DIALOG_X_POSITION, DIALOG_Y_POSITION + 2, WIDGET_COLOUR_FOREGROUND );

  /* Print the savestates, mostly normally, but with the currently
     selected file inverted */
  for( i = top; i < numsavestates && i < top + ENTRIES_PER_SCREEN; i++ ) {
    if( i == current ) {
      widget_print_savestate( savestates[i], i-top, 1 );
    } else {
      widget_print_savestate( savestates[i], i-top, 0 );
    }
  }

  widget_printstring( DIALOG_X_POSITION * 8 + 4, (DIALOG_Y_POSITION + ENTRIES_PER_SCREEN + 3) * 8, WIDGET_COLOUR_FOREGROUND,
				     "\012A\001 = select" );

  widget_print_show_screenshot_label( savestates[ current ] );

  if( i < numsavestates )
    widget_down_arrow( DIALOG_X_POSITION, DIALOG_Y_POSITION + ENTRIES_PER_SCREEN + 1, WIDGET_COLOUR_FOREGROUND );

  /* Display that lot */
  widget_display_lines( DIALOG_Y_POSITION, DIALOG_HEIGHT );

  return 0;
}

static inline void
widget_get_position_detail( int position, int *x, int *y )
{
  *x = ( DIALOG_X_POSITION + 1 ) * 8;
  *y = ( DIALOG_Y_POSITION + 2 ) * 8 + position * 8;
}

/* Print a savestate onto the dialog box */
static int
widget_print_savestate( struct widget_stateent *savestate, int position,
				        int inverted )
{
  int x, y;

  widget_print_savestate_detail( savestate, position, inverted );

  widget_get_position_detail( position, &x, &y );
  if ( savestate->has_screenshot )
    widget_draw_submenu_arrow( x + 208, y+24, WIDGET_COLOUR_FOREGROUND );

  return 0;
}

static int
widget_print_savestate_detail( struct widget_stateent *savestate, int position,
				               int inverted )
{
  char name[4], info[26];
  int x, y;

  widget_get_position_detail( position, &x, &y );

  int foreground = WIDGET_COLOUR_FOREGROUND,

      background = inverted ? WIDGET_COLOUR_HIGHLIGHT
                            : WIDGET_COLOUR_BACKGROUND;

  widget_rectangle( x, y, 168, 8, background );

  snprintf( name, 4, "\012%s\001", savestate->name );
  name[3] = '\0';

  widget_printstring( x + 1, y, foreground, name );

  snprintf( info, 26, "%s", savestate->info );
  info[25] = '\0';

  widget_printstring( x + 24, y, foreground, info );

  return 0;
}

static void
widget_print_show_screenshot_label( struct widget_stateent *savestate )
{
  int col = savestate->has_screenshot ? WIDGET_COLOUR_FOREGROUND :  WIDGET_COLOUR_DISABLED;
  widget_printstring( ( DIALOG_X_POSITION + 10 ) * 8 + 4, (DIALOG_Y_POSITION + ENTRIES_PER_SCREEN + 3) * 8, col,
				        "\012X\001 = show screenshot" );
}

int
widget_savestate_selector_finish( widget_finish_state finished )
{
  /* Return with null if we didn't finish cleanly */
  if( finished != WIDGET_FINISHED_OK ) {
    if( widget_savestate_name ) free( widget_savestate_name );
    widget_savestate_name = NULL;
  }

  saved_position = 1;
  saved_top_savestate = top_savestate;
  saved_current_savestate = current_savestate;

  return 0;
}

int
widget_savestate_selector_load_draw( void *data )
{
  is_saving = 0;
  return widget_savestate_selector_draw( data );
}

int
widget_savestate_selector_save_draw( void *data )
{
  is_saving = 1;
  return widget_savestate_selector_draw( data );
}

void
widget_savestate_selector_keyhandler( input_key key )
{
  if( widget_numsavestates == 0 ) {
    if( key == INPUT_KEY_Escape ) widget_end_widget( WIDGET_FINISHED_CANCEL );
    return;
  }

  if ( showing_screenshot && !settings_current.od_quicksave_show_back_preview )
    uidisplay_frame_restore();

  new_current_savestate = current_savestate;

  switch(key) {
  case INPUT_KEY_Home: /* Power   */
  case INPUT_KEY_End:  /* RetroFW */
    widget_end_all( WIDGET_FINISHED_CANCEL );
    return;

  case INPUT_KEY_Alt_L: /* B */
  case INPUT_JOYSTICK_FIRE_2:
    if ( showing_screenshot ) {
      showing_screenshot = 0;
      widget_print_all_savestates( widget_savestates, widget_numsavestates,
				  top_savestate, current_savestate );
    } else {
      widget_end_widget( WIDGET_FINISHED_CANCEL );
    }
    break;

  case INPUT_KEY_Down: /* Down */
  case INPUT_KEY_6:
  case INPUT_KEY_j:
  case INPUT_JOYSTICK_DOWN:
    if( current_savestate < widget_numsavestates - 1 ) new_current_savestate++;
    else new_current_savestate = 0;
    break;

  case INPUT_KEY_Up:    /* Up */
  case INPUT_KEY_7:	
  case INPUT_KEY_k:
  case INPUT_JOYSTICK_UP:
    if( current_savestate ) new_current_savestate--;
    else new_current_savestate = widget_numsavestates - 1;
    break;

  case INPUT_KEY_Tab: /* L1 */
    new_current_savestate = ( current_savestate > ENTRIES_PER_SCREEN ) ?
                              current_savestate - ENTRIES_PER_SCREEN   :
                              0;
    break;

  case INPUT_KEY_BackSpace: /* R1 */
    new_current_savestate = current_savestate + ENTRIES_PER_SCREEN;
    if( new_current_savestate >= widget_numsavestates )
      new_current_savestate = widget_numsavestates - 1;
    break;

  case INPUT_KEY_Page_Up:  /* L2 */
    new_current_savestate = 0;
    break;

  case INPUT_KEY_Page_Down: /* R2 */
    new_current_savestate = widget_numsavestates - 1;
    break;

  case INPUT_KEY_Control_L: /* A */
  case INPUT_KEY_Return:
  case INPUT_KEY_KP_Enter:
  case INPUT_JOYSTICK_FIRE_1:
    widget_savestate_name = strdup( widget_savestates[ current_savestate ]->name );
    if( exit_all_widgets ) {
	  widget_end_all( WIDGET_FINISHED_OK );
    } else {
	  widget_end_widget( WIDGET_FINISHED_OK );
    }
    break;

  case INPUT_KEY_Left: /* Left */
  case INPUT_JOYSTICK_LEFT:
    if ( showing_screenshot ) {
      showing_screenshot = 0;
      widget_print_all_savestates( widget_savestates, widget_numsavestates,
				  top_savestate, current_savestate );
    }
    break;

  case INPUT_KEY_space: /* X */
  case INPUT_KEY_Right: /* Right */
  case INPUT_JOYSTICK_RIGHT:
    /* widget show screenshot */
    if ( widget_savestates[ current_savestate ]->has_screenshot ) {
      if ( !settings_current.od_quicksave_show_back_preview )
        uidisplay_frame_save();
      showing_screenshot = 1;
      savestate_print_screenshot_back( widget_savestates[ current_savestate ] );
      widget_print_savestate_detail( widget_savestates[ current_savestate ], (ENTRIES_PER_SCREEN + 2), 0 );
      widget_display_lines( (ENTRIES_PER_SCREEN + 2), 1 );
    }
    break;

  default:	/* Keep gcc happy */
    break;

  }

  /* If we moved the cursor */
  if( new_current_savestate != current_savestate ) {

    /* If we've got off the top or bottom of the currently displayed
       file list, then reset the top-left corner and display the whole
       thing */
    if( new_current_savestate < top_savestate ) {

      top_savestate = new_current_savestate;
      widget_print_all_savestates( widget_savestates, widget_numsavestates,
				  top_savestate, new_current_savestate );

    } else if( new_current_savestate >= top_savestate+ENTRIES_PER_SCREEN ) {

      top_savestate = new_current_savestate;
      top_savestate -= ENTRIES_PER_SCREEN - 1;
      widget_print_all_savestates( widget_savestates, widget_numsavestates,
				  top_savestate, new_current_savestate );

    } else {

      /* Otherwise, print the current file uninverted and the
	 new current file inverted */
      if ( !widget_savestates[ new_current_savestate ]->has_screenshot && previous_black_screen )
        showing_screenshot = 0;
      else
        showing_screenshot = 1;

      if ( showing_screenshot ) {
        showing_screenshot = 0;
        widget_print_all_savestates( widget_savestates, widget_numsavestates,
				    top_savestate, new_current_savestate );
      } else {
        widget_print_savestate( widget_savestates[ current_savestate ],
			       current_savestate - top_savestate, 0 );

        widget_print_savestate( widget_savestates[ new_current_savestate ],
			       new_current_savestate - top_savestate, 1 );

        widget_print_show_screenshot_label( widget_savestates[ new_current_savestate ] );

        widget_display_lines( DIALOG_Y_POSITION, DIALOG_HEIGHT );
      }
    }

    /* Reset the current file marker */
    current_savestate = new_current_savestate;

  }
}
#endif /* ifdef GCWZERO */