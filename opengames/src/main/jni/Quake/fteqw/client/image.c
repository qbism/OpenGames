#include "quakedef.h"
#ifdef GLQUAKE
#include "glquake.h"
#endif

#ifdef D3DQUAKE
//#include "d3dquake.h"
#endif

#ifdef NPFTE
//#define Con_Printf(f, ...)
//hope you're on a littleendian machine
#define LittleShort(s) s
#define LittleLong(s) s
#else
cvar_t r_dodgytgafiles = SCVAR("r_dodgytgafiles", "0");	//Certain tgas are upside down.
													//This is due to a bug in tenebrae.
													//(normally) the textures are actually the right way around.
													//but some people have gone and 'fixed' those broken ones by flipping.
													//these images appear upside down in any editor but correct in tenebrae
													//set this to 1 to emulate tenebrae's bug.
cvar_t r_dodgypcxfiles = SCVAR("r_dodgypcxfiles", "0");	//Quake 2's PCX loading isn't complete,
													//and some Q2 mods include PCX files
													//that only work with this assumption
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

typedef struct {	//cm = colourmap
	char	id_len;		//0
	char	cm_type;	//1
	char	version;	//2
	short	cm_idx;		//3
	short	cm_len;		//5
	char	cm_size;	//7
	short	originx;	//8 (ignored)
	short	originy;	//10 (ignored)
	short	width;		//12-13
	short	height;		//14-15
	qbyte	bpp;		//16
	qbyte	attribs;	//17
} tgaheader_t;

char *ReadGreyTargaFile (qbyte *data, int flen, tgaheader_t *tgahead, int asgrey)	//preswapped header
{
	int				columns, rows, numPixels;
	int				row, column;
	qbyte			*pixbuf, *pal;
	qboolean		flipped;

	qbyte *pixels = BZ_Malloc(tgahead->width * tgahead->height * (asgrey?1:4));

	if (tgahead->version!=1
		&& tgahead->version!=3)
	{
		Con_Printf("LoadGrayTGA: Only type 1 and 3 greyscale targa images are understood.\n");
		BZ_Free(pixels);
		return NULL;
	}

    if (tgahead->version==1 && tgahead->bpp != 8 &&
		tgahead->cm_size != 24 && tgahead->cm_len != 256)
	{
		Con_Printf("LoadGrayTGA: Strange palette type\n");
		BZ_Free(pixels);
		return NULL;
	}

	columns = tgahead->width;
	rows = tgahead->height;
	numPixels = columns * rows;

	flipped = !((tgahead->attribs & 0x20) >> 5);
#ifndef NPFTE
	if (r_dodgytgafiles.value)
		flipped = true;
#endif

	if (tgahead->version == 1)
	{
		pal = data;
		data += tgahead->cm_len*3;
		if (asgrey)
		{
			for(row=rows-1; row>=0; row--)
			{
				if (flipped)
					pixbuf = pixels + row*columns;
				else
					pixbuf = pixels + ((rows-1)-row)*columns;

				for(column=0; column<columns; column++)
					*pixbuf++= *data++;
			}
		}
		else
		{
			for(row=rows-1; row>=0; row--)
			{
				if (flipped)
					pixbuf = pixels + row*columns*4;
				else
					pixbuf = pixels + ((rows-1)-row)*columns*4;

				for(column=0; column<columns; column++)
				{
					*pixbuf++= pal[*data*3+2];
					*pixbuf++= pal[*data*3+1];
					*pixbuf++= pal[*data*3+0];
					*pixbuf++= 255;
					data++;
				}
			}
		}
		return pixels;
	}
	//version 3 now

	if (asgrey)
	{
		for(row=rows-1; row>=0; row--)
		{
			if (flipped)
				pixbuf = pixels + row*columns;
			else
				pixbuf = pixels + ((rows-1)-row)*columns;

			pixbuf = pixels + row*columns;
			for(column=0; column<columns; column++)
				*pixbuf++= *data++;
		}
	}
	else
	{
		for(row=rows-1; row>=0; row--)
		{
			if (flipped)
				pixbuf = pixels + row*columns*4;
			else
				pixbuf = pixels + ((rows-1)-row)*columns*4;

			for(column=0; column<columns; column++)
			{
				*pixbuf++= *data;
				*pixbuf++= *data;
				*pixbuf++= *data;
				*pixbuf++= 255;
				data++;
			}
		}
	}

	return pixels;
}

