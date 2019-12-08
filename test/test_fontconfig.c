#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <fontconfig/fontconfig.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H


#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <assert.h>

#if defined (__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined (__clang__)
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_ZERO_COMMAND_MEMORY
#include "nuklear.h"


static inline const char *
string_from_fc_weight(int weight)
{
	return (weight < 80) ? "thin" : (weight < 150) ? "regular" : "heavy";
}

static void
print_charsetpage(FcChar32 basepoint, FcChar32 map[FC_CHARSET_MAP_SIZE])
{
	bool inrange = true;
	nk_rune range[101] = {0};
	int range_cursor = 0;
	range[0] = basepoint;


	for (int i = 0; i < FC_CHARSET_MAP_SIZE; i++) {
		for (int j = 0; j < 32; j++) {
			//case 0 continue in range
			if ( (map[i] & (1 << j)) && inrange)
				continue;
			//case 1, breaks
			else if ( !(map[i] & (1 << j)) && inrange) {
				range_cursor++;
				assert((range_cursor % 2) == 1);
				range[range_cursor] = basepoint + i * 32 + j;
				inrange = false;
			}
			//case 2, not in range, continue
			else if ( !(map[i] & (1 << j)) && !inrange)
				continue;
			//case 3, not in range, new char starts
			else if ( (map[i] & (1 << j)) && !inrange) {
				range_cursor++;
				assert((range_cursor % 2) == 0);
				range[range_cursor] = basepoint + i * 32 + j;
				inrange = true;
			}
		}
	}
	if (range_cursor == 0)
		return;
	for (int i = 0; i < range_cursor; i+=2)
		printf("(%d, %d) ", range[i], range[i+1]);
	printf("\n");
}

static void print_pattern(FcPattern *pat)
{
	FcChar8 *s;
	double d;
	FcCharSet *cset;
	int i;

	if (FcPatternGetString(pat, FC_FAMILY, 0, &s) == FcResultMatch)
		fprintf(stdout, "family: %s\n", s);
	else
		fprintf(stdout, "no family.\n");

	if (FcPatternGetString(pat, FC_STYLE, 0, &s) == FcResultMatch)
		fprintf(stdout, "style: %s\n", s);
	else
		fprintf(stdout, "no style\n");

	if (FcPatternGetString(pat, FC_FULLNAME, 0, &s) == FcResultMatch)
		fprintf(stdout, "fullname: %s\n", s);
	else
		fprintf(stdout, "not fullname\n");

	if (FcPatternGetString(pat, FC_FILE, 0, &s) == FcResultMatch)
		fprintf(stdout, "file path:%s\n", s);
	else
		fprintf(stdout, "no file path\n");

	if (FcPatternGetDouble(pat, FC_DPI, 0, &d) == FcResultMatch)
		fprintf(stdout, "dpi: %f\n", d);
	else
		fprintf(stdout, "no dpi\n");

	if (FcPatternGetInteger(pat, FC_SLANT, 0, &i) == FcResultMatch)
		printf("slant: %s\n", i == FC_SLANT_ROMAN ? "roman" :
		       (i == FC_SLANT_ITALIC) ? "italic" : "obique");
	else
		printf("no slant\n");

	if (FcPatternGetInteger(pat, FC_WEIGHT, 0, &i) == FcResultMatch)
		printf("weight : %s\n", string_from_fc_weight(i));
	else
		printf("no weight\n");

	if (FcPatternGetDouble(pat, FC_SIZE, 0, &d) == FcResultMatch)
		fprintf(stdout, "point size: %f\n", d);
	else
		fprintf(stdout, "no point size\n");

	if (FcPatternGetCharSet(pat, FC_CHARSET, 0, &cset) == FcResultMatch) {
		//how to print the charset?
		FcChar32 basepoint = FC_CHARSET_DONE, next;
		FcChar32 map[FC_CHARSET_MAP_SIZE];
		for (basepoint = FcCharSetFirstPage(cset, map, &next);
		     basepoint != FC_CHARSET_DONE;
		     basepoint = FcCharSetNextPage(cset, map, &next))
			print_charsetpage(basepoint, map);
	} else
		printf("no charset\n");
	printf("\n");
}

static void
union_unicode_range(const nk_rune left[2], const nk_rune right[2], nk_rune out[2])
{
	nk_rune tmp[2];
	tmp[0] = left[0] < right[0] ? left[0] : right[0];
	tmp[1] = left[1] > right[1] ? left[1] : right[1];
	out[0] = tmp[0];
	out[1] = tmp[1];
}

//return true if left and right are insersected, else false
static bool
intersect_unicode_range(const nk_rune left[2], const nk_rune right[2])
{
	return (left[0] <= right[1] && left[1] >= right[1]) ||
		(left[0] <= right[0] && left[1] >= right[0]);
}

static int
unicode_range_compare(const void *l, const void *r)
{
	const nk_rune *range_left = (const nk_rune *)l;
	const nk_rune *range_right = (const nk_rune *)r;
	return ((int)range_left[0] - (int)range_right[0]);
}

