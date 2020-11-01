/* regex.c: Regusar expresion compatibility routines for OpenDingux (linux-uclibc)
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

#include <sys/types.h>          /* UNIX types      POSIX */
#include <regex.h>              /* Regular Exp     POSIX */
#include <string.h> 
#include <libspectrum.h>

#include "compat.h"

#define MAX_LENGTH  256
#define MAX_MATCHES 100

static char* cut_patterns( const char* re_expression, const char* text );
static int   cut_pattern( const regex_t* reg_expr, const char* text, char* remain );

char*
compat_chop_expressions( const char** re_expressions, const char* text )
{
  int  i;
  char *new_search;

  new_search = libspectrum_new( char, MAX_LENGTH );
  strlcpy( new_search, text, MAX_LENGTH );

  i = 0;
  while ( re_expressions[i] ) {
    strlcpy( new_search, cut_patterns( re_expressions[i], new_search ), MAX_LENGTH );
    i++;
  }

  return new_search;
}

static char*
cut_patterns( const char* re_expression, const char* text )
{
  char remain[MAX_LENGTH] = {'\0'};
  char *new_search;
  regex_t reg_expr;

  new_search = libspectrum_new( char, MAX_LENGTH );
  strlcpy( new_search, text, MAX_LENGTH );

  if ( regcomp( &reg_expr, re_expression, REG_ICASE|REG_EXTENDED|REG_NEWLINE ) )
    return new_search;

  while ( !cut_pattern( &reg_expr, new_search, &remain[0] ) )
    strlcpy( new_search, &remain[0], MAX_LENGTH );

  strlcpy( new_search, &remain[0], MAX_LENGTH );
  regfree( &reg_expr );

  return new_search;
}

static int
cut_pattern( const regex_t* reg_expr, const char* text, char* remain )
{
  regmatch_t matches[MAX_MATCHES];

  if ( !text ) return 1;

  if ( regexec( reg_expr, text, MAX_MATCHES, matches, 0 ) ) {
    strlcpy( remain, text, MAX_LENGTH );
    return 1;
  }

  strlcpy( remain, text, matches[0].rm_so + 1 );
  if ( text[matches[0].rm_eo] ) strlcat( remain, &text[matches[0].rm_eo], MAX_LENGTH );

  return 0;
}