//remember to free it
qbyte *ReadTargaFile(qbyte *buf, int length, int *width, int *height, qboolean *hasalpha, int asgrey)
{
	unsigned char *data;

	qboolean flipped;

	tgaheader_t tgaheader;

	if (buf[16] != 8 && buf[16] != 16 && buf[16] != 24 && buf[16] != 32)
		return NULL;	//BUMMER!

	tgaheader.id_len = buf[0];
	tgaheader.cm_type = buf[1];
	tgaheader.version = buf[2];
	tgaheader.cm_idx = LittleShort(*(short *)&buf[3]);
	tgaheader.cm_len = LittleShort(*(short *)&buf[5]);
	tgaheader.cm_size = buf[7];
	tgaheader.originx = LittleShort(*(short *)&buf[8]);
	tgaheader.originy = LittleShort(*(short *)&buf[10]);
	tgaheader.width = LittleShort(*(short *)&buf[12]);
	tgaheader.height = LittleShort(*(short *)&buf[14]);
	tgaheader.bpp = buf[16];
	tgaheader.attribs = buf[17];

	flipped = !((tgaheader.attribs & 0x20) >> 5);
#ifndef NPFTE
	if (r_dodgytgafiles.value)
		flipped = true;
#endif

	data=buf+18;
	data += tgaheader.id_len;

	*width = tgaheader.width;
	*height = tgaheader.height;

	if (asgrey == 2)	//grey only, load as 8 bit..
	{
		if (!tgaheader.version == 1 && !tgaheader.version == 3)
			return NULL;
	}
	if (tgaheader.version == 1 || tgaheader.version == 3)
	{
		return ReadGreyTargaFile(data, length, &tgaheader, asgrey);
	}
	else if (tgaheader.version == 10 || tgaheader.version == 9 || tgaheader.version == 11)
	{
		//9:paletted
		//10:bgr(a)
		//11:greyscale
#undef getc
#define getc(x) *data++
		unsigned row, rows=tgaheader.height, column, columns=tgaheader.width, packetHeader, packetSize, j;
		qbyte *pixbuf, *targa_rgba=BZ_Malloc(rows*columns*(asgrey?1:4)), *inrow;

		qbyte blue, red, green, alphabyte;

		byte_vec4_t palette[256];

		if (tgaheader.version == 9)
		{
			for (row = 0; row < 256; row++)
			{
				palette[row][0] = row;
				palette[row][1] = row;
				palette[row][2] = row;
				palette[row][3] = 255;
			}
			if (tgaheader.bpp != 8)
				return NULL;
		}
		if (tgaheader.version == 10)
		{
			if (tgaheader.bpp == 8)
				return NULL;

			*hasalpha = (tgaheader.bpp==32);
		}
		if (tgaheader.version == 11)
		{
			for (row = 0; row < 256; row++)
			{
				palette[row][0] = row;
				palette[row][1] = row;
				palette[row][2] = row;
				palette[row][3] = 255;
			}
			if (tgaheader.bpp != 8)
				return NULL;
		}

		if (tgaheader.cm_type)
		{
			switch(tgaheader.cm_size)
			{
			case 24:
				for (row = 0; row < tgaheader.cm_len; row++)
				{
					palette[row][0] = *data++;
					palette[row][1] = *data++;
					palette[row][2] = *data++;
					palette[row][3] = 255;
				}
				break;
			case 32:
				for (row = 0; row < tgaheader.cm_len; row++)
				{
					palette[row][0] = *data++;
					palette[row][1] = *data++;
					palette[row][2] = *data++;
					palette[row][3] = *data++;
				}
				*hasalpha = true;
				break;
			}
		}

		for(row=rows-1; row>=0; row--)
		{
			if (flipped)
				pixbuf = targa_rgba + row*columns*(asgrey?1:4);
			else
				pixbuf = targa_rgba + ((rows-1)-row)*columns*(asgrey?1:4);
			for(column=0; column<columns; )
			{
				packetHeader=*data++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{        // run-length packet
					switch (tgaheader.bpp)
					{
						case 8:	//we made sure this was version 11
								blue = palette[*data][0];
								green = palette[*data][1];
								red = palette[*data][2];
								alphabyte = palette[*data][3];
								data++;
								break;

						case 16:
								inrow = data;
								data+=2;
								red = ((inrow[1] & 0x7c)>>2) *8;					//red
								green =	(((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
								blue = (inrow[0] & 0x1f)*8;					//blue
								alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?
								break;
						case 24:
								blue = *data++;
								green = *data++;
								red = *data++;
								alphabyte = 255;
								break;
						case 32:
								blue = *data++;
								green = *data++;
								red = *data++;
								alphabyte = *data++;
								break;
						default:
								blue = 127;
								green = 127;
								red = 127;
								alphabyte = 127;
								break;
					}

					if (!asgrey)	//keep colours
					{
						for(j=0;j<packetSize;j++)
						{
							*pixbuf++=red;
							*pixbuf++=green;
							*pixbuf++=blue;
							*pixbuf++=alphabyte;
							column++;
							if (column==columns)
							{ // run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*4;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*4;
							}
						}
					}
					else	//convert to greyscale
					{
						for(j=0;j<packetSize;j++)
						{
							*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
							column++;
							if (column==columns)
							{ // run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*1;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*1;
							}
						}
					}
				}
				else
				{                            // non run-length packet
					if (!asgrey)	//keep colours
					{
						for(j=0;j<packetSize;j++)
						{
							switch (tgaheader.bpp)
							{
								case 8:
										blue = palette[*data][0];
										green = palette[*data][1];
										red = palette[*data][2];
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = palette[*data][3];
										data++;
										break;
								case 16:
										inrow = data;
										data+=2;
										red = ((inrow[1] & 0x7c)>>2) *8;					//red
										green =	(((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
										blue = (inrow[0] & 0x1f)*8;					//blue
										alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?

										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = alphabyte;
										break;
								case 24:
										blue = *data++;
										green = *data++;
										red = *data++;
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = 255;
										break;
								case 32:
										blue = *data++;
										green = *data++;
										red = *data++;
										alphabyte = *data++;
										*pixbuf++ = red;
										*pixbuf++ = green;
										*pixbuf++ = blue;
										*pixbuf++ = alphabyte;
										break;
								default:
										blue = 127;
										green = 127;
										red = 127;
										alphabyte = 127;
										break;
							}
							column++;
							if (column==columns)
							{ // pixel packet run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*4;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*4;
							}
						}
					}
					else	//convert to grey
					{
						for(j=0;j<packetSize;j++)
						{
							switch (tgaheader.bpp)
							{
								case 8:
										blue = palette[*data][0];
										green = palette[*data][1];
										red = palette[*data][2];
										*pixbuf++ = (blue + green + red)/3;
										data++;
										break;
								case 16:
										inrow = data;
										data+=2;
										red = ((inrow[1] & 0x7c)>>2) *8;					//red
										green =	(((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
										blue = (inrow[0] & 0x1f)*8;					//blue
										alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?

										*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
										break;
								case 24:
										blue = *data++;
										green = *data++;
										red = *data++;
										*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
										break;
								case 32:
										blue = *data++;
										green = *data++;
										red = *data++;
										alphabyte = *data++;
										*pixbuf++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
										break;
								default:
										blue = 127;
										green = 127;
										red = 127;
										alphabyte = 127;
										break;
							}
							column++;
							if (column==columns)
							{ // pixel packet run spans across rows
								column=0;
								if (row>0)
									row--;
								else
									goto breakOut;
								if (flipped)
									pixbuf = targa_rgba + row*columns*1;
								else
									pixbuf = targa_rgba + ((rows-1)-row)*columns*1;
							}
						}
					}
				}
			}
		}
		breakOut:;

		return targa_rgba;
	}
	else if (tgaheader.version == 2)
	{
		qbyte *initbuf=BZ_Malloc(tgaheader.height*tgaheader.width* (asgrey?1:4));
		qbyte *inrow, *outrow;
		int x, y, mul;
		qbyte blue, red, green;

		if (tgaheader.bpp == 8)
			return NULL;

		mul = tgaheader.bpp/8;
		*hasalpha = mul==4;
//flip +convert to 32 bit
		if (asgrey)
			outrow = &initbuf[(int)(0)*tgaheader.width];
		else
			outrow = &initbuf[(int)(0)*tgaheader.width*mul];
		for (y = 0; y < tgaheader.height; y+=1)
		{
			if (flipped)
				inrow = &data[(int)(tgaheader.height-y-1)*tgaheader.width*mul];
			else
				inrow = &data[(int)(y)*tgaheader.width*mul];

			if (!asgrey)
			{
				switch(mul)
				{
				case 2:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						*outrow++ = ((inrow[1] & 0x7c)>>2) *8;					//red
						*outrow++ = (((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
						*outrow++ = (inrow[0] & 0x1f)*8;					//blue
						*outrow++ = (int)(inrow[1]&0x80)*2-1;			//alpha?
						inrow+=2;
					}
					break;
				case 3:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						*outrow++ = inrow[2];
						*outrow++ = inrow[1];
						*outrow++ = inrow[0];
						*outrow++ = 255;
						inrow+=3;
					}
					break;
				case 4:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						*outrow++ = inrow[2];
						*outrow++ = inrow[1];
						*outrow++ = inrow[0];
						*outrow++ = inrow[3];
						inrow+=4;
					}
					break;
				}
			}
			else
			{
				switch(mul)
				{
				case 2:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						red = ((inrow[1] & 0x7c)>>2) *8;					//red
						green = (((inrow[1] & 0x03)<<3) + ((inrow[0] & 0xe0)>>5))*8;	//green
						blue = (inrow[0] & 0x1f)*8;					//blue
//						alphabyte = (int)(inrow[1]&0x80)*2-1;			//alpha?

						*outrow++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
						inrow+=2;
					}
					break;
				case 3:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						red = inrow[2];
						green = inrow[1];
						blue = inrow[0];
						*outrow++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
						inrow+=3;
					}
					break;
				case 4:
					for (x = 0; x < tgaheader.width; x+=1)
					{
						red = inrow[2];
						green = inrow[1];
						blue = inrow[0];
						*outrow++ = red*NTSC_RED + green*NTSC_GREEN + blue*NTSC_BLUE;
						inrow+=4;
					}
					break;
				}
			}
		}

		return initbuf;
	}
	else
		Con_Printf("Unsupported version\n");
return NULL;
}

#ifdef AVAIL_PNGLIB
	#ifndef AVAIL_ZLIB
		#error PNGLIB requires ZLIB
	#endif

	#undef channels

	#ifndef PNG_SUCKS_WITH_SETJMP
		#if defined(MINGW)
			#include "./mingw-libs/png.h"
		#elif defined(_WIN32)
			#include "png.h"
		#else
			#include <png.h>
		#endif
	#endif

	#ifdef DYNAMIC_LIBPNG
		#define PSTATIC(n)
		static dllhandle_t *libpng_handle;
		#define LIBPNG_LOADED() (libpng_handle != NULL)
	#else
		#define LIBPNG_LOADED() 1
		#define PSTATIC(n) = &n
		#ifdef _MSC_VER
			#ifdef _WIN64
				#pragma comment(lib, MSVCLIBSPATH "libpng64.lib")
			#else
				#pragma comment(lib, MSVCLIBSPATH "libpng.lib")
			#endif
		#endif
	#endif

#ifndef PNG_NORETURN
#define PNG_NORETURN
#endif
#ifndef PNG_ALLOCATED
#define PNG_ALLOCATED
#endif

#define png_const_infop png_infop
#define png_const_structp png_structp
#define png_const_bytep png_bytep

void (PNGAPI *qpng_error) PNGARG((png_structp png_ptr, png_const_charp error_message)) PSTATIC(png_error);
void (PNGAPI *qpng_read_end) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_read_end);
void (PNGAPI *qpng_read_image) PNGARG((png_structp png_ptr, png_bytepp image)) PSTATIC(png_read_image);
png_byte (PNGAPI *qpng_get_bit_depth) PNGARG((png_const_structp png_ptr, const png_const_infop info_ptr)) PSTATIC(png_get_bit_depth);
png_byte (PNGAPI *qpng_get_channels) PNGARG((png_const_structp png_ptr, png_const_infop info_ptr)) PSTATIC(png_get_channels);
png_size_t (PNGAPI *qpng_get_rowbytes) PNGARG((png_const_structp png_ptr, png_const_infop info_ptr)) PSTATIC(png_get_rowbytes);
void (PNGAPI *qpng_read_update_info) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_read_update_info);
void (PNGAPI *qpng_set_strip_16) PNGARG((png_structp png_ptr)) PSTATIC(png_set_strip_16);
void (PNGAPI *qpng_set_expand) PNGARG((png_structp png_ptr)) PSTATIC(png_set_expand);
void (PNGAPI *qpng_set_gray_to_rgb) PNGARG((png_structp png_ptr)) PSTATIC(png_set_gray_to_rgb);
void (PNGAPI *qpng_set_tRNS_to_alpha) PNGARG((png_structp png_ptr)) PSTATIC(png_set_tRNS_to_alpha);
png_uint_32 (PNGAPI *qpng_get_valid) PNGARG((png_const_structp png_ptr, png_const_infop info_ptr, png_uint_32 flag)) PSTATIC(png_get_valid);
#if PNG_LIBPNG_VER > 10400
void (PNGAPI *qpng_set_expand_gray_1_2_4_to_8) PNGARG((png_structp png_ptr)) PSTATIC(png_set_expand_gray_1_2_4_to_8);
#else
void (PNGAPI *qpng_set_gray_1_2_4_to_8) PNGARG((png_structp png_ptr)) PSTATIC(png_set_gray_1_2_4_to_8);
#endif
void (PNGAPI *qpng_set_filler) PNGARG((png_structp png_ptr, png_uint_32 filler, int flags)) PSTATIC(png_set_filler);
void (PNGAPI *qpng_set_palette_to_rgb) PNGARG((png_structp png_ptr)) PSTATIC(png_set_palette_to_rgb);
png_uint_32 (PNGAPI *qpng_get_IHDR) PNGARG((png_structp png_ptr, png_infop info_ptr, png_uint_32 *width, png_uint_32 *height,
			int *bit_depth, int *color_type, int *interlace_method, int *compression_method, int *filter_method)) PSTATIC(png_get_IHDR);
void (PNGAPI *qpng_read_info) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_read_info);
void (PNGAPI *qpng_set_sig_bytes) PNGARG((png_structp png_ptr, int num_bytes)) PSTATIC(png_set_sig_bytes);
void (PNGAPI *qpng_set_read_fn) PNGARG((png_structp png_ptr, png_voidp io_ptr, png_rw_ptr read_data_fn)) PSTATIC(png_set_read_fn);
void (PNGAPI *qpng_destroy_read_struct) PNGARG((png_structpp png_ptr_ptr, png_infopp info_ptr_ptr, png_infopp end_info_ptr_ptr)) PSTATIC(png_destroy_read_struct);
png_infop (PNGAPI *qpng_create_info_struct) PNGARG((png_structp png_ptr)) PSTATIC(png_create_info_struct);
png_structp (PNGAPI *qpng_create_read_struct) PNGARG((png_const_charp user_png_ver, png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn)) PSTATIC(png_create_read_struct);
int (PNGAPI *qpng_sig_cmp) PNGARG((png_const_bytep sig, png_size_t start, png_size_t num_to_check)) PSTATIC(png_sig_cmp);

void (PNGAPI *qpng_write_end) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_write_end);
void (PNGAPI *qpng_write_image) PNGARG((png_structp png_ptr, png_bytepp image)) PSTATIC(png_write_image);
void (PNGAPI *qpng_write_info) PNGARG((png_structp png_ptr, png_infop info_ptr)) PSTATIC(png_write_info);
void (PNGAPI *qpng_set_IHDR) PNGARG((png_structp png_ptr, png_infop info_ptr, png_uint_32 width, png_uint_32 height,
			int bit_depth, int color_type, int interlace_method, int compression_method, int filter_method)) PSTATIC(png_set_IHDR);
void (PNGAPI *qpng_set_compression_level) PNGARG((png_structp png_ptr, int level)) PSTATIC(png_set_compression_level);
void (PNGAPI *qpng_init_io) PNGARG((png_structp png_ptr, png_FILE_p fp)) PSTATIC(png_init_io);
png_voidp (PNGAPI *qpng_get_io_ptr) PNGARG((png_structp png_ptr)) PSTATIC(png_get_io_ptr);
void (PNGAPI *qpng_destroy_write_struct) PNGARG((png_structpp png_ptr_ptr, png_infopp info_ptr_ptr)) PSTATIC(png_destroy_write_struct);
png_structp (PNGAPI *qpng_create_write_struct) PNGARG((png_const_charp user_png_ver, png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn)) PSTATIC(png_create_write_struct);

qboolean LibPNG_Init(void)
{
#ifdef DYNAMIC_LIBPNG
	static dllfunction_t pngfuncs[] =
	{
		{(void **) &qpng_error,							"png_error"},
		{(void **) &qpng_read_end,						"png_read_end"},
		{(void **) &qpng_read_image,					"png_read_image"},
		{(void **) &qpng_get_bit_depth,					"png_get_bit_depth"},
		{(void **) &qpng_get_channels,					"png_get_channels"},
		{(void **) &qpng_get_rowbytes,					"png_get_rowbytes"},
		{(void **) &qpng_read_update_info,				"png_read_update_info"},
		{(void **) &qpng_set_strip_16,					"png_set_strip_16"},
		{(void **) &qpng_set_expand,					"png_set_expand"},
		{(void **) &qpng_set_gray_to_rgb,				"png_set_gray_to_rgb"},
		{(void **) &qpng_set_tRNS_to_alpha,				"png_set_tRNS_to_alpha"},
		{(void **) &qpng_get_valid,						"png_get_valid"},
		#if PNG_LIBPNG_VER > 10400
		{(void **) &qpng_set_expand_gray_1_2_4_to_8,	"png_set_expand_gray_1_2_4_to_8"},
                #else
		{(void **) &qpng_set_gray_1_2_4_to_8,	"png_set_gray_1_2_4_to_8"},
                #endif
		{(void **) &qpng_set_filler,					"png_set_filler"},
		{(void **) &qpng_set_palette_to_rgb,			"png_set_palette_to_rgb"},
		{(void **) &qpng_get_IHDR,						"png_get_IHDR"},
		{(void **) &qpng_read_info,						"png_read_info"},
		{(void **) &qpng_set_sig_bytes,					"png_set_sig_bytes"},
		{(void **) &qpng_set_read_fn,					"png_set_read_fn"},
		{(void **) &qpng_destroy_read_struct,			"png_destroy_read_struct"},
		{(void **) &qpng_create_info_struct,			"png_create_info_struct"},
		{(void **) &qpng_create_read_struct,			"png_create_read_struct"},
		{(void **) &qpng_sig_cmp,						"png_sig_cmp"},

		{(void **) &qpng_write_end,						"png_write_end"},
		{(void **) &qpng_write_image,					"png_write_image"},
		{(void **) &qpng_write_info,					"png_write_info"},
		{(void **) &qpng_set_IHDR,						"png_set_IHDR"},
		{(void **) &qpng_set_compression_level,			"png_set_compression_level"},
		{(void **) &qpng_init_io,						"png_init_io"},
		{(void **) &qpng_destroy_write_struct,			"png_destroy_write_struct"},
		{(void **) &qpng_create_write_struct,			"png_create_write_struct"},
		{NULL, NULL}
	};

	if (!LIBPNG_LOADED())
		libpng_handle = Sys_LoadLibrary("libpng", pngfuncs);
#endif
	return LIBPNG_LOADED();
}



#if defined(MING)	//hehehe... add annother symbol so the statically linked cygwin libpng can link
#undef setjmp
int setjmp (jmp_buf jb)
{
	return _setjmp(jb);
}
#endif

typedef struct {
	char *data;
	int readposition;
	int filelen;
} pngreadinfo_t;

void PNGAPI png_default_read_data(png_structp png_ptr, png_bytep data, png_size_t length);

void VARGS readpngdata(png_structp png_ptr,png_bytep data,png_size_t len)
{
	pngreadinfo_t *ri = (pngreadinfo_t*)qpng_get_io_ptr(png_ptr);
	if (ri->readposition+len > ri->filelen)
	{
		qpng_error(png_ptr, "unexpected eof");
		return;
	}
	memcpy(data, &ri->data[ri->readposition], len);
	ri->readposition+=len;
}

qbyte *png_rgba;
qbyte *ReadPNGFile(qbyte *buf, int length, int *width, int *height, const char *fname)
{
	qbyte header[8], **rowpointers = NULL, *data = NULL;
	png_structp png;
	png_infop pnginfo;
	int y, bitdepth, colortype, interlace, compression, filter, bytesperpixel;
	unsigned long rowbytes;
	pngreadinfo_t ri;
	png_uint_32 pngwidth, pngheight;

	memcpy(header, buf, 8);

	if (qpng_sig_cmp(header, 0, 8))
	{
		return (png_rgba = NULL);
	}

	if (!(png = qpng_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
	{
		return (png_rgba = NULL);
	}

	if (!(pnginfo = qpng_create_info_struct(png)))
	{
		qpng_destroy_read_struct(&png, &pnginfo, NULL);
		return (png_rgba = NULL);
	}

	if (setjmp(png_jmpbuf(png)))
	{
error:
		if (data)
			BZ_Free(data);
		if (rowpointers)
			BZ_Free(rowpointers);
        qpng_destroy_read_struct(&png, &pnginfo, NULL);
        return (png_rgba = NULL);
    }

	ri.data=buf;
	ri.readposition=8;
	ri.filelen=length;
	qpng_set_read_fn(png, &ri, readpngdata);

	qpng_set_sig_bytes(png, 8);
	qpng_read_info(png, pnginfo);
	qpng_get_IHDR(png, pnginfo, &pngwidth, &pngheight, &bitdepth, &colortype, &interlace, &compression, &filter);

	*width = pngwidth;
	*height = pngheight;

	if (colortype == PNG_COLOR_TYPE_PALETTE)
	{
		qpng_set_palette_to_rgb(png);
		qpng_set_filler(png, 255, PNG_FILLER_AFTER);
	}

	if (colortype == PNG_COLOR_TYPE_GRAY && bitdepth < 8)
	{
		#if PNG_LIBPNG_VER > 10400
			qpng_set_expand_gray_1_2_4_to_8(png);
		#else
			qpng_set_gray_1_2_4_to_8(png);
		#endif
	}

	if (qpng_get_valid( png, pnginfo, PNG_INFO_tRNS))
		qpng_set_tRNS_to_alpha(png);

	if (bitdepth >= 8 && colortype == PNG_COLOR_TYPE_RGB)
		qpng_set_filler(png, 255, PNG_FILLER_AFTER);

	if (colortype == PNG_COLOR_TYPE_GRAY || colortype == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		qpng_set_gray_to_rgb( png );
		qpng_set_filler(png, 255, PNG_FILLER_AFTER);
	}

	if (bitdepth < 8)
		qpng_set_expand (png);
	else if (bitdepth == 16)
		qpng_set_strip_16(png);


	qpng_read_update_info(png, pnginfo);
	rowbytes = qpng_get_rowbytes(png, pnginfo);
	bytesperpixel = qpng_get_channels(png, pnginfo);
	bitdepth = qpng_get_bit_depth(png, pnginfo);

	if (bitdepth != 8 || bytesperpixel != 4)
	{
		Con_Printf ("Bad PNG color depth and/or bpp (%s)\n", fname);
		qpng_destroy_read_struct(&png, &pnginfo, NULL);
		return (png_rgba = NULL);
	}

	data = BZF_Malloc(*height * rowbytes);
	rowpointers = BZF_Malloc(*height * sizeof(*rowpointers));

	if (!data || !rowpointers)
		goto error;

	for (y = 0; y < *height; y++)
		rowpointers[y] = data + y * rowbytes;

	qpng_read_image(png, rowpointers);
	qpng_read_end(png, NULL);

	qpng_destroy_read_struct(&png, &pnginfo, NULL);
	BZ_Free(rowpointers);
	return (png_rgba = data);
}



#ifndef NPFTE
int Image_WritePNG (char *filename, int compression, qbyte *pixels, int width, int height)
{
	char name[MAX_OSPATH];
	int i;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_byte **row_pointers;

	if (!FS_NativePath(filename, FS_GAMEONLY, name, sizeof(name)))
		return false;

	if (!(fp = fopen (name, "wb")))
	{
		FS_CreatePath (filename, FS_GAMEONLY);
		if (!(fp = fopen (name, "wb")))
			return false;
	}

    if (!(png_ptr = qpng_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
	{
		fclose(fp);
		return false;
	}

    if (!(info_ptr = qpng_create_info_struct(png_ptr)))
	{
		qpng_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		return false;
    }

	if (setjmp(png_jmpbuf(png_ptr)))
	{
err:
        qpng_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
		return false;
    }

	qpng_init_io(png_ptr, fp);
	compression = bound(0, compression, 100);

// had to add these when I migrated from libpng 1.4.x to 1.5.x
#ifndef Z_NO_COMPRESSION
#define Z_NO_COMPRESSION         0
#endif
#ifndef Z_BEST_COMPRESSION
#define Z_BEST_COMPRESSION       9
#endif
	qpng_set_compression_level(png_ptr, Z_NO_COMPRESSION + (compression*(Z_BEST_COMPRESSION-Z_NO_COMPRESSION))/100);

	qpng_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	qpng_write_info(png_ptr, info_ptr);

	row_pointers = BZ_Malloc (sizeof(png_byte *) * height);
	if (!row_pointers)
		goto err;
	for (i = 0; i < height; i++)
		row_pointers[height - i - 1] = pixels + i * width * 3;
	qpng_write_image(png_ptr, row_pointers);
	qpng_write_end(png_ptr, info_ptr);
	BZ_Free(row_pointers);
	qpng_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	return true;
}
#endif


#endif

#ifdef AVAIL_JPEGLIB
#define XMD_H	//fix for mingw

#if defined(MINGW)
	#define JPEG_API VARGS
	#include "./mingw-libs/jpeglib.h"
	#include "./mingw-libs/jerror.h"
#elif defined(_WIN32)
	#define JPEG_API VARGS
	#include "jpeglib.h"
	#include "jerror.h"
#else
//	#include <jinclude.h>
	#include <jpeglib.h>
	#include <jerror.h>
#endif

#ifdef DYNAMIC_LIBJPEG
	#define JSTATIC(n)
	static dllhandle_t *libjpeg_handle;
	#define LIBJPEG_LOADED() (libjpeg_handle != NULL)
#else
	#ifdef _MSC_VER
		#ifdef _WIN64
			#pragma comment(lib, MSVCLIBSPATH "libjpeg64.lib")
		#else
			#pragma comment(lib, MSVCLIBSPATH "jpeg.lib")
		#endif
	#endif
	#define JSTATIC(n) = &n
	#define LIBJPEG_LOADED() (1)
#endif

#ifndef JPEG_FALSE
#define JPEG_boolean boolean
#endif

#define qjpeg_create_compress(cinfo) \
    qjpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, \
			(size_t) sizeof(struct jpeg_compress_struct))
#define qjpeg_create_decompress(cinfo) \
    qjpeg_CreateDecompress((cinfo), JPEG_LIB_VERSION, \
			  (size_t) sizeof(struct jpeg_decompress_struct))

#ifdef DYNAMIC_LIBJPEG
boolean (VARGS *qjpeg_resync_to_restart) JPP((j_decompress_ptr cinfo, int desired))										JSTATIC(jpeg_resync_to_restart);
boolean (VARGS *qjpeg_finish_decompress) JPP((j_decompress_ptr cinfo))												JSTATIC(jpeg_finish_decompress);
JDIMENSION (VARGS *qjpeg_read_scanlines) JPP((j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines))	JSTATIC(jpeg_read_scanlines);
boolean (VARGS *qjpeg_start_decompress) JPP((j_decompress_ptr cinfo))													JSTATIC(jpeg_start_decompress);
int (VARGS *qjpeg_read_header) JPP((j_decompress_ptr cinfo, boolean require_image))									JSTATIC(jpeg_read_header);
void (VARGS *qjpeg_CreateDecompress) JPP((j_decompress_ptr cinfo, int version, size_t structsize))					JSTATIC(jpeg_CreateDecompress);
void (VARGS *qjpeg_destroy_decompress) JPP((j_decompress_ptr cinfo))													JSTATIC(jpeg_destroy_decompress);

struct jpeg_error_mgr * (VARGS *qjpeg_std_error) JPP((struct jpeg_error_mgr * err))									JSTATIC(jpeg_std_error);

void (VARGS *qjpeg_finish_compress) JPP((j_compress_ptr cinfo))														JSTATIC(jpeg_finish_compress);
JDIMENSION (VARGS *qjpeg_write_scanlines) JPP((j_compress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION num_lines))		JSTATIC(jpeg_write_scanlines);
void (VARGS *qjpeg_start_compress) JPP((j_compress_ptr cinfo, boolean write_all_tables))								JSTATIC(jpeg_start_compress);
void (VARGS *qjpeg_set_quality) JPP((j_compress_ptr cinfo, int quality, boolean force_baseline))						JSTATIC(jpeg_set_quality);
void (VARGS *qjpeg_set_defaults) JPP((j_compress_ptr cinfo))															JSTATIC(jpeg_set_defaults);
void (VARGS *qjpeg_CreateCompress) JPP((j_compress_ptr cinfo, int version, size_t structsize))						JSTATIC(jpeg_CreateCompress);
void (VARGS *qjpeg_destroy_compress) JPP((j_compress_ptr cinfo))														JSTATIC(jpeg_destroy_compress);
#endif

qboolean LibJPEG_Init(void)
{
	#ifdef DYNAMIC_LIBJPEG
	static dllfunction_t jpegfuncs[] =
	{
		{(void **) &qjpeg_resync_to_restart,		"jpeg_resync_to_restart"},
		{(void **) &qjpeg_finish_decompress,		"jpeg_finish_decompress"},
		{(void **) &qjpeg_read_scanlines,			"jpeg_read_scanlines"},
		{(void **) &qjpeg_start_decompress,			"jpeg_start_decompress"},
		{(void **) &qjpeg_read_header,				"jpeg_read_header"},
		{(void **) &qjpeg_CreateDecompress,			"jpeg_CreateDecompress"},
		{(void **) &qjpeg_destroy_decompress,		"jpeg_destroy_decompress"},

		{(void **) &qjpeg_std_error,				"jpeg_std_error"},

		{(void **) &qjpeg_finish_compress,			"jpeg_finish_compress"},
		{(void **) &qjpeg_write_scanlines,			"jpeg_write_scanlines"},
		{(void **) &qjpeg_start_compress,			"jpeg_start_compress"},
		{(void **) &qjpeg_set_quality,				"jpeg_set_quality"},
		{(void **) &qjpeg_set_defaults,				"jpeg_set_defaults"},
		{(void **) &qjpeg_CreateCompress,			"jpeg_CreateCompress"},
		{(void **) &qjpeg_destroy_compress,			"jpeg_destroy_compress"},

		{NULL, NULL}
	};

	if (!LIBJPEG_LOADED())
		libjpeg_handle = Sys_LoadLibrary("libjpeg", jpegfuncs);
	#endif

	return LIBJPEG_LOADED();
}

/*begin jpeg read*/

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */




/* Expanded data source object for stdio input */

typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */

  qbyte * infile;		/* source stream */
  int currentpos;
  int maxlen;
  JOCTET * buffer;		/* start of buffer */
  JPEG_boolean start_of_file;	/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096	/* choose an efficiently fread'able size */


METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  src->start_of_file = true;
}

METHODDEF(JPEG_boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
	my_source_mgr *src = (my_source_mgr*) cinfo->src;
	size_t nbytes;

	nbytes = src->maxlen - src->currentpos;
	if (nbytes > INPUT_BUF_SIZE)
		nbytes = INPUT_BUF_SIZE;
	memcpy(src->buffer, &src->infile[src->currentpos], nbytes);
	src->currentpos+=nbytes;

	if (nbytes <= 0) {
		if (src->start_of_file)	/* Treat empty input file as fatal error */
		ERREXIT(cinfo, JERR_INPUT_EMPTY);
		WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = false;

	return true;
}


METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  my_source_mgr *src = (my_source_mgr*) cinfo->src;

  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) fill_input_buffer(cinfo);
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}



METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
}


#undef GLOBAL
#define GLOBAL(x) x

GLOBAL(void)
ftejpeg_mem_src (j_decompress_ptr cinfo, qbyte * infile, int maxlen)
{
  my_source_mgr *src;

  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(my_source_mgr));
    src = (my_source_mgr*) cinfo->src;
    src->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  INPUT_BUF_SIZE * sizeof(JOCTET));
  }

  src = (my_source_mgr*) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  #ifdef DYNAMIC_LIBJPEG
  	src->pub.resync_to_restart = qjpeg_resync_to_restart; /* use default method */
  #else
  	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  #endif
  src->pub.term_source = term_source;
  src->infile = infile;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */

  src->currentpos = 0;
  src->maxlen = maxlen;
}

