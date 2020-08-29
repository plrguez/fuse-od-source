/* filesel.c: File selection dialog box
   Copyright (c) 2001-2015 Matan Ziv-Av, Philip Kendall, Russell Marks,
			   Marek Januszewski
   Copyright (c) 2015 Sergio Baldoví

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

#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_STRCASECMP
#include <strings.h>
#endif      /* #ifdef HAVE_STRINGS_STRCASECMP */
#include <sys/stat.h>
#include <unistd.h>

#ifdef WIN32
#include <windows.h>
#include <direct.h>
#include <ctype.h>
#endif				/* #ifdef WIN32 */

#include "fuse.h"
#include "ui/ui.h"
#include "utils.h"
#include "widget_internals.h"

#if defined AMIGA || defined __MORPHOS__
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>

struct Library *AslBase;

#ifndef __MORPHOS__
struct AslIFace *IAsl;
struct Library *DOSBase;
#endif				/* #ifndef __MORPHOS__ */

#ifndef __MORPHOS__
struct DOSIFace *IDOS;
struct Library *ExecBase;
#endif				/* #ifndef __MORPHOS__ */


int err = 0;

char *amiga_asl( char *title, BOOL is_saving );

#endif /* ifdef AMIGA */

struct widget_dirent **widget_filenames; /* Filenames in the current
					    directory */
size_t widget_numfiles;	  /* The number of files in the current
			     directory */

static const char *title;
static int is_saving;

#ifdef WIN32
static int is_drivesel = 0;
static int is_rootdir;
#endif				/* #ifdef WIN32 */

#ifdef GCWZERO
#define ENTRIES_PER_SCREEN (is_saving ? ( last_filename ? 30 : 32 ) : 36)
#else
#define ENTRIES_PER_SCREEN (is_saving ? 32 : 36)
#endif

/* The number of the filename in the top-left corner of the current
   display, that of the filename which the `cursor' is on, and that
   which it will be on after this keypress */
static size_t top_left_file, current_file, new_current_file;
#ifdef GCWZERO
typedef struct widget_file_filter_t {
  widget_filter_class    class;
  const char*            filters_write;
  const char*            filters_read;
} widget_file_filter_t;

static widget_file_filter_t widget_file_filters[FILTER_CLASS_GENERAL+1] = {
  { FILTER_CLASS_SNAPSHOT,
                          "szx;z80;sna",
                          "szx;z80;sna;mgtsnp;slt;snapshot;snp;sp;zx-state" },
  { FILTER_CLASS_TAPE,
                          "tzx",
#ifdef HAVE_LIB_AUDIOFILE
                          "tzx;tap;pzx;ltp;raw;spc;sta;csw;wav" },
#else
                          "tzx;tap;pzx;ltp;raw;spc;sta;csw" },
#endif
  { FILTER_CLASS_DISK_PLUS3,
                          "dsk;udi;fdi",         "dsk;udi;fdi" },
  { FILTER_CLASS_DISK_TRDOS,
                          "trd;udi;fdi;sad",     "trd;scl;udi;fdi;sad" },
  { FILTER_CLASS_DISK_OPUS,
                          "opu;opd",             "opu;opd" },
  { FILTER_CLASS_DISK_DIDAKTIK,
                          "d40;d80;udi;fdi;sad", "d40;d80;udi;fdi;sad" },
  { FILTER_CLASS_DISK_PLUSD,
                          "mgt;img;udi;fdi;sad", "mgt;img;udi;fdi;sad" },
  { FILTER_CLASS_DISK_GENERIC,
                          "udi;fdi;sad",         "udi;fdi;sad;td0" },
  { FILTER_CLASS_MICRODRIVE,
                          "mdr", "mdr" },
  { FILTER_CLASS_CARTRIDGE_IF2,
                          NULL, "rom" },
  { FILTER_CLASS_CARTRIDGE_TIMEX,
                          NULL, "dck" },
  { FILTER_CLASS_BINARY,
                          NULL, NULL },
  { FILTER_CLASS_ROM,
                          NULL, "rom" },
#ifdef LIBSPECTRUM_SUPPORTS_ZLIB_COMPRESSION
#ifdef LIBSPECTRUM_SUPPORTS_BZ2_COMPRESSION
  { FILTER_CLASS_COMPRESSED,
                          NULL, "gz;zip;bz2" },
#else
  { FILTER_CLASS_COMPRESSED,
                          NULL, "gz;zip" },
#elseif LIBSPECTRUM_SUPPORTS_BZ2_COMPRESSION
  { FILTER_CLASS_COMPRESSED,
                          NULL, "bz2" },
#endif
#endif
  { FILTER_CLASS_RECORDING,
                          "rzx", "rzx" },
  { FILTER_CLASS_SCREENSHOT,
                          "scr", "scr" },
  { FILTER_CLASS_SCREENSHOT_PNG,  
                          "png", NULL },
  { FILTER_CLASS_SCREENSHOT_MLT,
                          "mlt", "mlt" },
  { FILTER_CLASS_SCALABLE_VECTOR_GRAPHICS,
                          "svg", "svg" },
  { FILTER_CLASS_MOVIE_FILE,
                          "fmf", "fmf" },
  { FILTER_CLASS_AY_LOGGING,
                          "psg", "psg" },
  { FILTER_CLASS_POKE_FILE,
                          NULL, "pok" },
  { FILTER_CLASS_CONTROL_MAPPING,
                          "fcm", "fcm" },
  { FILTER_CLASS_MEDIA_IF_RS232,
                          NULL, NULL },
  { FILTER_CLASS_PROFILER,
                          "prf", NULL },
  { FILTER_CLASS_GENERAL },
};

/* Filter extensions for filesel widget */
#define MAX_FILTER_EXTENSIONS 20
#define MAX_ROW_EXTENSIONS 10
static char* filter_extensions[ MAX_ROW_EXTENSIONS ][ MAX_FILTER_EXTENSIONS ];
static int last_row_extensions = -1;
static widget_filter_class last_filter = FILTER_CLASS_GENERAL;
static widget_filter_class last_position = FILTER_CLASS_GENERAL;

typedef struct widget_last_positions_t {
  widget_filter_class    class;
  size_t                 last_top_left_file;
  size_t                 last_current_file;
  int                    apply_filters;
  char*                  last_directory;
} widget_last_positions_t;

