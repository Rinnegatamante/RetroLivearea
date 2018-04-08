#include <png.h>
#include <vitasdk.h>
#include <stdio.h>
#include "sqlite3.h" 
#include <vita2d.h> 
#include <stdlib.h>
#include <string.h>
#include <libimagequant.h>

enum {
	MAIN_MENU,
	ICON_SCAN_START,
	ASSETS_SCAN_START,
	PATCHED_SCAN_START,
	RECURSIVE_ICONS,
	RECURSIVE_ASSETS,
	RECURSIVE_UNPATCH,
	RESTART_REQUIRED,
	EXIT
};

extern char* sqlite3_temp_directory;

// Debugging log
#ifndef RELEASE
void LOG(char* format, ...){
	char str[512] = { 0 };
	va_list va;

	va_start(va, format);
	vsnprintf(str, 512, format, va);
	va_end(va);
	
	FILE* dfd = fopen("ux0:/data/RetroLivearea.log", "a");
	fwrite(str, strlen(str), 1, dfd);
	fwrite("\n", 1, 1, dfd);
	fclose(dfd);
}
#endif

char text[256];
char bubbles[256][256];
int bubbles_idx = 0;
int icon_idx = 0;

static int scan_callback(void *data, int argc, char **argv, char **azColName){
	sprintf(bubbles[bubbles_idx], argv[0]);
	bubbles_idx++;
	return 0;
}

static int dummy_callback(void *data, int argc, char **argv, char **azColName){
	return 0;
}

vita2d_pgf* debug_font;
void drawText(uint32_t y, char* text, uint32_t color){
	int i;
	for (i=0;i<3;i++){
		vita2d_start_drawing();
		vita2d_pgf_draw_text(debug_font, 2, y, color, 1.0, text	);
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
	}
}

void drawLoopText(uint32_t y, char* text, uint32_t color){
	vita2d_pgf_draw_text(debug_font, 2, y, color, 1.0, text	);
}

void clearScreen(){
	int i;
	for (i=0;i<3;i++){
		vita2d_start_drawing();
		vita2d_clear_screen();
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
	}
}

void drawIcon(uint32_t x, uint32_t y, vita2d_texture *image){
	int i;
	uint32_t w = vita2d_texture_get_width(image);
	uint32_t h = vita2d_texture_get_height(image);
	float wscale = 128.0f / ((float)w);
	float hscale = 128.0f / ((float)h);
	for (i=0;i<3;i++){
		vita2d_start_drawing();
		vita2d_draw_texture_scale(image, x, y, wscale, hscale);
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
	}
}

void drawBackground(uint32_t x, uint32_t y, vita2d_texture *image){
	int i;
	uint32_t w = vita2d_texture_get_width(image);
	uint32_t h = vita2d_texture_get_height(image);
	float wscale = 840.0f / ((float)w);
	float hscale = 500.0f / ((float)h);
	for (i=0;i<3;i++){
		vita2d_start_drawing();
		vita2d_draw_texture_scale(image, x, y, wscale, hscale);
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
	}
}

void drawLittleBackground(uint32_t x, uint32_t y, vita2d_texture *image){
	int i;
	uint32_t w = vita2d_texture_get_width(image);
	uint32_t h = vita2d_texture_get_height(image);
	float wscale = 280.0f / ((float)w);
	float hscale = 158.0f / ((float)h);
	for (i=0;i<3;i++){
		vita2d_start_drawing();
		vita2d_draw_texture_scale(image, x, y, wscale, hscale);
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
	}
}

void drawCenteredIcon(uint32_t x, uint32_t y, vita2d_texture *image){
	int i;
	uint32_t w = vita2d_texture_get_width(image);
	uint32_t h = vita2d_texture_get_height(image);
	uint32_t c_x = (280 - w) / 2;
	uint32_t c_y = (158 - h) / 2;
	for (i=0;i<3;i++){
		vita2d_start_drawing();
		vita2d_draw_texture(image, x + c_x, y + c_y);
		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_swap_buffers();
	}
}