qbyte *ReadJPEGFile(qbyte *infile, int length, int *width, int *height)
{
	qbyte *mem=NULL, *in, *out;
	int i;

	/* This struct contains the JPEG decompression parameters and pointers to
	* working space (which is allocated as needed by the JPEG library).
	*/
	struct jpeg_decompress_struct cinfo;
	/* We use our private extension JPEG error handler.
	* Note that this struct must live as long as the main JPEG parameter
	* struct, to avoid dangling-pointer problems.
	*/
	struct my_error_mgr jerr;
	/* More stuff */
	JSAMPARRAY buffer;		/* Output row buffer */
	int size_stride;		/* physical row width in output buffer */

	if (!LIBJPEG_LOADED())
		return NULL;

	/* Step 1: allocate and initialize JPEG decompression object */

	/* We set up the normal JPEG error routines, then override error_exit. */
	#ifdef DYNAMIC_LIBJPEG
		cinfo.err = qjpeg_std_error(&jerr.pub);
	#else
		cinfo.err = jpeg_std_error(&jerr.pub);
	#endif
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer))
	{
		// If we get here, the JPEG code has signaled an error.
badjpeg:
		#ifdef DYNAMIC_LIBJPEG
			qjpeg_destroy_decompress(&cinfo);
		#else
			jpeg_destroy_decompress(&cinfo);
		#endif

		if (mem)
			BZ_Free(mem);
		return 0;
	}
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_create_decompress(&cinfo);
	#else
		jpeg_create_decompress(&cinfo);
	#endif

	ftejpeg_mem_src(&cinfo, infile, length);

	#ifdef DYNAMIC_LIBJPEG
		(void) qjpeg_read_header(&cinfo, true);
	#else
		(void) jpeg_read_header(&cinfo, true);
	#endif

	#ifdef DYNAMIC_LIBJPEG
		(void) qjpeg_start_decompress(&cinfo);
	#else
		(void) jpeg_start_decompress(&cinfo);
	#endif


	if (cinfo.output_components == 0)
	{
		#ifdef _DEBUG
		Con_Printf("No JPEG Components, not a JPEG.\n");
		#endif
		goto badjpeg;
	}
	if (cinfo.output_components!=3 && cinfo.output_components != 1)
	{
		#ifdef _DEBUG
		Con_Printf("Bad number of components in JPEG: '%d', should be '3'.\n",cinfo.output_components);
		#endif
		goto badjpeg;
	}
	size_stride = cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */
	buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, size_stride, 1);

	out=mem=BZ_Malloc(cinfo.output_height*cinfo.output_width*4);
	memset(out, 0, cinfo.output_height*cinfo.output_width*4);

	if (cinfo.output_components == 1)
	{
		while (cinfo.output_scanline < cinfo.output_height)
		{
			#ifdef DYNAMIC_LIBJPEG
				(void) qjpeg_read_scanlines(&cinfo, buffer, 1);
			#else
				(void) jpeg_read_scanlines(&cinfo, buffer, 1);
			#endif

			in = buffer[0];
			for (i = 0; i < cinfo.output_width; i++)
			{//rgb to rgba
				*out++ = *in;
				*out++ = *in;
				*out++ = *in;
				*out++ = 255;
				in++;
			}
		}
	}
	else
	{
		while (cinfo.output_scanline < cinfo.output_height)
		{
			#ifdef DYNAMIC_LIBJPEG
				(void) qjpeg_read_scanlines(&cinfo, buffer, 1);
			#else
				(void) jpeg_read_scanlines(&cinfo, buffer, 1);
			#endif

			in = buffer[0];
			for (i = 0; i < cinfo.output_width; i++)
			{//rgb to rgba
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 255;
			}
		}
	}

	#ifdef DYNAMIC_LIBJPEG
		(void) qjpeg_finish_decompress(&cinfo);
	#else
		(void) jpeg_finish_decompress(&cinfo);
	#endif

	#ifdef DYNAMIC_LIBJPEG
		qjpeg_destroy_decompress(&cinfo);
	#else
		jpeg_destroy_decompress(&cinfo);
	#endif

	*width = cinfo.output_width;
	*height = cinfo.output_height;

	return mem;

}
/*end read*/
#ifndef NPFTE
/*begin write*/
#define OUTPUT_BUF_SIZE 4096
typedef struct  {
	struct jpeg_error_mgr pub;

	jmp_buf setjmp_buffer;
} jpeg_error_mgr_wrapper;

