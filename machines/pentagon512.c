/* pentagon512.c: Pentagon 512K specific routines. This machine is expected to
                  be a post-1996 Pentagon (a 512k v1.x 1024SL?). It is
                  different to the Pentagon 128k model as we want to be able to
                  exchange snapshots with emulators that do not support this
                  model but do support the older style Pentagon (SPIN,
                  Spectaculator, xzx-pro etc. etc.)..
   Copyright (c) 1999-2011 Philip Kendall and Fredrick Meunier

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

#include "config.h"

#include "libspectrum.h"

#include "compat.h"
#include "machine.h"
#include "machines.h"
#include "machines_periph.h"
#include "memory_pages.h"
#include "pentagon.h"
#include "periph.h"
#include "peripherals/disk/beta.h"
#include "settings.h"
#include "spec128.h"
#include "spec48.h"

static int pentagon_reset( void );
static int pentagon_memory_map( void );

int 
pentagon512_init( fuse_machine_info *machine )
{
  machine->machine = LIBSPECTRUM_MACHINE_PENT512;
  machine->id = "pentagon512";

  machine->reset = pentagon_reset;

  machine->timex = 0;
  machine->ram.port_from_ula  = pentagon_port_from_ula;
  machine->ram.contend_delay  = spectrum_contend_delay_none;
  machine->ram.contend_delay_no_mreq = spectrum_contend_delay_none;
  machine->ram.valid_pages    = 32;

  machine->unattached_port = spectrum_unattached_port_none;

  machine->shutdown = NULL;

  machine->memory_map = pentagon_memory_map;

  return 0;
}

static int
pentagon_reset(void)
{
  int error;

  error = machine_load_rom( 0, settings_current.rom_pentagon512_0,
                            settings_default.rom_pentagon512_0, 0x4000 );
  if( error ) return error;
  error = machine_load_rom( 1, settings_current.rom_pentagon512_1,
                            settings_default.rom_pentagon512_1, 0x4000 );
  if( error ) return error;
  error = machine_load_rom( 2, settings_current.rom_pentagon512_3,
                            settings_default.rom_pentagon512_3, 0x4000 );
  if( error ) return error;
  error = machine_load_rom_bank( beta_memory_map_romcs, 0,
                                 settings_current.rom_pentagon512_2,
                                 settings_default.rom_pentagon512_2, 0x4000 );
  if( error ) return error;

  error = spec128_common_reset( 0 );
  if( error ) return error;

  periph_clear();
  machines_periph_pentagon();

  /* Earlier style Betadisk 128 interface */
  periph_set_present( PERIPH_TYPE_BETA128_PENTAGON, PERIPH_PRESENT_ALWAYS );

  periph_set_present( PERIPH_TYPE_COVOX_FB, PERIPH_PRESENT_OPTIONAL );

  periph_update();

  beta_builtin = 1;
  beta_active = 1;

  machine_current->ram.last_byte2 = 0;
  machine_current->ram.special = 0;

  spec48_common_display_setup();

  return 0;
}

static int
pentagon_memory_map( void )
{
  int rom, page, screen;

  screen = ( machine_current->ram.last_byte & 0x08 ) ? 7 : 5;
  if( memory_current_screen != screen ) {
    display_update_critical( 0, 0 );
    display_refresh_main_screen();
    memory_current_screen = screen;
  }

  if( beta_active && !( machine_current->ram.last_byte & 0x10 ) ) {
    rom = 2;
  } else {
    rom = ( machine_current->ram.last_byte & 0x10 ) >> 4;
  }

  machine_current->ram.current_rom = rom;

  spec128_select_rom( rom );

  page = machine_current->ram.last_byte & 0x07;

  page += ( machine_current->ram.last_byte & 0xC0 ) >> 3;

  spec128_select_page( page );
  machine_current->ram.current_page = page;

  memory_romcs_map();

  return 0;
}
