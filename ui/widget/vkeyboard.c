/* vkeyboard.c: Virtual keyboard widget
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

#ifdef VKEYBOARD
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "fuse.h"
#include "utils.h"
#include "ui/uidisplay.h"
#include "ui/vkeyboard.h"
#include "widget_internals.h"
#include "keyboard.h"

static int position = 1;
static void widget_print_keyboard( void );
static void widget_input_keyboard( input_key key );
static void widget_release_keyboard( input_key key );
static void widget_print_key( int row, int key, int active );
static void widget_vkeyboard_input( keyboard_key_name spectrum_key, int press );
static void widget_vkeyboard_options_input( input_key, int press );

typedef struct keyboard_t {
  const char *label;
  keyboard_key_name spectrum_key;
} keyboard_t;

typedef struct keyboard_options_key {
  const char *label;
  input_key keyboard_key;
} keyboard_options_t;

static const keyboard_t vkeyboard[4][10] = {
  { { "1", KEYBOARD_1 },     { "2", KEYBOARD_2 }, { "3", KEYBOARD_3 }, { "4", KEYBOARD_4 },       { "5", KEYBOARD_5 },
    { "6", KEYBOARD_6 },     { "7", KEYBOARD_7 }, { "8", KEYBOARD_8 }, { "9", KEYBOARD_9 },       { "0", KEYBOARD_0 } },
  { { "Q", KEYBOARD_q },     { "W", KEYBOARD_w }, { "E", KEYBOARD_e }, { "R", KEYBOARD_r },       { "T", KEYBOARD_t },
    { "Y", KEYBOARD_y },     { "U", KEYBOARD_u }, { "I", KEYBOARD_i }, { "O", KEYBOARD_o },       { "P", KEYBOARD_p } },
  { { "A", KEYBOARD_a },     { "S", KEYBOARD_s }, { "D", KEYBOARD_d }, { "F", KEYBOARD_f },       { "G", KEYBOARD_g },
    { "H", KEYBOARD_h },     { "J", KEYBOARD_j }, { "K", KEYBOARD_k }, { "L", KEYBOARD_l },       { "En", KEYBOARD_Enter } },
  { { "Cs", KEYBOARD_Caps }, { "Z", KEYBOARD_z }, { "X", KEYBOARD_x }, { "C", KEYBOARD_c },       { "V", KEYBOARD_v },
    { "B", KEYBOARD_b },     { "N", KEYBOARD_n }, { "M", KEYBOARD_m }, { "Ss", KEYBOARD_Symbol }, { "Sp", KEYBOARD_space } },
};

static const keyboard_options_t vkeyboard_options[4][10] = {
  { { "1", INPUT_KEY_1 },     { "2", INPUT_KEY_2 }, { "3", INPUT_KEY_3 },      { "4", INPUT_KEY_4 },      { "5", INPUT_KEY_5 },
    { "6", INPUT_KEY_6 },     { "7", INPUT_KEY_7 }, { "8", INPUT_KEY_8 },      { "9", INPUT_KEY_9 },      { "0", INPUT_KEY_0 } },
  { { "q", INPUT_KEY_q },     { "w", INPUT_KEY_w }, { "e", INPUT_KEY_e },      { "r", INPUT_KEY_r },      { "t", INPUT_KEY_t },
    { "y", INPUT_KEY_y },     { "u", INPUT_KEY_u }, { "i", INPUT_KEY_i },      { "o", INPUT_KEY_o },      { "p", INPUT_KEY_p } },
  { { "a", INPUT_KEY_a },     { "s", INPUT_KEY_s }, { "d", INPUT_KEY_d },      { "f", INPUT_KEY_f },      { "g", INPUT_KEY_g },
    { "h", INPUT_KEY_h },     { "j", INPUT_KEY_j }, { "k", INPUT_KEY_k },      { "l", INPUT_KEY_l },      { "En", INPUT_KEY_Return } },
  { { "z", INPUT_KEY_z },     { "x", INPUT_KEY_x }, { "c", INPUT_KEY_c },      { "v", INPUT_KEY_v },      { "b", INPUT_KEY_b },
    { "n", INPUT_KEY_n },     { "m", INPUT_KEY_m }, { ".", INPUT_KEY_period }, { "-", INPUT_KEY_minus },  { "Sp", INPUT_KEY_space } },
};

static const keyboard_options_t vkeyboard_options_u[4][10] = {
  { { ",", INPUT_KEY_comma }, { "[", INPUT_KEY_bracketleft }, { "&", INPUT_KEY_ampersand }, { "]", INPUT_KEY_bracketright }, { "+", INPUT_KEY_plus },
    { "<", INPUT_KEY_less },  { ">", INPUT_KEY_greater },     { "(", INPUT_KEY_parenleft }, { ")", INPUT_KEY_parenright },   { "=", INPUT_KEY_equal } },
  { { "Q", INPUT_KEY_Q },     { "W", INPUT_KEY_W },           { "E", INPUT_KEY_E },         { "R", INPUT_KEY_R },            { "T", INPUT_KEY_T },
    { "Y", INPUT_KEY_Y },     { "U", INPUT_KEY_U },           { "I", INPUT_KEY_I },         { "O", INPUT_KEY_O },            { "P", INPUT_KEY_P } },
  { { "A", INPUT_KEY_A },     { "S", INPUT_KEY_S },           { "D", INPUT_KEY_D },         { "F", INPUT_KEY_F },            { "G", INPUT_KEY_G },
    { "H", INPUT_KEY_H },     { "J", INPUT_KEY_J },           { "K", INPUT_KEY_K },         { "L", INPUT_KEY_L },            { "En", INPUT_KEY_Return } },
  { { "Z", INPUT_KEY_Z },     { "X", INPUT_KEY_X },           { "C", INPUT_KEY_C },         { "V", INPUT_KEY_V },            { "B", INPUT_KEY_B },
    { "N", INPUT_KEY_N },     { "M", INPUT_KEY_M },           { ":", INPUT_KEY_colon },     { "_", INPUT_KEY_underscore },   { "/", INPUT_KEY_slash } },
};

static int fixed_keys_released[4][10] = {};
static int fixed_keys[4][10] = {};
static int one_time_fixed_keys[4][10] = {};
static int actual_row = 0, actual_key = 0;
static int press_row = 0, press_key = 0;
static int keyboard_upper = 0;

#define LOCK_KEY                  1 /* Blue bright 0 */
#define FIXED_KEY                 2 /* Red bright 0 */
#define SELECTED_KEY              5 /* Cyan bright 0 */
#define NOT_SELECTED_KEY         16 /* Alpha value not selected Key */
#define NOT_SELECTED_KEY_OPTIONS 15 /* White brigth 1 */
#define INK_KEY                  15 /* White bright 1 */
#define INK_KEY_OPTIONS           0 /* Black bright 0 */

