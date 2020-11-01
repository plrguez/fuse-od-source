/* sdldisplay.c: Routines for dealing with the SDL display
   Copyright (c) 2000-2006 Philip Kendall, Matan Ziv-Av, Fredrick Meunier
   Copyright (c) 2015 Adrien Destugues

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

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>

#include <libspectrum.h>

#include "display.h"
#include "fuse.h"
#include "machine.h"
#include "peripherals/scld.h"
#include "screenshot.h"
#include "settings.h"
#include "ui/ui.h"
#include "ui/scaler/scaler.h"
#include "ui/uidisplay.h"
#include "utils.h"
#if VKEYBOARD
#include "ui/vkeyboard.h"
#endif
#ifdef GCWZERO
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "options.h"
#endif

SDL_Surface *sdldisplay_gc = NULL;   /* Hardware screen */
static SDL_Surface *tmp_screen=NULL; /* Temporary screen for scalers */

#if VKEYBOARD
static SDL_Surface *keyb_screen = NULL;
static SDL_Surface *keyb_screen_save = NULL;
static int init_vkeyboard_canvas = 0;
static SDL_Rect vkeyboard_position[6] = {
  {8,    8,   0, 0},
  {148,  8,   0, 0},
  {8,    176, 0, 0},
  {148,  176, 0, 0},
  {80,   138, 0, 0},
  {80,   64,  0, 0},
};
#define VKEYB_WIDTH  168
#define VKEYB_HEIGHT 48

#ifdef GCWZERO
/* Positions for GCWZERO scaling (Border option) */
static SDL_Rect vkeyboard_position_no_border[4] = {
  {32,   24,  0, 0},
  {121,  24,  0, 0},
  {32,   170, 0, 0},
  {121,  170, 0, 0},
};
static SDL_Rect vkeyboard_position_small_border[4] = {
  {28,   22,  0, 0},
  {125,  22,  0, 0},
  {28,   172, 0, 0},
  {125,  172, 0, 0},
};
static SDL_Rect vkeyboard_position_medium_border[4] = {
  {22,   18,  0, 0},
  {131,  18,  0, 0},
  {22,   176, 0, 0},
  {131,  176, 0, 0},
};
static SDL_Rect vkeyboard_position_large_border[4] = {
  {14,   12,  0, 0},
  {140,  12,  0, 0},
  {14,   182, 0, 0},
  {140,  182, 0, 0},
};
#endif
#endif /* VKEYBOARD */

#ifdef GCWZERO
size_t od_info_length;
static SDL_Surface *od_status_line_ovelay;
static SDL_Rect od_status_line_position = { 5, 225, 242, 10 };
static SDL_Surface *red_cassette[4], *green_cassette[4];
static SDL_Surface *red_mdr[4], *green_mdr[4];
static SDL_Surface *red_disk[4], *green_disk[4];
#define SWAP_ICONS(a, b) b = a[1]; a[1] = a[3]; a[3] = b; b = a[0]; a[0] = a[2]; a[2] = b;
#else
static SDL_Surface *red_cassette[2], *green_cassette[2];
static SDL_Surface *red_mdr[2], *green_mdr[2];
static SDL_Surface *red_disk[2], *green_disk[2];
#endif

static ui_statusbar_state sdl_disk_state, sdl_mdr_state, sdl_tape_state;
static int sdl_status_updated;

static int tmp_screen_width;

static Uint32 colour_values[16];

static SDL_Color colour_palette[] = {
  {   0,   0,   0,   0 }, 
  {   0,   0, 192,   0 }, 
  { 192,   0,   0,   0 }, 
  { 192,   0, 192,   0 }, 
  {   0, 192,   0,   0 }, 
  {   0, 192, 192,   0 }, 
  { 192, 192,   0,   0 }, 
  { 192, 192, 192,   0 }, 
  {   0,   0,   0,   0 }, 
  {   0,   0, 255,   0 }, 
  { 255,   0,   0,   0 }, 
  { 255,   0, 255,   0 }, 
  {   0, 255,   0,   0 }, 
  {   0, 255, 255,   0 }, 
  { 255, 255,   0,   0 }, 
  { 255, 255, 255,   0 }
};

static Uint32 bw_values[16];

#if defined(VKEYBOARD) || defined(GCWZERO)
static SDL_Surface *overlay_alpha_surface = NULL;
static Uint32 colour_values_a[20];
static Uint32 bw_values_a[20];
#endif

/* This is a rule of thumb for the maximum number of rects that can be updated
   each frame. If more are generated we just update the whole screen */
#define MAX_UPDATE_RECT 300
static SDL_Rect updated_rects[MAX_UPDATE_RECT];
static int num_rects = 0;
static libspectrum_byte sdldisplay_force_full_refresh = 1;

static int max_fullscreen_height;
static int min_fullscreen_height;
static int fullscreen_width = 0;
static int fullscreen_x_off = 0;
static int fullscreen_y_off = 0;

/* The current size of the display (in units of DISPLAY_SCREEN_*) */
static float sdldisplay_current_size = 1;

static libspectrum_byte sdldisplay_is_full_screen = 0;

#ifdef GCWZERO
typedef enum sdldisplay_od_boder_types {
  Full,
  Large,
  Medium,
  Small,
  None,
  End
} sdldisplay_t_od_border;

typedef struct od_screen_scaling {
  sdldisplay_t_od_border border_type;
  Uint16       w;
  Uint16       h;
  SDL_Rect*    vkeyboard;
} od_t_screen_scaling;

static od_t_screen_scaling od_screen_scalings_2x[] = {
  { Full,    320, 240, &vkeyboard_position[0] },
  { Large,   304, 228, &vkeyboard_position_large_border[0] },
  { Medium,  288, 216, &vkeyboard_position_medium_border[0] },
  { Small,   272, 204, &vkeyboard_position_small_border[0] },
  { None,    256, 192, &vkeyboard_position_no_border[0] },
};
#ifndef RETROFW
static od_t_screen_scaling od_screen_scalings_1x_640[] = {
  { Full,    320, 240, &vkeyboard_position[0] },
  { Large,   300, 225, &vkeyboard_position_large_border[0] }, /* No 4:3 AR */
  { Medium,  288, 216, &vkeyboard_position_medium_border[0] },
  { Small,   272, 208, &vkeyboard_position_small_border[0] }, /* No 4:3 AR */
  { None,    256, 192, &vkeyboard_position_no_border[0] },
};
static od_t_screen_scaling od_screen_scalings_1x_480[] = {
  { Full,    320, 240, &vkeyboard_position[0] },
  { Large,   304, 224, &vkeyboard_position_large_border[0] }, /* No 4:3 AR */
  { Medium,  288, 216, &vkeyboard_position_medium_border[0] },
  { Small,   272, 208, &vkeyboard_position_small_border[0] }, /* No 4:3 AR */
  { None,    256, 192, &vkeyboard_position_no_border[0] },
};
#endif
static sdldisplay_t_od_border sdldisplay_last_od_border = Full;
static sdldisplay_t_od_border sdldisplay_current_od_border = Full;
static SDL_Rect clip_area;
static libspectrum_byte sdldisplay_is_triple_buffer = 0;
static libspectrum_byte sdldisplay_flips_triple_buffer = 0;
typedef enum sdldisplay_od_system_types {
      OPENDINGUX,
      RETROFW_1,
      RETROFW_2
} sdldisplay_t_od_system;
static sdldisplay_t_od_system sdldisplay_od_system_type = OPENDINGUX;
#ifndef RETROFW
typedef enum sdldisplay_od_panel_types {
      P320240,
      P640480,
      P480320
} sdldisplay_t_od_panel_type;
static sdldisplay_t_od_panel_type sdl_od_panel_type = P320240;
#endif
#endif

