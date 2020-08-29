/* sdlhotkeys.c: Hotkeys processing
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

#include <SDL.h>
#include "settings.h"
#include "ui/ui.h"
#include "ui/hotkeys.h"

#ifdef GCWZERO
#define MAX_COMBO_KEYS_PENDING 10
static int combo_done;
static SDL_Event *combo_keys[MAX_COMBO_KEYS_PENDING];
static int last_combo_key = 0;
static int push_combo_event( Uint8* flags );
static int filter_combo_done( const SDL_Event *event );
static int is_combo_possible( const SDL_Event *event );

/* I allways forget what is push and what is drop */
#define DROP_EVENT 0
#define PUSH_EVENT 1

/*
 Current keys used in combos: L1, R1, Select, Start, X, Y, A, B
*/
#define FLAG_SELECT 0x01
#define FLAG_START  0x02
#define FLAG_L1     0x04
#define FLAG_R1     0x08
#define FLAG_A      0x10
#define FLAG_B      0x20
#define FLAG_X      0x40
#define FLAG_Y      0x80

/*
  Combos currently are mapped to Fx functions used in Fuse:
    L1 + R1 + B      Toggle triple buffer
    L1 + R1 + X      Joystick

    L1 + Select + Y  Tape play (F8)

    L1 + A           Tape open (F7)
    L1 + B           Save file (F2)
    L1 + X           Open file (F3)
    L1 + Y           Media menu

    R1 + A           General options (F4)
    R1 + B           Reset machine (F5)
    R1 + X           Exit fuse (F10)
    R1 + Y           Machine select (F9)
*/

#define OPEN_JOYSTICK   (FLAG_L1|FLAG_R1|FLAG_X)
#define TRIPLE_BUFFER   (FLAG_L1|FLAG_R1|FLAG_B)

#define TAPE_PLAY       (FLAG_L1|FLAG_SELECT|FLAG_X)

#define GENERAL_OPTIONS (FLAG_R1|FLAG_A)
#define RESET           (FLAG_R1|FLAG_B)
#define EXIT            (FLAG_R1|FLAG_X)
#define MACHINE_SELECT  (FLAG_R1|FLAG_Y)

#define TAPE_OPEN       (FLAG_L1|FLAG_A)
#define SAVE_FILES      (FLAG_L1|FLAG_B)
#define OPEN_FILES      (FLAG_L1|FLAG_X)
#define OPEN_MEDIA      (FLAG_L1|FLAG_Y)

int is_combo_possible( const SDL_Event *event )
{
  /* There are not combos in widgets or keyboard */
#ifdef VKEYBOARD
  if ( !settings_current.od_hotkey_combos || ui_widget_level >= 0 || vkeyboard_enabled ) return 0;
#else
  if ( !settings_current.od_hotkey_combos || ui_widget_level >= 0 ) return 0;
#endif

 /* Mot filter if R1 and R1 are mapped to joysticks */
  if ( ( event->key.keysym.sym == SDLK_TAB &&
         ( ( settings_current.joystick_1_output && settings_current.joystick_1_fire_5 ) ||
           ( settings_current.joystick_gcw0_output && settings_current.joystick_gcw0_l1 ) ) ) ||
       ( event->key.keysym.sym == SDLK_BACKSPACE &&
         ( ( settings_current.joystick_1_output && settings_current.joystick_1_fire_6 ) ||
           ( settings_current.joystick_gcw0_output && settings_current.joystick_gcw0_r1 ) ) ) )
    return 0;

  return 1;
}

int
filter_combo_done( const SDL_Event *event )
{
  int i, filter_released_key = 0;

  /* Not filter if not combo or not release key*/
  if ( !combo_done || event->type != SDL_KEYUP )
    return filter_released_key;

  /* Search for released key and check to filter*/
  for( i = 0; i < last_combo_key && i < MAX_COMBO_KEYS_PENDING; i++)
    if ( combo_keys[i] && combo_keys[i]->key.keysym.sym == event->key.keysym.sym ) {
      free(combo_keys[i]);
      combo_keys[i] = NULL;
      filter_released_key = 1;
    }

  /* Any combo pending */
  for( i = 0; i < last_combo_key && i < MAX_COMBO_KEYS_PENDING; i++)
    if ( combo_keys[i] && filter_released_key )
      return filter_released_key;

  /* All combo keys released */
  combo_done = 0;
  last_combo_key = 0;
  return filter_released_key;
}

int
push_combo_event( Uint8* flags )
{
  SDL_Event combo_event;
  SDLKey combo_key = 0;
  int toggle_triple_buffer = 0;

  /* Nothing to do */
  if ( !flags ) return 0;

  /* Search for valid combos */
  /* First must be checked the combos with Select or Start or L1+R1*/
  switch (*flags) {
  case OPEN_JOYSTICK:
    combo_key = SDLK_F12; break;

  case TRIPLE_BUFFER:
    toggle_triple_buffer = 1; break;

  case TAPE_PLAY:
    combo_key = SDLK_F8; break;

  case GENERAL_OPTIONS:
    combo_key = SDLK_F4; break;

  case MACHINE_SELECT:
    combo_key = SDLK_F9; break;

  case OPEN_MEDIA:
    combo_key = SDLK_F11; break;

  case TAPE_OPEN:
    combo_key = SDLK_F7; break;

  case OPEN_FILES:
    combo_key = SDLK_F3; break;

  case SAVE_FILES:
    combo_key = SDLK_F2; break;

  case EXIT:
    combo_key = SDLK_F10; break;

  case RESET:
    combo_key = SDLK_F5; break;

  default:
    break;
  }

  /* Push combo event */
  if ( combo_key ) {
    /* Prepare the SDL event */
    SDL_memset( &combo_event, 0, sizeof( combo_event ) );
    combo_event.type = SDL_KEYDOWN;
    combo_event.key.type = SDL_KEYDOWN;
    combo_event.key.state = SDL_PRESSED;
    combo_event.key.keysym.sym = combo_key;

    /* Push it */
    SDL_PushEvent( &combo_event );

    /* Clean flags and mark combo as done */
    *flags = 0x00;
    combo_done = 1;
    return 1;

  /* Switch triple buffer */
  } else if ( toggle_triple_buffer ) {
    settings_current.od_triple_buffer = !settings_current.od_triple_buffer;

    /* Clean flags and mark combo as done */
    *flags = 0x00;
    combo_done = 1;
    return 1;

  /* Nothing to do */
  } else
    return 0;
}