typedef struct {
	struct jpeg_destination_mgr pub;

	vfsfile_t *vfs;


	JOCTET  buffer[OUTPUT_BUF_SIZE];		/* start of buffer */
} my_destination_mgr;

METHODDEF(void) init_destination (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}
METHODDEF(JPEG_boolean) empty_output_buffer (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return true;
}
METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE - dest->pub.free_in_buffer);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

void ftejpeg_mem_dest (j_compress_ptr cinfo, vfsfile_t *vfs)
{
	my_destination_mgr *dest;

	if (cinfo->dest == NULL)
	{	/* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)
						(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
						sizeof(my_destination_mgr));
		dest = (my_destination_mgr*) cinfo->dest;
		//    dest->buffer = (JOCTET *)
		//      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
		//				  OUTPUT_BUF_SIZE * sizeof(JOCTET));
	}

	dest = (my_destination_mgr*) cinfo->dest;
	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;
	dest->pub.free_in_buffer = 0; /* forces fill_input_buffer on first read */
	dest->pub.next_output_byte = NULL; /* until buffer loaded */
	dest->vfs = vfs;
}



METHODDEF(void) jpeg_error_exit (j_common_ptr cinfo)
{
  longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}
void screenshotJPEG(char *filename, int compression, qbyte *screendata, int screenwidth, int screenheight)	//input is rgb NOT rgba
{
	qbyte	*buffer;
	vfsfile_t	*outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	if (!LIBJPEG_LOADED())
		return;

	if (!(outfile = FS_OpenVFS(filename, "wb", FS_GAMEONLY)))
	{
		FS_CreatePath (filename, FS_GAME);
		if (!(outfile = FS_OpenVFS(filename, "wb", FS_GAMEONLY)))
		{
			Con_Printf("Error opening %s\n", filename);
			return;
		}
	}

	#ifdef DYNAMIC_LIBJPEG
		cinfo.err = qjpeg_std_error(&jerr.pub);
	#else
		cinfo.err = jpeg_std_error(&jerr.pub);
	#endif
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		#ifdef DYNAMIC_LIBJPEG
			qjpeg_destroy_compress(&cinfo);
		#else
			jpeg_destroy_compress(&cinfo);
		#endif
		VFS_CLOSE(outfile);
		FS_Remove(filename, FS_GAME);
		Con_Printf("Failed to create jpeg\n");
		return;
	}
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_create_compress(&cinfo);
	#else
		jpeg_create_compress(&cinfo);
	#endif

	buffer = screendata;

	ftejpeg_mem_dest(&cinfo, outfile);
	cinfo.image_width = screenwidth;
	cinfo.image_height = screenheight;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_set_defaults(&cinfo);
	#else
		jpeg_set_defaults(&cinfo);
	#endif
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_set_quality (&cinfo, bound(0, compression, 100), true);
	#else
		jpeg_set_quality (&cinfo, bound(0, compression, 100), true);
	#endif
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_start_compress(&cinfo, true);
	#else
		jpeg_start_compress(&cinfo, true);
	#endif

	while (cinfo.next_scanline < cinfo.image_height)
	{
	    *row_pointer = &buffer[(cinfo.image_height - cinfo.next_scanline - 1) * cinfo.image_width * 3];
	    #ifdef DYNAMIC_LIBJPEG
	    	qjpeg_write_scanlines(&cinfo, row_pointer, 1);
	    #else
	    	jpeg_write_scanlines(&cinfo, row_pointer, 1);
	    #endif
	}
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_finish_compress(&cinfo);
	#else
		jpeg_finish_compress(&cinfo);
	#endif
	VFS_CLOSE(outfile);
	#ifdef DYNAMIC_LIBJPEG
		qjpeg_destroy_compress(&cinfo);
	#else
		jpeg_destroy_compress(&cinfo);
	#endif
}
#endif
#endif

