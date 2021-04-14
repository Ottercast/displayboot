#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>


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

int fbfd = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
long int screensize = 0;
char *mapped_fb = 0;



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
	  exit(1);
	}

	err = FT_New_Face(ft, "./UbuntuMono.ttf", 0, &face);
	if (err != 0) {
	  printf("Failed to load face\n");
	  exit(1);
	}

	err = FT_Set_Pixel_Sizes(face, 0, 32);
	if (err != 0) {
	  printf("Failed to set pixel size\n");
	  exit(1);
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
			exit(1);
		}

		FT_Int32 render_flags = FT_RENDER_MODE_NORMAL;
		err = FT_Render_Glyph(face->glyph, render_flags);
		if (err != 0)
		{
			printf("Failed to render the glyph\n");
			exit(1);
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

void init_fb()
{
	fbfd = open("/dev/fb0", O_RDWR);
	if (!fbfd) {
		printf("Error: cannot open framebuffer device.\n");
		exit(1);
	}

	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		printf("Error reading fixed information.\n");
		exit(1);
	}

	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("Error reading variable information.\n");
		exit(1);
	}

	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	mapped_fb = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if ((int)mapped_fb == -1)
	{
		printf("Error: failed to map framebuffer device to memory.\n");
		exit(1);
	}
}

int main()
{
	init_fb();
	init_ft();

	FILE *f = fopen("fb.raw", "rb");
	fread(background, 1, sizeof(background), f);
	fclose(f);

	FILE *bindfp = fopen("/sys/class/vtconsole/vtcon1/bind", "w");
	if (bindfp != NULL)
	{
		const char *unbind = "0\n";
		fwrite(unbind, strlen(unbind), 1, bindfp);
		fclose(bindfp);
	}

	memcpy(fb, background, sizeof(fb));
	draw_string(330, 200, "Loading...", 10);
	memcpy(mapped_fb, fb, screensize);

	FILE *journalctlfp;
	char message[1024];
	journalctlfp = popen("journalctl -f -n 0", "r");
	if (journalctlfp == NULL)
	{
		printf("Failed to run journalctl\n" );
		exit(1);
	}

	while (fgets(message, sizeof(message), journalctlfp) != NULL)
	{
		message[sizeof(message) - 1] = 0x00;
		if (strstr(message, "systemd[1]: Started") != NULL)
		{
			char* token = message;

			token = strtok (token, ":");
			for (int i = 0; i < 3; ++i)
			{
				token = strtok (NULL, ":");
			}
			
			if (token != NULL)
			{
				// Skip " Started "
				token += 9; 
				// Skip last newline
				token[strlen(token) - 2] = 0x00;

				//printf("%s\n", token);

				memcpy(fb, background, sizeof(fb));
				draw_string(330, 200, "Starting", 8);
				draw_string(330, 230, token, strlen(token));
				memcpy(mapped_fb, fb, screensize);
			}
			
		}


	
	}

	pclose(journalctlfp);

	munmap(mapped_fb, screensize);
	close(fbfd);
	return 0;
}