static int image_width;
static int image_height;

static int timex;

static void init_scalers( void );
static int sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                                        Uint32 *bw_values );
#if VKEYBOARD
static int sdldisplay_allocate_colours_alpha( int numColours, Uint32 *colour_values,
                                             Uint32 *bw_values );
#endif
#if GCWZERO
static void uidisplay_status_overlay( void );
#endif

static int sdldisplay_load_gfx_mode( void );

static void
init_scalers( void )
{
  scaler_register_clear();

  scaler_register( SCALER_NORMAL );
#ifndef GCWZERO
  scaler_register( SCALER_2XSAI );
  scaler_register( SCALER_SUPER2XSAI );
  scaler_register( SCALER_SUPEREAGLE );
  scaler_register( SCALER_ADVMAME2X );
  scaler_register( SCALER_ADVMAME3X );
  scaler_register( SCALER_DOTMATRIX );
  scaler_register( SCALER_PALTV );
  scaler_register( SCALER_HQ2X );
#else
  scaler_register( SCALER_PALTV );
#endif
  if( machine_current->timex ) {
    scaler_register( SCALER_HALF );
    scaler_register( SCALER_HALFSKIP );
    scaler_register( SCALER_TIMEXTV );
#ifndef GCWZERO
    scaler_register( SCALER_TIMEX1_5X );
    scaler_register( SCALER_TIMEX2X );
#endif
  } else {
#ifdef GCWZERO
    scaler_register( SCALER_DOTMATRIX );
    scaler_register( SCALER_DOUBLESIZE );
    scaler_register( SCALER_TV2X );
    scaler_register( SCALER_PALTV2X );
    scaler_register( SCALER_2XSAI );
    scaler_register( SCALER_SUPER2XSAI );
    scaler_register( SCALER_SUPEREAGLE );
    scaler_register( SCALER_ADVMAME2X );
    scaler_register( SCALER_HQ2X );
#else
    scaler_register( SCALER_DOUBLESIZE );
    scaler_register( SCALER_TRIPLESIZE );
    scaler_register( SCALER_QUADSIZE );
    scaler_register( SCALER_TV2X );
    scaler_register( SCALER_TV3X );
    scaler_register( SCALER_TV4X );
    scaler_register( SCALER_PALTV2X );
    scaler_register( SCALER_PALTV3X );
    scaler_register( SCALER_HQ3X );
    scaler_register( SCALER_HQ4X );
#endif
  }
  
  if( scaler_is_supported( current_scaler ) ) {
    scaler_select_scaler( current_scaler );
  } else {
    scaler_select_scaler( SCALER_NORMAL );
  }
}

static int
sdl_convert_icon( SDL_Surface *source, SDL_Surface **icon, int red )
{
  SDL_Surface *copy;   /* Copy with altered palette */
  int i;

  SDL_Color colors[ source->format->palette->ncolors ];

  copy = SDL_ConvertSurface( source, source->format, SDL_SWSURFACE );

  for( i = 0; i < copy->format->palette->ncolors; i++ ) {
    colors[i].r = red ? copy->format->palette->colors[i].r : 0;
    colors[i].g = red ? 0 : copy->format->palette->colors[i].g;
    colors[i].b = 0;
  }

  SDL_SetPalette( copy, SDL_LOGPAL, colors, 0, i );

  icon[0] = SDL_ConvertSurface( copy, tmp_screen->format, SDL_SWSURFACE );
#ifdef GCWZERO
  icon[3] = SDL_ConvertSurface( copy, tmp_screen->format, SDL_SWSURFACE );
#endif

  SDL_FreeSurface( copy );

  icon[1] = SDL_CreateRGBSurface( SDL_SWSURFACE,
                                  (icon[0]->w)<<1, (icon[0]->h)<<1,
                                  icon[0]->format->BitsPerPixel,
                                  icon[0]->format->Rmask,
                                  icon[0]->format->Gmask,
                                  icon[0]->format->Bmask,
                                  icon[0]->format->Amask
                                );

  ( scaler_get_proc16( SCALER_DOUBLESIZE ) )(
        (libspectrum_byte*)icon[0]->pixels,
        icon[0]->pitch,
        (libspectrum_byte*)icon[1]->pixels,
        icon[1]->pitch, icon[0]->w, icon[0]->h
      );

#ifdef GCWZERO
  icon[2] = SDL_CreateRGBSurface( SDL_SWSURFACE,
                                  ((icon[0]->w)>>1) + 1, ((icon[0]->h)>>1) + 1,
                                  icon[0]->format->BitsPerPixel,
                                  icon[0]->format->Rmask,
                                  icon[0]->format->Gmask,
                                  icon[0]->format->Bmask,
                                  icon[0]->format->Amask
                                );

  ( scaler_get_proc16( SCALER_HALF ) )(
        (libspectrum_byte*)icon[0]->pixels,
        icon[0]->pitch,
        (libspectrum_byte*)icon[2]->pixels,
        icon[2]->pitch, icon[0]->w, icon[0]->h
      );
#endif

  return 0;
}

static int
sdl_load_status_icon( const char*filename, SDL_Surface **red, SDL_Surface **green )
{
  char path[ PATH_MAX ];
  SDL_Surface *temp;    /* Copy of image as loaded */

  if( utils_find_file_path( filename, path, UTILS_AUXILIARY_LIB ) ) {
    fprintf( stderr, "%s: Error getting path for icons\n", fuse_progname );
    return -1;
  }

  if((temp = SDL_LoadBMP(path)) == NULL) {
    fprintf( stderr, "%s: Error loading icon \"%s\" text:%s\n", fuse_progname,
             path, SDL_GetError() );
    return -1;
  }

  if(temp->format->palette == NULL) {
    fprintf( stderr, "%s: Icon \"%s\" is not paletted\n", fuse_progname, path );
    return -1;
  }

  sdl_convert_icon( temp, red, 1 );
  sdl_convert_icon( temp, green, 0 );

  SDL_FreeSurface( temp );

  return 0;
}

