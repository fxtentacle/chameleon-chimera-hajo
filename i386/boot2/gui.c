/*
 *  gui.c
 *  
 *
 *  Created by Jasmin Fazlic on 18.12.08.
 *  Copyright 2008/09 Jasmin Fazlic All rights reserved.
 *  Copyright 2008/09 iNDi All rights reserved.
 *
 */

#include "gui.h"
#include "appleboot.h"
#include "vers.h"

#ifdef EMBED_THEME
#include "art.h"
#define LOADPNG(img) \
	if(!loadThemeImage(#img".png")) \
		if(!loadPixmapFromEmbeddedPng(__## img ##_png, __## img ##_png_len, images[imageCnt].image)) \
			return 1; \
	imageCnt++
#else
#define LOADPNG(img) \
	if(!loadThemeImage(#img".png")) \
		return 1;
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define VIDEO(x) (bootArgs->Video.v_ ## x)

#define vram VIDEO(baseAddr)

int lasttime=0; // we need this for animating maybe

extern int gDeviceCount;


image_t images[] = {

	{.name = "background",						.image = 0},
	{.name = "logo",							.image = 0},

	{.name = "device_generic",					.image = 0},
	{.name = "device_hfsplus",					.image = 0},
	{.name = "device_ext3",						.image = 0},
	{.name = "device_fat16",					.image = 0},
	{.name = "device_fat32",					.image = 0},
	{.name = "device_ntfs",						.image = 0},
	{.name = "device_cdrom",					.image = 0},
	{.name = "device_selection",				.image = 0},
	{.name = "device_scroll_prev",				.image = 0},
	{.name = "device_scroll_next",				.image = 0},

	{.name = "menu_boot",						.image = 0},
	{.name = "menu_verbose",					.image = 0},
	{.name = "menu_ignore_caches",				.image = 0},
	{.name = "menu_single_user",				.image = 0},
	{.name = "menu_memory_info",				.image = 0},
	{.name = "menu_video_info",					.image = 0},
	{.name = "menu_help",						.image = 0},
	{.name = "menu_verbose_disabled",			.image = 0},
	{.name = "menu_ignore_caches_disabled",		.image = 0},
	{.name = "menu_single_user_disabled",		.image = 0},
	{.name = "menu_selection",					.image = 0},

	{.name = "progress_bar",					.image = 0},
	{.name = "progress_bar_background",			.image = 0},

	{.name = "text_scroll_prev",				.image = 0},
	{.name = "text_scroll_next",				.image = 0},

	{.name = "font_small",						.image = 0},
	{.name = "font_console",					.image = 0},

};

enum {
	iBackground = 0,
	iLogo,

	iDeviceGeneric,
	iDeviceHFS,
	iDeviceEXT3,
	iDeviceFAT16,
	iDeviceFAT32,
	iDeviceNTFS,
	iDeviceCDROM,
	iSelection,
	iDeviceScrollPrev,
	iDeviceScrollNext,

	iMenuBoot,
	iMenuVerbose,
	iMenuIgnoreCaches,
	iMenuSingleUser,
	iMenuMemoryInfo,
	iMenuVideoInfo,
	iMenuHelp,
	iMenuVerboseDisabled,
	iMenuIgnoreCachesDisabled,
	iMenuSingleUserDisabled,
	iMenuSelection,

	iProgressBar,
	iProgressBarBackground,

	iTextScrollPrev,
	iTextScrollNext,
	
	iFontConsole,
	iFontSmall,
};

int imageCnt = 0;

extern int	gDeviceCount;
extern int	selectIndex;

extern MenuItem *menuItems;

char prompt[BOOT_STRING_LEN];

int prompt_pos=0;

char prompt_text[] = "boot: ";
 
menuitem_t infoMenuItems[] =
{
	{ .text = "Boot" },
	{ .text = "Boot Verbose" },
	{ .text = "Boot Ignore Caches" },
	{ .text = "Boot Single User" },
	{ .text = "Memory Info" },
	{ .text = "Video Info" },
	{ .text = "Help" }
};


int  initFont(font_t *font, image_t *image);
void colorFont(font_t *font, uint32_t color);
int  loadGraphics();
void makeRoundedCorners(pixmap_t *p);

BOOL testForQemu();

int infoMenuSelection = 0;
int infoMenuItemsCount = sizeof(infoMenuItems)/sizeof(infoMenuItems[0]);

BOOL infoMenuNativeBoot = 0;

unsigned long screen_params[4];		// here we store the used screen resolution

pixmap_t *getCroppedPixmapAtPosition( pixmap_t *from, position_t pos, uint16_t width, uint16_t height )
{
	
	pixmap_t *cropped = MALLOC( sizeof( pixmap_t ) );
	if( !cropped )
		return 0;
	cropped->pixels = MALLOC( width * height * 4 );
	if ( !cropped->pixels )
		return 0;
	
	cropped->width = width;
	cropped->height = height;
	
	int destx = 0, desty = 0;
	int srcx = pos.x, srcy = pos.y;
	
	for( ; desty < height; desty++, srcy++)
	{
		for( destx = 0, srcx = pos.x; destx < width; destx++, srcx++ )
		{
			pixel( cropped, destx, desty ).value = pixel( from, srcx, srcy ).value;
		}
	}
	return cropped;
}

int createBackBuffer( window_t *window )
{
	gui.backbuffer = MALLOC(sizeof(pixmap_t));
	if(!gui.backbuffer)
		return 1;
	
	gui.backbuffer->pixels = MALLOC( window->width * window->height * 4 );
	if(!gui.backbuffer->pixels)
	{
		free(gui.backbuffer);
		gui.backbuffer = 0;
		return 1;
	}
	
	gui.backbuffer->width = gui.screen.width;
	gui.backbuffer->height = gui.screen.height;
 
	return 0;
}

int createWindowBuffer( window_t *window )
{
	window->pixmap = MALLOC(sizeof(pixmap_t));
	if(!window->pixmap)
		return 1;
	
	window->pixmap->pixels = MALLOC( window->width * window->height * 4 );
	if(!window->pixmap->pixels)
	{
		free(window->pixmap);
		window->pixmap = 0;
		return 1;
	}
	
	window->pixmap->width = window->width;
	window->pixmap->height = window->height;
		
	return 0;
}

void fillPixmapWithColor(pixmap_t *pm, uint32_t color)
{
	int x,y;
	
	// fill with given color AARRGGBB
	for( x=0; x < pm->width; x++ )
		for( y=0; y< pm->height; y++)
			pixel(pm,x,y).value = color;
}

void drawBackground()
{
	// reset text cursor
	gui.screen.cursor.x = gui.screen.hborder;
	gui.screen.cursor.y = gui.screen.vborder;
	
	fillPixmapWithColor( gui.screen.pixmap, gui.screen.bgcolor);
	
	// draw background.png into background buffer
	blend( images[iBackground].image, gui.screen.pixmap, gui.background.pos );
	
	// draw logo.png into background buffer
	blend( images[iLogo].image, gui.screen.pixmap, gui.logo.pos);
	
	memcpy( gui.backbuffer->pixels, gui.screen.pixmap->pixels, gui.backbuffer->width * gui.backbuffer->height * 4 );
}

void loadThemeValues(config_file_t *theme, BOOL overide)
{
	BOOL themeEnabled;

	if ( (getBoolForKey("Enabled", &themeEnabled, theme ) ) || ( overide ) )
	{
		unsigned int screen_width  = gui.screen.width;
		unsigned int screen_height = gui.screen.height;
		
		unsigned int pixel;
		
		int	alpha;				// transparency level 0 (obligue) - 255 (transparent)
		
		uint32_t color;			// color value formatted RRGGBB

		int val, len;
		const char *string;	
		
		/*
		 * Parse screen parameters
		 */
		
		if(getColorForKey("screen_bgcolor", &color, theme ))
			gui.screen.bgcolor = (color & 0x00FFFFFF);

		if(getIntForKey("screen_textmargin_h", &val, theme))
			gui.screen.hborder = MIN( gui.screen.width , val );
		
		if(getIntForKey("screen_textmargin_v", &val, theme))
			gui.screen.vborder = MIN( gui.screen.height , val );

		/*
		 * Parse background parameters
		 */
		
		if(getDimensionForKey("background_pos_x", &pixel, theme, screen_width , images[iBackground].image->width ) )
			gui.background.pos.x = pixel;

		if(getDimensionForKey("background_pos_y", &pixel, theme, screen_height , images[iBackground].image->height ) )
			gui.background.pos.y = pixel;
		
		/*
		 * Parse logo parameters
		 */

		if(getDimensionForKey("logo_pos_x", &pixel, theme, screen_width , images[iLogo].image->width ) )
			gui.logo.pos.x = pixel;
		
		if(getDimensionForKey("logo_pos_y", &pixel, theme, screen_height , images[iLogo].image->height ) )
			gui.logo.pos.y = pixel;

		/*
		 * Parse progress bar parameters
		 */
		
		if(getDimensionForKey("progressbar_pos_x", &pixel, theme, screen_width , 0 ) )
			gui.progressbar.pos.x = pixel;
		
		if(getDimensionForKey("progressbar_pos_y", &pixel, theme, screen_height , 0 ) )
			gui.progressbar.pos.y = pixel;

		/*
		 * Parse countdown text parameters
		 */
		
		if(getDimensionForKey("countdown_pos_x", &pixel, theme, screen_width , 0 ) )
			gui.countdown.pos.x = pixel;
		
		if(getDimensionForKey("countdown_pos_y", &pixel, theme, screen_height , 0 ) )
			gui.countdown.pos.y = pixel;
		
		/*
		 * Parse devicelist parameters
		 */
	
		if(getIntForKey("devices_max_visible", &val, theme ))
			gui.maxdevices = MIN( val, gDeviceCount );
		
		if(getIntForKey("devices_iconspacing", &val, theme ))
			gui.devicelist.iconspacing = val;
		
		// check layout for horizontal or vertical
		if(getValueForKey( "devices_layout", &string, &len, theme ) )
			if (!strcmp (string, "vertical"))		
				gui.layout = VerticalLayout;
			else
				gui.layout = HorizontalLayout;
		else
			gui.layout = HorizontalLayout;
		
		switch (gui.layout)
		{
				
			case VerticalLayout:
				gui.devicelist.height = ( ( images[iSelection].image->height + font_console.chars[0]->height + gui.devicelist.iconspacing ) * MIN( gui.maxdevices, gDeviceCount ) + ( images[iDeviceScrollPrev].image->height + images[iDeviceScrollNext].image->height ) + gui.devicelist.iconspacing );
				gui.devicelist.width  = ( images[iSelection].image->width + gui.devicelist.iconspacing );
				
				if(getDimensionForKey("devices_pos_x", &pixel, theme, gui.screen.width , images[iSelection].image->width ) )
					gui.devicelist.pos.x = pixel;
				
				if(getDimensionForKey("devices_pos_y", &pixel, theme, gui.screen.height , gui.devicelist.height ) )
					gui.devicelist.pos.y = pixel;
				
				break;
				
			default:
				
			case HorizontalLayout:
				
				gui.devicelist.width = ( ( images[iSelection].image->width + gui.devicelist.iconspacing ) * MIN( gui.maxdevices, gDeviceCount ) + ( images[iDeviceScrollPrev].image->width + images[iDeviceScrollNext].image->width ) + gui.devicelist.iconspacing );
				gui.devicelist.height = ( images[iSelection].image->height + font_console.chars[0]->height + gui.devicelist.iconspacing );
				
				if(getDimensionForKey("devices_pos_x", &pixel, theme, gui.screen.width , gui.devicelist.width ) )
					gui.devicelist.pos.x = pixel;
				else
					gui.devicelist.pos.x = ( gui.screen.width - gui.devicelist.width ) / 2;
				
				if(getDimensionForKey("devices_pos_y", &pixel, theme, gui.screen.height , images[iSelection].image->height ) )
					gui.devicelist.pos.y = pixel;
				else
					gui.devicelist.pos.y = ( gui.screen.height - gui.devicelist.height ) / 2;
				
				break;
		}

		if(getColorForKey("devices_bgcolor", &color, theme))
			gui.devicelist.bgcolor = (color & 0x00FFFFFF);
			
		if(getIntForKey("devices_transparency", &alpha, theme))
			gui.devicelist.bgcolor = gui.devicelist.bgcolor | (( 255 - ( alpha & 0xFF) ) << 24);

		/*
		 * Parse infobox parameters
		 */
		
		if(getIntForKey("infobox_width", &val, theme))
			gui.infobox.width = MIN( screen_width , val );
		
		if(getIntForKey("infobox_height", &val, theme))
			gui.infobox.height = MIN( screen_height , val );

		if(getDimensionForKey("infobox_pos_x", &pixel, theme, screen_width , gui.infobox.width ) )
			gui.infobox.pos.x = pixel;
		
		if(getDimensionForKey("infobox_pos_y", &pixel, theme, screen_height , gui.infobox.height ) )
			gui.infobox.pos.y = pixel;
		
		if(getIntForKey("infobox_textmargin_h", &val, theme))
			gui.infobox.hborder = MIN( gui.infobox.width , val );
		
		if(getIntForKey("infobox_textmargin_v", &val, theme))
			gui.infobox.vborder = MIN( gui.infobox.height , val );

		if(getColorForKey("infobox_bgcolor", &color, theme))
			gui.infobox.bgcolor = (color & 0x00FFFFFF);
		
		if(getIntForKey("infobox_transparency", &alpha, theme))
			gui.infobox.bgcolor = gui.infobox.bgcolor | (( 255 - ( alpha & 0xFF) ) << 24);
	
		/*
		 * Parse menu parameters
		 */
			
		if(getDimensionForKey("menu_width", &pixel, theme, gui.screen.width , 0 ) )
			gui.menu.width = pixel;
		else
			gui.menu.width = images[iMenuSelection].image->width;
		
		if(getDimensionForKey("menu_height", &pixel, theme, gui.screen.height , 0 ) )
			gui.menu.height = pixel;
		else
			gui.menu.height = (infoMenuItemsCount) * images[iMenuSelection].image->height;
	
		
		if(getDimensionForKey("menu_pos_x", &pixel, theme, screen_width , gui.menu.width ) )
			gui.menu.pos.x = pixel;
		
		if(getDimensionForKey("menu_pos_y", &pixel, theme, screen_height , gui.menu.height ) )
			gui.menu.pos.y = pixel;

		if(getIntForKey("menu_textmargin_h", &val, theme))
			gui.menu.hborder = MIN( gui.menu.width , val );
		
		if(getIntForKey("menu_textmargin_v", &val, theme))
			gui.menu.vborder = MIN( gui.menu.height , val );

		if(getColorForKey("menu_bgcolor", &color, theme))
			gui.menu.bgcolor = (color & 0x00FFFFFF);
		
		if(getIntForKey("menu_transparency", &alpha, theme))
			gui.menu.bgcolor = gui.menu.bgcolor | (( 255 - ( alpha & 0xFF) ) << 24);		
		
		/*
		 * Parse bootprompt parameters
		 */

		if(getDimensionForKey("bootprompt_width", &pixel, theme, screen_width , 0 ) )
			gui.bootprompt.width = pixel;
		
		if(getIntForKey("bootprompt_height", &val, theme))
			gui.bootprompt.height = MIN( screen_height , val );

		if(getDimensionForKey("bootprompt_pos_x", &pixel, theme, screen_width , gui.bootprompt.width ) )
			gui.bootprompt.pos.x = pixel;
		
		if(getDimensionForKey("bootprompt_pos_y", &pixel, theme, screen_height , gui.bootprompt.height ) )
			gui.bootprompt.pos.y = pixel;
		
		if(getIntForKey("bootprompt_textmargin_h", &val, theme))
			gui.bootprompt.hborder = MIN( gui.bootprompt.width , val );
	
		if(getIntForKey("bootprompt_textmargin_v", &val, theme))
			gui.bootprompt.vborder = MIN( gui.bootprompt.height , val );

		if(getColorForKey("bootprompt_bgcolor", &color, theme))
			gui.bootprompt.bgcolor = (color & 0x00FFFFFF);
			
		if(getIntForKey("bootprompt_transparency", &alpha, theme))
			gui.bootprompt.bgcolor = gui.bootprompt.bgcolor | (( 255 - ( alpha & 0xFF) ) << 24);
		
		if(getColorForKey("font_small_color", &color, theme))
			gui.screen.font_small_color = (color & 0x00FFFFFF);
		
		if(getColorForKey("font_console_color", &color, theme))
			gui.screen.font_console_color = (color & 0x00FFFFFF);
		
	}
	
}
 
/*
void freePixmap(pixmap_t *pm)
{
	if(pm)
	{
		if(pm->pixels)
		{
			free(pm->pixels);
			pm->pixels=0;
		}
	}
	pm = 0;
}
*/

const char *theme_name="Default";	

int initGUI()
{
	int val, len;
	char dirspec[256];

#ifdef EMBED_THEME

	config_file_t *embedded = &bootInfo->themeDefault;

	// build xml dictionary for embedded theme.plist
	ParseXMLFile( (char *) __theme_plist, &embedded->dictionary);

	// parse display size parameters
	if(getIntForKey("screen_width", &val, &bootInfo->themeDefault))
		screen_params[0] = val;
	
	if(getIntForKey("screen_height", &val, &bootInfo->themeDefault))
		screen_params[1] = val;
#else

	/*
	 * Return Error since GUI can't run when embedtheme is not used.
	 */
	
	return 1;
	
#endif

  // Initalizing GUI strucutre.
  bzero(&gui, sizeof(gui_t));
	
	screen_params[2] = 32;
	
	// find theme name in boot.plist
	getValueForKey( "Theme", &theme_name, &len, &bootInfo->bootConfig );
	
	sprintf(dirspec, "/Extra/Themes/%s/theme.plist", theme_name);

	if ( !loadConfigFile(dirspec, &bootInfo->themeConfig) )
	{		
		// parse display size parameters
		if(getIntForKey("screen_width", &val, &bootInfo->themeConfig))
			screen_params[0] = val;

		if(getIntForKey("screen_height", &val, &bootInfo->themeConfig))
			screen_params[1] = val;
	}

	// find best matching vesa mode for our requested width & height
	getGraphicModeParams(screen_params);

	// set our screen structure with the mode width & height
	gui.screen.width = screen_params[0];	
	gui.screen.height = screen_params[1];
	
	// load graphics otherwise fail and return
	if( !loadGraphics() )
	{
		// set embedded theme.plist as defaults
		loadThemeValues(&bootInfo->themeDefault, YES);
		
		// set user overides
		loadThemeValues(&bootInfo->themeConfig, NO);
		
		colorFont(&font_small,   gui.screen.font_small_color);
		colorFont(&font_console, gui.screen.font_console_color);
		
		// create the screen & window buffers
		if( !createBackBuffer( &gui.screen ) )
			if( !createWindowBuffer( &gui.screen ) )
				if( !createWindowBuffer( &gui.devicelist ) )
					if( !createWindowBuffer( &gui.bootprompt ) )
						if( !createWindowBuffer( &gui.infobox ) )
							if( !createWindowBuffer( &gui.menu ) ) 
							{							
								drawBackground();

								// lets copy the screen into the back buffer
								memcpy( gui.backbuffer->pixels, gui.screen.pixmap->pixels, gui.backbuffer->width * gui.backbuffer->height * 4 );

								setVideoMode( GRAPHICS_MODE, 0 );
								
								gui.initialised = YES;
									
								return 0;
							}	
	}

	// something went wrong
	return 1;
}


void drawDeviceIcon(BVRef device, pixmap_t *buffer, position_t p)
{
	int devicetype;
	
	if( diskIsCDROM(device) )
		devicetype = iDeviceCDROM;				// Use CDROM icon
	else
	{	
		switch (device->part_type)
		{
			case kPartitionTypeHFS:

				// TODO: add apple raid icon choices
				
				devicetype = iDeviceHFS;		// Use HFS icon
				break;
				
			case kPartitionTypeHPFS:
				devicetype = iDeviceNTFS;		// Use HPFS / NTFS icon
				break;
				
			case kPartitionTypeFAT16:
				devicetype = iDeviceFAT16;		// Use FAT16 icon
				break;
				
			case kPartitionTypeFAT32:
				devicetype = iDeviceFAT32;		// Use FAT32 icon
				break;
				
			case kPartitionTypeEXT3:
				devicetype = iDeviceEXT3;		// Use EXT2/3 icon
				break;
				
			default:
				devicetype = iDeviceGeneric;	// Use Generic icon
				break;
		}
	}
	
	// draw icon
	blend( images[devicetype].image, buffer, centeredAt( images[devicetype].image, p ));
	
	p.y += (images[iSelection].image->height / 2) + font_console.chars[0]->height;
	
	// draw volume label 
	drawStrCenteredAt( device->label, &font_small, buffer, p);	

}

void drawDeviceList (int start, int end, int selection)
{
	int i;
	position_t p, p_prev, p_next;

	//uint8_t	maxDevices = MIN( gui.maxdevices, menucount );
		
	fillPixmapWithColor( gui.devicelist.pixmap, gui.devicelist.bgcolor);

	makeRoundedCorners( gui.devicelist.pixmap);

	switch (gui.layout)
	{

		case VerticalLayout:
			p.x = (gui.devicelist.width /2);
			p.y =  ( ( images[iSelection].image->height / 2 ) + images[iDeviceScrollPrev].image->height + gui.devicelist.iconspacing );

			// place scroll indicators at top & bottom edges
			p_prev = pos ( gui.devicelist.width / 2 , gui.devicelist.iconspacing );
			p_next = pos ( p_prev.x, gui.devicelist.height - gui.devicelist.iconspacing );
			
			break;
			
		default:	// use Horizontal layout as the default

		case HorizontalLayout:
			p.x = (gui.devicelist.width - ( gui.devicelist.width / gui.maxdevices ) * gui.maxdevices ) / 2 + ( images[iSelection].image->width / 2) + images[iDeviceScrollPrev].image->width + gui.devicelist.iconspacing;
			p.y = ((gui.devicelist.height - font_console.chars[0]->height ) - images[iSelection].image->height) / 2 + ( images[iSelection].image->height / 2 );

			// place scroll indicators at left & right edges
			p_prev = pos ( images[iDeviceScrollPrev].image->width / 2  + gui.devicelist.iconspacing / 2, gui.devicelist.height / 2 );
			p_next = pos ( gui.devicelist.width - ( images[iDeviceScrollNext].image->width / 2 + gui.devicelist.iconspacing / 2), gui.devicelist.height / 2 );
			
			break;
			
	}
	
	// draw visible device icons
	for ( i=0; i < gui.maxdevices; i++ )
	{
		BVRef param = menuItems[start+i].param;

		if((start+i) == selection)
		{
			 if(param->flags & kBVFlagNativeBoot)
				infoMenuNativeBoot = YES;
			 else
			 {
				infoMenuNativeBoot = NO;
				if(infoMenuSelection >= INFOMENU_NATIVEBOOT_START && infoMenuSelection <= INFOMENU_NATIVEBOOT_END)
				infoMenuSelection = 0;
			 }
			 
			if(gui.menu.draw)
				drawInfoMenuItems();
			 
			blend( images[iSelection].image, gui.devicelist.pixmap, centeredAt( images[iSelection].image, p ) );
			
#if DEBUG
			gui.debug.cursor = pos( 10, 100);
			dprintf( &gui.screen, "label     %s\n",   param->label );
			dprintf( &gui.screen, "biosdev   0x%x\n", param->biosdev );
			dprintf( &gui.screen, "type      0x%x\n", param->type );
			dprintf( &gui.screen, "flags     0x%x\n", param->flags );
			dprintf( &gui.screen, "part_no   %d\n",   param->part_no );
			dprintf( &gui.screen, "part_boff 0x%x\n", param->part_boff );
			dprintf( &gui.screen, "part_type 0x%x\n", param->part_type );
			dprintf( &gui.screen, "bps       0x%x\n", param->bps );
			dprintf( &gui.screen, "name      %s\n",   param->name );
			dprintf( &gui.screen, "type_name %s\n",   param->type_name );
			dprintf( &gui.screen, "modtime   %d\n",   param->modTime );
#endif
		}
		
		drawDeviceIcon( param, gui.devicelist.pixmap, p );
		
		if (gui.layout == HorizontalLayout)
		{
			p.x += images[iSelection].image->width + gui.devicelist.iconspacing; 
		}
		if (gui.layout == VerticalLayout)
		{
			p.y += ( images[iSelection].image->height + font_console.chars[0]->height + gui.devicelist.iconspacing ); 
		}
	}

	// draw prev indicator
	if(start)
		blend( images[iDeviceScrollPrev].image, gui.devicelist.pixmap, centeredAt( images[iDeviceScrollPrev].image, p_prev ) );

	// draw next indicator
	if( end < gDeviceCount - 1 )
		blend( images[iDeviceScrollNext].image, gui.devicelist.pixmap, centeredAt( images[iDeviceScrollNext].image, p_next ) );

	gui.redraw = YES;
	
	updateVRAM();
	
}
 
void clearGraphicBootPrompt()
{
	// clear text buffer
	prompt[0] = '\0';
	prompt_pos=0;

	
	if(	gui.bootprompt.draw == YES )
	{
		gui.bootprompt.draw = NO;
		gui.redraw = YES;
		// this causes extra frames to be drawn
		//updateVRAM();
	}

	return;
}

void updateGraphicBootPrompt(int key)
{
	if ( key == kBackspaceKey )
		prompt[--prompt_pos] = '\0';
	else 
	{
		prompt[prompt_pos] = key;
		prompt_pos++;
		prompt[prompt_pos] = '\0';
	}

	fillPixmapWithColor( gui.bootprompt.pixmap, gui.bootprompt.bgcolor);

	makeRoundedCorners( gui.bootprompt.pixmap);

	position_t p_text = pos( gui.bootprompt.hborder , ( ( gui.bootprompt.height -  font_console.chars[0]->height) ) / 2 );

	// print the boot prompt text
	drawStr(prompt_text, &font_console, gui.bootprompt.pixmap, p_text);
	
	// get the position of the end of the boot prompt text to display user input
	position_t p_prompt = pos( p_text.x + ( ( strlen(prompt_text) ) * font_console.chars[0]->width ), p_text.y );

	// calculate the position of the cursor
	int	offset = (  prompt_pos - ( ( gui.bootprompt.width / font_console.chars[0]->width ) - strlen(prompt_text) - 2 ) );	

	if ( offset < 0)
		offset = 0;
	
	drawStr( prompt+offset, &font_console, gui.bootprompt.pixmap, p_prompt);

	gui.menu.draw = NO;
	gui.bootprompt.draw = YES;
	gui.redraw = YES;
	
	updateVRAM();
	
	return;
}

inline
void vramwrite (void *data, int width, int height)
{
	if (VIDEO (depth) == 32 && VIDEO (rowBytes) == gui.backbuffer->width * 4)
		memcpy((uint8_t *)vram, gui.backbuffer->pixels, VIDEO (rowBytes)*VIDEO (height));
	else
	{
		uint32_t r, g, b;
		int i, j;
		for (i = 0; i < VIDEO (height); i++)
			for (j = 0; j < VIDEO (width); j++)
			{
				b = ((uint8_t *) data)[4*i*width + 4*j];
				g = ((uint8_t *) data)[4*i*width + 4*j + 1];
				r = ((uint8_t *) data)[4*i*width + 4*j + 2];
				switch (VIDEO (depth))
				{
					case 32:
						*(uint32_t *)(((uint8_t *)vram)+i*VIDEO (rowBytes) + j*4) = (b&0xff) | ((g&0xff)<<8) | ((r&0xff)<<16);
						break;
					case 24:
						*(uint32_t *)(((uint8_t *)vram)+i*VIDEO (rowBytes) + j*3) = ((*(uint32_t *)(((uint8_t *)vram)+i*VIDEO (rowBytes) + j*3))&0xff000000)
						| (b&0xff) | ((g&0xff)<<8) | ((r&0xff)<<16);
						break;
					case 16:
						// Somehow 16-bit is always 15-bits really
						//						*(uint16_t *)(((uint8_t *)vram)+i*VIDEO (rowBytes) + j*2) = ((b&0xf8)>>3) | ((g&0xfc)<<3) | ((r&0xf8)<<8);
						//						break;							
					case 15:
						*(uint16_t *)(((uint8_t *)vram)+i*VIDEO (rowBytes) + j*2) = ((b&0xf8)>>3) | ((g&0xf8)<<2) | ((r&0xf8)<<7);
						break;														
				}
			}
	}
}

void updateVRAM()
{
	if (gui.redraw)
	{
		if (gui.devicelist.draw)
			blend( gui.devicelist.pixmap, gui.backbuffer, gui.devicelist.pos );

		if (gui.bootprompt.draw)
			blend( gui.bootprompt.pixmap, gui.backbuffer, gui.bootprompt.pos );
				
		if (gui.menu.draw)
			blend( gui.menu.pixmap, gui.backbuffer, gui.menu.pos );
		
		if (gui.infobox.draw)
			blend( gui.infobox.pixmap, gui.backbuffer, gui.infobox.pos );
	}
	
	vramwrite ( gui.backbuffer->pixels, gui.backbuffer->width, gui.backbuffer->height );

	if (gui.redraw)
	{
		memcpy( gui.backbuffer->pixels, gui.screen.pixmap->pixels, gui.backbuffer->width * gui.backbuffer->height * 4 );
		gui.redraw = NO;
	}
}

struct putc_info {
    char * str;
    char * last_str;
};

static void
sputc(int c, struct putc_info * pi)
{
    if (pi->last_str)
        if (pi->str == pi->last_str) {
            *(pi->str) = '\0';
            return;
        }
    *(pi->str)++ = c;
}

int gprintf( window_t * window, const char * fmt, ...)
{
	char *formattedtext;

	va_list ap;
	
	struct putc_info pi;

	if( formattedtext = (char *) MALLOC(1024) )
	{
		// format the text
		va_start(ap, fmt);
		pi.str = formattedtext;
		pi.last_str = 0;
		prf(fmt, ap, sputc, &pi);
		*pi.str = '\0';
		va_end(ap);
		
		position_t	origin, cursor, bounds;

		int i;
		int character;

		origin.x = MAX( window->cursor.x, window->hborder );
		origin.y = MAX( window->cursor.y, window->vborder );
		
		bounds.x = ( window->width - window->hborder );
		bounds.y = ( window->height - window->vborder );
		
		cursor = origin;
			
		font_t *font = &font_console;
			
		for( i=0; i< strlen(formattedtext); i++ )
		{
			character = formattedtext[i];
			
			character -= 32;
				
			// newline ?
			if( formattedtext[i] == '\n' )
			{
				cursor.x = window->hborder;
				cursor.y += font->height;

				if ( cursor.y > bounds.y )
					cursor.y = origin.y;

				continue;
			}
				
			// tab ?
			if( formattedtext[i] == '\t' )
				cursor.x += ( font->chars[0]->width * 5 );
			
			// draw the character
			if( font->chars[character])
				blend(font->chars[character], window->pixmap, cursor);

			cursor.x += font->chars[character]->width;

			// check x pos and do newline
			if ( cursor.x > bounds.x )
			{
				cursor.x = origin.x;
				cursor.y += font->height;
			}
			
			// check y pos and reset to origin.y
			if ( cursor.y > bounds.y )
				cursor.y = origin.y;
		}

		// update cursor postition
		window->cursor = cursor;
		
		free(formattedtext);
		
		return 0;

	}
	return 1;
}

int dprintf( window_t * window, const char * fmt, ...)
{
	char *formattedtext;
	
	va_list ap;
	
	//window = &gui.debug;
	
	struct putc_info pi;
	
	if( formattedtext = (char *) MALLOC(1024) )
	{
		// format the text
		va_start(ap, fmt);
		pi.str = formattedtext;
		pi.last_str = 0;
		prf(fmt, ap, sputc, &pi);
		*pi.str = '\0';
		va_end(ap);
		
		position_t	origin, cursor, bounds;
		
		int i;
		int character;
		
		origin.x = MAX( gui.debug.cursor.x, window->hborder );
		origin.y = MAX( gui.debug.cursor.y, window->vborder );
		
		bounds.x = ( window->width - window->hborder );
		bounds.y = ( window->height - window->vborder );
		
		cursor = origin;
		
		font_t *font = &font_console;
		
		for( i=0; i< strlen(formattedtext); i++ )
		{
			character = formattedtext[i];
			
			character -= 32;
			
			// newline ?
			if( formattedtext[i] == '\n' )
			{
				cursor.x = window->hborder;
				cursor.y += font->height;
				
				if ( cursor.y > bounds.y )
					cursor.y = origin.y;
				
				continue;
			}
			
			// tab ?
			if( formattedtext[i] == '\t' )
				cursor.x += ( font->chars[0]->width * 5 );
			
			// draw the character
			if( font->chars[character])
				blend(font->chars[character], gui.backbuffer, cursor);

			cursor.x += font->chars[character]->width;
			
			// check x pos and do newline
			if ( cursor.x > bounds.x )
			{
				cursor.x = origin.x;
				cursor.y += font->height;
			}
			
			// check y pos and reset to origin.y
			if ( cursor.y > bounds.y )
				cursor.y = origin.y;
		}
		
		// update cursor postition
		gui.debug.cursor = cursor;
		
		free(formattedtext);
		
		return 0;
		
	}
	return 1;
}

int vprf(const char * fmt, va_list ap)
{
	int i;
	int character;

	char *formattedtext;
	window_t *window = &gui.screen;
	struct putc_info pi;
	
	position_t	origin, cursor, bounds;
	font_t *font = &font_console;
	
	if( formattedtext = (char *) MALLOC(1024) )
	{
		// format the text
		pi.str = formattedtext;
		pi.last_str = 0;
		prf(fmt, ap, sputc, &pi);
		*pi.str = '\0';
		
		origin.x = MAX( window->cursor.x, window->hborder );
		origin.y = MAX( window->cursor.y, window->vborder );
		bounds.x = ( window->width - ( window->hborder * 2 ) );
		bounds.y = ( window->height - ( window->vborder * 2 ) );
		cursor = origin;
			
		for( i=0; i< strlen(formattedtext); i++ )
		{
			character = formattedtext[i];
			character -= 32;
			
			// newline ?
			if( formattedtext[i] == '\n' )
			{
				cursor.x = window->hborder;
				cursor.y += font->height;
				if ( cursor.y > bounds.y )
				{
					gui.redraw = YES;
					updateVRAM();
					cursor.y = window->vborder;
				}
				window->cursor.y = cursor.y;
				continue;
			}

			// tab ?
			if( formattedtext[i] == '\t' )
			{
				cursor.x = ( cursor.x / ( font->chars[0]->width * 8 ) + 1 ) * ( font->chars[0]->width * 8 );
				continue;
			}
			cursor.x += font->chars[character]->width;
			
			// check x pos and do newline
			if ( cursor.x > bounds.x )
			{
				cursor.x = origin.x;
				cursor.y += font->height;
			}
				
			// check y pos and reset to origin.y
			if ( cursor.y > ( bounds.y + font->chars[0]->height) )
			{
				gui.redraw = YES;
				updateVRAM();
				cursor.y = window->vborder;
			}
			// draw the character
			if( font->chars[character])
				blend(font->chars[character], gui.backbuffer, cursor);
		}
		// save cursor postition
		window->cursor.x = cursor.x;
		updateVRAM();
		free(formattedtext);
		return 0;
	}
	return 1;
}

void drawStr(char *ch, font_t *font, pixmap_t *blendInto, position_t p)
{
	int i=0;
	int y=0; // we need this to support multilines '\n'
	int x=0;
	
	for(i=0;i<strlen(ch);i++)
	{
		int cha=(int)ch[i];
		
		cha-=32;
		
		// newline ?
		if( ch[i] == '\n' )
		{
			x = 0;
			y += font->height;
			continue;
		}
		
		// tab ?
		if( ch[i] == '\t' )
			x+=(font->chars[0]->width*5);
		
		if(font->chars[cha])
			blend(font->chars[cha], blendInto, pos(p.x+x, p.y+y));
		
		x += font->chars[cha]->width;
	}
}

void drawStrCenteredAt(char *text, font_t *font, pixmap_t *blendInto, position_t p)
{
	int i = 0;
	int width = 0;

	// calculate the width in pixels
	for(i=0;i<strlen(text);i++)
		width += font->chars[text[i]-32]->width;

	p.x = ( p.x - ( width / 2 ) );
	p.y = ( p.y - ( font->height / 2 ) ); 
	
	if ( p.x == -6 )
	{
		p.x = 0;
	}
	
	for(i=0;i<strlen(text);i++)
	{
		int cha=(int)text[i];
		
		cha-=32;
 
		if(font->chars[cha])
		{
			blend(font->chars[cha], blendInto, p);
			p.x += font->chars[cha]->width;
		}
	}
	
}

int initFont(font_t *font, image_t *data)
{
	unsigned int x = 0, y = 0, x2 = 0, x3 = 0;
	
	int start = 0, end = 0, count = 0, space = 0;
	
	BOOL monospaced = NO;
	
	font->height = data->image->height;	

	for( x = 0; x < data->image->width; x++)
	{
		start = end;
				
		// if the pixel is red we've reached the end of the char
		if( pixel( data->image, x, 0 ).value == 0xFFFF0000)
		{
			end = x + 1;

			if( (font->chars[count] = MALLOC(sizeof(pixmap_t)) ) )
			{
				font->chars[count]->width = ( end - start) - 1;
				font->chars[count]->height = font->height;
			
				if ( ( font->chars[count]->pixels = MALLOC( font->chars[count]->width * data->image->height * 4) ) )
				{
					space += ( font->chars[count]->width * data->image->height * 4 );
					// we skip the first line because there are just the red pixels for the char width
					for( y = 1; y< (font->height); y++)
					{
						for( x2 = start, x3 = 0; x2 < end; x2++, x3++)
						{
							pixel( font->chars[count], x3, y ) = pixel( data->image, x2, y );
						}	
					}
					
					// check if font is monospaced
					if( ( count > 0 ) && ( font->width != font->chars[count]->width ) )
						monospaced = YES;
						
					font->width = font->chars[count]->width;
					
					count++;
				}
			}
		}
	}

	if(monospaced)
		font->width = 0;

	return 0;
}

void colorFont(font_t *font, uint32_t color)
{
	if( !color )
		return;
	
	int x, y, width, height;
	int count = 0;
	pixel_t *buff;
	
	while( font->chars[count++] )
	{
		width = font->chars[count-1]->width;
		height = font->chars[count-1]->height;
		for( y = 0; y < height; y++ )
		{
			for( x = 0; x < width; x++ )
			{
				buff = &(pixel( font->chars[count-1], x, y ));
				if( buff->ch.a )
				{
					buff->ch.r = (color & 0xFFFF0000) >> 16;
					buff->ch.g = (color & 0xFF00FF00) >> 8;
					buff->ch.b = (color & 0xFF0000FF);
				}
			}
		}
	}
}

int loadThemeImage(image)
{
	char dirspec[256];
	sprintf(dirspec,"/Extra/Themes/%s/%s", theme_name, image);
	if ((loadPixmapFromPng(dirspec, images[imageCnt].image)) == 1 )
		return 1;
	dirspec[0] = '\0';
	sprintf(dirspec,"bt(0,0)/Extra/Themes/%s/%s", theme_name, image);
	return 	loadPixmapFromPng(dirspec, images[imageCnt].image);
}

int loadGraphics()
{
	int i;

	for( i=0; i < sizeof(images) / sizeof(images[0]); i++)
	{
		// create pixmap_t buffer for each image
		images[i].image = MALLOC(sizeof(pixmap_t));
	}

	LOADPNG(background);
	LOADPNG(logo);

	LOADPNG(device_generic);
	LOADPNG(device_hfsplus);
	LOADPNG(device_ext3);
	LOADPNG(device_fat16);
	LOADPNG(device_fat32);
	LOADPNG(device_ntfs);
	LOADPNG(device_cdrom);
	LOADPNG(device_selection);
	LOADPNG(device_scroll_prev);
	LOADPNG(device_scroll_next);

	LOADPNG(menu_boot);
	LOADPNG(menu_verbose);
	LOADPNG(menu_ignore_caches);
	LOADPNG(menu_single_user);
	LOADPNG(menu_memory_info);
	LOADPNG(menu_video_info);
	LOADPNG(menu_help);
	LOADPNG(menu_verbose_disabled);
	LOADPNG(menu_ignore_caches_disabled);
	LOADPNG(menu_single_user_disabled);
	LOADPNG(menu_selection);

	LOADPNG(progress_bar);
	LOADPNG(progress_bar_background);

	LOADPNG(text_scroll_prev);
	LOADPNG(text_scroll_next);

	LOADPNG(font_console);
	LOADPNG(font_small);
	
	initFont( &font_console, &images[iFontConsole]);
	initFont( &font_small, &images[iFontSmall]);
	
	return 0;

}
 
void makeRoundedCorners(pixmap_t *p)
{
	int x,y;
	int width=p->width-1;
	int height=p->height-1;
	
	// 10px rounded corner alpha values
	uint8_t roundedCorner[10][10] =
	{
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0xC0, 0xFF},
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0x00, 0x00, 0x00, 0x40, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0x00, 0x00, 0x40, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0x00, 0x40, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0x00, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0x40, 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
		{ 0xEF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
	};
	
	uint8_t alpha=0;
	
	for( y=0; y<10; y++)
	{
		for( x=0; x<10; x++)
		{
			// skip if the pixel should be visible
			if(roundedCorner[y][x] != 0xFF) 
			{ 
				alpha = ( roundedCorner[y][x] ? (uint8_t) (roundedCorner[y][x] * pixel(p, x, y).ch.a) / 255 : 0 );
				// Upper left corner
				pixel(p, x, y).ch.a = alpha;

				// upper right corner
				pixel(p, width-x,y).ch.a = alpha;

				// lower left corner
				pixel(p, x, height-y).ch.a = alpha;

				// lower right corner
				pixel(p, width-x, height-y).ch.a = alpha;
			}
		}
	}
}

void showInfoBox(char *title, char *text)
{
	int i, key, lines, visiblelines;

	int currentline=0;
	int cnt=0;
	int offset=0;
	
	if( !title || !text )
		return;
	
	position_t pos_title = pos ( gui.infobox.vborder, gui.infobox.vborder );

	// calculate number of lines in the title
	for ( i = 0, lines = 1; i<strlen(title); i++ )
		if( title[i] == '\n')
			lines++;
	
	// y position of text is lines in title * height of font
	position_t pos_text =  pos( pos_title.x , pos_title.y + ( font_console.height * lines ));
	
	// calculate number of lines in the text
	for ( i=0, lines = 1; i<strlen(text); i++ )
		if( text[i] == '\n')
			lines++;
	
	// if text ends with \n strip off
	if( text[i] == '\n' || text[i] == '\0')
		lines--;
	
	visiblelines = ( ( gui.infobox.height - ( gui.infobox.vborder * 2 ) ) / font_console.height ) - 1;
	
	// lets display the text and allow scroll thru using up down / arrows
	while(1)
	{
		// move to current line in text
		for( offset = 0, i = 0; offset < strlen(text); offset++ )
		{
			if( currentline == i)
				break;
			if( text[offset] =='\n')
				i++;
		}

		// find last visible line in text and place \0
		for( i = offset, cnt = 0; i < strlen(text); i++)
		{
			if(text[i]=='\n')
				cnt++;
			if ( cnt == visiblelines )
			{
				text[i]='\0';
				break;
			}
		}
				
		fillPixmapWithColor( gui.infobox.pixmap, gui.infobox.bgcolor);

		makeRoundedCorners( gui.infobox.pixmap);
			
		// print the title if present
		if( title )
			drawStr(title, &font_console, gui.infobox.pixmap, pos_title);

		// print the text
		drawStr( text + offset, &font_console, gui.infobox.pixmap, pos_text);

		// restore \n in text
		if ( cnt == visiblelines )
			text[i] = '\n';
		
		position_t pos_indicator =  pos( gui.infobox.width - ( images[iTextScrollPrev].image->width - ( gui.infobox.vborder / 2) ), pos_text.y );
		
		// draw prev indicator
		if(offset)
		{
			blend( images[iTextScrollPrev].image, gui.infobox.pixmap, centeredAt( images[iTextScrollPrev].image, pos_indicator ));
		}
		
		// draw next indicator
		if( lines > ( currentline + visiblelines ) )
		{
			pos_indicator.y = ( gui.infobox.height - ( ( images[iTextScrollNext].image->width + gui.infobox.vborder ) / 2 ) );
			blend( images[iTextScrollNext].image, gui.infobox.pixmap, centeredAt( images[iTextScrollNext].image, pos_indicator ) );
		}

		gui.bootprompt.draw = NO;
		gui.infobox.draw = YES;
		gui.redraw = YES;
		
		updateVRAM();
		
		key = getc();
			
		if( key == kUpArrowkey )
			if( currentline > 0 )
				currentline--;

		if( key == kDownArrowkey )
			if( lines > ( currentline + visiblelines ) )
				currentline++;

		if( key == kEscapeKey || key == 'q' || key == 'Q')
		{
			gui.infobox.draw = NO;
			gui.redraw = YES;
			updateVRAM();
			break;
		}
	}
}

void animateProgressBar()
{
	int y;
	
	if( time18() > lasttime)
	{
		lasttime = time18();

		pixmap_t *buffBar = images[iProgressBar].image;

		uint32_t buff = buffBar->pixels[0].value;
	
		memcpy( buffBar->pixels, buffBar->pixels + 1, ( (buffBar->width*buffBar->height) - 1 ) * 4 );

		for( y = buffBar->height - 1; y > 0; y--)
			pixel(buffBar, buffBar->width - 1, y) = pixel(buffBar, buffBar->width - 1, y - 1);

		pixel(buffBar, buffBar->width-1, 0).value = buff;
	}
}

void drawProgressBar(pixmap_t *blendInto, uint16_t width, position_t p, uint8_t progress)
{
	if(progress>100)
		return;
	
	p.x = ( p.x - ( width / 2 ) );

	int todraw = (width * progress) / 100;

	pixmap_t *buff = images[iProgressBar].image;
	pixmap_t *buffBG = images[iProgressBarBackground].image;
	if(!buff || !buffBG)
		return;
	
	pixmap_t progressbar;
	progressbar.pixels=MALLOC(width * 4 * buff->height);
	if(!progressbar.pixels)
		return; 
	
	progressbar.width = width;
	progressbar.height = buff->height;

	int x=0,x2=0,y=0;
	
	for(y=0; y<buff->height; y++)
	{
		for(x=0; x<todraw; x++, x2++)
		{
			if(x2 == (buff->width-1)) x2=0;
			pixel(&progressbar, x,y).value = pixel(buff, x2,y).value;
		}
		x2=0;
	}

	for(y=0; y<buff->height; y++)
	{
		for(x=todraw, x2 = 0; x < width - 1; x++, x2++)
		{
			if(x2 == (buffBG->width -2 )) x2 = 0;
				pixel(&progressbar, x,y).value = pixel(buffBG, x2,y).value;
		}
		if(progress < 100)
			pixel(&progressbar, width - 1, y).value = pixel(buffBG, buffBG->width - 1, y).value;
		if(progress == 0)
			pixel(&progressbar, 0, y).value = pixel(buffBG, buffBG->width - 1, y).value;
		x2=0;
	}
	 
	blend(&progressbar, blendInto, p);
	animateProgressBar();
	free(progressbar.pixels);
}

void drawInfoMenuItems()
{
	int i,n;
	
	position_t position;
	
	pixmap_t *selection = images[iMenuSelection].image;
	
	pixmap_t *pbuff;

	fillPixmapWithColor(gui.menu.pixmap, gui.menu.bgcolor);

	makeRoundedCorners(gui.menu.pixmap);
	
	uint8_t offset = infoMenuNativeBoot ? 0 : infoMenuItemsCount - 1;

	position = pos(0,0);
	
	for ( i = 0, n = iMenuBoot; i < infoMenuItemsCount; i++, n++)
	{
		if (i == infoMenuSelection)
			blend(selection, gui.menu.pixmap, position);

		pbuff = images[n].image;
		if (offset && i >= INFOMENU_NATIVEBOOT_START && i <= INFOMENU_NATIVEBOOT_END)
			blend( images[n + (iMenuHelp - iMenuBoot)].image , gui.menu.pixmap, 
				pos((position.x + (gui.menu.hborder / 2)), position.y + ((selection->height - pbuff->height) / 2)));
		else
			blend( pbuff, gui.menu.pixmap, 
				pos((position.x + (gui.menu.hborder / 2)), position.y + ((selection->height - pbuff->height) / 2)));

		drawStr(infoMenuItems[i].text, &font_console, gui.menu.pixmap, 
			pos(position.x + (pbuff->width + gui.menu.hborder), 
				position.y + ((selection->height - font_console.height) / 2)));
		position.y += images[iMenuSelection].image->height;
	
	}
	
	gui.redraw = YES;
}

int drawInfoMenu()
{
	drawInfoMenuItems();

	gui.menu.draw = YES;

	updateVRAM();
	
	return 1;
}

int updateInfoMenu(int key)
{
	switch (key)
	{

		case kUpArrowkey:	// up arrow
				if (infoMenuSelection > 0)
				{
					if(!infoMenuNativeBoot && infoMenuSelection == INFOMENU_NATIVEBOOT_END + 1)
						infoMenuSelection -= 4;
					
					else
						infoMenuSelection--;
						drawInfoMenuItems();
						updateVRAM();
					
				} else {
					
					gui.menu.draw = NO;
					gui.redraw = YES;

					updateVRAM();
					
					return CLOSE_INFO_MENU;
				}
				break;

		case kDownArrowkey:	// down arrow
				if (infoMenuSelection < infoMenuItemsCount - 1)
				{
					if(!infoMenuNativeBoot && infoMenuSelection == INFOMENU_NATIVEBOOT_START - 1)
						infoMenuSelection += 4;
					else
						infoMenuSelection++;
					drawInfoMenuItems();
					updateVRAM();
				}
				break;

		case kReturnKey:
				key = 0;
				if( infoMenuSelection == MENU_SHOW_MEMORY_INFO )
					showInfoBox( "Memory Info. Press q to quit.\n", getMemoryInfoString());

				else if( infoMenuSelection == MENU_SHOW_VIDEO_INFO )
					showInfoBox( getVBEInfoString(), getVBEModeInfoString() );
			
				else if( infoMenuSelection == MENU_SHOW_HELP )
					showHelp();
					
				else
				{
					int buff = infoMenuSelection;
					infoMenuSelection = 0;
					return buff;
				}
				break;
		}
	return DO_NOT_BOOT;
}

uint16_t bootImageWidth = 0; 
uint16_t bootImageHeight = 0; 

uint8_t *bootImageData = NULL; 

BOOL usePngImage = YES;

//==========================================================================
// loadBootGraphics

void loadBootGraphics()
{
	char dirspec[256];

	sprintf(dirspec,"/Extra/Themes/%s/boot.png", theme_name);
	if (loadPngImage(dirspec, &bootImageWidth, &bootImageHeight, &bootImageData) != 0)
	{
		dirspec[0] = '\0';
		sprintf(dirspec,"bt(0,0)/Extra/Themes/%s/boot.png", theme_name);
		if (loadPngImage(dirspec, &bootImageWidth, &bootImageHeight, &bootImageData) != 0)
#ifdef EMBED_THEME
			if((loadEmbeddedPngImage(__boot_png, __boot_png_len, &bootImageWidth, 
				&bootImageHeight, &bootImageData)) != 0)
#endif
				usePngImage = NO; 
	}
}

//==========================================================================
// drawBootGraphics

void drawBootGraphics()
{
	int pos;
	int length;
	const char *dummyVal;
	BOOL legacy_logo;
	uint16_t x, y; 
	
	if (getBoolForKey("Legacy Logo", &legacy_logo, &bootInfo->bootConfig) && legacy_logo)
		usePngImage = NO; 
	
	else if (bootImageData == NULL)
		loadBootGraphics();
		
	// parse screen size parameters
	if(getIntForKey("boot_width", &pos, &bootInfo->themeConfig))
		screen_params[0] = pos;
	else
		screen_params[0] = DEFAULT_SCREEN_WIDTH;
	
	if(getIntForKey("boot_height", &pos, &bootInfo->themeConfig))
		screen_params[1] = pos;
	else
		screen_params[1] = DEFAULT_SCREEN_HEIGHT;
	
	screen_params[2] = 32;
	
	gui.screen.width = screen_params[0];
	gui.screen.height = screen_params[1];
	
	// find best matching vesa mode for our requested width & height
	getGraphicModeParams(screen_params);

	setVideoMode( GRAPHICS_MODE, 0 );
	
	if (getValueForKey("-checkers", &dummyVal, &length, &bootInfo->bootConfig)) 
		drawCheckerBoard(); 
	else
		// Fill the background to 75% grey (same as BootX). 
		drawColorRectangle(0, 0, screen_params[0], screen_params[1], 0x01 ); 
	
	if ( (bootImageData) && (usePngImage) )
	{ 
		x = ( screen_params[0] - MIN( bootImageWidth, screen_params[0] ) ) / 2; 
		y = ( screen_params[1] - MIN( bootImageHeight, screen_params[1] ) ) / 2; 
		
		// Draw the image in the center of the display. 
		blendImage(x, y, bootImageWidth, bootImageHeight, bootImageData); 
		
	} else { 
		
		uint8_t *appleBootPict; 
		bootImageData = NULL; 
		bootImageWidth = kAppleBootWidth; 
		bootImageHeight = kAppleBootHeight; 
		
		// Prepare the data for the default Apple boot image. 
		appleBootPict = (uint8_t *) decodeRLE(gAppleBootPictRLE, kAppleBootRLEBlocks, 
											  bootImageWidth * bootImageHeight); 
		if (appleBootPict)
		{ 
			convertImage(bootImageWidth, bootImageHeight, appleBootPict, &bootImageData); 
			if (bootImageData)
			{	
				x = ( screen_params[0] - MIN( kAppleBootWidth, screen_params[0] ) ) / 2; 
				y = ( screen_params[1] - MIN( kAppleBootHeight, screen_params[1] ) ) / 2; 
				
				drawDataRectangle( x, y, kAppleBootWidth, kAppleBootHeight, bootImageData );
				free(bootImageData);
			}
			free(appleBootPict); 
		} 
	} 

	return;
}

BOOL testForQemu()
{
	char *buffer = getVBEInfoString();
	char qemutext[] = "Bochs/Plex86";
	int i=0;
	for(;i<strlen(buffer)-strlen(qemutext);i++)
		if(!strncmp(buffer+i, qemutext, strlen(qemutext)))
			return YES;
	
	return NO;
}