/*

 This is called for filter Events. The current use is for start combos keys
 before the events will be passed to the ui_event function

 Return values are:
    0 will drop event
    1 will push event

 L1 or R1 start a combo if they are not mapped to any Joystick.
 When a combo is started if any key not used in combos is pressed or released
 then the event is pushed.
 Auto-repeat keys in combos are droped.
 if a combo is done then the release of keys in combo are dropped at init
*/
int
filter_combo_events( const SDL_Event *event )
{
  static Uint8  flags  = 0;
  int i, not_in_combo  = 0;

  /* Filter release of combo keys */
  if ( filter_combo_done( event ) ) return (DROP_EVENT);

  if ( !is_combo_possible( event ) ) return (PUSH_EVENT);

  switch ( event->type ) {
  case SDL_KEYDOWN:
    switch ( event->key.keysym.sym ) {
    case SDLK_ESCAPE:    /* Select */
      if ( flags ) {
        if ( flags & FLAG_SELECT )
          return (DROP_EVENT); /* Filter Repeat key */
        else
          flags |= FLAG_SELECT;
      } else
        return (PUSH_EVENT);
      break;

    case SDLK_RETURN:    /* Start */
      if ( flags ) {
        if ( flags & FLAG_START )
          return (DROP_EVENT); /* Filter Repeat key */
        else
          flags |= FLAG_START;
      } else
        return 1;
      break;

    case SDLK_TAB:        /* L1 */
      if ( flags & FLAG_L1 )
        return (DROP_EVENT); /* Filter Repeat key */
      else
        flags |= FLAG_L1;
      break;

    case SDLK_BACKSPACE:  /* R1 */
      if (flags & FLAG_R1)
        return (DROP_EVENT); /* Filter Repeat key */
      else
        flags |= FLAG_R1;
      break;

    case SDLK_LCTRL:     /* A  */
      if ( flags ) {
        if ( flags & FLAG_A )
          return (DROP_EVENT); /* Filter Repeat key */
        else
          flags |= FLAG_A;
      } else
        return (PUSH_EVENT);
      break;

    case SDLK_LALT:      /* B  */
      if ( flags ) {
        if ( flags & FLAG_B)
          return (DROP_EVENT); /* Filter Repeat key */
        else
          flags |= FLAG_B;
      } else
        return (PUSH_EVENT);
      break;

    case SDLK_SPACE:     /* X  */
      if ( flags ) {
        if ( flags & FLAG_X )
          return (DROP_EVENT); /* Filter Repeat key */
        else
          flags |= FLAG_X;
      } else
        return (PUSH_EVENT);
      break;

    case SDLK_LSHIFT:      /* Y  */
      if ( flags ) {
        if ( flags & FLAG_Y)
          return (DROP_EVENT); /* Filter Repeat key */
        else
          flags |= FLAG_Y;
      } else
        return (PUSH_EVENT);
      break;

    /* Key not in combo */
    default:
      not_in_combo = 1;
      break;
    }

    break;

  /* Any release key break the combo */
  case SDL_KEYUP:
    switch ( event->key.keysym.sym ) {
    case SDLK_ESCAPE:     /* Select */
      if ( flags & FLAG_SELECT )
        flags &= ~FLAG_SELECT;
      else
        return (PUSH_EVENT);
      break;

    case SDLK_RETURN:    /* Start */
      if ( flags & FLAG_START )
        flags &= ~FLAG_START;
      else
        return (PUSH_EVENT);
      break;

    case SDLK_TAB:        /* L1 */
      flags &= ~FLAG_L1;
      break;

    case SDLK_BACKSPACE:  /* R1 */
      flags &= ~FLAG_R1;
      break;

    case SDLK_LCTRL:      /* A  */
      if ( flags & FLAG_A )
        flags &= ~FLAG_A;
      else
        return (PUSH_EVENT);
      break;

    case SDLK_LALT:    /* B  */
      if ( flags & FLAG_B )
        flags &= ~FLAG_B;
      else
        return (PUSH_EVENT);
      break;

    /* Key not in combo */
    default:
      not_in_combo = 1;
      break;
    }

    break;

  default:
    not_in_combo = 1;
    break;
  }

  if ( !not_in_combo ) {
    /* All Combo keys released? clean all saved events */
    if ( !flags ) {
      for( i = 0; i < last_combo_key && i < MAX_COMBO_KEYS_PENDING; i++)
        if ( combo_keys[i] ) {
          free( combo_keys[i] );
          combo_keys[i] = NULL;
        }
      last_combo_key = 0;
    }

    /* Save actual event, excluding releases */
    if ( event->type == SDL_KEYDOWN ) {
      combo_keys[last_combo_key] = malloc( sizeof( SDL_Event ) );
      memcpy( combo_keys[last_combo_key], event, sizeof( SDL_Event ) );
      last_combo_key++;
    }

    /* Push combo event */
    push_combo_event( &flags );

    return (DROP_EVENT);
  }

  return (PUSH_EVENT);
}
#endif /* GCWZERO */