#ifdef GCWZERO
/* Initializations for OpenDingux/RetroFW */
void
uidisplay_od_init( SDL_Rect **modes )
{
/* On OpenDingux/RetroFW fix Full Screen */
  settings_current.full_screen = 1;
  settings_current.sdl_fullscreen_mode = utils_safe_strdup( '\0' );

/*
 * Dirty hack. There should be a better solution.
 * In RetroFW 1 the video driver don't downscale correctly. This is problematic
 * with filters higher than 1x, the can be choosed but the image is not correct.
 * This should prevent to use any filter that will be problematic.
 * Also for Timex Machines this will force to use the Timex Half filters...
 *
 * First:
 * for OpenDingux devices like GCW0, RG350 the info about downscaling is at:
 *        /sys/devices/platform/jz-lcd.0/allow_downscaling
 *        If it can be read then it's done.
 *
 * Second:
 * Read /etc/os-release. RetroFW 2 downscale right, so if NAME is Buildroot then
 * the full_screen_mode will be fixed to 320x240.
 *
 * This avoids that the use of scalers with a scale factor greater than 1x
 * which in RetroFW1 makes it try to use the actual resolution and not downscaling
 * to the resolution of display.
 *
 * But probably this will be problematic on devices with no 320x240 resolution.
 *
 *  For RetroFW 2.x
 *    NAME=RetroFW
 *    ID=retrofw
 *    VERSION_ID=2.2
 *    PRETTY_NAME="RetroFW v2.2"
 *    ANSI_COLOR="0;36"
 *    HOME_URL="https://retrofw.github.io/"
 *    BUG_REPORT_URL="https://github.com/retrofw/retrofw.github.io/issues"
 *
 * For RetroFW 1
 *    NAME=Buildroot
 *    VERSION=2018.02.11
 *    ID=buildroot
 *    VERSION_ID=2018.02.11
 *    PRETTY_NAME="Buildroot 2018.02.11"
*
 * For OpenDingux:
 *    NAME=Buildroot
 *    VERSION=2014.08-g156cb719e
 *    ID=buildroot
 *    VERSION_ID=2014.08
 *    PRETTY_NAME="Buildroot 2014.08"
 */

#ifdef RETROFW
  /* We are on RetroFW */
  char line[100];
  FILE* os_release = fopen( "/etc/os-release", "r" );
  if ( os_release ) {
    while ( fgets(line, sizeof( line ), os_release ) != NULL ) {
      char* ptok = strtok( line, "=" );
      if ( strcmp( ptok, "NAME" ) == 0 ) {
        ptok = strtok( NULL, "=" );
        /* And we are on RetroFW 1.X */
        if ( strcmp( ptok, "Buildroot\n" ) == 0) {
          settings_current.sdl_fullscreen_mode = utils_safe_strdup( "320x240" );
          sdldisplay_od_system_type = RETROFW_1;
        } else {
          sdldisplay_od_system_type = RETROFW_2;
        }
        break;
      }
    }
    fclose( os_release );
  }
#else
/* We are on OpenDingux */
  sdldisplay_od_system_type = OPENDINGUX;
#endif
}
#endif

int
uidisplay_init( int width, int height )
{
  SDL_Rect **modes;
  int no_modes;
  int i = 0, mw = 0, mh = 0, mn = 0;

  /* Get available fullscreen/software modes */
#ifdef GCWZERO
  modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
#else
  modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_SWSURFACE);
#endif

  no_modes = ( modes == (SDL_Rect **) 0 || modes == (SDL_Rect **) -1 ) ? 1 : 0;

#ifdef GCWZERO
  uidisplay_od_init( modes );
#endif

  if( settings_current.sdl_fullscreen_mode &&
      strcmp( settings_current.sdl_fullscreen_mode, "list" ) == 0 ) {

    fprintf( stderr,
    "=====================================================================\n"
    " List of available SDL fullscreen modes:\n"
    "---------------------------------------------------------------------\n"
    "  No. width height\n"
    "---------------------------------------------------------------------\n"
    );
    if( no_modes ) {
      fprintf( stderr, "  ** The modes list is empty%s...\n",
               modes == (SDL_Rect **) -1 ? ", all resolutions allowed" : "" );
    } else {
      for( i = 0; modes[i]; i++ ) {
        fprintf( stderr, "% 3d  % 5d % 5d\n", i + 1, modes[i]->w, modes[i]->h );
      }
    }
    fprintf( stderr,
    "=====================================================================\n");
    fuse_exiting = 1;
    return 0;
  }

  if( !no_modes ) {
    for( i=0; modes[i]; ++i ); /* count modes */
  }

  if( settings_current.sdl_fullscreen_mode ) {
    if( sscanf( settings_current.sdl_fullscreen_mode, " %dx%d", &mw, &mh ) != 2 ) {
      if( !no_modes &&
          sscanf( settings_current.sdl_fullscreen_mode, " %d", &mn ) == 1 &&
          mn <= i ) {
        mw = modes[mn - 1]->w; mh = modes[mn - 1]->h;
      }
    }
  }

  /* Check if there are any modes available, or if our resolution is restricted
     at all */
  if( mh > 0 ) {
    /* set from command line */
    max_fullscreen_height = min_fullscreen_height = mh;
    fullscreen_width = mw;
  } else if( no_modes ){
    /* Just try whatever we have and see what happens */
    max_fullscreen_height = 480;
    min_fullscreen_height = 240;
  } else {
    /* Record the largest supported fullscreen software mode */
    max_fullscreen_height = modes[0]->h;

    /* Record the smallest supported fullscreen software mode */
    for( i=0; modes[i]; ++i ) {
      min_fullscreen_height = modes[i]->h;
    }
  }

  image_width = width;
  image_height = height;

  timex = machine_current->timex;

  init_scalers();

  if ( scaler_select_scaler( current_scaler ) )
    scaler_select_scaler( SCALER_NORMAL );

  if( sdldisplay_load_gfx_mode() ) return 1;

  SDL_WM_SetCaption( "Fuse", "Fuse" );

  /* We can now output error messages to our output device */
  display_ui_initialised = 1;

  sdl_load_status_icon( "cassette.bmp", red_cassette, green_cassette );
  sdl_load_status_icon( "microdrive.bmp", red_mdr, green_mdr );
  sdl_load_status_icon( "plus3disk.bmp", red_disk, green_disk );

#if GCWZERO
  if (sdldisplay_current_od_border) {
    SDL_Surface *swap;
    SWAP_ICONS( red_cassette, swap );
    SWAP_ICONS( green_cassette, swap );
    SWAP_ICONS( red_mdr, swap );
    SWAP_ICONS( green_mdr, swap );
    SWAP_ICONS( red_disk, swap );
    SWAP_ICONS( green_disk, swap );
  }
#endif

  return 0;
}