void widget_print_key( int row, int key, int active )
{
  int paper, ink, x, y;

  if ( one_time_fixed_keys[row][key] )
    paper = LOCK_KEY;
  else if ( fixed_keys[row][key] )
    paper = FIXED_KEY;
  else if ( active )
    paper = SELECTED_KEY;
  else if ( ui_widget_level >= 0 )
    paper = NOT_SELECTED_KEY_OPTIONS;
  else
    paper = NOT_SELECTED_KEY;
  ink = (ui_widget_level >= 0) ? INK_KEY_OPTIONS : INK_KEY;
  x = key * 16 + 4;
  y = row * 10 + 4;

  widget_rectangle(x, y, 16, 10, paper);
  widget_printstring(x + 2, y + 1, ink,
                     (ui_widget_level >= 0)
                       ? (keyboard_upper) ? vkeyboard_options_u[row][key].label
                                          : vkeyboard_options[row][key].label
                       : vkeyboard[row][key].label);
}

void
widget_print_keyboard( void )
{
  int row, key;
  for (row = 0; row < 4; row++) {
    for (key = 0; key < 10; key++) {
      widget_print_key(row, key, ( actual_row == row && actual_key == key ));
    }
  }
}

void
widget_vkeyboard_input( keyboard_key_name spectrum_key, int press ) {
  if (press)
    keyboard_press(spectrum_key);
  else
    keyboard_release(spectrum_key);
}