#ifndef NPFTE
/*
==============
WritePCXfile
==============
*/
void WritePCXfile (const char *filename, qbyte *data, int width, int height,
	int rowbytes, qbyte *palette, qboolean upload) //data is 8bit.
{
	int		i, j, length;
	pcx_t	*pcx;
	qbyte		*pack;

	pcx = Hunk_TempAlloc (width*height*2+1000);
	if (pcx == NULL)
	{
		Con_Printf("SCR_ScreenShot_f: not enough memory\n");
		return;
	}

	pcx->manufacturer = 0x0a;	// PCX id
	pcx->version = 5;			// 256 color
 	pcx->encoding = 1;		// uncompressed
	pcx->bits_per_pixel = 8;		// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	Q_memset (pcx->palette,0,sizeof(pcx->palette));
	pcx->color_planes = 1;		// chunky image
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);		// not a grey scale
	Q_memset (pcx->filler,0,sizeof(pcx->filler));

// pack the image
	pack = (qbyte *)(pcx+1);

	data += rowbytes * (height - 1);

	for (i=0 ; i<height ; i++)
	{
		for (j=0 ; j<width ; j++)
		{
			if ( (*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}

		data += rowbytes - width;
		data -= rowbytes * 2;
	}

// write the palette
	*pack++ = 0x0c;	// palette ID qbyte
	for (i=0 ; i<768 ; i++)
		*pack++ = *palette++;

// write output file
	length = pack - (qbyte *)pcx;

	if (upload)
		CL_StartUpload((void *)pcx, length);
	else
		COM_WriteFile (filename, pcx, length);
}
#endif


/*
============
LoadPCX
============
*/
qbyte *ReadPCXFile(qbyte *buf, int length, int *width, int *height)
{
	pcx_t	*pcx;
//	pcx_t pcxbuf;
	qbyte	*palette;
	qbyte	*pix;
	int		x, y;
	int		dataByte, runLength;
	int		count;
	qbyte *data;

	qbyte	*pcx_rgb;

	unsigned short xmin, ymin, swidth, sheight;

//
// parse the PCX file
//

	pcx = (pcx_t *)buf;

	xmin = LittleShort(pcx->xmin);
	ymin = LittleShort(pcx->ymin);
	swidth = LittleShort(pcx->xmax)-xmin+1;
	sheight = LittleShort(pcx->ymax)-ymin+1;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| swidth >= 1024
		|| sheight >= 1024)
	{
		return NULL;
	}

	*width = swidth;
	*height = sheight;

#ifndef NPFTE
	if (r_dodgypcxfiles.value)
		palette = host_basepal;
	else
#endif
		palette = buf + length-768;

	data = (char *)(pcx+1);

	count = (swidth) * (sheight);
	pcx_rgb = BZ_Malloc( count * 4);

	for (y=0 ; y<sheight ; y++)
	{
		pix = pcx_rgb + 4*y*(swidth);
		for (x=0 ; x<swidth ; )
		{
			dataByte = *data;
			data++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (x+runLength>swidth)
				{
					Con_Printf("corrupt pcx\n");
					BZ_Free(pcx_rgb);
					return NULL;
				}
				dataByte = *data;
				data++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				pix[0] = palette[dataByte*3];
				pix[1] = palette[dataByte*3+1];
				pix[2] = palette[dataByte*3+2];
				pix[3] = 255;
				if (dataByte == 255)
					pix[3] = 0;
				pix += 4;
				x++;
			}
		}
	}

	return pcx_rgb;
}