static int
sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                             Uint32 *bw_values )
{
  int i;
  Uint8 red, green, blue, grey;

  for( i = 0; i < numColours; i++ ) {

      red = colour_palette[i].r;
    green = colour_palette[i].g;
     blue = colour_palette[i].b;

    /* Addition of 0.5 is to avoid rounding errors */
    grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;

    colour_values[i] = SDL_MapRGB( tmp_screen->format,  red, green, blue );
    bw_values[i]     = SDL_MapRGB( tmp_screen->format, grey,  grey, grey );
  }

  return 0;
}

#if defined(VKEYBOARD) || defined(GCWZERO)
static int
sdldisplay_allocate_colours_alpha( int numColours, Uint32 *colour_values,
                                  Uint32 *bw_values ) {
  int i;
  Uint8 red, green, blue, grey;

  for ( i = 0; i < numColours; i++ ) {

      red = colour_palette[i].r;
    green = colour_palette[i].g;
     blue = colour_palette[i].b;

    /* Addition of 0.5 is to avoid rounding errors */
    grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;

    colour_values[i] = SDL_MapRGBA( keyb_screen->format,  red, green, blue, 0xf0 );
    bw_values[i]     = SDL_MapRGBA( keyb_screen->format, grey,  grey, grey, 0xf0 );
  }

    red = 0x60;
  green = 0x60;
   blue = 0x7a;
   grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;
  colour_values[16] = SDL_MapRGBA( keyb_screen->format,  red, green, blue, 0x50 );
  bw_values[16]     = SDL_MapRGBA( keyb_screen->format, grey,  grey, grey, 0x50 );

    red = 0x1f;
  green = 0xad;
   blue = 0xe1;
   grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;
  colour_values[17] = SDL_MapRGBA( keyb_screen->format,  red, green, blue, 0x50 );
  bw_values[17]     = SDL_MapRGBA( keyb_screen->format, grey,  grey, grey, 0x50 );

    red = 0xff;
  green = 0xff;
   blue = 0xff;
   grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;
  colour_values[18] = SDL_MapRGBA( keyb_screen->format,  red, green, blue, 0x00 );
  bw_values[18]     = SDL_MapRGBA( keyb_screen->format, grey,  grey, grey, 0x00 );

    red = 0xff;
  green = 0xff;
   blue = 0xff;
   grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;
  colour_values[19] = SDL_MapRGBA( keyb_screen->format,  red, green, blue, 0xff );
  bw_values[19]     = SDL_MapRGBA( keyb_screen->format, grey,  grey, grey, 0xff );

  return 0;
}
#endif /* VKEYBOARD */

static void
sdldisplay_find_best_fullscreen_scaler( void )
{
  static int windowed_scaler = -1;
  static int searching_fullscreen_scaler = 0;

  /* Make sure we have at least more than half of the screen covered in
     fullscreen to avoid the "postage stamp" on machines that don't support
     320x240 anymore e.g. Mac notebooks */
  if( settings_current.full_screen ) {
    int i = 0;

    if( searching_fullscreen_scaler ) return;
    searching_fullscreen_scaler = 1;
    while( i < SCALER_NUM &&
           ( image_height*sdldisplay_current_size <= min_fullscreen_height/2 ||
             image_height*sdldisplay_current_size > max_fullscreen_height ) ) {
      if( windowed_scaler == -1) windowed_scaler = current_scaler;
      while( !scaler_is_supported(i) ) i++;
      scaler_select_scaler( i++ );
      sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
      /* if we failed to find a suitable size scaler, just use normal (what the
         user had originally may be too big) */
      if( image_height * sdldisplay_current_size <= min_fullscreen_height/2 ||
          image_height * sdldisplay_current_size > max_fullscreen_height ) {
        scaler_select_scaler( SCALER_NORMAL );
        sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
      }
    }
    searching_fullscreen_scaler = 0;
  } else {
    if( windowed_scaler != -1 ) {
      scaler_select_scaler( windowed_scaler );
      windowed_scaler = -1;
      sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
    }
  }
}

static int
sdldisplay_load_gfx_mode( void )
{
  Uint16 *tmp_screen_pixels;

  sdldisplay_force_full_refresh = 1;

  /* Free the old surface */
  if( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen );
    tmp_screen = NULL;
  }

#if VKEYBOARD
  if ( keyb_screen ) {
    SDL_FreeSurface( keyb_screen );
    keyb_screen = NULL;
  }
#endif

#ifdef GCWZERO
  if ( od_status_line_ovelay ) {
    SDL_FreeSurface( od_status_line_ovelay );
    od_status_line_ovelay = NULL;
  }
#endif

  tmp_screen_width = (image_width + 3);

  sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );

  sdldisplay_find_best_fullscreen_scaler();

  /* Create the surface that contains the scaled graphics in 16 bit mode */
#ifdef GCWZERO
  Uint32 flags;
  if (settings_current.od_triple_buffer) {
#ifdef RETROFW
    sdldisplay_flips_triple_buffer = 1;
#else
    sdldisplay_flips_triple_buffer = 0;
#endif
    flags = settings_current.full_screen ? (SDL_FULLSCREEN | SDL_HWSURFACE | SDL_TRIPLEBUF)
    : (SDL_HWSURFACE | SDL_TRIPLEBUF);
  } else {
    while ( sdldisplay_is_triple_buffer && ++sdldisplay_flips_triple_buffer < 3 )
      SDL_Flip( sdldisplay_gc );
    flags = settings_current.full_screen ? (SDL_FULLSCREEN | SDL_HWSURFACE)
    : SDL_HWSURFACE;
  }

  int display_width, display_height;
#ifndef RETROFW
  sdl_od_panel_type = option_enumerate_general_gcw0_od_panel_type();
