/*
 * icons.[ch] written by Paolo Bacchilega, who writes:
 * "There is no problem for me, you can license my code under whatever licence you wish :)"
 */

#include <string.h> /* strcmp */
#include <glib.h>
#include <gtk/gtk.h>
#include "icons.h"

#ifdef HAVE_GIO
 #include <gio/gio.h>
#endif

#define VOID_PIXBUF_KEY "void-pixbuf"

#ifdef HAVE_GIO
static const char *
get_static_string( const char *s )
{
    static GHashTable *static_strings = NULL;
    const char *result;

    if ( s == NULL )
        return NULL;

    if ( static_strings == NULL )
        static_strings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    if ( ! g_hash_table_lookup_extended( static_strings, s, (gpointer) &result, NULL )) {
        result = g_strdup( s );
        g_hash_table_insert( static_strings,
                             (gpointer) result,
                             GINT_TO_POINTER( 1 ));
    }

    return result;
}
#endif


typedef struct {
    GtkIconTheme * icon_theme;
    int            icon_size;
    GHashTable   * cache;
} IconCache;


static IconCache *icon_cache[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };


static GdkPixbuf*
create_void_pixbuf (int width,
                    int height)
{
    GdkPixbuf *p;

    p = gdk_pixbuf_new( GDK_COLORSPACE_RGB,
                        TRUE,
                        8,
                        width,
                        height);
    gdk_pixbuf_fill( p, 0xFFFFFF00 );

    return p;
}


static int
get_size_in_pixels( GtkWidget   * widget,
                    GtkIconSize   icon_size )
{
    int width, height;

    gtk_icon_size_lookup_for_settings( gtk_widget_get_settings( widget ),
                                       icon_size,
                                       &width, 
                                       &height );
    return MAX( width, height );
}


static IconCache * 
icon_cache_new (GtkWidget * for_widget,
		int         icon_size)
{
    IconCache * icon_cache;
	
    g_return_val_if_fail( for_widget != NULL, NULL );
	
    icon_cache = g_new0( IconCache, 1 );
    icon_cache->icon_theme = gtk_icon_theme_get_for_screen( gtk_widget_get_screen( for_widget )); 
    icon_cache->icon_size = get_size_in_pixels( for_widget, icon_size );
    icon_cache->cache = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
	
    g_hash_table_insert( icon_cache->cache, (void*)VOID_PIXBUF_KEY, create_void_pixbuf( icon_cache->icon_size, icon_cache->icon_size ));
	
    return icon_cache;
}

#ifdef HAVE_GIO
static const char *
_icon_cache_get_icon_key( GIcon * icon )
{
    const char * key = NULL;
	
    if ( G_IS_THEMED_ICON( icon )) {
        char ** icon_names;
        char  * name;
	
        g_object_get( icon, "names", &icon_names, NULL );
        name = g_strjoinv( ",", icon_names );
		
        key = get_static_string( name );
		
        g_free( name );
        g_strfreev( icon_names );	
    }
    else if ( G_IS_FILE_ICON( icon )) {
        GFile * file;
        char  * filename;

        file = g_file_icon_get_file( G_FILE_ICON( icon ));
        filename = g_file_get_path( file );
		
        key = get_static_string( filename );
		
        g_free( filename );
        g_object_unref( file );
    }
	
    return key;
}


static GdkPixbuf *
get_themed_icon_pixbuf( GThemedIcon  *icon,
		        int           size,
		        GtkIconTheme *icon_theme )
{
    char        ** icon_names;
    GtkIconInfo  * icon_info;
    GdkPixbuf    * pixbuf;
    GError       * error = NULL;

    g_object_get( icon, "names", &icon_names, NULL );

    icon_info = gtk_icon_theme_choose_icon( icon_theme, (const char **)icon_names, size, 0 );
    if ( icon_info == NULL )
        icon_info = gtk_icon_theme_lookup_icon( icon_theme, "text-x-generic", size, GTK_ICON_LOOKUP_USE_BUILTIN );

    pixbuf = gtk_icon_info_load_icon( icon_info, &error );
    if ( pixbuf == NULL ) {
        if( error && error->message )
            g_warning( "could not load icon pixbuf: %s\n", error->message );
        g_clear_error( &error );
    }

    gtk_icon_info_free( icon_info );
    g_strfreev( icon_names );

    return pixbuf;
}