qbyte *ReadPCXData(qbyte *buf, int length, int width, int height, qbyte *result)
{
	pcx_t	*pcx;
//	pcx_t pcxbuf;
	qbyte	*palette;
	qbyte	*pix;
	int		x, y;
	int		dataByte, runLength;
	int		count;
	qbyte *data;

	unsigned short xmin, ymin, swidth, sheight;

//
// parse the PCX file
//

	pcx = (pcx_t *)buf;

	xmin = LittleShort(pcx->xmin);
	ymin = LittleShort(pcx->ymin);
	swidth = LittleShort(pcx->xmax)-xmin+1;
	sheight = LittleShort(pcx->ymax)-ymin+1;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8)
	{
		return NULL;
	}

	if (width != swidth ||
		height > sheight)
	{
		Con_Printf("unsupported pcx size\n");
		return NULL;	//we can't feed the requester with enough info
	}


	palette = buf + length-768;

	data = (char *)(pcx+1);

	count = (swidth) * (sheight);

	for (y=0 ; y<height ; y++)
	{
		pix = result + y*swidth;
		for (x=0 ; x<swidth ; )
		{
			dataByte = *data;
			data++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (x+runLength>swidth)
				{
					Con_Printf("corrupt pcx\n");
					return NULL;
				}
				dataByte = *data;
				data++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				*pix++ = dataByte;
				x++;
			}
		}
	}

	return result;
}

qbyte *ReadPCXPalette(qbyte *buf, int len, qbyte *out)
{
	pcx_t	*pcx;


//
// parse the PCX file
//

	pcx = (pcx_t *)buf;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| LittleShort(pcx->xmax) >= 1024
		|| LittleShort(pcx->ymax) >= 1024)
	{
		return NULL;
	}

	memcpy(out, (qbyte *)pcx + len - 768, 768);

	return out;
}


typedef struct bmpheader_s
{
/*	unsigned short	Type;*/
	unsigned long	Size;
	unsigned short	Reserved1;
	unsigned short	Reserved2;
	unsigned long	OffsetofBMPBits;
	unsigned long	SizeofBITMAPINFOHEADER;
	signed long		Width;
	signed long		Height;
	unsigned short	Planes;
	unsigned short	BitCount;
	unsigned long	Compression;
	unsigned long	ImageSize;
	signed long		TargetDeviceXRes;
	signed long		TargetDeviceYRes;
	unsigned long	NumofColorIndices;
	unsigned long	NumofImportantColorIndices;
} bmpheader_t;

qbyte *ReadBMPFile(qbyte *buf, int length, int *width, int *height)
{
	unsigned int i;
	bmpheader_t h;
	qbyte *data;

	if (buf[0] != 'B' || buf[1] != 'M')
		return NULL;

	memcpy(&h, (bmpheader_t *)(buf+2), sizeof(h));	
	h.Size = LittleLong(h.Size);
	h.Reserved1 = LittleShort(h.Reserved1);
	h.Reserved2 = LittleShort(h.Reserved2);
	h.OffsetofBMPBits = LittleLong(h.OffsetofBMPBits);
	h.SizeofBITMAPINFOHEADER = LittleLong(h.SizeofBITMAPINFOHEADER);
	h.Width = LittleLong(h.Width);
	h.Height = LittleLong(h.Height);
	h.Planes = LittleShort(h.Planes);
	h.BitCount = LittleShort(h.BitCount);
	h.Compression = LittleLong(h.Compression);
	h.ImageSize = LittleLong(h.ImageSize);
	h.TargetDeviceXRes = LittleLong(h.TargetDeviceXRes);
	h.TargetDeviceYRes = LittleLong(h.TargetDeviceYRes);
	h.NumofColorIndices = LittleLong(h.NumofColorIndices);
	h.NumofImportantColorIndices = LittleLong(h.NumofImportantColorIndices);

	if (h.Compression)	//probably RLE?
		return NULL;

	*width = h.Width;
	*height = h.Height;

	if (h.NumofColorIndices != 0 || h.BitCount == 8)	//8 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[256];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>256)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255/*data[i*4+3]*/<<16);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data32[i] = pal[buf[x]];
				i++;
			}
			buf += h.Width;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 4)	//4 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[16];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>16)
			return NULL;
		if (h.Width&1)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255/*data[i*4+3]*/<<16);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width/2; x++)
			{
				data32[i++] = pal[buf[x]>>4];
				data32[i++] = pal[buf[x]&15];
			}
			buf += h.Width>>1;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 24)	//24 bit... no 16?
	{
		int x, y;
		buf += h.OffsetofBMPBits;
		data = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data[i*4+0] = buf[x*3+2];
				data[i*4+1] = buf[x*3+1];
				data[i*4+2] = buf[x*3+0];
				data[i*4+3] = 255;
				i++;
			}
			buf += h.Width*3;
		}

		return data;
	}
	else
		return NULL;

	return NULL;
}

/*void WriteBMPFile(char *filename, qbyte *in, int width, int height)
{
	unsigned int i;
	bmpheader_t *h;
	qbyte *data;
	qbyte *out;

	out = BZ_Malloc(sizeof(bmpheader_t)+width*3*height);



	*(short*)((qbyte *)h-2) = *(short*)"BM";
	h->Size = LittleLong(in->Size);
	h->Reserved1 = LittleShort(in->Reserved1);
	h->Reserved2 = LittleShort(in->Reserved2);
	h->OffsetofBMPBits = LittleLong(in->OffsetofBMPBits);
	h->SizeofBITMAPINFOHEADER = LittleLong(in->SizeofBITMAPINFOHEADER);
	h->Width = LittleLong(in->Width);
	h->Height = LittleLong(in->Height);
	h->Planes = LittleShort(in->Planes);
	h->BitCount = LittleShort(in->BitCount);
	h->Compression = LittleLong(in->Compression);
	h->ImageSize = LittleLong(in->ImageSize);
	h->TargetDeviceXRes = LittleLong(in->TargetDeviceXRes);
	h->TargetDeviceYRes = LittleLong(in->TargetDeviceYRes);
	h->NumofColorIndices = LittleLong(in->NumofColorIndices);
	h->NumofImportantColorIndices = LittleLong(in->NumofImportantColorIndices);

	if (h.Compression)	//probably RLE?
		return NULL;

	*width = h.Width;
	*height = h.Height;

	if (h.NumofColorIndices != 0 || h.BitCount == 8)	//8 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[256];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>256)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255/<<16);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data32[i] = pal[buf[x]];
				i++;
			}
			buf += h.Width;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 4)	//4 bit
	{
		int x, y;
		unsigned int *data32;
		unsigned int	pal[16];
		if (!h.NumofColorIndices)
			h.NumofColorIndices = (int)pow(2, h.BitCount);
		if (h.NumofColorIndices>16)
			return NULL;
		if (h.Width&1)
			return NULL;

		data = buf+2;
		data += sizeof(h);

		for (i = 0; i < h.NumofColorIndices; i++)
		{
			pal[i] = data[i*4+0] + (data[i*4+1]<<8) + (data[i*4+2]<<16) + (255<<16);
		}

		buf += h.OffsetofBMPBits;
		data32 = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width/2; x++)
			{
				data32[i++] = pal[buf[x]>>4];
				data32[i++] = pal[buf[x]&15];
			}
			buf += h.Width>>1;
		}

		return (qbyte *)data32;
	}
	else if (h.BitCount == 24)	//24 bit... no 16?
	{
		int x, y;
		buf += h.OffsetofBMPBits;
		data = BZ_Malloc(h.Width * h.Height*4);
		for (y = 0; y < h.Height; y++)
		{
			i = (h.Height-1-y) * (h.Width);
			for (x = 0; x < h.Width; x++)
			{
				data[i*4+0] = buf[x*3+2];
				data[i*4+1] = buf[x*3+1];
				data[i*4+2] = buf[x*3+0];
				data[i*4+3] = 255;
				i++;
			}
			buf += h.Width*3;
		}

		return data;
	}
	else
		return NULL;

	return NULL;
}*/


#ifndef NPFTE


// saturate function, stolen from jitspoe
void SaturateR8G8B8(qbyte *data, int size, float sat)
{
	int i;
	float r, g, b, v;

	if (sat > 1)
	{
		for(i=0; i < size; i+=3)
		{
			r = data[i];
			g = data[i+1];
			b = data[i+2];

			v = r * NTSC_RED + g * NTSC_GREEN + b * NTSC_BLUE;
			r = v + (r - v) * sat;
			g = v + (g - v) * sat;
			b = v + (b - v) * sat;

			// bounds check
			if (r < 0)
				r = 0;
			else if (r > 255)
				r = 255;

			if (g < 0)
				g = 0;
			else if (g > 255)
				g = 255;

			if (b < 0)
				b = 0;
			else if (b > 255)
				b = 255;

			// scale down to avoid overbright lightmaps
			v = v / (r * NTSC_RED + g * NTSC_GREEN + b * NTSC_BLUE);
			if (v > NTSC_SUM)
				v = NTSC_SUM;
			else
				v *= v;

			data[i]   = r*v;
			data[i+1] = g*v;
			data[i+2] = b*v;
		}
	}
	else // avoid bounds check for desaturation
	{
		if (sat < 0)
			sat = 0;

		for(i=0; i < size; i+=3)
		{
			r = data[i];
			g = data[i+1];
			b = data[i+2];

			v = r * NTSC_RED + g * NTSC_GREEN + b * NTSC_BLUE;

			data[i]   = v + (r - v) * sat;
			data[i+1] = v + (g - v) * sat;
			data[i+2] = v + (b - v) * sat;
		}
	}
}