#endif
  sdldisplay_current_od_border = option_enumerate_general_gcw0_od_border();
  if ( ( sdldisplay_current_od_border && !sdldisplay_last_od_border ) ||
       ( !sdldisplay_current_od_border && sdldisplay_last_od_border ) ) {
    SDL_Surface *swap;
    SWAP_ICONS( red_cassette, swap );
    SWAP_ICONS( green_cassette, swap );
    SWAP_ICONS( red_mdr, swap );
    SWAP_ICONS( green_mdr, swap );
    SWAP_ICONS( red_disk, swap );
    SWAP_ICONS( green_disk, swap );
  }
  if ( sdldisplay_current_od_border ) {
    int scale = ( libspectrum_machine_capabilities( machine_current->machine ) & LIBSPECTRUM_MACHINE_CAPABILITY_TIMEX_VIDEO ) ? 2 : 1;
    od_t_screen_scaling *ssc = &od_screen_scalings_2x[0];

#ifndef RETROFW
    if ( sdldisplay_current_size <= 1 )
      switch (sdl_od_panel_type) {
      case P640480:
        ssc = &od_screen_scalings_1x_640[0];
        break;
      case P480320:
        ssc = &od_screen_scalings_1x_480[0];
		FILE* integer_scaling_file = fopen("/sys/class/graphics/fb0/device/integer_scaling", "w");
		if (integer_scaling_file) {
			fwrite("N", 1, 1, integer_scaling_file);
			fclose(integer_scaling_file);
		}
        break;
      default:
        break;
      }
#endif /* ifndef RETROFW */

    clip_area.x = ( DISPLAY_ASPECT_WIDTH - ssc[sdldisplay_current_od_border].w ) * scale / 2;
    clip_area.y = ( DISPLAY_SCREEN_HEIGHT - ssc[sdldisplay_current_od_border].h ) * scale / 2;

    clip_area.w = ssc[sdldisplay_current_od_border].w * scale;
    clip_area.h = ssc[sdldisplay_current_od_border].h * scale;

    display_width = clip_area.w * sdldisplay_current_size;
    display_height = clip_area.h * sdldisplay_current_size;

  /* Full Border */
  } else {
    display_width =  settings_current.full_screen && fullscreen_width
        ? fullscreen_width
        : image_width * sdldisplay_current_size;
    display_height = settings_current.full_screen && fullscreen_width
        ? max_fullscreen_height
        : image_height * sdldisplay_current_size;
  }
  sdldisplay_gc = SDL_SetVideoMode( display_width, display_height, 16, flags );
#else
  sdldisplay_gc = SDL_SetVideoMode(
    settings_current.full_screen && fullscreen_width ? fullscreen_width :
      image_width * sdldisplay_current_size,
    settings_current.full_screen && fullscreen_width ? max_fullscreen_height :
      image_height * sdldisplay_current_size,
    16,
    settings_current.full_screen ? (SDL_FULLSCREEN|SDL_SWSURFACE)
                                 : SDL_SWSURFACE
  );
#endif
  if( !sdldisplay_gc ) {
    fprintf( stderr, "%s: couldn't create SDL graphics context\n", fuse_progname );
    fuse_abort();
  }

  settings_current.full_screen =
      !!( sdldisplay_gc->flags & ( SDL_FULLSCREEN | SDL_NOFRAME ) );
  sdldisplay_is_full_screen = settings_current.full_screen;

#ifdef GCWZERO
  settings_current.od_triple_buffer = !!( sdldisplay_gc->flags & SDL_TRIPLEBUF );
  sdldisplay_is_triple_buffer = settings_current.od_triple_buffer;

  sdldisplay_last_od_border = sdldisplay_current_od_border;
#endif

  /* Distinguish 555 and 565 mode */
  if( sdldisplay_gc->format->Gmask >> sdldisplay_gc->format->Gshift == 0x1f )
    scaler_select_bitformat( 555 );
  else
    scaler_select_bitformat( 565 );

  /* Create the surface used for the graphics in 16 bit before scaling */

  /* Need some extra bytes around when using 2xSaI */
  tmp_screen_pixels = (Uint16*)calloc(tmp_screen_width*(image_height+3), sizeof(Uint16));
  tmp_screen = SDL_CreateRGBSurfaceFrom(tmp_screen_pixels,
                                        tmp_screen_width,
                                        image_height + 3,
                                        16, tmp_screen_width*2,
                                        sdldisplay_gc->format->Rmask,
                                        sdldisplay_gc->format->Gmask,
                                        sdldisplay_gc->format->Bmask,
                                        sdldisplay_gc->format->Amask );

  if( !tmp_screen ) {
    fprintf( stderr, "%s: couldn't create tmp_screen\n", fuse_progname );
    fuse_abort();
  }

#if VKEYBOARD
  /* Create the surface that contains the keyboard graphics in 32 bit mode */
  SDL_Surface *swap_screen;
  swap_screen = SDL_CreateRGBSurface(SDL_HWSURFACE,
                                     machine_current->timex ? VKEYB_WIDTH * 2  : VKEYB_WIDTH,
                                     machine_current->timex ? VKEYB_HEIGHT * 2 : VKEYB_HEIGHT,
                                     16,
                                     sdldisplay_gc->format->Rmask,
                                     sdldisplay_gc->format->Gmask,
                                     sdldisplay_gc->format->Bmask,
                                     ( SDL_BYTEORDER == SDL_BIG_ENDIAN ? 0x000000ff : 0xff000000 ) );
  if ( !swap_screen ) {
    fprintf( stderr, "%s: couldn't create keyb_screen\n", fuse_progname );
    fuse_abort();
  }
  keyb_screen = SDL_DisplayFormatAlpha( swap_screen );
  SDL_FreeSurface( swap_screen );
#endif

#ifdef GCWZERO
  /* Create the surface that contains status in scaling */
  SDL_Surface *od_tmp_screen;
  od_tmp_screen = SDL_CreateRGBSurface(SDL_HWSURFACE,
                                       machine_current->timex ? od_status_line_position.w * 2 : od_status_line_position.w,
                                       machine_current->timex ? od_status_line_position.h * 2 : od_status_line_position.h,
                                       16,
                                       sdldisplay_gc->format->Rmask,
                                       sdldisplay_gc->format->Gmask,
                                       sdldisplay_gc->format->Bmask,
                                       ( SDL_BYTEORDER == SDL_BIG_ENDIAN ? 0x000000ff : 0xff000000 ) );
  if ( !od_tmp_screen ) {
    fprintf( stderr, "%s: couldn't create status line overlay screen\n", fuse_progname );
    fuse_abort();
  }
  od_status_line_ovelay = SDL_DisplayFormatAlpha( od_tmp_screen );
  SDL_FreeSurface( od_tmp_screen );
#endif

  fullscreen_x_off = ( sdldisplay_gc->w - image_width * sdldisplay_current_size ) *
                     sdldisplay_is_full_screen  / 2;
  fullscreen_y_off = ( sdldisplay_gc->h - image_height * sdldisplay_current_size ) *
                     sdldisplay_is_full_screen / 2;

  sdldisplay_allocate_colours( 16, colour_values, bw_values );
#if defined(VKEYBOARD) || defined(GCWZERO)
  sdldisplay_allocate_colours_alpha( 16, colour_values_a, bw_values_a );
#endif

  /* Redraw the entire screen... */
  display_refresh_all();

  return 0;
}

int
uidisplay_hotswap_gfx_mode( void )
{
  fuse_emulation_pause();

  /* Free the old surface */
  if( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }

#if VKEYBOARD
  if ( keyb_screen ) {
    SDL_FreeSurface( keyb_screen );
    keyb_screen = NULL;
  }
#endif

#ifdef GCWZERO
  if ( od_status_line_ovelay ) {
    SDL_FreeSurface( od_status_line_ovelay );
    od_status_line_ovelay = NULL;
  }
#endif

  /* Setup the new GFX mode */
  if( sdldisplay_load_gfx_mode() ) return 1;

  /* reset palette */
  SDL_SetColors( sdldisplay_gc, colour_palette, 0, 16 );

  /* Mac OS X resets the state of the cursor after a switch to full screen
     mode */
  if ( settings_current.full_screen || ui_mouse_grabbed ) {
    SDL_ShowCursor( SDL_DISABLE );
    SDL_WarpMouse( 128, 128 );
  } else {
    SDL_ShowCursor( SDL_ENABLE );
  }

  fuse_emulation_unpause();

  return 0;
}