vita2d_texture *extractIcon(char *filename){
	FILE *fh = fopen(filename, "rb");
	fseek(fh, 0x0C, SEEK_SET);
	uint32_t icon_offs, icon_size;
	fread(&icon_offs, 1, 4, fh);
	fread(&icon_size, 1, 4, fh);
	icon_size -= icon_offs;
	fseek(fh, icon_offs, SEEK_SET);
	char *icon0 = (char*)malloc(icon_size);
	fread(icon0, 1, icon_size, fh);
	fclose(fh);
	vita2d_texture *res = vita2d_load_PNG_buffer(icon0);
	free(icon0);
	return res;
}

vita2d_texture *extractBackground(char *filename){
	FILE *fh = fopen(filename, "rb");
	fseek(fh, 0x18, SEEK_SET);
	uint32_t pic_offs, pic_size;
	fread(&pic_offs, 1, 4, fh);
	fread(&pic_size, 1, 4, fh);
	pic_size -= pic_offs;
	fseek(fh, pic_offs, SEEK_SET);
	char *pic1 = (char*)malloc(pic_size);
	fread(pic1, 1, pic_size, fh);
	fclose(fh);
	vita2d_texture *res = vita2d_load_PNG_buffer(pic1);
	free(pic1);
	return res;
}

void forkTemplate(char *filename){
	FILE *f = fopen("app0:sce_sys/livearea/contents/template.xml", "rb");
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t* content = (uint8_t*)malloc(size);
	fread(content, 1, size, f);
	fclose(f);
	f = fopen(filename, "wb");
	fwrite(content, 1, size, f);
	fclose(f);
	free(content);
}