//we can only merge one range at a time
static int
merge_unicode_range(const nk_rune *left, const nk_rune *right, nk_rune *out)
{
	//get the range
	int left_size = 0;
	while(*(left+left_size)) left_size++;
	int right_size = 0;
	while(*(right+right_size)) right_size++;
	//sort the range,
	nk_rune sorted_ranges[left_size+right_size];
	memcpy(sorted_ranges, left, sizeof(nk_rune) * left_size);
	memcpy(sorted_ranges+left_size, right, sizeof(nk_rune) * right_size);
	qsort(sorted_ranges, (left_size+right_size)/2, sizeof(nk_rune) * 2,
	      unicode_range_compare);
	//merge algorithm
	nk_rune merged[left_size+right_size+1];
	merged[0] = sorted_ranges[0];
	merged[1] = sorted_ranges[1];
	int m = 0;
	for (int i = 0; i < (left_size+right_size) / 2; i++) {
		if (intersect_unicode_range(&sorted_ranges[i*2],
					    &merged[2*m]))
			union_unicode_range(&sorted_ranges[i*2], &merged[2*m],
					    &merged[2*m]);
		else {
			m++;
			merged[2*m] = sorted_ranges[2*i];
			merged[2*m+1] = sorted_ranges[2*i+1];
		}
	}
	m++;
	merged[2*m] = 0;

	if (!out)
		return 2*m;
	memcpy(out, merged, (2*m+1) * sizeof(nk_rune));
	return 2*m;
}

static void
save_the_font_images(const nk_rune *glyph_range)
{
	int w, h;
	struct nk_font_config cfg = nk_font_config(16);
	cfg.range = glyph_range;

	struct nk_font_atlas atlas;
	nk_font_atlas_init_default(&atlas);
	nk_font_atlas_begin(&atlas);
	/* struct nk_font * font = nk_font_atlas_add_from_file( */
	/*	&atlas, */
	/*	"/usr/share/fonts/TTF/Inconsolata-Regular.ttf", */
	/*	16, &cfg); */
	const void *data = nk_font_atlas_bake(&atlas, &w, &h, NK_FONT_ATLAS_ALPHA8);

	printf("we had a texture of size %d, %d\n", w, h);
	//this may not be right...
	cairo_surface_t *image_surface =
		cairo_image_surface_create_for_data((unsigned char*)data,
						    CAIRO_FORMAT_A8,
						    w, h,
						    cairo_format_stride_for_width(CAIRO_FORMAT_A8, w));

	cairo_surface_write_to_png(image_surface, "/tmp/save_as_png.png");
	cairo_surface_destroy(image_surface);
	nk_font_atlas_end(&atlas, nk_handle_id(0), NULL);
	nk_font_atlas_clear(&atlas);
	//I will need to write a allocator for nuklear though, this sucks.
}

//an implementation using cairo
struct nk_wl_user_font {
	int size;
	float scale;
	FT_Face face;
	FT_Library ft_lib;
	cairo_font_face_t *cairo_face;
	struct nk_user_font nk_font;
};

enum nk_wl_font_slant {
	NK_WL_SLANT_ROMAN,
	NK_WL_SLANT_ITALIC,
	NK_WL_SLANT_OBLIQUE,
};

//we accept only true type font
struct nk_wl_font_config {
	const char *name;
	enum nk_wl_font_slant slant;
	//fc config offers light, medium, demibold, bold, black
	//demibold, bold and black is true, otherwise false.
	bool bold; //classified as bold
};

int
main(int argc, char *argv[])
{
	FcInit();
	FcConfig* config = FcInitLoadConfigAndFonts();
	FcPattern* pat = FcNameParse((const FcChar8*)"WenQuanYi Micro Hei");
	//get the same
	print_pattern(pat);

	FcConfigSubstitute(config, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	print_pattern(pat);

	char* fontFile; //this is what we'd return if this was a function
	// find the font
	FcResult result;
	FcPattern* font = FcFontMatch(config, pat, &result);
	print_pattern(font);

	if (font)
	{
		FcChar8* file = NULL;
		if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch)
		{
			//we found the font, now print it.
			//This might be a fallback font
			fontFile = (char*)file;
			printf("%s\n",fontFile);
		}
	}
	FcPatternDestroy(font);
	FcPatternDestroy(pat);
	/* FcFontSetDestroy(sys_fonts); */
	/* FcFontSetDestroy(app_fonts); */
	FcConfigDestroy(config);
	FcFini();

	int total_range  = merge_unicode_range(nk_font_chinese_glyph_ranges(),
					       nk_font_korean_glyph_ranges(), NULL);
	nk_rune rune_range[total_range+1];
	merge_unicode_range(nk_font_chinese_glyph_ranges(),
			    nk_font_korean_glyph_ranges(), rune_range);
	for (int i = 0; i < total_range/2; i++) {
		printf("(%x, %x), ", rune_range[i*2], rune_range[i*2+1]);
	}
	printf("%x\n", rune_range[total_range]);
	save_the_font_images(rune_range);
	return 0;
}