SDL_Surface *saved = NULL;

void
uidisplay_frame_save( void )
{
  if( saved ) {
    SDL_FreeSurface( saved );
    saved = NULL;
  }

  saved = SDL_ConvertSurface( tmp_screen, tmp_screen->format,
                              SDL_SWSURFACE );
}

void
uidisplay_frame_restore( void )
{
  if( saved ) {
    SDL_BlitSurface( saved, NULL, tmp_screen, NULL );
    sdldisplay_force_full_refresh = 1;
  }
}

static void
sdl_blit_icon( SDL_Surface **icon,
               SDL_Rect *r, Uint32 tmp_screen_pitch,
               Uint32 dstPitch )
{
  int x, y, w, h, dst_x, dst_y, dst_h;

  if( timex ) {
    r->x<<=1;
    r->y<<=1;
    r->w<<=1;
    r->h<<=1;
  }

  x = r->x;
  y = r->y;
  w = r->w;
  h = r->h;
  r->x++;
  r->y++;

  if( SDL_BlitSurface( icon[timex], NULL, tmp_screen, r ) ) return;

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &w, &h, image_width, image_height );

  dst_y = y * sdldisplay_current_size + fullscreen_y_off;
  dst_h = h;
  dst_x = x * sdldisplay_current_size + fullscreen_x_off;

  scaler_proc16(
	(libspectrum_byte*)tmp_screen->pixels +
			(x+1) * tmp_screen->format->BytesPerPixel +
	                (y+1) * tmp_screen_pitch,
	tmp_screen_pitch,
	(libspectrum_byte*)sdldisplay_gc->pixels +
			dst_x * sdldisplay_gc->format->BytesPerPixel +
			dst_y * dstPitch,
	dstPitch, w, dst_h
  );

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Adjust rects for the destination rect size */
  updated_rects[num_rects].x = dst_x;
  updated_rects[num_rects].y = dst_y;
  updated_rects[num_rects].w = w * sdldisplay_current_size;
  updated_rects[num_rects].h = dst_h * sdldisplay_current_size;

  num_rects++;
}