int main(){
	
	// Initialization
	char *zErrMsg = 0;
	int exit_code = 0;
	vita2d_init();
	vita2d_set_clear_color(RGBA8(0x00, 0x00, 0x00, 0xFF));
	debug_font = vita2d_load_default_pgf();
	uint32_t white = RGBA8(0xFF, 0xFF, 0xFF, 0xFF);
	uint32_t green = RGBA8(0x00, 0xFF, 0x00, 0xFF);
	uint32_t red = RGBA8(0xFF, 0x00, 0x00, 0xFF);
	int state = MAIN_MENU;
	SceCtrlData pad;
	sqlite3 *db;
	int fd;
	sceIoMkdir("ux0:data/RetroLivearea", 0777);
	
	// Main loop
	for (;;){
		switch (state){
			case MAIN_MENU:
				vita2d_start_drawing();
				vita2d_clear_screen();
				drawLoopText(20,"RetroLivearea v.1.0 by Rinnegatamante",white);
				drawLoopText(60,"Press Cross to scan for unpatched PSX/PSP icon bubbles.",white);
				drawLoopText(80,"Press Triangle to scan for unpatched PSX/PSP assets bubbles.",white);
				drawLoopText(100,"Press Square to scan for patched PSX/PSP bubbles.",white);
				drawLoopText(120,"Press Start to exit.",white);
				drawLoopText(400,"Thanks to my Patroners for their awesome support:",white);
				drawLoopText(420,"Billy McLaughlin II",white);
				drawLoopText(440,"Styde Pregny",white);
				drawLoopText(460,"XandridFire",white);
				sceCtrlPeekBufferPositive(0, &pad, 1);
				vita2d_end_drawing();
				vita2d_wait_rendering_done();
				vita2d_swap_buffers();
				if (pad.buttons & SCE_CTRL_CROSS) state = ICON_SCAN_START;
				else if (pad.buttons & SCE_CTRL_TRIANGLE) state = ASSETS_SCAN_START;
				else if (pad.buttons & SCE_CTRL_SQUARE) state = PATCHED_SCAN_START;
				else if (pad.buttons & SCE_CTRL_START) state = EXIT;
				break;
			case ASSETS_SCAN_START:				
				clearScreen();
				bubbles_idx = 0;
				
				// Opening app.db for v2 patch
				drawText(20,"*** Scanning ***",white);
				drawText(40,"Opening app database",white);
				fd = sqlite3_open("ur0:/shell/db/app.db", &db);
				if(fd != SQLITE_OK){
					char error[512];
					sprintf(error, "ERROR: Can't open app database: %s", sqlite3_errmsg(db));
					drawText(60,error,red);
					drawText(80,"Operation aborted...",white);
					sceKernelDelayThread(2000000);
				}else{
		
					// Scanning database
					char query[1024];
					sprintf(query,"%s","SELECT titleId FROM tbl_livearea WHERE (style == 'ps1emu' or style == 'pspemu' AND titleId NOT LIKE 'PSPEMU%')");	
					drawText(60,"Searching for unpatched bubbles",white);
					fd = sqlite3_exec(db, query, scan_callback, NULL, &zErrMsg);
					if( fd != SQLITE_OK ){
						char error[512];
						sprintf(error, "ERROR: SQL error: %s", zErrMsg);
						sqlite3_free(zErrMsg);
						drawText(400,error,red);
					}
					
					// Closing app.db
					drawText(80,"Closing app database",white);
					sqlite3_close(db);
					
					if (bubbles_idx > 0){
						sprintf(text, "Found %d unpatched bubbles!", bubbles_idx);
						drawText(100,text,green);
						drawText(120,"Press Cross to start the patch process",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!(pad.buttons & SCE_CTRL_CROSS));
						sceKernelDelayThread(500000);
						icon_idx = 0;
						state = RECURSIVE_ASSETS;
					}else{
						drawText(100,"No unpatched bubbles found...",red);
						drawText(120,"Press Triangle to return to the main menu",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!(pad.buttons & SCE_CTRL_TRIANGLE));
						sceKernelDelayThread(500000);
						state = MAIN_MENU;
					}
					
				}
				break;
			case PATCHED_SCAN_START:				
				clearScreen();
				bubbles_idx = 0;
				
				// Opening app.db for v2 patch
				drawText(20,"*** Scanning ***",white);
				drawText(40,"Opening app database",white);
				fd = sqlite3_open("ur0:/shell/db/app.db", &db);
				if(fd != SQLITE_OK){
					char error[512];
					sprintf(error, "ERROR: Can't open app database: %s", sqlite3_errmsg(db));
					drawText(60,error,red);
					drawText(80,"Operation aborted...",white);
					sceKernelDelayThread(2000000);
				}else{
		
					// Scanning database
					char query[1024];
					sprintf(query,"%s","SELECT titleId FROM tbl_livearea WHERE (org_Path LIKE 'ux0:data/RetroLivearea/%')");	
					drawText(60,"Searching for patched bubbles",white);
					fd = sqlite3_exec(db, query, scan_callback, NULL, &zErrMsg);
					if( fd != SQLITE_OK ){
						char error[512];
						sprintf(error, "ERROR: SQL error: %s", zErrMsg);
						sqlite3_free(zErrMsg);
						drawText(400,error,red);
					}
					
					// Closing app.db
					drawText(80,"Closing app database",white);
					sqlite3_close(db);
					
					if (bubbles_idx > 0){
						sprintf(text, "Found %d patched bubbles!", bubbles_idx);
						drawText(100,text,green);
						drawText(120,"Press Cross to start the unpatch process",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!(pad.buttons & SCE_CTRL_CROSS));
						sceKernelDelayThread(500000);
						icon_idx = 0;
						state = RECURSIVE_UNPATCH;
					}else{
						drawText(100,"No patched bubbles found...",red);
						drawText(120,"Press Triangle to return to the main menu",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!(pad.buttons & SCE_CTRL_TRIANGLE));
						sceKernelDelayThread(500000);
						state = MAIN_MENU;
					}
					
				}
				break;
			case ICON_SCAN_START:
				clearScreen();
				bubbles_idx = 0;
				
				// Opening app.db
				drawText(20,"*** Scanning ***",white);
				drawText(40,"Opening app database",white);
				fd = sqlite3_open("ur0:/shell/db/app.db", &db);
				if(fd != SQLITE_OK){
					char error[512];
					sprintf(error, "ERROR: Can't open app database: %s", sqlite3_errmsg(db));
					drawText(60,error,red);
					drawText(80,"Operation aborted...",white);
					sceKernelDelayThread(2000000);
				}else{
		
					// Scanning database
					char query[1024];
					sprintf(query,"%s","SELECT titleId FROM tbl_appinfo_icon WHERE (icon0Type == 1 or icon0Type == 0)");	
					drawText(60,"Searching for unpatched bubbles",white);
					fd = sqlite3_exec(db, query, scan_callback, NULL, &zErrMsg);
					if( fd != SQLITE_OK ){
						char error[512];
						sprintf(error, "ERROR: SQL error: %s", zErrMsg);
						sqlite3_free(zErrMsg);
						drawText(400,error,red);
					}
					
					// Closing app.db
					drawText(80,"Closing app database",white);
					sqlite3_close(db);
					
					if (bubbles_idx > 0){
						sprintf(text, "Found %d unpatched bubbles!", bubbles_idx);
						drawText(100,text,green);
						drawText(120,"Press Cross to start the patch process",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!(pad.buttons & SCE_CTRL_CROSS));
						sceKernelDelayThread(500000);
						icon_idx = 0;
						state = RECURSIVE_ICONS;
					}else{
						drawText(100,"No unpatched bubbles found...",red);
						drawText(120,"Press Triangle to return to the main menu",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!(pad.buttons & SCE_CTRL_TRIANGLE));
						sceKernelDelayThread(500000);
						state = MAIN_MENU;
					}
					
				}
				break;
			case RECURSIVE_ICONS:
				clearScreen();
				
				if (icon_idx == bubbles_idx){
					state = RESTART_REQUIRED;
				}else{
					
					sprintf(text, "*** Patching icons (%d/%d) ***", icon_idx+1, bubbles_idx);
					drawText(20,text,white);
					
					// Extracting icon0.png from eboot.pbp
					sprintf(text, "ux0:pspemu/PSP/GAME/%s/EBOOT.PBP", bubbles[icon_idx]);
					vita2d_texture *icon_texture = extractIcon(text);
					if (icon_texture == NULL){
						char error[512];
						sprintf(error, "ERROR: Can't open EBOOT.PBP file for %s.", bubbles[icon_idx]);
						drawText(60,error,red);
						sceKernelDelayThread(2000000);
					}else{
						drawIcon(700, 30, icon_texture);
						sprintf(text, "Do you want to patch %s icon?", bubbles[icon_idx]);
						drawText(40, text, white);
						drawText(80,"Cross = Yes",white);
						drawText(100,"Triangle = No",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!((pad.buttons & SCE_CTRL_TRIANGLE) || (pad.buttons & SCE_CTRL_CROSS)));
						sceKernelDelayThread(500000);
						if (pad.buttons & SCE_CTRL_CROSS){
							
							// Creating folder
							sprintf(text,"ux0:data/RetroLivearea/%s", bubbles[icon_idx]);
							sceIoMkdir(text, 0777);
							
							// Getting patched icon from display
							SceDisplayFrameBuf param;
							param.size = sizeof(SceDisplayFrameBuf);
							sceDisplayGetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);
							uint8_t *buffer = (uint8_t*)param.base;
							uint8_t *patched_raw = (uint8_t*)malloc(4 * 128 * 128);
							int i, y;
							buffer += 700 * 4 + (1024 * 30 * 4);
							for (y=0;y<128;y++){
								memcpy(&patched_raw[y * 4 * 128], buffer, 4 * 128);								
								buffer += 1024 * 4;
							}
							
							// Applying pngquant
							liq_attr *handle = liq_attr_create();
							liq_image *input_image = liq_image_create_rgba(handle, patched_raw, 128, 128, 0);
							liq_result *res;
							liq_image_quantize(input_image, handle, &res);
							uint8_t *quant_raw = (uint8_t*)malloc(128 * 128);
							liq_set_dithering_level(res, 1.0);
							liq_write_remapped_image(res, input_image, quant_raw, 128 * 128);
							const liq_palette *palette = liq_get_palette(res);

							// Saving patched icon
							sprintf(text, "ux0:data/RetroLivearea/%s/icon0.png", bubbles[icon_idx]);
							FILE *fh = fopen(text, "wb");
							png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);;
							png_infop info_ptr = png_create_info_struct(png_ptr);
							setjmp(png_jmpbuf(png_ptr));
							png_init_io(png_ptr, fh);
							png_set_IHDR(png_ptr, info_ptr, 128, 128,
								8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
								PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
							
							// Properly formatting palette
							png_color *pal = (png_color*)png_malloc(png_ptr, palette->count*sizeof(png_color));
							for (i=0;i<palette->count;i++){
								png_color *col = &pal[i];
								col->red = palette->entries[i].r;
								col->green = palette->entries[i].g;
								col->blue = palette->entries[i].b;
							}
							png_set_PLTE(png_ptr, info_ptr, pal, palette->count);
							
							png_write_info(png_ptr, info_ptr);
							
							// Writing data
							for (y=0;y<128;y++){
								png_write_row(png_ptr, &quant_raw[y * 128]);
							}
							
							// Ending file saving
							png_write_end(png_ptr, NULL);
							fclose(fh);
							png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
							png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
							free(quant_raw);
							free(patched_raw);
							liq_result_destroy(res);
							liq_image_destroy(input_image);
							liq_attr_destroy(handle);
							vita2d_free_texture(icon_texture);
							
							// Patching app.db
							fd = sqlite3_open("ur0:/shell/db/app.db", &db);
							char query[1024];
							sprintf(query,"UPDATE tbl_appinfo_icon SET icon0Type = '5',iconPath = 'ux0:data/RetroLivearea/%s/icon0.png' WHERE titleId == '%s'",bubbles[icon_idx],bubbles[icon_idx]);	
							fd = sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							if( fd != SQLITE_OK ){
								char error[512];
								sprintf(error, "ERROR: SQL error: %s", zErrMsg);
								sqlite3_free(zErrMsg);
								drawText(400,error,red);
								sceKernelDelayThread(2000000);
							}
							sqlite3_close(db);
							
						}
						icon_idx++;
					}
				
				}
					
				break;
			case RECURSIVE_ASSETS:
				clearScreen();
				
				if (icon_idx == bubbles_idx){
					state = RESTART_REQUIRED;
				}else{
					
					sprintf(text, "*** Patching assets (%d/%d) ***", icon_idx+1, bubbles_idx);
					drawText(20,text,white);
					
					// Extracting icon0.png from eboot.pbp
					sprintf(text, "ux0:pspemu/PSP/GAME/%s/EBOOT.PBP", bubbles[icon_idx]);
					vita2d_texture *icon_texture = extractIcon(text);
					if (icon_texture == NULL){
						char error[512];
						sprintf(error, "ERROR: Can't open EBOOT.PBP file for %s.", bubbles[icon_idx]);
						drawText(60,error,red);
						sceKernelDelayThread(2000000);
					}else{
						drawIcon(700, 30, icon_texture);
						sprintf(text, "Do you want to patch %s assets?", bubbles[icon_idx]);
						drawText(40, text, white);
						drawText(80,"Cross = Yes",white);
						drawText(100,"Triangle = No",white);
						drawText(200,"NOTE: If patch is processed, random assets will be shown on screen for a while.",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!((pad.buttons & SCE_CTRL_TRIANGLE) || (pad.buttons & SCE_CTRL_CROSS)));
						sceKernelDelayThread(500000);
						if (pad.buttons & SCE_CTRL_CROSS){
							
							// Creating folder
							sprintf(text,"ux0:data/RetroLivearea/%s", bubbles[icon_idx]);
							sceIoMkdir(text, 0777);
							
							//  Extracting pic1.png from eboot.pbp
							sprintf(text, "ux0:pspemu/PSP/GAME/%s/EBOOT.PBP", bubbles[icon_idx]);
							vita2d_texture *bg_texture = extractBackground(text);
							
							// Drawing 840x500 background on screen
							drawBackground(0, 0, bg_texture);
							
							// Getting patched background from display
							SceDisplayFrameBuf param;
							param.size = sizeof(SceDisplayFrameBuf);
							sceDisplayGetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);
							uint8_t *buffer = (uint8_t*)param.base;
							uint8_t *patched_raw = (uint8_t*)malloc(4 * 840 * 500);
							int i, y;
							for (y=0;y<500;y++){
								memcpy(&patched_raw[y * 4 * 840], buffer, 4 * 840);								
								buffer += 1024 * 4;
							}
							
							// Applying pngquant
							liq_attr *handle = liq_attr_create();
							liq_image *input_image = liq_image_create_rgba(handle, patched_raw, 840, 500, 0);
							liq_result *res;
							liq_image_quantize(input_image, handle, &res);
							uint8_t *quant_raw = (uint8_t*)malloc(840 * 500);
							liq_set_dithering_level(res, 1.0);
							liq_write_remapped_image(res, input_image, quant_raw, 840 * 500);
							const liq_palette *palette = liq_get_palette(res);

							// Saving patched background
							sprintf(text, "ux0:data/RetroLivearea/%s/bg.png", bubbles[icon_idx]);
							FILE *fh = fopen(text, "wb");
							png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);;
							png_infop info_ptr = png_create_info_struct(png_ptr);
							setjmp(png_jmpbuf(png_ptr));
							png_init_io(png_ptr, fh);
							png_set_IHDR(png_ptr, info_ptr, 840, 500,
								8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
								PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
							
							// Properly formatting palette
							png_color *pal = (png_color*)png_malloc(png_ptr, palette->count*sizeof(png_color));
							for (i=0;i<palette->count;i++){
								png_color *col = &pal[i];
								col->red = palette->entries[i].r;
								col->green = palette->entries[i].g;
								col->blue = palette->entries[i].b;
							}
							png_set_PLTE(png_ptr, info_ptr, pal, palette->count);
							
							png_write_info(png_ptr, info_ptr);
							
							// Writing data
							for (y=0;y<500;y++){
								png_write_row(png_ptr, &quant_raw[y * 840]);
							}
							
							// Ending file saving
							png_write_end(png_ptr, NULL);
							fclose(fh);
							png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
							png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
							free(quant_raw);
							free(patched_raw);
							liq_result_destroy(res);
							liq_image_destroy(input_image);
							liq_attr_destroy(handle);
							
							// Drawing startup on screen
							drawLittleBackground(0, 0, bg_texture);
							drawCenteredIcon(0, 0, icon_texture);
							
							// Getting patched background from display
							param.size = sizeof(SceDisplayFrameBuf);
							sceDisplayGetFrameBuf(&param, SCE_DISPLAY_SETBUF_NEXTFRAME);
							buffer = (uint8_t*)param.base;
							patched_raw = (uint8_t*)malloc(4 * 280 * 158);
							for (y=0;y<158;y++){
								memcpy(&patched_raw[y * 4 * 280], buffer, 4 * 280);								
								buffer += 1024 * 4;
							}
							
							// Applying pngquant
							handle = liq_attr_create();
							input_image = liq_image_create_rgba(handle, patched_raw, 280, 158, 0);
							liq_image_quantize(input_image, handle, &res);
							quant_raw = (uint8_t*)malloc(280 * 158);
							liq_set_dithering_level(res, 1.0);
							liq_write_remapped_image(res, input_image, quant_raw, 280 * 158);
							palette = liq_get_palette(res);

							// Saving patched startup
							sprintf(text, "ux0:data/RetroLivearea/%s/startup.png", bubbles[icon_idx]);
							fh = fopen(text, "wb");
							png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);;
							info_ptr = png_create_info_struct(png_ptr);
							setjmp(png_jmpbuf(png_ptr));
							png_init_io(png_ptr, fh);
							png_set_IHDR(png_ptr, info_ptr, 280, 158,
								8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
								PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
							
							// Properly formatting palette
							pal = (png_color*)png_malloc(png_ptr, palette->count*sizeof(png_color));
							for (i=0;i<palette->count;i++){
								png_color *col = &pal[i];
								col->red = palette->entries[i].r;
								col->green = palette->entries[i].g;
								col->blue = palette->entries[i].b;
							}
							png_set_PLTE(png_ptr, info_ptr, pal, palette->count);
							
							png_write_info(png_ptr, info_ptr);
							
							// Writing data
							for (y=0;y<158;y++){
								png_write_row(png_ptr, &quant_raw[y * 280]);
							}
							
							// Ending file saving
							png_write_end(png_ptr, NULL);
							fclose(fh);
							png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
							png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
							free(quant_raw);
							free(patched_raw);
							liq_result_destroy(res);
							liq_image_destroy(input_image);
							liq_attr_destroy(handle);
							vita2d_free_texture(icon_texture);
							vita2d_free_texture(bg_texture);
							
							// Creating template.xml file
							sprintf(text, "ux0:data/RetroLivearea/%s/template.xml", bubbles[icon_idx]);
							forkTemplate(text);
							
							// Patching app.db
							sqlite3_open("ur0:/shell/db/app.db", &db);
							char query[1024];
							sprintf(query,"UPDATE tbl_livearea SET style = 'a1', org_Path = 'ux0:data/RetroLivearea/%s', background_image = 'bg.png', gate_startupImage = 'startup.png' WHERE titleId == '%s'",bubbles[icon_idx],bubbles[icon_idx]);	
							sqlite3_exec(db, "DROP TRIGGER tgr_livearea_upd_bgimg", dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, "DROP TRIGGER tgr_livearea_upd_gtimg", dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, "CREATE TRIGGER tgr_livearea_upd_bgimg AFTER UPDATE OF background_image ON tbl_livearea WHEN OLD.background_image LIKE ' %' BEGIN UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.background_image AS INTEGER);END", dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, "CREATE TRIGGER tgr_livearea_upd_gtimg AFTER UPDATE OF gate_startupImage ON tbl_livearea WHEN OLD.gate_startupImage LIKE ' %' BEGIN UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.gate_startupImage AS INTEGER);END", dummy_callback, NULL, &zErrMsg);
							sprintf(query,"UPDATE tbl_appinfo SET val = 'gd' WHERE titleId == '%s' AND key == '566916785'",bubbles[icon_idx]);
							sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							sprintf(query,"UPDATE tbl_appinfo SET val = 'ux0:data/RetroLivearea/%s' WHERE titleId == '%s' AND key == '2630610402'",bubbles[icon_idx],bubbles[icon_idx]);
							fd = sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							if( fd != SQLITE_OK ){
								char error[512];
								sprintf(error, "ERROR: SQL error: %s", zErrMsg);
								sqlite3_free(zErrMsg);
								drawText(400,error,red);
								sceKernelDelayThread(2000000);
							}
							sqlite3_close(db);
							
						}
						icon_idx++;
					}
				
				}
					
				break;
			case RECURSIVE_UNPATCH:
				clearScreen();
				
				if (icon_idx == bubbles_idx){
					state = RESTART_REQUIRED;
				}else{
					
					sprintf(text, "*** Unatching bubbles (%d/%d) ***", icon_idx+1, bubbles_idx);
					drawText(20,text,white);
					
					// Extracting icon0.png from eboot.pbp
					sprintf(text, "ux0:pspemu/PSP/GAME/%s/EBOOT.PBP", bubbles[icon_idx]);
					vita2d_texture *icon_texture = extractIcon(text);
					if (icon_texture == NULL){
						char error[512];
						sprintf(error, "ERROR: Can't open EBOOT.PBP file for %s.", bubbles[icon_idx]);
						drawText(60,error,red);
						sceKernelDelayThread(2000000);
					}else{
						drawIcon(700, 30, icon_texture);
						sprintf(text, "Do you want to unpatch %s bubble?", bubbles[icon_idx]);
						drawText(40, text, white);
						drawText(80,"Cross = Yes",white);
						drawText(100,"Triangle = No",white);
						do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!((pad.buttons & SCE_CTRL_TRIANGLE) || (pad.buttons & SCE_CTRL_CROSS)));
						sceKernelDelayThread(500000);
						if (pad.buttons & SCE_CTRL_CROSS){
							
							
							// Patching app.db
							fd = sqlite3_open("ur0:/shell/db/app.db", &db);
							char query[1024];
							sprintf(query,"UPDATE tbl_livearea SET style = 'pspemu', org_Path = 'ur0:appmeta/%s/livearea/contents', background_image = 'bg0.png', gate_startupImage = 'startup.png' WHERE titleId == '%s'",bubbles[icon_idx],bubbles[icon_idx]);	
							sqlite3_exec(db, "DROP TRIGGER tgr_livearea_upd_bgimg", dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, "DROP TRIGGER tgr_livearea_upd_gtimg", dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, "CREATE TRIGGER tgr_livearea_upd_bgimg AFTER UPDATE OF background_image ON tbl_livearea WHEN OLD.background_image LIKE ' %' BEGIN UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.background_image AS INTEGER);END", dummy_callback, NULL, &zErrMsg);
							sqlite3_exec(db, "CREATE TRIGGER tgr_livearea_upd_gtimg AFTER UPDATE OF gate_startupImage ON tbl_livearea WHEN OLD.gate_startupImage LIKE ' %' BEGIN UPDATE tbl_livearea_file SET refcnt=refcnt-1 WHERE rowid=CAST(OLD.gate_startupImage AS INTEGER);END", dummy_callback, NULL, &zErrMsg);
							sprintf(query,"UPDATE tbl_appinfo SET val = 'ME' WHERE titleId == '%s' AND key == '566916785'",bubbles[icon_idx]);
							sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							sprintf(query,"UPDATE tbl_appinfo SET val = 'ur0:appmeta/%s/livearea/contents' WHERE titleId == '%s' AND key == '2630610402'",bubbles[icon_idx],bubbles[icon_idx]);
							sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							sprintf(query,"UPDATE tbl_appinfo_icon SET icon0Type = '0',iconPath = 'ur0:appmeta/%s/icon0.dds' WHERE titleId == '%s'",bubbles[icon_idx],bubbles[icon_idx]);	
							fd = sqlite3_exec(db, query, dummy_callback, NULL, &zErrMsg);
							if( fd != SQLITE_OK ){
								char error[512];
								sprintf(error, "ERROR: SQL error: %s", zErrMsg);
								sqlite3_free(zErrMsg);
								drawText(400,error,red);
								sceKernelDelayThread(2000000);
							}
							sqlite3_close(db);
							
						}
						icon_idx++;
					}
				
				}
					
				break;
			case RESTART_REQUIRED:
					
				drawText(200,"Operation completed!",green);
				drawText(240,"To make changes effective, a reboot is required",green);
				drawText(260,"Press Triangle to perform a console reboot",green);
					
				do{sceCtrlPeekBufferPositive(0, &pad, 1);}while(!(pad.buttons & SCE_CTRL_TRIANGLE));
				scePowerRequestColdReset();
				state = MAIN_MENU;
				break;
			case EXIT:
				exit_code = 1;
				break;
		}
		if (exit_code)	break;
	}
	
	return 0;
	
}