void
widget_vkeyboard_options_input( input_key key, int press ) {
  if (press) {
    switch (key) {
    case INPUT_KEY_Return:
      widget_vkeyboard_finish( WIDGET_FINISHED_OK );
      break;
    default:
      break;
    }

    ui_widget_keyhandler( key );
  }
}

void
widget_input_keyboard( input_key key )
{
  int r, k;

  switch ( key ) {
#if 0
  case INPUT_KEY_Resize:	/* Fake keypress used on window resize */
    widget_menu_draw( menu );
    break;
#endif

#ifdef GCWZERO
  case INPUT_KEY_Home:  /* Power */
  case INPUT_KEY_End:   /* RetroFW */
    widget_vkeyboard_finish( WIDGET_FINISHED_CANCEL );
    if (ui_widget_level >= 0)
      widget_end_all( WIDGET_FINISHED_CANCEL );
    return;
#endif

#ifdef GCWZERO
  case INPUT_KEY_Return: /* Start */
#else
  case INPUT_KEY_Escape:
  case INPUT_KEY_F11:
  case INPUT_JOYSTICK_FIRE_2:
#endif
    widget_vkeyboard_finish( WIDGET_FINISHED_CANCEL );
    if (ui_widget_level >= 0)
      widget_end_widget( WIDGET_FINISHED_CANCEL );
    return;

#ifdef GCWZERO
  case INPUT_KEY_Control_L: /* A */
#else
  case INPUT_KEY_Return:
#endif
    press_row = actual_row;
    press_key = actual_key;
    if (ui_widget_level >= 0) {
      input_key pressed_key = keyboard_upper ? vkeyboard_options_u[actual_row][actual_key].keyboard_key
                                             : vkeyboard_options[actual_row][actual_key].keyboard_key;
      widget_vkeyboard_options_input( pressed_key,1);
      return;
    } else {
      for (r = 0; r < 4; r++)
        for (k = 0; k < 10; k++)
          if (one_time_fixed_keys[r][k]) {
            widget_vkeyboard_input(vkeyboard[r][k].spectrum_key,1);
          }
      widget_vkeyboard_input(vkeyboard[actual_row][actual_key].spectrum_key,1);
    }
    break;

#ifdef GCWZERO
  case INPUT_KEY_Alt_L: /* B */
#endif
  case INPUT_KEY_Control_R:
    if (ui_widget_level >= 0) {
      widget_vkeyboard_finish( WIDGET_FINISHED_CANCEL );
      widget_vkeyboard_options_input( INPUT_KEY_Escape, 1 );
      return;
    } else {
      one_time_fixed_keys[actual_row][actual_key] = one_time_fixed_keys[actual_row][actual_key] ? 0 : 1;
      if (fixed_keys[actual_row][actual_key]) {
        fixed_keys_released[actual_row][actual_key] = 1;
        fixed_keys[actual_row][actual_key] = 0;
      }
    }
    break;

#ifdef GCWZERO
  case INPUT_KEY_space: /* X */
#endif
  case INPUT_KEY_Shift_R:
    if (ui_widget_level >= 0) {
      keyboard_upper = !keyboard_upper;
    } else {
      fixed_keys_released[actual_row][actual_key] = fixed_keys[actual_row][actual_key] ? 1 : 0;
      fixed_keys[actual_row][actual_key] = fixed_keys[actual_row][actual_key] ? 0 : 1;
      one_time_fixed_keys[actual_row][actual_key] = 0;
    }
    break;

#ifdef GCWZERO
  case INPUT_KEY_Shift_L: /* Y */
#else
  case INPUT_KEY_BackSpace:
#endif
  case INPUT_KEY_Alt_R:
    if (ui_widget_level >= 0) {
      widget_vkeyboard_options_input( INPUT_KEY_BackSpace, 1 );
    } else {
      for (r=0;r<4;r++)
        for(k=0;k<10;k++) {
          if (one_time_fixed_keys[r][k])
            one_time_fixed_keys[r][k] = 0;
          if (fixed_keys[r][k]) {
            fixed_keys[r][k] = 0;
            fixed_keys_released[r][k] = 0;
            widget_vkeyboard_input(vkeyboard[r][k].spectrum_key,0);
          }
        }
    }
    break;

#ifdef GCWZERO
  case INPUT_KEY_BackSpace: /* L1 */
#else
  case INPUT_KEY_Page_Up:
#endif
    if (ui_widget_level >= 0) {
      widget_vkeyboard_options_input( INPUT_KEY_BackSpace, 1 );
    } else {
      position++;
      if ( position == 4 )
        position = 0;
    }
    break;

#ifdef GCWZERO
  case INPUT_KEY_Tab: /* R1 */
#else
  case INPUT_KEY_Page_Down:
#endif
    if (ui_widget_level >= 0) {
      widget_vkeyboard_options_input( INPUT_KEY_BackSpace, 1 );
    } else {
      if ( position == 0 )
        position = 3;
      else
        position--;
    }
    break;

  case INPUT_KEY_Up:
    (actual_row == 0) ? actual_row = 3 : actual_row--;
    break;

  case INPUT_KEY_Down:
    (actual_row == 3) ? actual_row = 0 : actual_row++;
    break;

  case INPUT_KEY_Left:
    (actual_key == 0) ? actual_key = 9 : actual_key--;
    break;

  case INPUT_KEY_Right:
    (actual_key == 9) ? actual_key = 0 : actual_key++;
    break;

  default:
    if (ui_widget_level >= 0)
      widget_vkeyboard_input( key, 1 );
    break;
  }

  if (ui_widget_level >= 0) {
    widget_display_rasters( DISPLAY_BORDER_HEIGHT * -1, DISPLAY_SCREEN_HEIGHT );
  } else {
    for (r=0;r<4;r++)
      for(k=0;k<10;k++) {
        if (fixed_keys_released[r][k]) {
          widget_vkeyboard_input(vkeyboard[r][k].spectrum_key,0);
          fixed_keys_released[r][k] = 0;
        }
        if (fixed_keys[r][k])
          widget_vkeyboard_input(vkeyboard[r][k].spectrum_key,1);
      }
  }
}