void BoostGamma(qbyte *rgba, int width, int height)
{
#if defined(GLQUAKE)
	int i;
	extern qbyte gammatable[256];

	if (qrenderer != QR_OPENGL)
		return;//don't brighten in SW.

	for (i=0 ; i<width*height*4 ; i+=4)
	{
		rgba[i+0] = gammatable[rgba[i+0]];
		rgba[i+1] = gammatable[rgba[i+1]];
		rgba[i+2] = gammatable[rgba[i+2]];
		//and not alpha
	}
#endif
}







#if defined(GLQUAKE) || defined(D3DQUAKE)

#ifdef DDS
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                  0x83F3
#endif

typedef struct {
	unsigned int dwSize;
	unsigned int dwFlags;
	unsigned int dwFourCC;

	unsigned int unk[5];
} ddspixelformat;

typedef struct {
	unsigned int dwSize;
	unsigned int dwFlags;
	unsigned int dwHeight;
	unsigned int dwWidth;
	unsigned int dwPitchOrLinearSize;
	unsigned int dwDepth;
	unsigned int dwMipMapCount;
	unsigned int dwReserved1[11];
	ddspixelformat ddpfPixelFormat;
	unsigned int ddsCaps[4];
	unsigned int dwReserved2;
} ddsheader;


texid_tf GL_LoadTextureDDS(char *iname, unsigned char *buffer, int filesize)
{
	extern int		gl_filter_min;
	extern int		gl_filter_max;
	texid_t texnum;
	int nummips;
	int mipnum;
	int datasize;
	int intfmt;
	int pad;
	unsigned int w, h;
	int divsize, blocksize;

	ddsheader fmtheader;
	if (*(int*)buffer != *(int*)"DDS ")
		return r_nulltex;
	buffer+=4;

	memcpy(&fmtheader, buffer, sizeof(fmtheader));
	if (fmtheader.dwSize != sizeof(fmtheader))
		return r_nulltex;	//corrupt/different version

	buffer += fmtheader.dwSize;

	nummips = fmtheader.dwMipMapCount;
	if (nummips < 1)
		nummips = 1;

	if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT1")
	{
		intfmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;	//alpha or not? Assume yes, and let the drivers decide.
		pad = 8;
		divsize = 4;
		blocksize = 8;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT2" || *(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT3")
	{
		intfmt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		pad = 8;
		divsize = 4;
		blocksize = 16;
	}
	else if (*(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT4" || *(int*)&fmtheader.ddpfPixelFormat.dwFourCC == *(int*)"DXT5")
	{
		intfmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		pad = 8;
		divsize = 4;
		blocksize = 16;
	}
	else
		return r_nulltex;

	if (!qglCompressedTexImage2DARB)
		return r_nulltex;

	texnum = GL_AllocNewTexture(iname, fmtheader.dwWidth, fmtheader.dwHeight, 0);
	GL_MTBind(0, GL_TEXTURE_2D, texnum);

	datasize = fmtheader.dwPitchOrLinearSize;
	w = fmtheader.dwWidth;
	h = fmtheader.dwHeight;
	for (mipnum = 0; mipnum < nummips; mipnum++)
	{
//	(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data);
		if (datasize < pad)
			datasize = pad;
		datasize = max(divsize, w)/divsize * max(divsize, h)/divsize * blocksize;
		qglCompressedTexImage2DARB(GL_TEXTURE_2D, mipnum, intfmt, w, h, 0, datasize, buffer);
		if (qglGetError())
			Con_Printf("Incompatible dds file %s (mip %i)\n", iname, mipnum);
		buffer += datasize;
		w = (w+1)>>1;
		h = (h+1)>>1;
	}
	if (qglGetError())
		Con_Printf("Incompatible dds file %s\n", iname);


	if (nummips>1)
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	return texnum;
}
#endif

//returns r8g8b8a8
qbyte *Read32BitImageFile(qbyte *buf, int len, int *width, int *height, qboolean *hasalpha, char *fname)
{
	qbyte *data;
	if ((data = ReadTargaFile(buf, len, width, height, hasalpha, false)))
	{
		TRACE(("dbg: Read32BitImageFile: tga\n"));
		return data;
	}

#ifdef AVAIL_PNGLIB
	if ((buf[0] == 137 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') && (data = ReadPNGFile(buf, com_filesize, width, height, fname)))
	{
		TRACE(("dbg: Read32BitImageFile: png\n"));
		return data;
	}
#endif
#ifdef AVAIL_JPEGLIB
	//jpeg jfif only.
	if ((buf[0] == 0xff && buf[1] == 0xd8 && buf[2] == 0xff && buf[3] == 0xe0) && (data = ReadJPEGFile(buf, com_filesize, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: jpeg\n"));
		return data;
	}
#endif
	if ((data = ReadPCXFile(buf, com_filesize, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: pcx\n"));
		return data;
	}

	if ((buf[0] == 'B' && buf[1] == 'M') && (data = ReadBMPFile(buf, com_filesize, width, height)))
	{
		TRACE(("dbg: Read32BitImageFile: bitmap\n"));
		return data;
	}

	TRACE(("dbg: Read32BitImageFile: life sucks\n"));

	return NULL;
}

static struct
{
	char *name;
	int enabled;
} tex_extensions[] =
{//reverse order of preference - (match commas with optional file types)
	{".pcx", 1},	//pcxes are the original gamedata of q2. So we don't want them to override pngs.
#ifdef AVAIL_JPEGLIB
	{".jpg", 1},	//q3 uses some jpegs, for some reason
#endif
	{".bmp", 0},	//wtf? at least not lossy
#ifdef AVAIL_PNGLIB
	{".png", 1},	//pngs, fairly common, but slow
#endif
	{".tga", 1},	//fairly fast to load
#ifdef DDS
	{".dds", 1},	//compressed or something
#endif
	{"", 1}			//someone forgot an extension
};

static struct
{
	int args;
	char *path;

	int enabled;
} tex_path[] =
{
	/*if three args, first is the subpath*/
	/*the last two args are texturename then extension*/
	{2, "%s%s", 1},				/*directly named texture*/
	{3, "textures/%s/%s%s", 1},	/*fuhquake compatibility*/
	{3, "%s/%s%s", 1},			/*fuhquake compatibility*/
	{2, "textures/%s%s", 1},	/*directly named texture with textures/ prefix*/
	{2, "override/%s%s", 1}		/*tenebrae compatibility*/
};

int image_width, image_height;
texid_t R_LoadHiResTexture(char *name, char *subpath, unsigned int flags)
{
	qboolean alphaed;
	char *buf;
	unsigned char *data;
	texid_t tex;
//	int h;
	char fname[MAX_QPATH], nicename[MAX_QPATH];
	qboolean hasalpha;

	int i, e;

	if (!*name)
		return r_nulltex;

	image_width = 0;
	image_height = 0;

	COM_StripExtension(name, nicename, sizeof(nicename));

	while((data = strchr(nicename, '*')))
	{
		*data = '#';
	}

	snprintf(fname, sizeof(fname)-1, "%s/%s", subpath, name); /*should be safe if its null*/
	if (subpath && *subpath && !(flags & IF_REPLACE))
	{
		tex = R_FindTexture(fname, flags);
		if (TEXVALID(tex))	//don't bother if it already exists.
			return tex;
	}
	if (!(flags & IF_SUBDIRONLY) && !(flags & IF_REPLACE))
	{
		tex = R_FindTexture(name, flags);
		if (TEXVALID(tex))	//don't bother if it already exists.
			return tex;
	}

	if ((flags & IF_TEXTYPE) == IF_CUBEMAP)
	{
		int j;
		char *suf[] =
		{
			"rt", "lf", "ft", "bk", "up", "dn",	//FIXME: This is inverted and thus wrong
			"px", "nx", "py", "ny", "pz", "nz",
			"posx", "negx", "posy", "negy", "posz", "negz"
		};
		flags |= IF_REPLACE;

		tex = r_nulltex;
		for (i = 0; i < 6; i++)
		{
			tex = r_nulltex;
			for (e = sizeof(tex_extensions)/sizeof(tex_extensions[0])-1; e >=0 ; e--)
			{
				if (!tex_extensions[e].enabled)
					continue;

				buf = NULL;
				for (j = 0; j < sizeof(suf)/sizeof(suf[0])/6; j++)
				{
					snprintf(fname, sizeof(fname)-1, "%s%s%s", nicename, suf[i + 6*j], tex_extensions[e].name);
					buf = COM_LoadFile (fname, 5);
					if (buf)
						break;
				}

				if (buf)
				{
					hasalpha = false;
					if ((data = Read32BitImageFile(buf, com_filesize, &image_width, &image_height, &hasalpha, fname)))
					{
						extern cvar_t vid_hardwaregamma;
						if (!(flags&IF_NOGAMMA) && !vid_hardwaregamma.value)
							BoostGamma(data, image_width, image_height);
						tex = R_LoadTexture32 (name, image_width, image_height, data, (flags | IF_REPLACE) + (i << IF_TEXTYPESHIFT));

						BZ_Free(data);
						BZ_Free(buf);
						if (TEXVALID(tex))
							break;
					}
				}
			}
			if (!TEXVALID(tex))
				return r_nulltex;
		}
		return tex;
	}


	if (subpath && *subpath)
	{
		tex = R_LoadCompressed(fname);
		if (TEXVALID(tex))
			return tex;
	}
	if (!(flags & IF_SUBDIRONLY))
	{
		tex = R_LoadCompressed(name);
		if (TEXVALID(tex))
			return tex;
	}

#ifdef DDS
	snprintf(fname, sizeof(fname)-1, "dds/%s.dds", nicename); /*should be safe if its null*/
	if ((buf = COM_LoadFile (fname, 5)))
	{
		tex = GL_LoadTextureDDS(name, buf, com_filesize);
		if (TEXVALID(tex))
		{
			BZ_Free(buf);
			return tex;
		}
		BZ_Free(buf);
	}
#endif

	if (strchr(name, '/'))	//never look in a root dir for the pic
		i = 0;
	else
		i = 1;

	//should write this nicer.
	for (; i < sizeof(tex_path)/sizeof(tex_path[0]); i++)
	{
		if (!tex_path[i].enabled)
			continue;
		for (e = sizeof(tex_extensions)/sizeof(tex_extensions[0])-1; e >=0 ; e--)
		{
			if (!tex_extensions[e].enabled)
				continue;

			if (tex_path[i].args >= 3)
			{
				if (!subpath)
					continue;
				snprintf(fname, sizeof(fname)-1, tex_path[i].path, subpath, nicename, tex_extensions[e].name);
			}
			else
			{
				if (flags & IF_SUBDIRONLY)
					continue;
				snprintf(fname, sizeof(fname)-1, tex_path[i].path, nicename, tex_extensions[e].name);
			}
			TRACE(("dbg: Mod_LoadHiResTexture: trying %s\n", fname));
			if ((buf = COM_LoadFile (fname, 5)))
			{
#ifdef DDS
				tex = GL_LoadTextureDDS(name, buf, com_filesize);
				if (TEXVALID(tex))
				{
					BZ_Free(buf);
					return tex;
				}
#endif
				hasalpha = false;
				if ((data = Read32BitImageFile(buf, com_filesize, &image_width, &image_height, &hasalpha, fname)))
				{
					extern cvar_t vid_hardwaregamma;
					if (!(flags&IF_NOGAMMA) && !vid_hardwaregamma.value)
						BoostGamma(data, image_width, image_height);

					if (hasalpha)
						flags &= ~IF_NOALPHA;
					else if (!(flags & IF_NOALPHA))
					{
						unsigned int alpha_width, alpha_height, p;
						char aname[MAX_QPATH];
						unsigned char *alphadata;
						char *alph;
						if (tex_path[i].args >= 3)
							snprintf(aname, sizeof(aname)-1, tex_path[i].path, subpath, nicename, va("_alpha%s", tex_extensions[e].name));
						else
							snprintf(aname, sizeof(aname)-1, tex_path[i].path, nicename, va("_alpha%s", tex_extensions[e].name));
						if ((alph = COM_LoadFile (aname, 5)))
						{
							if ((alphadata = Read32BitImageFile(alph, com_filesize, &alpha_width, &alpha_height, &hasalpha, aname)))
							{
								if (alpha_width == image_width && alpha_height == image_height)
								{
									for (p = 0; p < alpha_width*alpha_height; p++)
									{
										data[(p<<2) + 3] = (alphadata[(p<<2) + 0] + alphadata[(p<<2) + 1] + alphadata[(p<<2) + 2])/3;
									}
								}
								BZ_Free(alphadata);
							}
							BZ_Free(alph);
						}
					}


					TRACE(("dbg: Mod_LoadHiResTexture: %s loaded\n", name));
					if (tex_path[i].args >= 3)
					{	//if it came from a special subpath (eg: map specific), upload it using the subpath prefix
						snprintf(fname, sizeof(fname)-1, "%s/%s", subpath, name);
						tex = R_LoadTexture32 (fname, image_width, image_height, data, flags);
					}
					else
						tex = R_LoadTexture32 (name, image_width, image_height, data, flags);

					BZ_Free(data);

					BZ_Free(buf);

					return tex;
				}
				else
				{
					Con_Printf("Unable to read file %s (format unsupported)\n", fname);
					BZ_Free(buf);
					continue;
				}
			}
		}
	}

	if (!(flags & IF_SUBDIRONLY))
	{
		/*still failed? attempt to load quake lmp files, which have no real format id*/
		snprintf(fname, sizeof(fname)-1, "%s%s", nicename, ".lmp");
		if ((buf = COM_LoadFile (fname, 5)))
		{
			extern cvar_t vid_hardwaregamma;
			TEXASSIGNF(tex, r_nulltex);
			if (com_filesize >= 8)
			{
				image_width = LittleLong(((int*)buf)[0]);
				image_height = LittleLong(((int*)buf)[1]);
				if (image_width*image_height+8 == com_filesize)
				{
					tex = R_LoadTexture8(name, image_width, image_height, buf+8, flags, 1);
				}
			}
			BZ_Free(buf);
			return tex;
		}

		//now look in wad files. (halflife compatability)
		data = W_GetTexture(name, &image_width, &image_height, &alphaed);
		if (data)
		{
			tex = R_LoadTexture32 (name, image_width, image_height, (unsigned*)data, flags);
			BZ_Free(data);
			return tex;
		}
	}
	return r_nulltex;
}
texid_t R_LoadReplacementTexture(char *name, char *subpath, unsigned int flags)
{
	if (!gl_load24bit.value)
		return r_nulltex;
	return R_LoadHiResTexture(name, subpath, flags);
}

extern cvar_t r_shadow_bumpscale_bumpmap;
texid_t R_LoadBumpmapTexture(char *name, char *subpath)
{
	char *buf, *data;
	texid_t tex;
//	int h;
	char fname[MAX_QPATH], nicename[MAX_QPATH];
	qboolean hasalpha;

	static char *extensions[] =
	{//reverse order of preference - (match commas with optional file types)
		".tga",
		""
	};

	int i, e;

	TRACE(("dbg: Mod_LoadBumpmapTexture: texture %s\n", name));

	COM_StripExtension(name, nicename, sizeof(nicename));

	tex = R_FindTexture(name, 0);
	if (TEXVALID(tex))	//don't bother if it already exists.
		return tex;

	tex = R_LoadCompressed(name);
	if (TEXVALID(tex))
		return tex;

	if (strchr(name, '/'))	//never look in a root dir for the pic
		i = 0;
	else
		i = 1;

	//should write this nicer.
	for (; i < sizeof(tex_path)/sizeof(tex_path[0]); i++)
	{
		if (!tex_path[i].enabled)
			continue;
		for (e = sizeof(extensions)/sizeof(char *)-1; e >=0 ; e--)
		{
			if (tex_path[i].args >= 3)
			{
				if (!subpath)
					continue;
				snprintf(fname, sizeof(fname)-1, tex_path[i].path, subpath, nicename, extensions[e]);
			}
			else
				snprintf(fname, sizeof(fname)-1, tex_path[i].path, nicename, extensions[e]);

			TRACE(("dbg: Mod_LoadBumpmapTexture: opening %s\n", fname));

			if ((buf = COM_LoadFile (fname, 5)))
			{
				if ((data = ReadTargaFile(buf, com_filesize, &image_width, &image_height, &hasalpha, 2)))	//Only load a greyscale image.
				{
					TRACE(("dbg: Mod_LoadBumpmapTexture: tga %s loaded\n", name));
					TEXASSIGNF(tex, R_LoadTexture8Bump(name, image_width, image_height, data, IF_NOALPHA|IF_NOGAMMA));
					BZ_Free(data);
				}
				else
				{
					BZ_Free(buf);
					continue;
				}

				BZ_Free(buf);

				return tex;
			}
		}
	}
	return r_nulltex;
}

#endif

// ocrana led functions
static int ledcolors[8][3] =
{
	// green
	{ 0, 255, 0 },
	{ 0, 127, 0 },
	// red
	{ 255, 0, 0 },
	{ 127, 0, 0 },
	// yellow
	{ 255, 255, 0 },
	{ 127, 127, 0 },
	// blue
	{ 0, 0, 255 },
	{ 0, 0, 127 }
};

void AddOcranaLEDsIndexed (qbyte *image, int h, int w)
{
	int tridx[8]; // transition indexes
	qbyte *point;
	int i, idx, x, y, rs;
	int r, g, b, rd, gd, bd;

	// calc row size, character size
	rs = w;
	h /= 16;
	w /= 16;

	// generate palettes
	for (i = 0; i < 4; i++)
	{
		// get palette
		r = ledcolors[i*2][0];
		g = ledcolors[i*2][1];
		b = ledcolors[i*2][2];
		rd = (r - ledcolors[i*2+1][0]) / 8;
		gd = (g - ledcolors[i*2+1][1]) / 8;
		bd = (b - ledcolors[i*2+1][2]) / 8;
		for (idx = 0; idx < 8; idx++)
		{
			tridx[idx] = GetPaletteIndex(r, g, b);
			r -= rd;
			g -= gd;
			b -= bd;
		}

		// generate LED into image
		b = (w * w + h * h) / 16;
		if (b < 1)
			b = 1;
		rd = w + 1;
		gd = h + 1;

		point = image + (8 * rs * h) + ((6 + i) * w);
		for (y = 1; y <= h; y++)
		{
			for (x = 1; x <= w; x++)
			{
				r = rd - (x*2); r *= r;
				g = gd - (y*2); g *= g;
				idx = (r + g) / b;

				if (idx > 7)
					*point++ = 0;
				else
					*point++ = tridx[idx];
			}
			point += rs - w;
		}
	}
}
#endif
