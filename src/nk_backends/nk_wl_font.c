#ifndef NK_WL_FONT_H
#define NK_WL_FONT_H

#include <string.h>
#include "nk_wl_internal.h"

#include <fontconfig/fontconfig.h>

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H


#ifdef __cplusplus
extern "C" {
#endif


struct nk_wl_user_font;

// to be implemented by all backends, there should be an additional parameter
// for code points, the nk_user_font is hidden from you. Do it for the pixel size
struct nk_wl_user_font *nk_wl_new_font(int size, int scale, const char *pattern);
//release the font when you are done
void nk_wl_font_release(struct nk_wl_user_font *font);

//then this function to apply to the nuklear
const struct nk_user_font *nk_wl_font_get(struct nk_wl_user_font *user_font);

//okay now, lets do it for cairo
struct nk_cairo_font {
	int size;
	float scale;
	FT_Face text_font;
	FT_Face icon_font;
	FT_Library ft_lib;
	struct nk_user_font nk_font;
};



// a synchronize function, could be called using threaded_run later firstly you
// probably want to find the font using fontconfig. Here in cairo we can using
// TTF or OTF, in EGL only TTF is available.

//to do the selection, we will have some properties, like slant, boldness.
struct nk_wl_user_font *nk_wl_new_font(int size, int scale, const char *pattern)
{
	int path_len = 0;
	const char *default_font = "Vera"; //it is not very available
	font_name = (strlen(font_name) == 0) ? default_font : font_name;

	FcInit();
	FcConfig *config = FcConfigGetCurrent();
	FcChar8 *file = NULL;
	const char *searched_path;
	FcResult result;
	FcPattern *pat = FcNameParse((const FcChar8 *)font_name);
	//I suppose there is the
	FcConfigSubstitute(config, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	FcPattern *font = FcFontMatch(config, pat, &result);
	if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
		searched_path = (const char *)file;
		path_len = strlen(searched_path);
		path_len = (path_len < path_cap -1) ? path_len : -1;
		if (path_len > 0)
			strcpy(path, searched_path);
		//return ed is a reference
		/* free(file); */
		FcPatternDestroy(pat);
		FcPatternDestroy(font);
	}
	FcConfigDestroy(config);
	FcFini();
	return path_len;
}


#ifdef __cplusplus
}
#endif

#endif /* EOF */