static widget_last_positions_t last_known_positions[ FILTER_CLASS_GENERAL+1 ] = {
  { FILTER_CLASS_SNAPSHOT,        0, 0, 1, NULL },
  { FILTER_CLASS_TAPE,            0, 0, 1, NULL },
  { FILTER_CLASS_DISK_PLUS3,      0, 0, 1, NULL },
  { FILTER_CLASS_DISK_TRDOS,      0, 0, 1, NULL },
  { FILTER_CLASS_DISK_OPUS,       0, 0, 1, NULL },
  { FILTER_CLASS_DISK_DIDAKTIK,   0, 0, 1, NULL },
  { FILTER_CLASS_DISK_PLUSD,      0, 0, 1, NULL },
  { FILTER_CLASS_DISK_GENERIC,    0, 0, 1, NULL },
  { FILTER_CLASS_MICRODRIVE,      0, 0, 1, NULL },
  { FILTER_CLASS_CARTRIDGE_IF2,   0, 0, 1, NULL },
  { FILTER_CLASS_CARTRIDGE_TIMEX, 0, 0, 1, NULL },
  { FILTER_CLASS_BINARY,          0, 0, 1, NULL },
  { FILTER_CLASS_ROM,             0, 0, 1, NULL },
#ifdef LIBSPECTRUM_SUPPORTS_ZLIB_COMPRESSION
#ifdef LIBSPECTRUM_SUPPORTS_BZ2_COMPRESSION
  { FILTER_CLASS_COMPRESSED,      0, 0, 1, NULL },
#else
  { FILTER_CLASS_COMPRESSED,      0, 0, 1, NULL },
#elseif LIBSPECTRUM_SUPPORTS_BZ2_COMPRESSION
  { FILTER_CLASS_COMPRESSED,      0, 0, 1, NULL },
#endif
#endif
  { FILTER_CLASS_RECORDING,       0, 0, 1, NULL },
  { FILTER_CLASS_SCREENSHOT,      0, 0, 1, NULL },
  { FILTER_CLASS_SCREENSHOT_PNG,  0, 0, 1, NULL },
  { FILTER_CLASS_SCREENSHOT_MLT,  0, 0, 1, NULL },
  { FILTER_CLASS_SCALABLE_VECTOR_GRAPHICS,
                                  0, 0, 1, NULL },
  { FILTER_CLASS_MOVIE_FILE,      0, 0, 1, NULL },
  { FILTER_CLASS_AY_LOGGING,      0, 0, 1, NULL },
  { FILTER_CLASS_POKE_FILE,       0, 0, 1, NULL },
  { FILTER_CLASS_CONTROL_MAPPING, 0, 0, 1, NULL },
  { FILTER_CLASS_MEDIA_IF_RS232,  0, 0, 1, NULL },
  { FILTER_CLASS_PROFILER,        0, 0, 1, NULL },
  { FILTER_CLASS_GENERAL,         0, 0, 1, NULL },
};

#define CURRENT_FILTERS \
( is_saving ? widget_file_filters[last_filter].filters_write \
            : widget_file_filters[last_filter].filters_read )

static  int widget_set_filter_extensions( const char* extensions );
static void widget_free_filter_extensions( void );
static  int widget_filter_extensions( const char *filename, int maybe_dir );
static  int widget_check_filename_has_extension( const char *filename, const char * extension );
static void widget_init_filter_class( widget_filter_class filter_class );
static void widget_finish_filter_class( widget_finish_state finished );
static char *widget_get_print_title( const char* title );
static void widget_print_default_save_filename( void );
#endif

static char *widget_get_filename( const char *title, int saving );

static int widget_add_filename( int *allocated, int *number,
                                struct widget_dirent ***namelist,
                                const char *name );
static void widget_scan( char *dir );
static int widget_select_file( const char *name );
static int widget_scan_compare( const widget_dirent **a,
				const widget_dirent **b );

#if !defined AMIGA && !defined __MORPHOS__
static char* widget_getcwd( void );
#endif /* ifndef AMIGA */
static int widget_print_all_filenames( struct widget_dirent **filenames, int n,
				       int top_left, int current,
				       const char *dir );
static int widget_print_filename( struct widget_dirent *filename, int position,
				  int inverted );
#ifdef WIN32
static void widget_filesel_chdrv( void );
static void widget_filesel_drvlist( void );
#endif				/* #ifdef WIN32 */
static int widget_filesel_chdir( void );

/* The filename to return */
char* widget_filesel_name;

/* Should we exit all widgets when we're done with this selector? */
static int exit_all_widgets;

static char *
widget_get_filename( const char *title, int saving )
{
  char *filename = NULL;

  widget_filesel_data data;

  data.exit_all_widgets = 1;
  data.title = title;

  if( saving ) {
    widget_do_fileselector_save( &data );
  } else {
    widget_do_fileselector( &data );
  }
  if( widget_filesel_name )
    filename = utils_safe_strdup( widget_filesel_name );

  return filename;
  
}

char *
ui_get_open_filename( const char *title )
{
#if !defined AMIGA && !defined __MORPHOS__
  return widget_get_filename( title, 0 );
#else
  return amiga_asl( title, FALSE );
#endif
}

char *
ui_get_save_filename( const char *title )
{
#if !defined AMIGA && !defined __MORPHOS__
#ifdef GCWZERO
  char* filename = widget_get_filename( title, 1 );
  if ( filename ) filename = widget_set_valid_file_extension_for_last_class( filename );
  if ( settings_current.od_confirm_overwrite_files &&
       /* Disks have their own overwrite confirm dialog */
       ( last_filter < FILTER_CLASS_DISK_PLUS3 ||
         last_filter > FILTER_CLASS_DISK_GENERIC ) ) {
    if (filename && compat_file_exists( filename ) ) {
        const char *filename1 = strrchr( filename, FUSE_DIR_SEP_CHR );
        filename1 = filename1 ? filename1 + 1 : filename;

        dont_refresh_display = 1;
        ui_confirm_save_t confirm = ui_confirm_save(
          "%s already exists.\n"
          "Do you want to overwrite it?",
          filename1
        );
        dont_refresh_display = 0;

        switch( confirm ) {

        case UI_CONFIRM_SAVE_SAVE: return filename;
        case UI_CONFIRM_SAVE_DONTSAVE: return NULL;
        case UI_CONFIRM_SAVE_CANCEL: return NULL;
        default: return NULL;
        }
    } else return filename;
  } else
    return filename;
#else
  return widget_get_filename( title, 1 );
#endif /* GCWZERO */
#else
  return amiga_asl( title, TRUE );
#endif
}