static void
sdl_icon_overlay( Uint32 tmp_screen_pitch, Uint32 dstPitch )
{
  SDL_Rect r = { 243, 218, red_disk[0]->w, red_disk[0]->h };
#ifdef GCWZERO
  switch (sdldisplay_current_od_border) {
  case None:
    r.x = 252;
    r.y = 204;
    break;
  case Small:
    r.x = 260;
    r.y = 210;
    break;
  case Medium:
    r.x = 268;
    r.y = 216;
    break;
  case Large:
    r.x = 274;
    r.y = 222;
    break;
  case Full:
  default:
    break;
  }
#endif

  switch( sdl_disk_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }

  r.x = 264;
  r.y = 218;
#ifdef GCWZERO
  switch (sdldisplay_current_od_border) {
  case None:
    r.x = 262;
    r.y = 204;
    break;
  case Small:
    r.x = 270;
    r.y = 210;
    break;
  case Medium:
    r.x = 278;
    r.y = 216;
    break;
  case Large:
    r.x = 284;
    r.y = 222;
    break;
  case Full:
  default:
    break;
  }
#endif
  r.w = red_mdr[0]->w;
  r.h = red_mdr[0]->h;

  switch( sdl_mdr_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_mdr, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_mdr, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }

  r.x = 285;
  r.y = 220;
#ifdef GCWZERO
  switch (sdldisplay_current_od_border) {
  case None:
    r.x = 272;
    r.y = 206;
    break;
  case Small:
    r.x = 280;
    r.y = 212;
    break;
  case Medium:
    r.x = 288;
    r.y = 218;
    break;
  case Large:
    r.x = 294;
    r.y = 224;
    break;
  case Full:
  default:
    break;
  }
#endif
  r.w = red_cassette[0]->w;
  r.h = red_cassette[0]->h;

  switch( sdl_tape_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    sdl_blit_icon( red_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  }

  sdl_status_updated = 0;
}

/* Set one pixel in the display */
void
uidisplay_putpixel( int x, int y, int colour )
{
  libspectrum_word *dest_base, *dest;

#if defined(VKEYBOARD) || defined(GCWZERO)
  if ( overlay_alpha_surface ) {
    uidisplay_putpixel_alpha(x - DISPLAY_BORDER_ASPECT_WIDTH, y - DISPLAY_BORDER_HEIGHT,
                             colour);
    return;
  }
#endif

  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;

  Uint32 palette_colour = palette_values[ colour ];

  if( machine_current->timex ) {
    x <<= 1; y <<= 1;
    dest_base = dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
    dest = (libspectrum_word*)
      ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
  } else {
    dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    *dest = palette_colour;
  }
}

#if defined(VKEYBOARD) || defined(GCWZERO)
/* Set one pixel in the display */
void
uidisplay_putpixel_alpha( int x, int y, int colour ) {
  libspectrum_dword *dest_base, *dest;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values_a : colour_values_a;
  Uint32 palette_colour = palette_values[ colour ];

  if ( machine_current->timex ) {
    x <<= 1;
    y <<= 1;
    dest_base = dest =
        (libspectrum_dword*) ( (libspectrum_byte*) overlay_alpha_surface->pixels +
        (x) * overlay_alpha_surface->format->BytesPerPixel +
        (y) * overlay_alpha_surface->pitch);

    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
    dest = (libspectrum_dword*)
        ( (libspectrum_byte*) dest_base + overlay_alpha_surface->pitch);
    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
  } else {
    dest =
        (libspectrum_dword*) ( (libspectrum_byte*) overlay_alpha_surface->pixels +
        (x) * overlay_alpha_surface->format->BytesPerPixel +
        (y) * overlay_alpha_surface->pitch);

    *dest = palette_colour;
  }
}

static void
uidisplay_status_overlay( void ) {
  SDL_Rect current_positions = od_status_line_position;

  switch (sdldisplay_current_od_border) {
  case Large:
    current_positions.x += 6;
    current_positions.y -= 2;
    break;

  case Medium:
    current_positions.x += 14;
    current_positions.y -= 7;
    break;

  case Small:
    current_positions.x += 22;
    current_positions.y -= 13;
    break;

  case None:
    current_positions.x += 30;
    current_positions.y -= 19;
    break;

  default:
    break;
  }

  int scale = ( libspectrum_machine_capabilities( machine_current->machine ) & LIBSPECTRUM_MACHINE_CAPABILITY_TIMEX_VIDEO ) ? 2 : 1;

  SDL_Rect r1 = { current_positions.x * scale, current_positions.y * scale,
                  current_positions.w * scale, current_positions.h * scale };

  SDL_Rect r2 = { 0, 0, ( od_info_length + 3 ) * scale, current_positions.h * scale };

  SDL_FillRect(od_status_line_ovelay, NULL, settings_current.bw_tv ? bw_values_a[18] : colour_values_a[18]);
  SDL_FillRect(od_status_line_ovelay, &r2, settings_current.bw_tv ? bw_values_a[17] : colour_values_a[17]);

  overlay_alpha_surface = od_status_line_ovelay;
  ui_widget_statusbar_print_info();
  overlay_alpha_surface = NULL;

  SDL_BlitSurface(od_status_line_ovelay, NULL, tmp_screen, &r1);

  updated_rects[num_rects].x = r1.x;
  updated_rects[num_rects].y = r1.y;
  updated_rects[num_rects].w = r1.w;
  updated_rects[num_rects].h = r1.h;
  num_rects++;

  display_refresh_rect( r1.x - 1 * scale, r1.y - 1 * scale, r1.w + 8 * scale, r1.h + 1 * scale, 1 );
}
#endif

#if VKEYBOARD
void
uidisplay_vkeyboard( void (*print_fn)(void), int position ) {
  static int old_position = -1;
  int current_position;
  SDL_Rect *current_positions = &vkeyboard_position[0];

#ifdef GCWZERO
  od_t_screen_scaling* ssc = &od_screen_scalings_2x[0];
#ifndef RETROFW
  if ( sdldisplay_current_size <= 1 )
    switch (sdl_od_panel_type) {
    case P640480:
      ssc = &od_screen_scalings_1x_640[0];
      break;
    case P480320:
      ssc = &od_screen_scalings_1x_480[0];
      break;
    default:
      break;
    }
#endif
  if ( ui_widget_level == -1 )
    current_positions = ssc[sdldisplay_current_od_border].vkeyboard;
#endif

  if (ui_widget_level >= 0)
    current_position = 4;
  else
    current_position = position;

  SDL_Rect r1 = { machine_current->timex ? current_positions[current_position].x * 2 : current_positions[current_position].x,
                  machine_current->timex ? current_positions[current_position].y * 2 : current_positions[current_position].y,
                  machine_current->timex ? VKEYB_WIDTH * 2  : VKEYB_WIDTH,
                  machine_current->timex ? VKEYB_HEIGHT * 2 : VKEYB_HEIGHT };

#ifdef GCWZERO
  if ( !init_vkeyboard_canvas ) {
    SDL_FillRect(keyb_screen, NULL, settings_current.bw_tv ? bw_values_a[16] : colour_values_a[16]);
    init_vkeyboard_canvas = 1;
  }
#endif

  if (ui_widget_level >= 0) {
    if (!keyb_screen_save) {
      keyb_screen_save = SDL_CreateRGBSurface(tmp_screen->flags, r1.w, r1.h,
                                              tmp_screen->format->BitsPerPixel,
                                              tmp_screen->format->Rmask,
                                              tmp_screen->format->Gmask,
                                              tmp_screen->format->Bmask,
                                              tmp_screen->format->Amask);
      SDL_BlitSurface(tmp_screen, &r1, keyb_screen_save, NULL);
    } else
      SDL_BlitSurface(keyb_screen_save, NULL, tmp_screen, &r1);
  }

  overlay_alpha_surface = keyb_screen;
  print_fn();
  overlay_alpha_surface = NULL;

  SDL_BlitSurface(keyb_screen, NULL, tmp_screen, &r1);

  updated_rects[num_rects].x = r1.x;
  updated_rects[num_rects].y = r1.y;
  updated_rects[num_rects].w = r1.w;
  updated_rects[num_rects].h = r1.h;
  num_rects++;

  if (ui_widget_level == -1) {
    int scale = ( libspectrum_machine_capabilities( machine_current->machine ) & LIBSPECTRUM_MACHINE_CAPABILITY_TIMEX_VIDEO ) ? 2 : 1;
    if (old_position != position)
      display_refresh_all();
    else
      display_refresh_rect( r1.x -1, r1.y - 1, r1.w + 8 * scale, r1.h + 1 * scale, 0 );
    old_position = position;
  }
}

void
uidisplay_vkeyboard_input( void (*input_fn)(input_key key), input_key key ) {
  input_fn(key);
}

void
uidisplay_vkeyboard_release( void (*release_fn)(input_key key), input_key key ) {
  release_fn(key);
}

void
uidisplay_vkeyboard_end( void ) {
  init_vkeyboard_canvas = 0;
  if (keyb_screen_save) {
    SDL_FreeSurface(keyb_screen_save);
    keyb_screen_save = NULL;
  }
  /*
     Don't refresh display from the use of keyboard in other Widgets
     The function widget_do already do a display_refresh_all at his end
     And the call here will affect to some operations like save screens
   */
  if (ui_widget_level == -1)
    display_refresh_all();
}
#endif /* VKEYBOARD */

/* Print the 8 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (8*x) , y ) */
void
uidisplay_plot8( int x, int y, libspectrum_byte data,
	         libspectrum_byte ink, libspectrum_byte paper )
{
  libspectrum_word *dest;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;

  Uint32 palette_ink = palette_values[ ink ];
  Uint32 palette_paper = palette_values[ paper ];

  if( machine_current->timex ) {
    int i;
    libspectrum_word *dest_base;

    x <<= 4; y <<= 1;

    dest_base =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    for( i=0; i<2; i++ ) {
      dest = dest_base;

      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x01 ) ? palette_ink : palette_paper;
      *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;

      dest_base = (libspectrum_word*)
        ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
    }
  } else {
    x <<= 3;

    dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;
  }
}

/* Print the 16 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (16*x) , y ) */
void
uidisplay_plot16( int x, int y, libspectrum_word data,
		  libspectrum_byte ink, libspectrum_byte paper )
{
  libspectrum_word *dest_base, *dest;
  int i;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;
  Uint32 palette_ink = palette_values[ ink ];
  Uint32 palette_paper = palette_values[ paper ];
  x <<= 4; y <<= 1;

  dest_base =
    (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                         (x+1) * tmp_screen->format->BytesPerPixel +
                         (y+1) * tmp_screen->pitch);

  for( i=0; i<2; i++ ) {
    dest = dest_base;

    *(dest++) = ( data & 0x8000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x4000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x2000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x1000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0800 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0400 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0200 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0100 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0080 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0040 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0020 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0010 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0008 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0004 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0002 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x0001 ) ? palette_ink : palette_paper;

    dest_base = (libspectrum_word*)
      ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
  }
}

