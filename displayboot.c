#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#define MAX_GLYPHS 255

// 340 x 800 x 4 (RGB888A)
#define FBSIZE_Y 340
#define FBSIZE_X 800
#define FBSIZE (FBSIZE_Y * FBSIZE_X * 4)

uint8_t fb[FBSIZE];
uint8_t background[FBSIZE];

static inline uint32_t get_pixel_pos(uint16_t x, uint16_t y)
{
	// Implicit rotation!
	uint16_t rot_x = y;
	uint16_t rot_y = FBSIZE_X - x; 

	return ((rot_y * FBSIZE_Y) + rot_x) * 4;
}

static inline void draw_pixel (uint16_t x, uint16_t y, uint32_t color)
{
	uint32_t offset = get_pixel_pos(x, y);
	if (offset > FBSIZE)
	{
		return; 
	}
	*(uint32_t*)(fb + offset) = color; 
}

char mytext[] = "Loading...";


FT_Library ft;
FT_Error err;
FT_Face face;

void init_ft()
{
	err = FT_Init_FreeType(&ft);
	if (err != 0) {
	  printf("Failed to initialize FreeType\n");
	  exit(EXIT_FAILURE);
	}

	err = FT_New_Face(ft, "./UbuntuMono.ttf", 0, &face);
	if (err != 0) {
	  printf("Failed to load face\n");
	  exit(EXIT_FAILURE);
	}

	err = FT_Set_Pixel_Sizes(face, 0, 32);
	if (err != 0) {
	  printf("Failed to set pixel size\n");
	  exit(EXIT_FAILURE);
	}

}

void draw_string(uint16_t pen_x, uint16_t pen_y, char* text, uint16_t len)
{
	for (int cindex = 0; cindex < len; ++cindex)
	{
		FT_UInt glyph_index = FT_Get_Char_Index(face, text[cindex]);

		FT_Int32 load_flags = FT_LOAD_DEFAULT;
		err = FT_Load_Glyph(face, glyph_index, load_flags);
		if (err != 0)
		{
			printf("Failed to load glyph\n");
			exit(EXIT_FAILURE);
		}

		FT_Int32 render_flags = FT_RENDER_MODE_NORMAL;
		err = FT_Render_Glyph(face->glyph, render_flags);
		if (err != 0)
		{
			printf("Failed to render the glyph\n");
			exit(EXIT_FAILURE);
		}

		for (size_t i = 0; i < face->glyph->bitmap.rows; i++)
		{
			for (size_t j = 0; j < face->glyph->bitmap.width; j++)
			{
				unsigned char pixel_brightness = face->glyph->bitmap.buffer[i * face->glyph->bitmap.pitch + j];

				if (pixel_brightness > 0)
				{
					uint32_t color = 0; 
					memset(&color, pixel_brightness, sizeof(color));

					draw_pixel (pen_x + j + face->glyph->bitmap_left, pen_y + i - face->glyph->bitmap_top, color);
				}
			}
		}

		pen_x += face->glyph->advance.x >> 6;
	}
}

int main()
{
	FILE *f = fopen("fb.raw", "rb");
	fread(background, 1, sizeof(background), f);
	fclose(f);


	memcpy(fb, background, sizeof(fb));

	init_ft();
	draw_string(330, 200, mytext, sizeof(mytext));
	
	fwrite(fb,1, sizeof(fb), stdout);
}