void
widget_release_keyboard( input_key key )
{
  int r, k;

  switch ( key ) {
#ifdef GCWZERO
  case INPUT_KEY_Control_L: /* A */
#else
  case INPUT_KEY_Return:
#endif
    press_row = 0;
    press_key = 0;
    if (ui_widget_level >= 0) {
      input_key pressed_key = keyboard_upper ? vkeyboard_options_u[actual_row][actual_key].keyboard_key
                                             : vkeyboard_options[actual_row][actual_key].keyboard_key;
      widget_vkeyboard_options_input(pressed_key,0);
    } else {
      for (r = 0; r < 4; r++)
        for (k = 0; k < 10; k++)
          if (one_time_fixed_keys[r][k]) {
            widget_vkeyboard_input(vkeyboard[r][k].spectrum_key,0);
            one_time_fixed_keys[r][k] = 0;
          }
      widget_vkeyboard_input(vkeyboard[actual_row][actual_key].spectrum_key,0);
    }
    return;

  default:
    break;
  }
}

int
widget_vkeyboard_draw( void *data )
{
  uidisplay_vkeyboard( &widget_print_keyboard, position );
  return 0;
}

void
widget_vkeyboard_keyhandler( input_key key )
{
  uidisplay_vkeyboard_input( &widget_input_keyboard, key );;
}

void
widget_vkeyboard_keyrelease( input_key key )
{
  uidisplay_vkeyboard_release( &widget_release_keyboard, key );;
}

int
widget_vkeyboard_finish( widget_finish_state finished )
{
  int r, k;
  for (r=0;r<4;r++)
    for(k=0;k<10;k++) {
      if (fixed_keys[r][k])
        keyboard_release(vkeyboard[r][k].spectrum_key);
      fixed_keys_released[r][k] = 0;
      fixed_keys[r][k] = 0;
      one_time_fixed_keys[r][k] = 0;
    }
  vkeyboard_enabled = 0;
  return 0;
}
#endif /* VKEYBOARD */