void
uidisplay_frame_end( void )
{
  SDL_Rect *r;
  Uint32 tmp_screen_pitch, dstPitch;
  SDL_Rect *last_rect;

  /* We check for a switch to fullscreen here to give systems with a
     windowed-only UI a chance to free menu etc. resources before
     the switch to fullscreen (e.g. Mac OS X) */
#ifdef GCWZERO
  if ( ( sdldisplay_is_full_screen != settings_current.full_screen  ||
      sdldisplay_is_triple_buffer != settings_current.od_triple_buffer ||
      sdldisplay_last_od_border != sdldisplay_current_od_border ) &&
#else
  if( sdldisplay_is_full_screen != settings_current.full_screen &&
#endif
      uidisplay_hotswap_gfx_mode() ) {
    fprintf( stderr, "%s: Error switching to fullscreen\n", fuse_progname );
    fuse_abort();
  }

#if VKEYBOARD
  if ( vkeyboard_enabled )
    ui_widget_print_vkeyboard();
#endif

#ifdef GCWZERO
  if ( settings_current.statusbar && ui_widget_level == -1 &&
       ( !sdldisplay_current_od_border || settings_current.od_statusbar_with_border ) )
    uidisplay_status_overlay();
#endif

  /* Force a full redraw if requested */
#ifdef GCWZERO
  if ( sdldisplay_force_full_refresh || sdldisplay_is_triple_buffer ) {
#else
  if ( sdldisplay_force_full_refresh ) {
#endif
    num_rects = 1;

    updated_rects[0].x = 0;
    updated_rects[0].y = 0;
    updated_rects[0].w = image_width;
    updated_rects[0].h = image_height;
  }

  if ( !(ui_widget_level >= 0) && num_rects == 0 && !sdl_status_updated )
    return;

  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_LockSurface( sdldisplay_gc );

  tmp_screen_pitch = tmp_screen->pitch;

  dstPitch = sdldisplay_gc->pitch;

  last_rect = updated_rects + num_rects;

  for( r = updated_rects; r != last_rect; r++ ) {
#ifdef GCWZERO
    if ( sdldisplay_current_od_border ) {
      if ( ( r->x > clip_area.x + clip_area.w ) ||
           ( r->y > clip_area.y + clip_area.h ) )
        continue;
      if ( r->x < clip_area.x )
        r->x = clip_area.x;
      if ( r->y < clip_area.y )
        r->y = clip_area.y;
      if ( r->x + r->w > clip_area.x + clip_area.w )
        r->w = clip_area.w - ( r->x - clip_area.x );
      if ( r->y + r->h > clip_area.y + clip_area.h )
        r->h = clip_area.h - ( r->y - clip_area.y );
    }
#endif
    int dst_y = r->y * sdldisplay_current_size + fullscreen_y_off;
    int dst_h = r->h;
    int dst_x = r->x * sdldisplay_current_size + fullscreen_x_off;

    scaler_proc16(
      (libspectrum_byte*)tmp_screen->pixels +
                        (r->x+1) * tmp_screen->format->BytesPerPixel +
	                (r->y+1)*tmp_screen_pitch,
      tmp_screen_pitch,
      (libspectrum_byte*)sdldisplay_gc->pixels +
	                 dst_x * sdldisplay_gc->format->BytesPerPixel +
			 dst_y*dstPitch,
      dstPitch, r->w, dst_h
    );

    /* Adjust rects for the destination rect size */
    r->x = dst_x;
    r->y = dst_y;
    r->w *= sdldisplay_current_size;
    r->h = dst_h * sdldisplay_current_size;
  }

#ifdef GCWZERO
  if ( settings_current.statusbar && (!sdldisplay_current_od_border || settings_current.od_statusbar_with_border) )
#else
  if ( settings_current.statusbar )
#endif
    sdl_icon_overlay( tmp_screen_pitch, dstPitch );

  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_UnlockSurface( sdldisplay_gc );

  /* Finally, blit all our changes to the screen */
#ifdef GCWZERO
  if ( sdldisplay_is_triple_buffer ) {
    SDL_Flip( sdldisplay_gc );
    if ( ++sdldisplay_flips_triple_buffer >= 3 ) sdldisplay_flips_triple_buffer = 0;
  } else
#endif
  SDL_UpdateRects( sdldisplay_gc, num_rects, updated_rects );

  num_rects = 0;
  sdldisplay_force_full_refresh = 0;
}

void
uidisplay_area( int x, int y, int width, int height )
{
  if ( sdldisplay_force_full_refresh )
    return;

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &width, &height, image_width, image_height );

  updated_rects[num_rects].x = x;
  updated_rects[num_rects].y = y;
  updated_rects[num_rects].w = width;
  updated_rects[num_rects].h = height;

  num_rects++;
}

int
uidisplay_end( void )
{
  int i;

  display_ui_initialised = 0;

  if ( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }

  if( saved ) {
    SDL_FreeSurface( saved ); saved = NULL;
  }

#if VKEYBOARD
  if ( keyb_screen ) {
    SDL_FreeSurface( keyb_screen );
    keyb_screen = NULL;
  }
#endif

#ifdef GCWZERO
  if ( od_status_line_ovelay ) {
    SDL_FreeSurface( od_status_line_ovelay );
    od_status_line_ovelay = NULL;
  }
#endif

#ifdef GCWZERO
  for( i=0; i<4; i++ ) {
#else
  for( i=0; i<2; i++ ) {
#endif
    if ( red_cassette[i] ) {
      SDL_FreeSurface( red_cassette[i] ); red_cassette[i] = NULL;
    }
    if ( green_cassette[i] ) {
      SDL_FreeSurface( green_cassette[i] ); green_cassette[i] = NULL;
    }
    if ( red_mdr[i] ) {
      SDL_FreeSurface( red_mdr[i] ); red_mdr[i] = NULL;
    }
    if ( green_mdr[i] ) {
      SDL_FreeSurface( green_mdr[i] ); green_mdr[i] = NULL;
    }
    if ( red_disk[i] ) {
      SDL_FreeSurface( red_disk[i] ); red_disk[i] = NULL;
    }
    if ( green_disk[i] ) {
      SDL_FreeSurface( green_disk[i] ); green_disk[i] = NULL;
    }
  }

  return 0;
}

/* The statusbar handling function */
int
ui_statusbar_update( ui_statusbar_item item, ui_statusbar_state state )
{
  switch( item ) {

  case UI_STATUSBAR_ITEM_DISK:
    sdl_disk_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_PAUSED:
    /* We don't support pausing this version of Fuse */
    return 0;

  case UI_STATUSBAR_ITEM_TAPE:
    sdl_tape_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_MICRODRIVE:
    sdl_mdr_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_MOUSE:
    /* We don't support showing a grab icon */
    return 0;

  }

  ui_error( UI_ERROR_ERROR, "Attempt to update unknown statusbar item %d",
            item );
  return 1;
}
