#include "vector.h"
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

#include <sequential.h>

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

static inline nk_rune*
charset_point(vector_t *charset, uint32_t cursor)
{
	if (charset->len > cursor)
		return vector_at(charset, cursor);
	assert(cursor == charset->len);
	return vector_newelem(charset);
}

static int
accum_charsetpage(FcChar32 basepoint, FcChar32 map[FC_CHARSET_MAP_SIZE], vector_t *cs)
{
	int range_cursor = -1;
	bool inrange = false;
	int count = 0;

	//if continue from last page
	if (cs->len) {
		range_cursor = cs->len-1;
		inrange = (*charset_point(cs, cs->len-1) == basepoint) ?
			true : false;
	}

	for (int i = 0; i < FC_CHARSET_MAP_SIZE; i++) {
		for (int j = 0; j < 32; j++) {
			FcChar32 codepoint = basepoint + i * 32 + j;

			//case 0 continue in range
			if ( (map[i] & (1 << j)) && inrange)
				continue;
			//case 1, break the range.
			else if ( !(map[i] & (1 << j)) && inrange) {
				//there could be special case like [0,0], it is not a big deal.
				range_cursor++;
				assert((range_cursor % 2) == 1);
				*charset_point(cs, range_cursor) = codepoint-1; //it could be?
				inrange = false;
				count++;
			}
			//case 2, not in range, continue
			else if ( !(map[i] & (1 << j)) && !inrange)
				continue;
			//case 3, not in range, new char starts
			else if ( (map[i] & (1 << j)) && !inrange) {
				range_cursor++;
				assert((range_cursor % 2) == 0);
				*charset_point(cs, range_cursor) = codepoint;
				inrange = true;
			}
		}
	}
	//close this page
	if (inrange) {
		*charset_point(cs, range_cursor+1) =
			basepoint + FC_CHARSET_MAP_SIZE * 32 -1;
		count += 1;
	}
	return count;
}

static vector_t
pattern_get_charset(FcPattern *pat)
{
	vector_t charset = {0};
	FcChar32 basepoint = FC_CHARSET_DONE, next;
	FcChar32 map[FC_CHARSET_MAP_SIZE];
	int range_count = 0;
	FcCharSet *cset;

	if (FcPatternGetCharSet(pat, FC_CHARSET, 0, &cset) != FcResultMatch)
		return charset;
	vector_init_zero(&charset, sizeof(nk_rune), NULL);

	for (basepoint = FcCharSetFirstPage(cset, map, &next);
	     basepoint != FC_CHARSET_DONE;
	     basepoint = FcCharSetNextPage(cset, map, &next))
		range_count += accum_charsetpage(basepoint, map, &charset);
	assert(charset.len % 2 == 0);
	*charset_point(&charset, charset.len) = 0;
	return charset;
}