static int widget_add_filename( int *allocated, int *number,
                                struct widget_dirent ***namelist,
                                const char *name ) {
  int i; size_t length;

  if( ++*number > *allocated ) {
    struct widget_dirent **oldptr = *namelist;

    *namelist = realloc( (*namelist), 2 * *allocated * sizeof(**namelist) );
    if( *namelist == NULL ) {
      for( i=0; i<*number-1; i++ ) {
	free( oldptr[i]->name );
	free( oldptr[i] );
      }
      free( oldptr );
      return -1;
    }
    *allocated *= 2;
  }

  (*namelist)[*number-1] = malloc( sizeof(***namelist) );
  if( !(*namelist)[*number-1] ) {
    for( i=0; i<*number-1; i++ ) {
      free( (*namelist)[i]->name );
      free( (*namelist)[i] );
    }
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

  length = strlen( name ) + 1;
  if( length < 16 ) length = 16;

  (*namelist)[*number-1]->name = malloc( length );
  if( !(*namelist)[*number-1]->name ) {
    free( (*namelist)[*number-1] );
    for( i=0; i<*number-1; i++ ) {
      free( (*namelist)[i]->name );
      free( (*namelist)[i] );
    }
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

  strncpy( (*namelist)[*number-1]->name, name, length );
  (*namelist)[*number-1]->name[ length - 1 ] = 0;

  return 0;
}

#if defined AMIGA || defined __MORPHOS__
char *
amiga_asl( char *title, BOOL is_saving ) {
  char *filename;
  struct FileRequester *filereq;

#ifndef __MORPHOS__
  if( AslBase = IExec->OpenLibrary( "asl.library", 52 ) ) {
    if( IAsl = ( struct AslIFace * ) IExec->GetInterface( AslBase,"main",1,NULL ) ) {
      filereq = IAsl->AllocAslRequestTags( ASL_FileRequest,
                                           ASLFR_RejectIcons,TRUE,
                                           ASLFR_TitleText,title,
                                           ASLFR_DoSaveMode,is_saving,
                                           ASLFR_InitialPattern,"#?.(sna|z80|szx|sp|snp|zxs|tap|tzx|csw|rzx|dsk|trd|scl|mdr|dck|hdf|rom|psg|scr|mlt|png|gz|bz2)",
                                           ASLFR_DoPatterns,TRUE,
                                           TAG_DONE );
      if( err = IAsl->AslRequest( filereq, NULL ) ) {
        filename = ( STRPTR ) IExec->AllocVec( 1024, MEMF_CLEAR );
#else				/* #ifndef __MORPHOS__ */
  if( AslBase = OpenLibrary( "asl.library", 0 ) ) {
      filereq = AllocAslRequestTags( ASL_FileRequest,
                                     ASLFR_RejectIcons,TRUE,
                                     ASLFR_TitleText,title,
                                     ASLFR_DoSaveMode,is_saving,
                                     ASLFR_InitialPattern,"#?.(sna|z80|szx|sp|snp|zxs|tap|tzx|csw|rzx|dsk|trd|scl|mdr|dck|hdf|rom|psg|scr|mlt|png|gz|bz2)",
                                     ASLFR_DoPatterns,TRUE,
                                     TAG_DONE );
      if( err = AslRequest( filereq, NULL ) ) {
        filename = ( STRPTR ) AllocVec( 1024, MEMF_CLEAR );
#endif				/* #ifndef __MORPHOS__ */

        strcpy( filename,filereq->fr_Drawer );	
#ifndef __MORPHOS__
        IDOS->AddPart( filename, filereq->fr_File, 1024 );
#else				/* #ifndef __MORPHOS__ */
        AddPart( filename, filereq->fr_File, 1024 );
#endif				/* #ifndef __MORPHOS__ */
        widget_filesel_name = utils_safe_strdup( filename );
#ifndef __MORPHOS__
        IExec->FreeVec( filename );
#else				/* #ifndef __MORPHOS__ */
        FreeVec( filename );
#endif				/* #ifndef __MORPHOS__ */
        err = WIDGET_FINISHED_OK;
      } else {
        err = WIDGET_FINISHED_CANCEL;
      }
#ifndef __MORPHOS__
      IExec->DropInterface( ( struct Interface * )IAsl );
    }
    IExec->CloseLibrary( AslBase );
#else				/* #ifndef __MORPHOS__ */
    CloseLibrary( AslBase );
#endif				/* #ifndef __MORPHOS__ */
  }
  return widget_filesel_name;
}
#else /* ifdef AMIGA */

static int widget_scandir( const char *dir, struct widget_dirent ***namelist,
			   int (*select_fn)(const char*) )
{
  compat_dir directory;

  int allocated, number;
  int i;
  int done = 0;

  *namelist = malloc( 32 * sizeof(**namelist) );
  if( !*namelist ) return -1;

  allocated = 32; number = 0;

  directory = compat_opendir( dir );
  if( !directory ) {
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

#ifdef WIN32
  /* Assume this is the root directory, unless we find an entry named ".." */
  is_rootdir = 1;
#endif				/* #ifdef WIN32 */

  while( !done ) {
    char name[ PATH_MAX ];

    compat_dir_result_t result =
      compat_readdir( directory, name, sizeof( name ) );

    switch( result )
    {
    case COMPAT_DIR_RESULT_OK:
      if( select_fn( name ) ) {
#ifdef WIN32
        if( is_rootdir && !strcmp( name, ".." ) ) {
          is_rootdir = 0;
        }
#endif				/* #ifdef WIN32 */
        if( widget_add_filename( &allocated, &number, namelist, name ) ) {
          compat_closedir( directory );
          return -1;
        }
      }
      break;

    case COMPAT_DIR_RESULT_END:
      done = 1;
      break;

    case COMPAT_DIR_RESULT_ERROR:
      for( i=0; i<number; i++ ) {
        free( (*namelist)[i]->name );
        free( (*namelist)[i] );
      }
      free( *namelist );
      *namelist = NULL;
      compat_closedir( directory );
      return -1;
    }

  }

  if( compat_closedir( directory ) ) {
    for( i=0; i<number; i++ ) {
      free( (*namelist)[i]->name );
      free( (*namelist)[i] );
    }
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

#ifdef WIN32
  if( is_rootdir ) {
    /* Add a fake ".." entry for drive selection */
    if( widget_add_filename( &allocated, &number, namelist, ".." ) ) {
      return -1;
    }
  }
#endif				/* #ifdef WIN32 */

#ifdef GCWZERO
  if ( settings_current.od_save_last_directory && 
       ( !settings_current.od_last_directory ||
         strcmp( settings_current.od_last_directory, dir ) ) ) {
    libspectrum_free( settings_current.od_last_directory );
    settings_current.od_last_directory = utils_safe_strdup( dir );
  }
#endif

  return number;
}

#ifdef WIN32
static int widget_scandrives( struct widget_dirent ***namelist )
{
  int allocated, number;
  unsigned long drivemask;
  int i;
  char drive[3];
  const char *driveletters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  drive[1] = ':';
  drive[2] = '\0';

  *namelist = malloc( 32 * sizeof(**namelist) );
  if( !*namelist ) return -1;

  allocated = 32; number = 0;

  drivemask = _getdrives();
  if( !drivemask ) {
    free( *namelist );
    *namelist = NULL;
    return -1;
  }

  for( i = 0; i < 26; i++ ) {
    if( drivemask & 1) {
      drive[0] = driveletters[i];
      if( widget_add_filename( &allocated, &number, namelist, drive ) ) {
        return -1;
      }
    }
    drivemask >>= 1;
  }

  return number;
}
#endif

static void widget_scan( char *dir )
{
  struct stat file_info;

  size_t i; int error;
  
  /* Free the memory belonging to the files in the previous directory */
  for( i=0; i<widget_numfiles; i++ ) {
    free( widget_filenames[i]->name );
    free( widget_filenames[i] );
  }

#ifdef WIN32
  if( dir ) {
    widget_numfiles = widget_scandir( dir, &widget_filenames,
				      widget_select_file );
  } else {
    widget_numfiles = widget_scandrives( &widget_filenames );
  }
#else				/* #ifdef WIN32 */
  widget_numfiles = widget_scandir( dir, &widget_filenames,
				    widget_select_file );
#endif				/* #ifdef WIN32 */

  if( widget_numfiles == (size_t)-1 ) return;

  for( i=0; i<widget_numfiles; i++ ) {
    error = stat( widget_filenames[i]->name, &file_info );
    widget_filenames[i]->mode = error ? 0 : file_info.st_mode;
  }

  qsort( widget_filenames, widget_numfiles, sizeof(struct widget_dirent*),
	 (int(*)(const void*,const void*))widget_scan_compare );

}

static int
widget_select_file( const char *name )
{
  if( !name ) return 0;

  /* Skip current directory */
  if( !strcmp( name, "." ) ) return 0;

#ifndef WIN32
#ifdef GCWZERO
  if (!settings_current.od_hidden_files)
#endif
  /* Skip hidden files/directories */
  if( strlen( name ) > 1 && name[0] == '.' && name[1] != '.' ) return 0;
#endif				/* #ifdef WIN32 */

#ifdef GCWZERO
  /* Filtered extensions */
  if ( settings_current.od_filter_known_extensions &&
       last_known_positions[last_filter].apply_filters &&
       widget_filter_extensions( name, 1 ) )
    return 0;
#endif

  return 1;
}

static int widget_scan_compare( const struct widget_dirent **a,
				const struct widget_dirent **b )
{
  int isdir1 = S_ISDIR( (*a)->mode ),
      isdir2 = S_ISDIR( (*b)->mode );

  if( isdir1 && !isdir2 ) {
    return -1;
  } else if( isdir2 && !isdir1 ) {
    return 1;
  } else {
    return strcmp( (*a)->name, (*b)->name );
  }

}
#endif /* ifdef AMIGA */

/* File selection widget */

static int
widget_filesel_draw( void *data )
{
  widget_filesel_data *filesel_data = data;
  char *directory;
  int error;

  exit_all_widgets = filesel_data->exit_all_widgets;
  title = filesel_data->title;

#if !defined AMIGA && !defined __MORPHOS__
#ifdef WIN32
  if( !is_drivesel ) {
    directory = widget_getcwd();
    if( directory == NULL ) return 1;
  } else {
    directory = NULL;
  }
#else				/* #ifdef WIN32 */
  directory = widget_getcwd();
  if( directory == NULL ) return 1;
#endif				/* #ifdef WIN32 */

  widget_scan( directory );
#ifdef GCWZERO
  /* Restore position and file selected
    Only for load operations */
  if ( !is_saving && last_known_positions[last_position].last_directory &&
       strcmp( directory, last_known_positions[last_position].last_directory ) == 0 ) {
    new_current_file = current_file = last_known_positions[last_position].last_current_file;
    top_left_file = last_known_positions[last_position].last_top_left_file;
  } else {
#endif
  new_current_file = current_file = 0;
  top_left_file = 0;
#ifdef GCWZERO
  }
#endif

  /* Create the dialog box */
  error = widget_dialog_with_border( 1, 2, 30, 22 );
  if( error ) {
    free( directory );
    return error; 
  }

#ifdef WIN32
  if( directory == NULL ) {
    directory = utils_safe_strdup( "Drive selection" );
  }
#endif				/* #ifdef WIN32 */

  /* Show all the filenames */
  widget_print_all_filenames( widget_filenames, widget_numfiles,
			      top_left_file, current_file, directory );

  free( directory );

#endif /* ifndef AMIGA */

  return 0;
}

int widget_filesel_finish( widget_finish_state finished ) {

  /* Return with null if we didn't finish cleanly */
  if( finished != WIDGET_FINISHED_OK ) {
    if( widget_filesel_name ) free( widget_filesel_name );
    widget_filesel_name = NULL;
  }
#ifdef GCWZERO
  /* Save last directory
     For load operations save last position and selected file if we are
     not cancelling  */
  widget_finish_filter_class( finished );
  widget_free_filter_extensions();
#endif
  return 0;
}

int
widget_filesel_load_draw( void *data )
{
  is_saving = 0;
  return widget_filesel_draw( data );
}

int
widget_filesel_save_draw( void *data )
{
  is_saving = 1;
  return widget_filesel_draw( data );
}

#if !defined AMIGA && !defined __MORPHOS__
static char* widget_getcwd( void )
{
  char *directory; size_t directory_length;
  char *ptr;

  directory_length = 64;
  directory = malloc( directory_length * sizeof( char ) );
  if( directory == NULL ) {
    return NULL;
  }

  do {
    ptr = getcwd( directory, directory_length );
    if( ptr ) break;
    if( errno == ERANGE ) {
      ptr = directory;
      directory_length *= 2;
      directory =
	(char*)realloc( directory, directory_length * sizeof( char ) );
      if( directory == NULL ) {
	free( ptr );
	return NULL;
      }
    } else {
      free( directory );
      return NULL;
    }
  } while(1);

#ifdef WIN32
  if( directory[0] && directory[1] == ':' ) {
    directory[0] = toupper( directory[0] );
  }
#endif

  return directory;
}

#ifdef GCWZERO
static char *widget_get_print_title( const char* title )
{
  const char *filter_title = NULL;
  const char *filters = NULL;
  const int title_length = 40;
  char * final_title;

  if ( !settings_current.od_filter_known_extensions ) {
    final_title = utils_safe_strdup( title );

  } else {
    /* Quit "Fuse - " */
    if ( strncmp( title, "Fuse - ", 7 ) == 0 )
      filter_title = title + 7;
    else
      filter_title = title;

    if ( last_known_positions[last_filter].apply_filters ) {
      filters = is_saving ? widget_file_filters[last_filter].filters_write
                          : widget_file_filters[last_filter].filters_read;
    }

    final_title = libspectrum_new( char, title_length );
    if ( filters ) {
      if ( ( strlen( filter_title ) + strlen( filters ) + 3 ) >= title_length ) {
        /* Title space (filters...)\0 */
        int i = title_length - strlen( filter_title ) - 7;
        snprintf( final_title, title_length, "%s (%.*s...)", filter_title, i, filters );
      } else
        snprintf( final_title, title_length, "%s (%s)", filter_title, filters );
    } else
      snprintf( final_title, title_length, "%s (*)", filter_title );
  }

  return final_title;
}

static void widget_print_default_save_filename( void )
{
  char *default_save, *c;
  char  print_button[128];
  const char* button_label = "\012Y\001 = Save\012";
  const int   label_wide = 184;

  if ( !last_filename ) return;

  default_save = widget_set_valid_file_extension_for_last_class( utils_last_filename( last_filename, 1 ) );
  if ( default_save ) {
    c = default_save;
    if ( widget_stringwidth( default_save ) > label_wide ) {
      int prefix = widget_stringwidth( "..." ) + 1;
      while( widget_stringwidth( default_save ) > label_wide - prefix ) default_save++;
      snprintf( print_button, sizeof( print_button ), "%s ...%s\001", button_label, default_save );
    } else {
      snprintf( print_button, sizeof( print_button ), "%s %s\001", button_label, default_save );
    }
    widget_printstring( 12, 21 * 8, WIDGET_COLOUR_FOREGROUND, print_button );
    libspectrum_free( c );
  }
}
#endif

static int widget_print_all_filenames( struct widget_dirent **filenames, int n,
				       int top_left, int current,
				       const char *dir )
{
  int i;
  int error;

  /* Give us a clean box to start with */
  error = widget_dialog_with_border( 1, 2, 30, 22 );
  if( error ) return error;

#ifdef GCWZERO
  char* title_with_filters = widget_get_print_title( title );
  widget_printstring( 10, 16, WIDGET_COLOUR_TITLE, title_with_filters );
  libspectrum_free( title_with_filters );
#else
  widget_printstring( 10, 16, WIDGET_COLOUR_TITLE, title );
#endif
  if( widget_stringwidth( dir ) > 223 ) {
    char buffer[128];
    int prefix = widget_stringwidth( "..." ) + 1;
    while( widget_stringwidth( dir ) > 223 - prefix ) dir++;
    snprintf( buffer, sizeof( buffer ), "...%s", dir );
    widget_print_title( 24, WIDGET_COLOUR_FOREGROUND, buffer );  
  } else {
    widget_print_title( 24, WIDGET_COLOUR_FOREGROUND, dir );
  }

#ifdef GCWZERO
  widget_print_filetitle( 32, widget_filenames[ current ], is_saving );
#endif

  if( top_left ) widget_up_arrow( 1, 5, WIDGET_COLOUR_FOREGROUND );

  /* Print the filenames, mostly normally, but with the currently
     selected file inverted */
  for( i = top_left; i < n && i < top_left + ENTRIES_PER_SCREEN; i++ ) {
    if( i == current ) {
      widget_print_filename( filenames[i], i-top_left, 1 );
    } else {
      widget_print_filename( filenames[i], i-top_left, 0 );
    }
  }

  if( is_saving )
  {
#ifdef GCWZERO
    widget_print_default_save_filename();
#endif
    widget_printstring( 12, 22 * 8, WIDGET_COLOUR_FOREGROUND,
#ifdef GCWZERO
				     "\012A\001 = select" );
#else
				     "\012RETURN\001 = select" );
#endif
    widget_printstring_right( 244, 22 * 8, WIDGET_COLOUR_FOREGROUND,
#ifdef GCWZERO
					   "\012X\001 = enter name" );
#else
					   "\012TAB\001 = enter name" );
#endif
  }

  if( i < n )
#ifdef GCWZERO
    widget_down_arrow( 1, is_saving ? ( last_filename ? 19 : 20 ) : 22, WIDGET_COLOUR_FOREGROUND );
#else
    widget_down_arrow( 1, is_saving ? 20 : 22, WIDGET_COLOUR_FOREGROUND );
#endif

  /* Display that lot */
  widget_display_lines( 2, 22 );

  return 0;
}

/* Print a filename onto the dialog box */
static int widget_print_filename( struct widget_dirent *filename, int position,
				  int inverted )
{
  char buffer[64], suffix[64], *dot = 0;
  int width, suffix_width = 0;
  int dir = S_ISDIR( filename->mode );
  int truncated = 0, suffix_truncated = 0;

#define FILENAME_WIDTH 112
#define MAX_SUFFIX_WIDTH 56

  int x = (position & 1) ? 132 : 16,
      y = 40 + (position >> 1) * 8;

  int foreground = WIDGET_COLOUR_FOREGROUND,

      background = inverted ? WIDGET_COLOUR_HIGHLIGHT
                            : WIDGET_COLOUR_BACKGROUND;

  widget_rectangle( x, y, FILENAME_WIDTH, 8, background );

  strncpy( buffer, filename->name, sizeof( buffer ) - dir - 1);
  buffer[sizeof( buffer ) - dir - 1] = '\0';

  if (dir)
    dir = widget_charwidth( FUSE_DIR_SEP_CHR );
  else {
    /* get filename extension */
    dot = strrchr( filename->name, '.' );

    /* if .gz or .bz2, we want the previous component too */
    if( dot &&( !strcasecmp( dot, ".gz" ) || !strcasecmp( dot, ".bz2" ) ) ) {
      char *olddot = dot;
      *olddot = '\0';
      dot = strrchr( filename->name, '.' );
      *olddot = '.';
      if (!dot)
	dot = olddot;
    }

    /* if the dot is at the start of the name, ignore it */
    if( dot == filename->name )
      dot = 0;
  }

  if( dot ) {
    /* split filename at extension separator */
    if( dot - filename->name < sizeof( buffer ) )
      buffer[dot - filename->name] = '\0';

    /* get extension width (for display purposes) */
    snprintf( suffix, sizeof( suffix ), "%s", dot );
    while( ( suffix_width = ( dot && !dir )
	     ? widget_stringwidth( suffix ) : 0 ) > 110 ) {
      suffix_truncated = 1;
      suffix[strlen( suffix ) - 1] = '\0';
    }
  }

  while( ( width = widget_stringwidth( buffer ) ) >=
	 FILENAME_WIDTH - dir - ( dot ? truncated + suffix_width : 0 ) ) {
    truncated = 2;
    if( suffix_width >= MAX_SUFFIX_WIDTH ) {
      suffix_truncated = 2;
      suffix[strlen (suffix) - 1] = '\0';
      suffix_width = widget_stringwidth (suffix);
    }
    else
      buffer[strlen (buffer) - 1] = '\0';
  }
  if( dir )
    strcat (buffer, FUSE_DIR_SEP_STR );

  widget_printstring( x + 1, y, foreground, buffer );
  if( truncated )
    widget_rectangle( x + width + 2, y, 1, 8, 4 );
  if( dot )
    widget_printstring( x + width + 2 + truncated, y,
			foreground ^ 2, suffix );
  if( suffix_truncated )
    widget_rectangle( x + FILENAME_WIDTH, y, 1, 8, 4 );

  return 0;
}
#endif /* ifndef AMIGA */

#ifdef WIN32
static void
widget_filesel_chdrv( void )
{
  char *fn;

  if( chdir( widget_filenames[ current_file ]->name ) ) {
    ui_error( UI_ERROR_ERROR, "Could not change directory" );
    return;
  }

  is_drivesel = 0;
  fn = widget_getcwd();
  widget_scan( fn ); free( fn );
  new_current_file = 0;
  /* Force a redisplay of all filenames */
  current_file = 1; top_left_file = 1;
}

static void
widget_filesel_drvlist( void )
{
  is_drivesel = 1;
  widget_scan( NULL );
  new_current_file = 0;
  /* Force a redisplay of all filenames */
  current_file = 1; top_left_file = 1;
}
#endif				/* #ifdef WIN32 */

static int
widget_filesel_chdir( void )
{
  char *fn, *ptr;

  /* Get the new directory name */
  fn = widget_getcwd();
  if( fn == NULL ) {
    widget_end_widget( WIDGET_FINISHED_CANCEL );
    return 1;
  }
  ptr = fn;
  fn = realloc( fn,
     ( strlen( fn ) + 1 + strlen( widget_filenames[ current_file ]->name ) +
       1 ) * sizeof(char)
  );
  if( fn == NULL ) {
    free( ptr );
    widget_end_widget( WIDGET_FINISHED_CANCEL );
    return 1;
  }
#ifndef GEKKO
  /* Wii getcwd() already has the slash on the end */
  strcat( fn, FUSE_DIR_SEP_STR );
#endif				/* #ifndef GEKKO */
  strcat( fn, widget_filenames[ current_file ]->name );

/*
in Win32 errno resulting from chdir on file is EINVAL which may mean many things
this will not be fixed in mingw - must use native function instead
http://thread.gmane.org/gmane.comp.gnu.mingw.user/9197
*/ 

  if( chdir( fn ) == -1 ) {
#ifndef WIN32
    if( errno == ENOTDIR ) {
#else   /* #ifndef WIN32 */
    if( GetFileAttributes( fn ) != FILE_ATTRIBUTE_DIRECTORY ) {
#endif  /* #ifndef WIN32 */
      widget_filesel_name = fn; fn = NULL;
      if( exit_all_widgets ) {
	widget_end_all( WIDGET_FINISHED_OK );
      } else {
	widget_end_widget( WIDGET_FINISHED_OK );
      }
    }
  } else {
    widget_scan( fn );
    new_current_file = 0;
    /* Force a redisplay of all filenames */
    current_file = 1; top_left_file = 1;
  }

  free( fn );

  return 0;
}

void
widget_filesel_keyhandler( input_key key )
{
#if !defined AMIGA && !defined __MORPHOS__
  char *fn, *ptr;
  char *dirtitle;
#endif

  /* If there are no files (possible on the Wii), can't really do anything */
  if( widget_numfiles == 0 ) {
    if( key == INPUT_KEY_Escape ) widget_end_widget( WIDGET_FINISHED_CANCEL );
    return;
  }
  
#if defined AMIGA || defined __MORPHOS__
  if( exit_all_widgets ) {
    widget_end_all( err );
  } else {
    widget_end_widget( err );
  }
#else  /* ifndef AMIGA */

  new_current_file = current_file;

  switch(key) {

#if 0
  case INPUT_KEY_Resize:	/* Fake keypress used on window resize */
    widget_dialog_with_border( 1, 2, 30, 20 );
    widget_print_all_filenames( widget_filenames, widget_numfiles,
				top_left_file, current_file        );
    break;
#endif

#ifdef GCWZERO
  case INPUT_KEY_Home: /* Power   */
  case INPUT_KEY_End:  /* RetroFW */
    widget_end_all( WIDGET_FINISHED_CANCEL );
    return;
#endif

#ifdef GCWZERO
  case INPUT_KEY_Alt_L: /* B */
#else
  case INPUT_KEY_Escape:
#endif
  case INPUT_JOYSTICK_FIRE_2:
    widget_end_widget( WIDGET_FINISHED_CANCEL );
    break;
  
  case INPUT_KEY_Left:
  case INPUT_KEY_5:
  case INPUT_KEY_h:
  case INPUT_JOYSTICK_LEFT:
    if( current_file > 0                 ) new_current_file--;
    break;

  case INPUT_KEY_Down:
  case INPUT_KEY_6:
  case INPUT_KEY_j:
  case INPUT_JOYSTICK_DOWN:
    if( current_file+2 < widget_numfiles ) new_current_file += 2;
    break;

  case INPUT_KEY_Up:
  case INPUT_KEY_7:		/* Up */
  case INPUT_KEY_k:
  case INPUT_JOYSTICK_UP:
    if( current_file > 1                 ) new_current_file -= 2;
    break;

  case INPUT_KEY_Right:
  case INPUT_KEY_8:
  case INPUT_KEY_l:
  case INPUT_JOYSTICK_RIGHT:
    if( current_file < widget_numfiles-1 ) new_current_file++;
    break;

#ifdef GCWZERO
  case INPUT_KEY_Tab: /* L1 */
#else
  case INPUT_KEY_Page_Up:
#endif
    new_current_file = ( current_file > ENTRIES_PER_SCREEN ) ?
                       current_file - ENTRIES_PER_SCREEN     :
                       0;
    break;

#ifdef GCWZERO
  case INPUT_KEY_BackSpace: /* R1 */
#else
  case INPUT_KEY_Page_Down:
#endif
    new_current_file = current_file + ENTRIES_PER_SCREEN;
    if( new_current_file >= widget_numfiles )
      new_current_file = widget_numfiles - 1;
    break;

#ifdef GCWZERO
  case INPUT_KEY_Page_Up:  /* L2 */
#else
  case INPUT_KEY_Home:
#endif
    new_current_file = 0;
    break;

#ifdef GCWZERO
  case INPUT_KEY_Page_Down: /* R2 */
#else
  case INPUT_KEY_End:
#endif
    new_current_file = widget_numfiles - 1;
    break;

#ifdef GCWZERO
  case INPUT_KEY_Escape: /* Select */
    /* Switch on/off apply filters */
    if ( settings_current.od_filter_known_extensions && CURRENT_FILTERS ) {
      last_known_positions[last_filter].apply_filters = !last_known_positions[last_filter].apply_filters;
      char *directory = widget_getcwd();
      if( directory ) {
        widget_scan( directory );
        new_current_file = 0;
        /* Force a redisplay of all filenames */
        current_file = 1; top_left_file = 1;
        /* Filters switch. Reset last position */
        if ( !is_saving ) {
          last_known_positions[last_position].last_current_file = 0;
          last_known_positions[last_position].last_top_left_file = 0;
        }
      }
    }
    break;

  case INPUT_KEY_Shift_L: /* Y */
    /* Save dialog and we are previosly load something */
    if ( is_saving && last_filename ) {
      char *default_name = widget_set_valid_file_extension_for_last_class( utils_last_filename( last_filename, 1 ) );
      if ( default_name ) {
        /*
           Code stolen from Tab case below...
           there should be better to do a specific function that use both two cases
           but for now I do not want to do more changes than necesary on Fuse standard code
         */
        if( !compat_is_absolute_path( default_name ) ) {
		  /* relative name */
          /* Get current dir name and allocate space for the leafname */
          fn = widget_getcwd();
          ptr = fn;
          if( fn )
    	    fn = realloc( fn, strlen( fn ) + strlen( default_name ) + 2 );
          if( !fn ) {
            free( ptr );
	        widget_end_widget( WIDGET_FINISHED_CANCEL );
	        return;
          }
          /* Append the leafname and return it */
          strncat( fn, FUSE_DIR_SEP_STR, 1 );
          strncat( fn, default_name, strlen( default_name ) );
        /* absolute name */
        } else {
	      fn = utils_safe_strdup( default_name );
        }
        widget_filesel_name = fn;
        if( exit_all_widgets ) {
	      widget_end_all( WIDGET_FINISHED_OK );
        } else {
	      widget_end_widget( WIDGET_FINISHED_OK );
        }
      }
    }
    break;

  case INPUT_KEY_space: /* X */
#else
  case INPUT_KEY_Tab:
#endif
    if( is_saving ) {
      widget_text_t text_data;
      text_data.title = title;
      text_data.allow = WIDGET_INPUT_ASCII;
      text_data.max_length = 30;
#ifdef GCWZERO
      if (last_filename)
        snprintf( text_data.text, 30, "%s", utils_last_filename( last_filename, 1 ) );
      else
#endif
      text_data.text[0] = 0;
      if( widget_do_text( &text_data ) ||
	  !widget_text_text || !*widget_text_text      )
	break;
      if( !compat_is_absolute_path( widget_text_text ) ) {
							/* relative name */
        /* Get current dir name and allocate space for the leafname */
        fn = widget_getcwd();
        ptr = fn;
        if( fn )
    	  fn = realloc( fn, strlen( fn ) + strlen( widget_text_text ) + 2 );
        if( !fn ) {
          free( ptr );
	  widget_end_widget( WIDGET_FINISHED_CANCEL );
	  return;
        }
        /* Append the leafname and return it */
        strcat( fn, FUSE_DIR_SEP_STR );
        strcat( fn, widget_text_text );
      } else {						/* absolute name */
	fn = utils_safe_strdup( widget_text_text );
      }
      widget_filesel_name = fn;
      if( exit_all_widgets ) {
	widget_end_all( WIDGET_FINISHED_OK );
      } else {
	widget_end_widget( WIDGET_FINISHED_OK );
      }
    }
    break;

#ifdef GCWZERO
  case INPUT_KEY_Control_L: /* A */
#else
  case INPUT_KEY_Return:
  case INPUT_KEY_KP_Enter:
#endif
  case INPUT_JOYSTICK_FIRE_1:
#ifdef WIN32
    if( is_drivesel ) {
      widget_filesel_chdrv();
    } else if( is_rootdir &&
	       !strcmp( widget_filenames[ current_file ]->name, ".." ) ) {
      widget_filesel_drvlist();
    } else {
#endif				/* #ifdef WIN32 */
      if( widget_filesel_chdir() ) return;
#ifdef WIN32
    }
#endif				/* #ifdef WIN32 */
    break;

  default:	/* Keep gcc happy */
    break;

  }

#ifdef WIN32
  if( is_drivesel ) {
    dirtitle = utils_safe_strdup( "Drive selection" );
  } else {
#endif				/* #ifdef WIN32 */
    dirtitle = widget_getcwd();
#ifdef WIN32
  }
#endif				/* #ifdef WIN32 */

  /* If we moved the cursor */
  if( new_current_file != current_file ) {

    /* If we've got off the top or bottom of the currently displayed
       file list, then reset the top-left corner and display the whole
       thing */
    if( new_current_file < top_left_file ) {

      top_left_file = new_current_file & ~1;
      widget_print_all_filenames( widget_filenames, widget_numfiles,
				  top_left_file, new_current_file, dirtitle );

    } else if( new_current_file >= top_left_file+ENTRIES_PER_SCREEN ) {

      top_left_file = new_current_file & ~1;
      top_left_file -= ENTRIES_PER_SCREEN - 2;
      widget_print_all_filenames( widget_filenames, widget_numfiles,
				  top_left_file, new_current_file, dirtitle );

    } else {

      /* Otherwise, print the current file uninverted and the
	 new current file inverted */

#ifdef GCWZERO
      widget_print_filetitle( 32, widget_filenames[ new_current_file ], is_saving );
#endif

      widget_print_filename( widget_filenames[ current_file ],
			     current_file - top_left_file, 0 );
	  
      widget_print_filename( widget_filenames[ new_current_file ],
			     new_current_file - top_left_file, 1 );
        
      widget_display_lines( 2, 21 );
    }

    /* Reset the current file marker */
    current_file = new_current_file;

  }

  free( dirtitle );
#endif /* ifdef AMIGA */
}

#ifdef GCWZERO
static void widget_init_filter_class( widget_filter_class filter_class )
{
  if ( !is_saving && !settings_current.od_independent_directory_access &&
       last_filter != filter_class &&
       ( ( widget_file_filters[last_filter].filters_read && last_known_positions[last_filter].apply_filters ) ||
         ( widget_file_filters[filter_class].filters_read && last_known_positions[filter_class].apply_filters ) ) ) {
    last_known_positions[FILTER_CLASS_GENERAL].last_current_file = 0;
    last_known_positions[FILTER_CLASS_GENERAL].last_top_left_file = 0;
  }

  last_filter = filter_class;
  if ( settings_current.od_independent_directory_access )
    last_position = filter_class;
  else
    last_position = FILTER_CLASS_GENERAL;

  if ( last_known_positions[last_position].last_directory )
    chdir( last_known_positions[last_position].last_directory );
  else
    last_known_positions[last_position].last_directory = widget_getcwd();
}

static void widget_finish_filter_class( widget_finish_state finished )
{

  if( finished == WIDGET_FINISHED_OK ) {
    if ( !is_saving )  {
      last_known_positions[last_position].last_current_file = current_file;
      last_known_positions[last_position].last_top_left_file = top_left_file;
    }
    if ( last_known_positions[last_position].last_directory )
      free( last_known_positions[last_position].last_directory );
    last_known_positions[last_position].last_directory = widget_getcwd();
  } else if ( last_known_positions[last_position].last_directory ) {
    char *last_dir = widget_getcwd();
    if ( !last_dir || strcmp( last_dir, last_known_positions[last_position].last_directory ) )
      chdir( last_known_positions[last_position].last_directory );
    if ( last_dir ) free( last_dir );
  }
}

/* Push extensions to filter files for filesel widget dialogs */
void
widget_filesel_set_filter_for_class( widget_filter_class filter_class, int saving )
{
  widget_init_filter_class( filter_class );
  widget_set_filter_extensions( saving ? widget_file_filters[filter_class].filters_write
                                       : widget_file_filters[filter_class].filters_read );
}

/*
 * This function may change the content of parameter filename
 */
char*
widget_set_valid_file_extension_for_class( char* filename, widget_filter_class class )
{
  char* extension;
  char* new_filename;
  size_t extension_length, file_length;
  
  if ( !filename ) return NULL;

  /* Init supported extensions for filter class to write */
  widget_set_filter_extensions( widget_file_filters[class].filters_write );

  /* Add the first supported extension if filename do not have an extension supported */
  if ( widget_filter_extensions( filename, 0 ) ) {
    /* Extension length */
    extension_length = strlen( filter_extensions[last_row_extensions][0] );
    file_length = strlen( filename );

    /* New filename with space to filename + '.' + extension + '\0' */
    new_filename = libspectrum_new( char, file_length + extension_length + 2 );
    if ( new_filename ) {
      /* Copy filename including '\0' */
      strncpy( new_filename, filename, file_length + 1 );

      /* Create default extension for write */
      extension = libspectrum_new( char, extension_length + 2 );
      strncpy( extension, ".\0", 2 );
      strncat( extension, filter_extensions[last_row_extensions][0], extension_length + 1 );

      /* And append to filename  */
      strncat( new_filename, extension, extension_length + 2 );

      /* Free memory */
      libspectrum_free( extension );
    } else
      new_filename = filename;

  /* Filename have supported extension */
  } else
    new_filename = filename;

  widget_free_filter_extensions();
  return new_filename;
}

char*
widget_set_valid_file_extension_for_last_class( char* filename )
{
  return widget_set_valid_file_extension_for_class( filename, last_filter );
}

/*
 *   Extensions to filter supplied must be separated by ";".
 *   Return 0 if all extensiones set
 */
static int widget_set_filter_extensions( const char* extensions )
{
  char* extension_test;
  char* next;
  int   i = 0;

  /* No free extensions */
  if ( last_row_extensions == MAX_ROW_EXTENSIONS )
    return 1;

  extension_test = utils_safe_strdup( extensions );

  /* No extensions to push */
  if ( !extension_test ) {
    libspectrum_free( extension_test );
    return 0;
  }
  last_row_extensions++;

  /* Search for extensions, separated by ; */
  next = strtok( extension_test, ";" );
  while ( next ) {
    filter_extensions[last_row_extensions][i] = utils_safe_strdup( next );
    next = strtok( NULL, ";" );
    i++;
  }

  libspectrum_free( extension_test );
  return 0;
}

/* Free las filter extensions set */
static void widget_free_filter_extensions( void )
{
  int i = 0;

  /* No filters to clean */
  if ( last_row_extensions == -1 ) return;

  while ( filter_extensions[last_row_extensions][i] ) {
    libspectrum_free( filter_extensions[last_row_extensions][i] );
    filter_extensions[last_row_extensions][i] = NULL;
    i++;
  }
  last_row_extensions--;
}

/*
 *  To check if a filename has extension
 *
 *    0 - Has the extension
 *    1 - Do not has the extension
 */
static int
widget_check_filename_has_extension( const char *filename, const char * extension )
{
  char*  filename_test;
  char*  file_extension;
  char*  save_extension = NULL;
  int    filter = 0;

  if ( ( filename && !extension ) || ( !filename && extension ) )
    return 1;

  filename_test = utils_safe_strdup( filename );

  if ( filename_test ) {
    /* Search for extension */
    file_extension = strtok( filename_test, "." );
    while ( file_extension ) {
      /* Free previous saved extension */
      libspectrum_free( save_extension );
      save_extension = utils_safe_strdup( file_extension );
      file_extension = strtok( NULL, "." );
    }

    /* File have extension, compare with needed */
    if ( save_extension ) {
      if ( strcasecmp( save_extension, extension ) != 0 ) {
        filter = 1; /* No same extension */
      }

    /* File without extension */
    } else {
      filter = 1; /* Filter */
    }
  }
  libspectrum_free( save_extension );
  libspectrum_free( filename_test );

  return filter;
}

/*
 * Check filename for filter extensions
 * 
 *   1 filter
 *   0 don't filter
 */
static int widget_filter_extensions( const char *filename, int maybe_dir )
{
  int i;
  struct stat file_info;
  int error, is_dir;

  /* No filters set */
  if ( last_row_extensions == -1 ) return 0;

  /* If not filters sets then don't filter*/
  if ( !filter_extensions[last_row_extensions][0] ) return 0;

  /* Do not filter directories */
  if ( maybe_dir ) {
    error = stat( filename, &file_info );
    if ( !error ) is_dir = S_ISDIR( file_info.st_mode );
    if( error || is_dir ) return 0;
  }

  /* If one of the extensions is valid then do not filter */
  i = 0;
  while ( ( i < MAX_FILTER_EXTENSIONS ) && filter_extensions[i] ) {
    if ( !widget_check_filename_has_extension( filename, filter_extensions[last_row_extensions][i] ) )
      return 0; /* Do not filter */
    i++;
  }

  /* Filter */
  return 1;
}
#endif