static GdkPixbuf *
get_file_icon_pixbuf( GFileIcon *icon,
		      int        size )
{
    GFile     * file;
    char      * filename;
    GdkPixbuf * pixbuf;
	
    file = g_file_icon_get_file( icon );
    filename = g_file_get_path( file );
    pixbuf = gdk_pixbuf_new_from_file_at_size( filename, size, -1, NULL );
    g_free( filename );
    g_object_unref( file );
	
    return pixbuf;
}


static GdkPixbuf *
_get_icon_pixbuf( GIcon        *icon,
		  int           size,
		  GtkIconTheme *theme )
{
    if ( icon == NULL )
        return NULL;
    if ( G_IS_THEMED_ICON (icon) )
        return get_themed_icon_pixbuf( G_THEMED_ICON( icon ), size, theme );
    if ( G_IS_FILE_ICON (icon) )
        return get_file_icon_pixbuf( G_FILE_ICON( icon ), size );
    return NULL;
}

                  
static GdkPixbuf *
icon_cache_get_mime_type_icon( IconCache  * icon_cache,
		               const char * mime_type )
{
    GIcon      * icon;
    const char * key = NULL;
    GdkPixbuf  * pixbuf;

    icon = g_content_type_get_icon( mime_type );
    key = _icon_cache_get_icon_key( icon );
	
    if (key == NULL)
        key = VOID_PIXBUF_KEY;

    pixbuf = g_hash_table_lookup( icon_cache->cache, key );
    if (pixbuf != NULL) {
        g_object_ref( pixbuf );
        return pixbuf;
    }
	
    pixbuf = _get_icon_pixbuf( icon, icon_cache->icon_size, icon_cache->icon_theme );
    g_hash_table_insert( icon_cache->cache, (gpointer) key, g_object_ref( pixbuf ));
	
    return pixbuf;
}

#else /* HAVE_GIO */

static GdkPixbuf *
icon_cache_get_mime_type_icon( IconCache  * icon_cache,
		               const char * mime_type )
{
    GdkPixbuf  * pixbuf;
    const char * stock_name;

    if( !strcmp( mime_type, DIRECTORY_MIME_TYPE ) )
        stock_name = GTK_STOCK_DIRECTORY;
    else
        stock_name = GTK_STOCK_FILE;

    pixbuf = g_hash_table_lookup( icon_cache->cache, stock_name );

    if( pixbuf != NULL )
    {
        g_object_ref( pixbuf );
        return pixbuf;
    }

    pixbuf = gtk_icon_theme_load_icon( icon_cache->icon_theme,
                                       stock_name,
                                       icon_cache->icon_size,
                                       0, NULL );
    g_hash_table_insert( icon_cache->cache, (gpointer) stock_name, g_object_ref( pixbuf ));
    return pixbuf;
}

#endif


GdkPixbuf * 
get_mime_type_icon( const char   * mime_type,
                    GtkIconSize    icon_size,
                    GtkWidget    * for_widget )
{
    int n;
    
    switch ( icon_size ) {
        case GTK_ICON_SIZE_MENU:            n = 1; break;
        case GTK_ICON_SIZE_SMALL_TOOLBAR:   n = 2; break;
        case GTK_ICON_SIZE_LARGE_TOOLBAR:   n = 3; break;
        case GTK_ICON_SIZE_BUTTON:          n = 4; break;
        case GTK_ICON_SIZE_DND:             n = 5; break;
        case GTK_ICON_SIZE_DIALOG:          n = 6; break; 
        default /*GTK_ICON_SIZE_INVALID*/:  n = 0; break;
    }	

    if( icon_cache[n] == NULL )
        icon_cache[n] = icon_cache_new( for_widget, icon_size );

    return icon_cache_get_mime_type_icon( icon_cache[n], mime_type );
}                    


const char * 
get_mime_type_from_filename( const char * file G_GNUC_UNUSED )
{
#ifdef HAVE_GIO
    char * tmp = g_content_type_guess( file, NULL, 0, NULL );
    const char * ret = get_static_string( tmp );
    g_free( tmp );
    return ret;
#else
    return "uncertain";
#endif
}