void
print_pattern(FcPattern *pat)
{
	FcChar8 *s;
	double d;
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

	vector_t charset = pattern_get_charset(pat);
	if (charset.len) {
		/* for (int i = 0; i < charset.len / 2; i++) */
		/*	printf("(%d, %d) ", *charset_point(&charset, 2*i), *charset_point(&charset, 2*i+1)); */
		/* printf("\n"); */
		vector_destroy(&charset);
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

//we can only merge one range at a time, we need to expose them
int
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
save_the_font_images(const nk_rune *glyph_range, const char *path)
{
	int w, h;
	struct nk_font_config cfg = nk_font_config(16);
	cfg.range = glyph_range;

	struct nk_font_atlas atlas;
	nk_font_atlas_init_default(&atlas);
	nk_font_atlas_begin(&atlas);
	nk_font_atlas_add_from_file(&atlas, path, 16, &cfg);
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

//not thread safe
struct nk_wl_ftlib {
	FT_Library ftlib;
	int ref;
} NK_WL_FTLIB = {0};


static inline FT_Library *
nk_wl_ft_ref()
{
	if (!NK_WL_FTLIB.ref)
		FT_Init_FreeType(&NK_WL_FTLIB.ftlib);
	NK_WL_FTLIB.ref++;
	return &NK_WL_FTLIB.ftlib;
}

static inline void
nk_wl_ft_unref(FT_Library *lib)
{
	if (!NK_WL_FTLIB.ref || lib != &NK_WL_FTLIB.ftlib)
		return;
	NK_WL_FTLIB.ref = MAX(0, NK_WL_FTLIB.ref-1);
	if (!NK_WL_FTLIB.ref)
		FT_Done_FreeType(NK_WL_FTLIB.ftlib);
}

//an implementation using cairo
struct nk_wl_user_font {
	int size;
	float scale;
	FT_Face face;
	FT_Library *ft_lib;
	cairo_font_face_t *cairo_face;
	struct nk_user_font nk_font;
	//we dont need range here, but need for EGL backend
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
	int pix_size, scale;
	int nranges;
	const nk_rune **ranges;

	//fc config offers light, medium, demibold, bold, black
	//demibold, bold and black is true, otherwise false.
	bool bold; //classified as bold
	//private: TTF only
	bool TTFonly;
};

static char *
nk_wl_find_font(const struct nk_wl_font_config *user_config)
{
	FcInit();
	FcConfig *config = FcInitLoadConfigAndFonts();
	FcPattern *font = NULL;
	FcPattern *pat = FcNameParse((const FcChar8 *)user_config->name);
	char *path = NULL;

	FcConfigSubstitute(config, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	//boldness
	if (user_config->bold)
		FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_DEMIBOLD);
	else
		FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_REGULAR);
	//slant
	FcPatternAddInteger(
		pat, FC_SLANT,
		(user_config->slant == NK_WL_SLANT_ROMAN) ? FC_SLANT_ROMAN :
		(user_config->slant == NK_WL_SLANT_OBLIQUE) ? FC_SLANT_OBLIQUE :
		FC_SLANT_ITALIC);

	FcResult result;
	FcFontSet *set = FcFontSort(config, pat, false, NULL, &result);
	if (!set)
		goto fini;
	//deal with TTF
	if (user_config->TTFonly) {
		const char *format = "TrueType";
		//select only the ttf
		FcPattern *ttf_pattern = FcPatternCreate();
		FcPatternAddString(ttf_pattern, FC_FONTFORMAT, (const FcChar8*)format);
		font = FcFontSetMatch(config, &set, 1, ttf_pattern, &result);
		FcPatternDestroy(ttf_pattern);
	} else
		font = FcFontSetMatch(config, &set, 1, pat, &result);
	FcFontSetDestroy(set);
	//return string
	FcChar8* file = NULL;
	if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch)
		goto fini;
	/* printf("%s\n", file); */
	path = strdup((const char *)file);
fini:
	if (font)
		FcPatternDestroy(font);
	FcPatternDestroy(pat);
	FcConfigDestroy(config);
	FcFini();
	return path;
}


static float
nk_cairo_text_width(nk_handle handle, float height, const char *text,
		    int utf8_len)
{
	struct nk_wl_user_font *user_font = handle.ptr;
	cairo_surface_t *recording = cairo_recording_surface_create(CAIRO_CONTENT_COLOR, NULL);
	cairo_t *cr = cairo_create(recording);
	cairo_text_extents_t extents;

	cairo_set_font_face(cr, user_font->cairo_face);
	cairo_set_font_size(cr, user_font->size);
	cairo_text_extents(cr, text, &extents);
	cairo_destroy(cr);
	cairo_surface_destroy(recording);
	return extents.width;
}



//sample implementaiton for with cairo
struct nk_user_font *
nk_wl_new_font(struct nk_wl_font_config config)
{
	int error;
	char *font_path = NULL;
	if (!config.name)
		config.name = "Vera";
	if (!config.pix_size)
		config.pix_size = 16;
	if (!config.scale)
		config.scale = 1;
	font_path = nk_wl_find_font(&config);
	if (!font_path)
		return NULL;
	//else, we try to creat
	struct nk_wl_user_font *user_font =
		malloc(sizeof(struct nk_wl_user_font));
	user_font->ft_lib = nk_wl_ft_ref();
	if (!user_font->ft_lib)
		goto err_lib;

	user_font->size = config.pix_size;
	user_font->scale = config.scale;
	error = FT_New_Face(*user_font->ft_lib, font_path, 0, &user_font->face);
	if (error)
		goto err_face;
	user_font->cairo_face = cairo_ft_font_face_create_for_ft_face(user_font->face, 0);
	if (!user_font->cairo_face)
		goto err_crface;
	user_font->nk_font.height = config.pix_size;
	user_font->nk_font.userdata.ptr = user_font;
	user_font->nk_font.width = nk_cairo_text_width;
	free(font_path);
	return &user_font->nk_font;

err_crface:
	FT_Done_Face(user_font->face);
err_face:
	nk_wl_ft_unref(user_font->ft_lib);
err_lib:
	free(user_font);
	free(font_path);
	return NULL;
}

void
nk_wl_destroy_font(struct nk_user_font *font)
{
	struct nk_wl_user_font *cairo_font =
		container_of(font, struct nk_wl_user_font, nk_font);

	cairo_font_face_destroy(cairo_font->cairo_face);
	FT_Done_Face(cairo_font->face);
	nk_wl_ft_unref(cairo_font->ft_lib);
	free(cairo_font);
}

int
main(int argc, char *argv[])
{
	struct nk_wl_font_config config = {
		.name = argv[1],
		.slant = NK_WL_SLANT_ROMAN,
		.pix_size = 16,
		.scale = 1,
		.nranges = 0,
		.ranges = NULL,
		.TTFonly = true,
	};

	struct nk_user_font *nk_font = nk_wl_new_font(config);
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1000, 1000);
	cairo_t *cr = cairo_create(surface);

	struct nk_wl_user_font *user_font =
		container_of(nk_font, struct nk_wl_user_font, nk_font);
	cairo_set_font_face(cr, user_font->cairo_face);
	cairo_set_font_size(cr, user_font->size);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_paint(cr);



	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_move_to(cr, 100, 100);
	char text[256];
	sprintf(text, "%s \nIcons test %s, %s:\n", "this is a test", u8"\uf192", u8"\ue708");
	printf(".this extents is %lf\n", nk_cairo_text_width(user_font->nk_font.userdata, 20, text, strlen(text)));

	cairo_show_text(cr, text);
	cairo_surface_write_to_png(surface, "/tmp/test_font.png");
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	//now we have the cairo_font_face_t

	nk_wl_destroy_font(nk_font);
	return 0;
}
