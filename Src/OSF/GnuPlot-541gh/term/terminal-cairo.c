// GNUPLOT - cairo.trm 
// Copyright 2007   Timothee Lecomte
//
/*
 * Modifications:
 *	Peter Danenberg, Ethan Merritt	- handle png output as well as pdf
 *	Peter Danenberg - crop for PNGs
 *	René Haber - added epscairo terminal
 *	Bastian Maerkisch - cairolatex terminal
 *	Henri Menke - add png option for graphics output
 */
#include <gnuplot.h>
#pragma hdrstop
#include <driver.h>
#include <post.h>

#ifdef HAVE_CAIROPDF // {

// @experimental {
#define TERM_BODY
#define TERM_PUBLIC static
#define TERM_TABLE
#define TERM_TABLE_START(x) GpTermEntry_Static x {
#define TERM_TABLE_END(x)   };
// } @experimental

#ifdef TERM_REGISTER
	register_term(pdfcairo)
#endif

//#ifdef TERM_PROTO
extern void PSLATEX_reset(GpTermEntry_Static * pThis);
TERM_PUBLIC void cairotrm_options(GpTermEntry_Static * pThis, GnuPlot * pGp);
TERM_PUBLIC void cairotrm_init(GpTermEntry_Static * pThis);
TERM_PUBLIC void cairotrm_graphics(GpTermEntry_Static * pThis);
TERM_PUBLIC void cairotrm_text(GpTermEntry_Static * pThis);
TERM_PUBLIC void cairotrm_move(GpTermEntry_Static * pThis, uint x, uint y);
TERM_PUBLIC void cairotrm_vector(GpTermEntry_Static * pThis, uint x, uint y);
TERM_PUBLIC void cairotrm_put_text(GpTermEntry_Static * pThis, uint x, uint y, const char * str);
TERM_PUBLIC void cairotrm_enhanced_flush(GpTermEntry_Static * pThis);
TERM_PUBLIC void cairotrm_enhanced_writec(GpTermEntry_Static * pThis, int c);
TERM_PUBLIC void cairotrm_enhanced_open(GpTermEntry_Static * pThis, char* fontname, double fontsize, double base, bool widthflag, bool showflag, int overprint);
TERM_PUBLIC void cairotrm_reset(GpTermEntry_Static * pThis);
TERM_PUBLIC int  cairotrm_justify_text(GpTermEntry_Static * pThis, enum JUSTIFY mode);
TERM_PUBLIC void cairotrm_point(GpTermEntry_Static * pThis, uint x, uint y, int pointstyle);
TERM_PUBLIC void cairotrm_linewidth(GpTermEntry_Static * pThis, double linewidth);
TERM_PUBLIC int  cairotrm_text_angle(GpTermEntry_Static * pThis, int ang);
TERM_PUBLIC void cairotrm_fillbox(GpTermEntry_Static * pThis, int style, uint x1, uint y1, uint width, uint height);
TERM_PUBLIC int  cairotrm_set_font(GpTermEntry_Static * pThis, const char * font);
TERM_PUBLIC void cairotrm_pointsize(GpTermEntry_Static * pThis, double ptsize);
TERM_PUBLIC void cairotrm_image(GpTermEntry_Static * pThis, uint M, uint N, coordval * image, gpiPoint * corner, t_imagecolor color_mode);
TERM_PUBLIC int  cairotrm_make_palette(GpTermEntry_Static * pThis, t_sm_palette * palette);
TERM_PUBLIC void cairotrm_filled_polygon(GpTermEntry_Static * pThis, int n, gpiPoint * corners);
TERM_PUBLIC void cairotrm_boxed_text(GpTermEntry_Static * pThis, uint x, uint y, int option);
TERM_PUBLIC void cairotrm_dashtype(GpTermEntry_Static * pThis, int type, t_dashtype * custom_dash_pattern);
#ifdef HAVE_WEBP
	TERM_PUBLIC void WEBP_options(GpTermEntry_Static * pThis, GnuPlot * pGp, int); // We call it from cairotrm_options 
#endif
//#endif // TERM_PROTO 

#ifndef TERM_PROTO_ONLY
#ifdef TERM_BODY
#include "cairo-pdf.h"
#ifdef HAVE_CAIROEPS
	#include "cairo-ps.h"
#endif
#include "wxterminal/gp_cairo.h"
#include "wxterminal/gp_cairo_helpers.h"
// @sobolev #include "glib.h" // For guint32 

#define CAIROTRM_DEFAULT_FONTNAME "Sans"

static cairo_status_t cairostream_write(void * closure, uchar * data, uint length);
static int cairostream_error[1];

/* Terminal id */
enum CAIROTRM_id {
	CAIROTRM_FONT,
	CAIROTRM_FONTSCALE,
	CAIROTRM_ENHANCED,
	CAIROTRM_NOENHANCED,
	CAIROTRM_SIZE,
	CAIROTRM_ROUNDED,
	CAIROTRM_BUTT,
	CAIROTRM_SQUARE,
	CAIROTRM_LINEWIDTH,
	CAIROTRM_DASHED,
	CAIROTRM_SOLID,
	CAIROTRM_MONO,
	CAIROTRM_COLOR,
	CAIROTRM_DASHLENGTH,
	CAIROTRM_TRANSPARENT,
	CAIROTRM_NOTRANSPARENT,
	CAIROTRM_CROP,
	CAIROTRM_NOCROP,
	CAIROTRM_BACKGROUND,
	CAIROTRM_POINTSCALE,
	CAIROLATEX_STANDALONE,
	CAIROLATEX_INPUT,
	CAIROLATEX_HEADER,
	CAIROLATEX_NOHEADER,
	CAIROLATEX_BLACKTEXT,
	CAIROLATEX_COLORTEXT,
	CAIROLATEX_EPS,
	CAIROLATEX_PDF,
	CAIROLATEX_PNG,
	CAIROLATEX_PNG_RESOLUTION,
	CAIROTRM_OTHER
};

/* Terminal type of postscript dialect */
enum CAIRO_TERMINALTYPE {
	CAIROTERM_EPS, 
	CAIROTERM_PDF, 
	CAIROTERM_PNG, 
	CAIROTERM_LATEX
};
// 
// One struct that takes all terminal parameters
// by Harald Harders <h.harders@tu-bs.de> 
//
struct cairo_params_t {
	enum {
		subtUndef = 0,
		subtEps   = 1,
		subtLatex = 2,
		subtPdf   = 3,
		subtPng   = 4,
	};
	cairo_params_t(int subtype)
	{
		Setup(subtype);
	}
	void   Setup(int subtype)
	{
		switch(subtype) {
			case subtEps:
				terminal = CAIROTERM_EPS;
				explicit_units = INCHES;
				enhanced = FALSE;
				dashed = FALSE;
				dash_length = 1.0f;
				background = {1.0, 1.0, 1.0 };
				mono = FALSE;
				linecap = BUTT;
				transparent = TRUE;
				crop = FALSE;
				fontname = NULL;
				fontsize = 12.0f;
				fontscale = 0.5f;
				width = 5*72.0f;
				height = 3*72.0f;
				base_linewidth = 0.25f;
				lw = 1.0f;
				ps = 1.0f;
				output = CAIROTRM_FONT;
				resolution = 0;
				break;
			case subtLatex:
				terminal = CAIROTERM_LATEX;
				explicit_units = INCHES;
				enhanced = FALSE;
				dashed = FALSE;
				dash_length = 1.0f;
				background = { 1.0, 1.0, 1.0 };
				mono = FALSE;
				linecap = BUTT;
				transparent = TRUE;
				crop = FALSE;
				fontname = NULL;
				fontsize = 11.0f;
				fontscale = 0.6f;
				width = 5*72.0f;
				height = 3*72.0f;
				base_linewidth = 0.25f;
				lw = 1.0f;
				ps = 1.0f;
				output = CAIROTRM_FONTSCALE;
				resolution = 300;
				break;
			case subtPdf:
				terminal = CAIROTERM_PDF;
				explicit_units = INCHES;
				enhanced = FALSE;
				dashed = FALSE;
				dash_length = 1.0f;
				background = {1.0, 1.0, 1.0 };
				mono = FALSE;
				linecap = BUTT;
				transparent = TRUE;
				crop = FALSE;
				fontname = NULL;
				fontsize = 12.0f;
				fontscale = 0.5f;
				width = 5*72.0f;
				height = 3*72.0f;
				base_linewidth = 0.25f;
				lw = 1.0f;
				ps = 1.0f;
				output = CAIROTRM_FONT;
				resolution = 0;
				break;
			case subtPng:
				terminal = CAIROTERM_PNG;
				explicit_units = PIXELS;
				enhanced = FALSE;
				dashed = FALSE;
				dash_length = 1.0f;
				background = {1.0, 1.0, 1.0};
				mono = FALSE;
				linecap = BUTT;
				transparent = FALSE;
				crop = FALSE;
				fontname = NULL;
				fontsize = 12.0f;
				fontscale = 1.0f;
				width = 640.0f;
				height = 480.0f;
				base_linewidth = 1.0f;
				lw = 1.0f;
				ps = 1.0f;
				output = CAIROTRM_FONT;
				resolution = 0;
				break;
		}
	}
	enum CAIRO_TERMINALTYPE terminal;
	GpSizeUnits explicit_units;
	bool   enhanced;
	bool   dashed;
	float  dash_length;
	rgb_color background;
	bool   mono;
	t_linecap linecap; // butt/rounded/square linecaps and linejoins 
	bool   transparent;
	bool   crop;
	char * fontname; // name of font 
	float  fontsize;  // size of font in pts 
	float  fontscale; // modified by this 
	float  width;
	float  height;
	float  base_linewidth;
	float  lw;
	float  ps;
	enum   CAIROTRM_id output; // format of the graphics produced by cairolatex 
	int    resolution; // in dpi; CAIROLATEX_PNG only 
};

#define CAIROEPS_PARAMS_DEFAULT { \
		CAIROTERM_EPS, INCHES, FALSE, FALSE, 1.0, {1.0, 1.0, 1.}, FALSE, FALSE, TRUE, FALSE, NULL, 12, 0.5, 5*72., 3*72., 0.25, 1.0, 1.0, FALSE \
}
static cairo_params_t cairoeps_params(cairo_params_t::subtEps);// = CAIROEPS_PARAMS_DEFAULT;
static const cairo_params_t cairoeps_params_default(cairo_params_t::subtEps);// = CAIROEPS_PARAMS_DEFAULT;

#ifdef HAVE_CAIROEPS
#define CAIROLATEX_PARAMS_DEFAULT { \
		CAIROTERM_LATEX, INCHES, FALSE, FALSE, 1.0, {1., 1., 1.}, FALSE, FALSE, TRUE, FALSE, NULL, \
		11, 0.6, 5*72., 3*72., 0.25, 1.0, 1.0, FALSE, 300 \
}
#else
#define CAIROLATEX_PARAMS_DEFAULT { \
		CAIROTERM_LATEX, INCHES, FALSE, FALSE, 1.0, {1., 1., 1.}, FALSE, FALSE, TRUE, FALSE, NULL, \
		11, 0.6, 5*72., 3*72., 0.25, 1.0, 1.0, TRUE, 300 \
}
#endif
static cairo_params_t cairolatex_params(cairo_params_t::subtLatex);// = CAIROLATEX_PARAMS_DEFAULT;
static const cairo_params_t cairolatex_params_default(cairo_params_t::subtLatex); //= CAIROLATEX_PARAMS_DEFAULT;

#define CAIROPDF_PARAMS_DEFAULT { \
		CAIROTERM_PDF, INCHES, FALSE, FALSE, 1.0, {1., 1., 1.}, FALSE, FALSE, TRUE, FALSE, NULL, \
		12, 0.5, 5*72., 3*72., 0.25, 1.0, 1.0, FALSE \
}
static cairo_params_t cairopdf_params(cairo_params_t::subtPdf); //= CAIROPDF_PARAMS_DEFAULT;
static const cairo_params_t cairopdf_params_default(cairo_params_t::subtPdf);//= CAIROPDF_PARAMS_DEFAULT;

#define CAIROPNG_PARAMS_DEFAULT { \
		CAIROTERM_PNG, PIXELS, FALSE, FALSE, 1.0, {1., 1., 1.}, FALSE, FALSE, FALSE, FALSE, NULL, \
		12, 1.0, 640., 480., 1.0, 1.0, 1.0, FALSE \
}
static cairo_params_t cairopng_params(cairo_params_t::subtPng); //= CAIROPNG_PARAMS_DEFAULT;
static const cairo_params_t cairopng_params_default(cairo_params_t::subtPng); //= CAIROPNG_PARAMS_DEFAULT;

static cairo_params_t * cairo_params = &cairopdf_params;
static const cairo_params_t * cairo_params_default = &cairopdf_params;

#ifdef PSLATEX_DRIVER
#define ISCAIROLATEX (cairo_params->terminal == CAIROTERM_LATEX)
static ps_params_t cairo_epslatex_params(PSTERM_EPSLATEX); //= EPSLATEX_PARAMS_DEFAULT;
#else
#define ISCAIROLATEX (FALSE)
#endif

char * cairo_enhanced_fontname = NULL;

plot_struct plot;

static struct gen_table cairotrm_opts[] = {
	{"fontscale",   CAIROTRM_FONTSCALE},
	{"f$ont",   CAIROTRM_FONT},
	{"enh$anced", CAIROTRM_ENHANCED},
	{"noenh$anced", CAIROTRM_NOENHANCED},
	{"si$ze", CAIROTRM_SIZE},
	{"round$ed", CAIROTRM_ROUNDED},
	{"butt", CAIROTRM_BUTT},
	{"square", CAIROTRM_SQUARE},
	{"lw", CAIROTRM_LINEWIDTH},
	{"linew$idth", CAIROTRM_LINEWIDTH},
	{"pointscale", CAIROTRM_POINTSCALE},
	{"ps", CAIROTRM_POINTSCALE},
	{"dash$ed", CAIROTRM_DASHED},
	{"solid", CAIROTRM_SOLID},
	{"mono$chrome", CAIROTRM_MONO},
	{"color", CAIROTRM_COLOR},
	{"col$our", CAIROTRM_COLOR},
	{"dl", CAIROTRM_DASHLENGTH},
	{"dashl$ength", CAIROTRM_DASHLENGTH},
	{"transp$arent", CAIROTRM_TRANSPARENT},
	{"notransp$arent", CAIROTRM_NOTRANSPARENT},
	{"crop", CAIROTRM_CROP},
	{"nocrop", CAIROTRM_NOCROP},
	{"backg$round", CAIROTRM_BACKGROUND},
	{"nobackg$round", CAIROTRM_TRANSPARENT},
	{"stand$alone", CAIROLATEX_STANDALONE},
	{"inp$ut", CAIROLATEX_INPUT},
	{"header", CAIROLATEX_HEADER},
	{"noheader", CAIROLATEX_NOHEADER},
	{"b$lacktext", CAIROLATEX_BLACKTEXT},
	{"colort$ext", CAIROLATEX_COLORTEXT},
	{"colourt$ext", CAIROLATEX_COLORTEXT},
	{"eps", CAIROLATEX_EPS},
	{"pdf", CAIROLATEX_PDF},
	{"png", CAIROLATEX_PNG},
	{"resolution", CAIROLATEX_PNG_RESOLUTION},
	{NULL, CAIROTRM_OTHER}
};
//
// "Called when terminal type is selected. This procedure should parse options on the command line.
// A list of the currently selected options should be stored in GPT.TermOptions[],
// in a form suitable for use with the set term command.
// GPT.TermOptions[] is used by the save command.  Use options_null() if no options are available." */
//
TERM_PUBLIC void cairotrm_options(GpTermEntry_Static * pThis, GnuPlot * pGp)
{
	char * s = NULL;
	char * font_setting = NULL;
	bool duplication = FALSE;
	bool set_font = FALSE, set_size = FALSE;
	bool set_capjoin = FALSE;
	bool set_standalone = FALSE, set_header = FALSE;
	char tmp_term_options[MAX_LINE_LEN+1] = "";
	// Initialize terminal-dependent values 
	if(sstreq(pThis->name, "pngcairo")) {
		cairo_params = &cairopng_params;
		cairo_params_default = &cairopng_params_default;
	}
	else if(sstreq(pThis->name, "epscairo")) {
		cairo_params = &cairoeps_params;
		cairo_params_default = &cairoeps_params_default;
	}
#ifdef HAVE_WEBP
	else if(sstreq(pThis->name, "webp")) {
		cairo_params = &cairopng_params;
		cairo_params_default = &cairopng_params_default;
	}
#endif
#ifdef PSLATEX_DRIVER
	else if(sstreq(pThis->name, "cairolatex")) {
		cairo_params = &cairolatex_params;
		cairo_params_default = &cairolatex_params_default;
		ps_params = &cairo_epslatex_params;
	}
#endif
	else {
		cairo_params = &cairopdf_params;
		cairo_params_default = &cairopdf_params_default;
	}
	/* Default to enhanced text mode */
	if(!pGp->Pgm.AlmostEquals(pGp->Pgm.GetPrevTokenIdx(), "termopt$ion") && !ISCAIROLATEX) {
		cairo_params->enhanced = TRUE;
		pThis->SetFlag(TERM_ENHANCED_TEXT);
	}
	while(!pGp->Pgm.EndOfCommand()) {
		FPRINTF((stderr, "processing token\n"));
		switch(pGp->Pgm.LookupTableForCurrentToken(&cairotrm_opts[0])) {
			case CAIROTRM_FONT:
			    pGp->Pgm.Shift();
			    if(!(s = pGp->TryToGetString()))
				    pGp->IntErrorCurToken("font: expecting string");
			    font_setting = sstrdup(s);
			    if(*s) {
				    char * sep = strchr(s, ',');
				    if(sep) {
					    sscanf(sep+1, "%f", &cairo_params->fontsize);
					    *sep = '\0';
				    }
				    SAlloc::F(cairo_params->fontname);
				    cairo_params->fontname = sstrdup(s);
			    }
			    SAlloc::F(s);
			    if(set_font) 
					duplication = TRUE;
			    set_font = TRUE;
			    break;
			case CAIROTRM_ENHANCED:
			    if(ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    pGp->Pgm.Shift();
			    cairo_params->enhanced = TRUE;
			    pThis->SetFlag(TERM_ENHANCED_TEXT);
			    break;
			case CAIROTRM_NOENHANCED:
			    if(ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    pGp->Pgm.Shift();
			    cairo_params->enhanced = FALSE;
			    pThis->ResetFlag(TERM_ENHANCED_TEXT);
			    break;
			case CAIROTRM_SIZE:
			    pGp->Pgm.Shift();
			    if(sstreq(pThis->name, "pngcairo"))
				    cairo_params->explicit_units = pGp->ParseTermSize(&cairo_params->width, &cairo_params->height, PIXELS);
#ifdef HAVE_WEBP
			    else if(sstreq(pThis->name, "webp"))
				    cairo_params->explicit_units = pGp->ParseTermSize(&cairo_params->width, &cairo_params->height, PIXELS);
#endif
			    else
				    cairo_params->explicit_units = pGp->ParseTermSize(&cairo_params->width, &cairo_params->height, INCHES);
			    if(set_size) duplication = TRUE;
			    set_size = TRUE;
			    break;
			case CAIROTRM_ROUNDED:
			    pGp->Pgm.Shift();
			    if(set_capjoin) duplication = TRUE;
			    cairo_params->linecap = ROUNDED;
			    set_capjoin = TRUE;
			    break;
			case CAIROTRM_BUTT:
			    pGp->Pgm.Shift();
			    if(set_capjoin) duplication = TRUE;
			    cairo_params->linecap = BUTT;
			    set_capjoin = TRUE;
			    break;
			case CAIROTRM_SQUARE:
			    pGp->Pgm.Shift();
			    if(set_capjoin) duplication = TRUE;
			    cairo_params->linecap = SQUARE;
			    set_capjoin = TRUE;
			    break;
			case CAIROTRM_LINEWIDTH:
			    pGp->Pgm.Shift();
			    cairo_params->lw = static_cast<float>(pGp->RealExpression());
			    if(cairo_params->lw < 0.0)
				    cairo_params->lw = cairo_params_default->lw;
			    break;
			case CAIROTRM_POINTSCALE:
			    pGp->Pgm.Shift();
			    cairo_params->ps = static_cast<float>(pGp->RealExpression());
			    if(cairo_params->ps < 0.0)
				    cairo_params->ps = 1.0;
			    break;
			case CAIROTRM_DASHED:
			case CAIROTRM_SOLID:
			    /* dashes always enabled in version 5 */
			    pGp->Pgm.Shift();
			    cairo_params->dashed = TRUE;
			    break;
			case CAIROTRM_MONO:
			    pGp->Pgm.Shift();
			    cairo_params->mono = TRUE;
			    pThis->SetFlag(TERM_MONOCHROME);
			    break;
			case CAIROTRM_COLOR:
			    pGp->Pgm.Shift();
			    cairo_params->mono = FALSE;
			    pThis->ResetFlag(TERM_MONOCHROME);
			    break;
			case CAIROTRM_DASHLENGTH:
			    pGp->Pgm.Shift();
			    cairo_params->dash_length = static_cast<float>(pGp->RealExpression());
			    if(cairo_params->dash_length < 0.0)
				    cairo_params->dash_length = cairo_params_default->dash_length;
			    break;
			case CAIROTRM_FONTSCALE:
			    pGp->Pgm.Shift();
			    cairo_params->fontscale = static_cast<float>(pGp->Pgm.EndOfCommand() ? -1.0 : pGp->RealExpression());
			    if(cairo_params->fontscale <= 0.0)
				    cairo_params->fontscale = cairo_params_default->fontscale;
			    /* set_fontscale = TRUE; */
			    break;
			case CAIROTRM_TRANSPARENT:
			    pGp->Pgm.Shift();
			    cairo_params->transparent = TRUE;
			    break;
			case CAIROTRM_NOTRANSPARENT:
			    pGp->Pgm.Shift();
			    cairo_params->transparent = FALSE;
			    break;
			case CAIROTRM_CROP:
			    pGp->Pgm.Shift();
			    if((cairo_params->terminal == CAIROTERM_PNG) || (cairo_params->terminal == CAIROTERM_EPS))
				    cairo_params->crop = TRUE;
			    break;
			case CAIROTRM_NOCROP:
			    pGp->Pgm.Shift();
			    if((cairo_params->terminal == CAIROTERM_PNG) || (cairo_params->terminal == CAIROTERM_EPS))
				    cairo_params->crop = FALSE;
			    break;
			case CAIROTRM_BACKGROUND:
		    {
			    int wxt_background;
			    pGp->Pgm.Shift();
			    wxt_background = pGp->ParseColorName();
			    cairo_params->background.r = (double)((wxt_background >> 16) & 0xff) / 255.0;
			    cairo_params->background.g = (double)((wxt_background >>  8) & 0xff) / 255.0;
			    cairo_params->background.b = (double)( wxt_background        & 0xff) / 255.0;
			    cairo_params->transparent = FALSE;
			    break;
		    }
#ifdef PSLATEX_DRIVER
			case CAIROLATEX_STANDALONE:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    ps_params->epslatex_standalone = TRUE;
				pGp->Pgm.Shift();
			    if(set_standalone) 
					duplication = TRUE;
			    set_standalone = TRUE;
			    break;
			case CAIROLATEX_INPUT:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    ps_params->epslatex_standalone = FALSE;
			    pGp->Pgm.Shift();
			    if(set_standalone) duplication = TRUE;
			    set_standalone = TRUE;
			    break;
			case CAIROLATEX_HEADER:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    pGp->Pgm.Shift();
			    SAlloc::F(epslatex_header);
			    // Protect against pGp->IntError() bail from pGp->TryToGetString() 
			    epslatex_header = NULL;
			    epslatex_header = pGp->TryToGetString();
			    if(!epslatex_header)
				    pGp->IntErrorCurToken("String containing header information expected");
			    if(set_header) duplication = TRUE;
			    set_header = TRUE;
			    break;
			case CAIROLATEX_NOHEADER:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    ZFREE(epslatex_header);
			    if(set_header) 
					duplication = TRUE;
			    set_header = TRUE;
			    pGp->Pgm.Shift();
			    break;
			case CAIROLATEX_BLACKTEXT:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    ps_params->blacktext = TRUE;
			    pGp->Pgm.Shift();
			    break;
			case CAIROLATEX_COLORTEXT:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    ps_params->blacktext = FALSE;
			    pGp->Pgm.Shift();
			    break;
			case CAIROLATEX_EPS:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
#ifdef HAVE_CAIROEPS
			    cairo_params->output = CAIROLATEX_EPS;
#else
			    pGp->IntErrorCurToken("eps output not supported");
#endif
			    pGp->Pgm.Shift();
			    break;
			case CAIROLATEX_PDF:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    cairo_params->output = CAIROLATEX_PDF;
			    pGp->Pgm.Shift();
			    break;
			case CAIROLATEX_PNG:
			    if(!ISCAIROLATEX)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    cairo_params->output = CAIROLATEX_PNG;
			    pGp->Pgm.Shift();
			    break;
			case CAIROLATEX_PNG_RESOLUTION:
			    if(cairo_params->output != CAIROLATEX_PNG)
				    pGp->IntErrorCurToken("extraneous argument in set terminal %s", pThis->name);
			    pGp->Pgm.Shift();
			    cairo_params->resolution = pGp->IntExpression();
			    if(cairo_params->resolution <= 0)
				    cairo_params->resolution = 300;
			    break;
#endif /* PSLATEX_DRIVER */
			case CAIROTRM_OTHER:
			    // Ignore irrelevant keywords used by other png/pdf drivers 
			    if(pGp->Pgm.AlmostEqualsCur("true$color") || pGp->Pgm.AlmostEqualsCur("inter$lace") || pGp->Pgm.AlmostEqualsCur("nointer$lace")) {
				    pGp->Pgm.Shift();
				    break;
			    }
#ifdef HAVE_WEBP
			    if(sstreq(pThis->name, "webp")) {
				    int save_token = pGp->Pgm.GetCurTokenIdx();
				    WEBP_options(pThis, pGp, 0);
				    if(save_token != pGp->Pgm.GetCurTokenIdx())
					    break;
			    }
#endif
			default:
			    pGp->IntErrorCurToken("unrecognized terminal option");
			    break;
		}
		if(duplication)
			pGp->IntError(pGp->Pgm.GetPrevTokenIdx(), "Duplicated or contradicting arguments in cairo terminal options.");
	}
#if 0
	/* Copy terminal-dependent values to the generic equivalent */
	cairo_params->width = cairo_params_default->width;
	cairo_params->height = cairo_params_default->height;
	cairo_params->base_linewidth = cairo_params_default->base_linewidth;
#endif
#ifdef PSLATEX_DRIVER
	if(ISCAIROLATEX) {
		const char * output = "";
		switch(cairo_params->output) {
			default:
			case CAIROLATEX_PDF: output = "pdf"; break;
			case CAIROLATEX_EPS: output = "eps"; break;
			case CAIROLATEX_PNG: output = "png"; break;
		}
		sprintf(tmp_term_options, " %s %s", output, ps_params->epslatex_standalone ? "standalone" : "input");
		GPT._TermOptions.Cat(tmp_term_options);
		if(epslatex_header)
			sprintf(tmp_term_options, " header \"%s\"", epslatex_header);
		else
			sprintf(tmp_term_options, " noheader");
		GPT._TermOptions.Cat(tmp_term_options);
		sprintf(tmp_term_options, " %s", ps_params->blacktext ? "blacktext" : "colortext");
		GPT._TermOptions.Cat(tmp_term_options);
	}
#endif
	// Save options back into options string in normalized format 
	if(cairo_params->transparent)
		GPT._TermOptions.Cat(ISCAIROLATEX ? " nobackground" : " transparent");
	else {
		sprintf(tmp_term_options, " background \"#%02x%02x%02x\"", (int)(255 * cairo_params->background.r), (int)(255 * cairo_params->background.g), (int)(255 * cairo_params->background.b));
		GPT._TermOptions.Cat(tmp_term_options);
	}
	if(cairo_params->crop)
		GPT._TermOptions.Cat(" crop");
	GPT._TermOptions.Cat(cairo_params->enhanced ? " enhanced" : " noenhanced");
	if(set_font) {
		snprintf(tmp_term_options, sizeof(tmp_term_options), " font \"%s\"", font_setting);
		SAlloc::F(font_setting);
		GPT._TermOptions.Cat(tmp_term_options);
	}
	// if(set_fontscale) 
	{
		snprintf(tmp_term_options, sizeof(tmp_term_options), " fontscale %.1f", cairo_params->fontscale);
		GPT._TermOptions.Cat(tmp_term_options);
	}
	if(cairo_params->mono)
		GPT._TermOptions.Cat(" monochrome");
	if(1 || set_size) {
		if(cairo_params->explicit_units == CM)
			snprintf(tmp_term_options, sizeof(tmp_term_options), " size %.2fcm, %.2fcm ", 2.54*cairo_params->width/72., 2.54*cairo_params->height/72.0);
		else if(cairo_params->explicit_units == PIXELS)
			snprintf(tmp_term_options, sizeof(tmp_term_options), " size %d, %d ", (int)cairo_params->width, (int)cairo_params->height);
		else
			snprintf(tmp_term_options, sizeof(tmp_term_options), " size %.2fin, %.2fin ", cairo_params->width/72.0, cairo_params->height/72.0);
		GPT._TermOptions.Cat(tmp_term_options);
	}
	if(set_capjoin) {
		GPT._TermOptions.Cat(cairo_params->linecap == ROUNDED ? " rounded" : cairo_params->linecap == BUTT ? " butt" : " square");
	}
	if(cairo_params->lw != cairo_params_default->lw) {
		snprintf(tmp_term_options, sizeof(tmp_term_options), " linewidth %g", cairo_params->lw);
		GPT._TermOptions.Cat(tmp_term_options);
	}
	if(cairo_params->ps != 1.0) {
		snprintf(tmp_term_options, sizeof(tmp_term_options), " pointscale %g", cairo_params->ps);
		GPT._TermOptions.Cat(tmp_term_options);
	}

	if(cairo_params->dash_length != cairo_params_default->dash_length) {
		snprintf(tmp_term_options, sizeof(tmp_term_options), " dashlength %g", cairo_params->dash_length);
		GPT._TermOptions.Cat(tmp_term_options);
	}
	// sync settings with ps_params for latex terminal 
#ifdef PSLATEX_DRIVER
	if(ISCAIROLATEX) {
		ps_params->color = !cairo_params->mono;
		ps_params->font[sizeof(ps_params->font)-1] = '\0';
		if(!cairo_params->fontname)
			ps_params->font[0] = '\0';
		else
			strncpy(ps_params->font, cairo_params->fontname, sizeof(ps_params->font) - 1);
		ps_params->fontsize = cairo_params->fontsize;
		ps_params->fontscale = cairo_params->fontscale;
		if(cairo_params->transparent) {
			ps_params->background.r = -1;
			ps_params->background.g = -1;
			ps_params->background.b = -1;
		}
		else {
			ps_params->background.r = cairo_params->background.r;
			ps_params->background.g = cairo_params->background.g;
			ps_params->background.b = cairo_params->background.b;
		}
	}
#endif

#ifdef HAVE_WEBP
	// Add webp-specific options to the terminal option string 
	if(sstreq(pThis->name, "webp"))
		WEBP_options(pThis, pGp, 1);
#endif
}
//
// "Called once, when the device is first selected."
// Is the 'main' function of the terminal. 
//
void cairotrm_init(GpTermEntry_Static * pThis)
{
	GnuPlot * p_gp = pThis->P_Gp;
	cairo_surface_t * surface = NULL;
	FPRINTF((stderr, "Init\n"));
	// do a sanity check for once 
	if(!sstreq(pThis->name, "epscairo") && !sstreq(pThis->name, "cairolatex") && !sstreq(pThis->name, "pdfcairo") && !sstreq(pThis->name, "pngcairo"))
		p_gp->IntErrorCurToken("Unrecognized cairo terminal");
	// cairolatex requires a file 
	if(ISCAIROLATEX && !GPT.P_OutStr)
		p_gp->OsError(p_gp->Pgm.GetCurTokenIdx(), "cairolatex terminal cannot write to standard output");
	// We no longer rely on this having been done in cairotrm_reset() 
	if(plot.cr)
		cairo_destroy(plot.cr);
	// initialisations 
	gp_cairo_initialize_plot(&plot);
	plot.device_xmax = static_cast<int>(cairo_params->width);
	plot.device_ymax = static_cast<int>(cairo_params->height);
	plot.dashlength = cairo_params->dash_length;
	if(sstreq(pThis->name, "pdfcairo")) {
		// Output can either be a file or stdout 
#ifndef _WIN32
		if(!GPT.P_OutStr) {
#endif
		// Redirection to a printer on Windows requires the use of gpoutfile via the cairostream_write callback.
		surface = cairo_pdf_surface_create_for_stream((cairo_write_func_t)cairostream_write, cairostream_error,
			plot.device_xmax /*double width_in_points*/, plot.device_ymax /*double height_in_points*/);
#ifndef _WIN32
	}
	else {
		surface = cairo_pdf_surface_create(GPT.P_OutStr, plot.device_xmax /*double width_in_points*/, plot.device_ymax /*double height_in_points*/);
	}
#endif
		// it is up to the pdf viewer to do the hinting 
		plot.hinting = 0;
		// disable OPERATOR_SATURATE, not implemented in cairo pdf backend.
		// Unfortunately the can result in noticeable seams between adjacent
		// polygons. 
		plot.polygons_saturate = FALSE;
		plot.dashlength /= 2; // Empirical correction to make pdf output look more like wxt and png 
	}
	else if(sstreq(pThis->name, "pngcairo")) {
		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, plot.device_xmax /*double width_in_points*/, plot.device_ymax /*double height_in_points*/);
		plot.hinting = 100; // png is bitmapped, let's do the full hinting 
		plot.polygons_saturate = true; // png is produced by cairo "image" backend, which has full support of OPERATOR_SATURATE 
	}
#ifdef HAVE_CAIROEPS
	else if(sstreq(pThis->name, "epscairo")) {
		// Output can either be a file or stdout 
		if(!GPT.P_OutStr) {
			surface = cairo_ps_surface_create_for_stream((cairo_write_func_t)cairostream_write, cairostream_error,
				plot.device_xmax /*double width_in_points*/, plot.device_ymax /*double height_in_points*/);
		}
		else {
			surface = cairo_ps_surface_create(GPT.P_OutStr, plot.device_xmax /*double width_in_points*/, plot.device_ymax /*double height_in_points*/);
		}
		cairo_ps_surface_set_eps(surface, TRUE);
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 11, 1)
		if(!cairo_params->crop) {
			// Normally, cairo clips the bounding box of eps files, but
			// newer versions of Cairo let us enforce the bounding box. See
			// http://old.nabble.com/EPS-target%3A-force-boundingbox-td29050435.html 
			char bb[100];
			sprintf(bb, "%%%%BoundingBox: 0 0 %i %i", plot.device_xmax, plot.device_ymax);
			cairo_ps_surface_dsc_comment(surface, bb);
			cairo_ps_surface_dsc_begin_page_setup(surface);
			sprintf(bb, "%%%%PageBoundingBox: 0 0 %i %i", plot.device_xmax, plot.device_ymax);
			cairo_ps_surface_dsc_comment(surface, bb);
		}
#endif
		/* it is up to the pdf viewer to do the hinting */
		plot.hinting = 0;
		/* disable OPERATOR_SATURATE, not implemented in cairo pdf backend,
		 * results in bitmap fallback. However, polygons are drawn with seams
		 * between each other. */
		plot.polygons_saturate = FALSE;
		/* Empirical correction to make pdf output look more like wxt and png */
		plot.dashlength /= 2;
	}
#endif /* HAVE_CAIROEPS */
#ifdef PSLATEX_DRIVER
	else if(ISCAIROLATEX) {
		// Output can only go to a stream 
		switch(cairo_params->output) {
			default:
			case CAIROLATEX_PDF:
			    EPSLATEX_reopen_output(pThis, "pdf");
			    surface = cairo_pdf_surface_create_for_stream((cairo_write_func_t)cairostream_write, cairostream_error,
				    plot.device_xmax /*double width_in_points*/, plot.device_ymax /*double height_in_points*/);
			    break;
			case CAIROLATEX_PNG:
			    EPSLATEX_reopen_output(pThis, "png");
			    /* We manually scale up to 300 dpi instead of the default 72 dpi */
			    plot.upsampling_rate = cairo_params->resolution / 72.0;
			    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
				    (int)floor(plot.device_xmax * plot.upsampling_rate) /*double width_in_points*/,
				    (int)floor(plot.device_ymax * plot.upsampling_rate) /*double height_in_points*/);
			    break;
#ifdef HAVE_CAIROEPS
			case CAIROLATEX_EPS:
			    EPSLATEX_reopen_output(pThis, "eps");
			    surface = cairo_ps_surface_create_for_stream((cairo_write_func_t)cairostream_write, cairostream_error,
				    plot.device_xmax /*double width_in_points*/, plot.device_ymax /*double height_in_points*/);
			    cairo_ps_surface_set_eps(surface, TRUE);
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 11, 1)
			    {
				    /* Always enforce the bounding box. */
				    char bb[100];
				    sprintf(bb, "%%%%BoundingBox: 0 0 %i %i", plot.device_xmax, plot.device_ymax);
				    cairo_ps_surface_dsc_comment(surface, bb);
				    cairo_ps_surface_dsc_begin_page_setup(surface);
				    sprintf(bb, "%%%%PageBoundingBox: 0 0 %i %i", plot.device_xmax, plot.device_ymax);
				    cairo_ps_surface_dsc_comment(surface, bb);
			    }
#endif
			    break;
#endif
		}
		switch(cairo_params->output) {
			case CAIROLATEX_PNG:
			    // png is bitmapped, let's do the full hinting 
			    plot.hinting = 100;
			    // png is produced by cairo "image" backend, which has full support of OPERATOR_SATURATE 
			    plot.polygons_saturate = TRUE;
			    break;
			default:
			    /* it is up to the pdf viewer to do the hinting */
			    plot.hinting = 0;
			    /* disable OPERATOR_SATURATE, not implemented in cairo pdf backend,
			     * results in bitmap fallback. However, polygons are drawn with seams
			     * between each other. */
			    plot.polygons_saturate = FALSE;
			    /* Empirical correction to make pdf output look more like wxt and png */
			    plot.dashlength /= 2;
			    break;
		}
	}
#endif
	plot.cr = cairo_create(surface);
	cairo_surface_destroy(surface);
	FPRINTF((stderr, "status = %s\n", cairo_status_to_string(cairo_status(plot.cr))));
	FPRINTF((stderr, "Init finished \n"));
}
//
// "Called just before a plot is going to be displayed."
// Should clear the terminal. 
//
void cairotrm_graphics(GpTermEntry_Static * pThis)
{
	/* Initialize background */
	plot.background.r = cairo_params->background.r;
	plot.background.g = cairo_params->background.g;
	plot.background.b = cairo_params->background.b;
	gp_cairo_set_background(cairo_params->background);
	if(ISCAIROLATEX || cairo_params->transparent)
		gp_cairo_clear_background(&plot);
	else
		gp_cairo_solid_background(&plot);
	// update the window scale factor first, cairo needs it 
	plot.xscale = 1.0;
	plot.yscale = 1.0;
	// We manually scale up the PNG for higher pixel density 
	if(ISCAIROLATEX && cairo_params->output == CAIROLATEX_PNG) {
		plot.xscale = plot.upsampling_rate;
		plot.yscale = plot.upsampling_rate;
	}
	// update graphics state properties 
	plot.linecap = cairo_params->linecap;
	FPRINTF((stderr, "Graphics1\n"));
	// set the transformation matrix of the context, and other details 
	// depends on plot.xscale and plot.yscale 
	gp_cairo_initialize_context(&plot);
	// set or refresh terminal size according to the window size 
	// oversampling_scale is updated in gp_cairo_initialize_context 
	plot.xmax = (uint)plot.device_xmax*plot.oversampling_scale;
	plot.ymax = (uint)plot.device_ymax*plot.oversampling_scale;
	// initialize encoding 
	plot.encoding = GPT._Encoding;
	// Adjust for the mismatch of floating point coordinate range 0->max	
	// and integer terminal pixel coordinates [0:max-1].			
	pThis->SetMax((plot.device_xmax - 1) * plot.oversampling_scale, (plot.device_ymax - 1) * plot.oversampling_scale);
	pThis->tscale = plot.oversampling_scale;
#ifdef _WIN32
	// On Windows, always explicitly set the resolution to 96dpi to avoid applying the "text scaling" factor. 
	gp_cairo_set_resolution(96);
#endif
	// set font details (ChrH, ChrV) according to settings 
	cairotrm_set_font(pThis, "");
	pThis->SetTic((uint)(pThis->CV()/2.5));
#ifdef PSLATEX_DRIVER
	// Init latex output, requires terminal size 
	if(ISCAIROLATEX)
		EPSLATEX_common_init(pThis);
#endif
#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 11, 1)
	// Put "invisible" points in two corners to enforce bounding box. 
	if((ISCAIROLATEX) || ((cairo_params->terminal == CAIROTERM_EPS) && !cairo_params->crop)) {
		cairo_set_line_width(plot.cr, 1);
		cairo_set_source_rgb(plot.cr, cairo_params->background.r, cairo_params->background.g, cairo_params->background.b);
		cairo_move_to(plot.cr, 0, 0);
		cairo_line_to(plot.cr, 1, 0);
		cairo_move_to(plot.cr, plot.xmax - 1, plot.ymax);
		cairo_line_to(plot.cr, plot.xmax, plot.ymax);
		cairo_stroke(plot.cr);
	}
#endif
	FPRINTF((stderr, "Graphics xmax %d ymax %d ChrV %d ChrH %d\n", pThis->MaxX, pThis->MaxY, pThis->CV(), pThis->CH()));
}

/* cairo mechanism to allow writing to an output stream */
cairo_status_t cairostream_write(void * closure, uchar * data, uint length)
{
	// in case of a cairolatex terminal, output is redirected to a secondary file 
	if(length != fwrite(data, 1, length, (ISCAIROLATEX) ? GPT.P_GpPsFile : GPT.P_GpOutFile))
		return CAIRO_STATUS_WRITE_ERROR;
	return CAIRO_STATUS_SUCCESS;
}

void cairopng_write_cropped_image(cairo_surface_t * surface)
{
	uchar * data = cairo_image_surface_get_data(surface);
	const int width = cairo_image_surface_get_width(surface);
	const int height = cairo_image_surface_get_height(surface);
	const int stride = cairo_image_surface_get_stride(surface);
	int i, j, x1 = 0, y1 = 0, x2 = width, y2 = height;
	uint32 * row;
	uint32 BG = (cairo_params->transparent) ? 0x0 : ~0x0;
	// Row-wise, top-down iteration 
	for(i = 0; i < height; i++) {
		row = (uint32*)(data + i * stride);
		for(j = 0; j < width; j++) {
			if(row[j] != BG) {
				y1 = i;
				goto found_y1;
			}
		}
	}
found_y1:
	// Row-wise, bottom-up iteration 
	for(i = height - 1; i >= y1; i--) {
		row = (uint32*)(data + i * stride);
		for(j = 0; j < width; j++) {
			if(row[j] != BG) {
				y2 = i;
				goto found_y2;
			}
		}
	}
found_y2:
	// Column-wise, left-to-right iteration 
	for(j = 0; j < width; j++) {
		for(i = y1; i <= y2; i++) {
			row = (uint32 *)(data + i * stride);
			if(row[j] != BG) {
				x1 = j;
				goto found_x1;
			}
		}
	}
found_x1:
	/* Column-wise, right-to-left iteration */
	for(j = width - 1; j >= x1; j--) {
		for(i = y1; i <= y2; i++) {
			row = (uint32 *)(data + i * stride);
			if(row[j] != BG) {
				x2 = j;
				goto found_x2;
			}
		}
	}
found_x2:
	{
		const int padding = 10;
		const int clip_width = MIN(x2 - x1 + padding, width);
		const int clip_height = MIN(y2 - y1 + padding, height);
		cairo_surface_t * clip = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, clip_width, clip_height);
		cairo_t * clip_cr = cairo_create(clip);
		cairo_set_source_surface(clip_cr, surface, -MAX(x1 - padding / 2, 0), -MAX(y1 - padding / 2, 0));
		cairo_rectangle(clip_cr, 0, 0, clip_width, clip_height);
		cairo_fill(clip_cr);
		// Write out the clipped version of the surface and clean up 
		cairo_surface_write_to_png_stream(clip, (cairo_write_func_t)cairostream_write, cairostream_error);
		cairo_surface_destroy(clip);
		cairo_destroy(clip_cr);
	}
}

void cairotrm_text(GpTermEntry_Static * pThis)
{
	FPRINTF((stderr, "Text0\n"));
	// don't forget to stroke the last path if vector was the last command 
	gp_cairo_stroke(&plot);
	// and don't forget to draw the polygons if draw_polygon was the last command 
	gp_cairo_end_polygon(&plot);
	FPRINTF((stderr, "status = %s\n", cairo_status_to_string(cairo_status(plot.cr))));
	// finish the page - cairo_destroy still has to be called for the whole documentation to be written 
	cairo_show_page(plot.cr);
	if(sstreq(pThis->name, "pngcairo") || (sstreq(pThis->name, "cairolatex") && cairo_params->output == CAIROLATEX_PNG)) {
		cairo_surface_t * surface = cairo_get_target(plot.cr);
		if(cairo_params->crop)
			cairopng_write_cropped_image(surface);
		else
			cairo_surface_write_to_png_stream(surface, (cairo_write_func_t)cairostream_write, cairostream_error);
	}
	FPRINTF((stderr, "status = %s\n", cairo_status_to_string(cairo_status(plot.cr))));
	FPRINTF((stderr, "Text finished\n"));
}
//
// sent when gnuplot exits and when the terminal or the output change.
//
void cairotrm_reset(GpTermEntry_Static * pThis)
{
	if(plot.cr)
		cairo_destroy(plot.cr);
	plot.cr = NULL;
#ifdef PSLATEX_DRIVER
	/* finish latex output */
	if(ISCAIROLATEX)
		PSLATEX_reset(pThis);
#endif
	FPRINTF((stderr, "cairotrm_reset\n"));
}

void cairotrm_move(GpTermEntry_Static * pThis, uint x, uint y)
{
	gp_cairo_move(&plot, x, pThis->MaxY - y);
}

void cairotrm_vector(GpTermEntry_Static * pThis, uint x, uint y)
{
	gp_cairo_vector(&plot, x, pThis->MaxY - y);
}

void cairotrm_put_text(GpTermEntry_Static * pThis, uint x, uint y, const char * string)
{
	GnuPlot * p_gp = pThis->P_Gp;
	if(!isempty(string)) {
		// if ignore_enhanced_text is set, draw with the normal routine.
		// This is meant to avoid enhanced syntax when the enhanced mode is on 
		if(p_gp->Enht.Ignore || !cairo_params->enhanced) {
			gp_cairo_draw_text(&plot, x, pThis->MaxY - y, string, NULL, NULL);
			return;
		}
		// If there are no mark-up characters we can use the normal routine 
		if(!strpbrk(string, "{}^_@&~") && !contains_unicode(string)) {
			gp_cairo_draw_text(&plot, x, pThis->MaxY - y, string, NULL, NULL);
			return;
		}
		/* Uses enhanced_recursion() to analyse the string to print.
		 * enhanced_recursion() calls _enhanced_open() to initialize the text drawing,
		 * then it calls _enhanced_writec() which buffers the characters to draw,
		 * and finally _enhanced_flush() to draw the buffer with the correct justification.
		 */
		gp_cairo_enhanced_init(&plot, strlen(string));
		// set up the global variables needed by enhanced_recursion() 
		p_gp->Enht.FontScale = cairo_params->fontscale;
		strncpy(p_gp->Enht.EscapeFormat, "%c", sizeof(p_gp->Enht.EscapeFormat));
		/* Set the recursion going. We say to keep going until a closing brace,
		 * but we don't really expect to find one. If the return value is not
		 * the nul-terminator of the string, that can only mean that we did find
		 * an unmatched closing brace in the string. We increment past it (else
		 * we get stuck in an infinite loop) and try again.
		 */
		while(*(string = enhanced_recursion(pThis, (char *)string, TRUE, cairo_enhanced_fontname, plot.fontsize, 0.0, TRUE, TRUE, 0))) {
			cairotrm_enhanced_flush(pThis);
			// we can only get here if *str == '}' 
			p_gp->EnhErrCheck(string);
			if(!*++string)
				break; /* end of string */
			// else carry on and process the rest of the string 
		}
		// EAM FIXME: I still don't understand why this is needed 
		gp_cairo_clear_bold_font(&plot);
		gp_cairo_enhanced_finish(&plot, x, pThis->MaxY - y); // finish 
	}
}

void cairotrm_enhanced_flush(GpTermEntry_Static * pThis)
{
	gp_cairo_enhanced_flush(&plot);
}

void cairotrm_enhanced_writec(GpTermEntry_Static * pThis, int c)
{
	gp_cairo_enhanced_writec(&plot, c);
}

void cairotrm_enhanced_open(GpTermEntry_Static * pThis, char* fontname, double fontsize, double base, bool widthflag, bool showflag, int overprint)
{
	gp_cairo_enhanced_open(&plot, fontname, fontsize, base, widthflag, showflag, overprint);
}

void cairotrm_linetype(GpTermEntry_Static * pThis, int lt)
{
	gp_cairo_set_linetype(&plot, lt);
	//* Version 5: always set solid lines at this point.  If a dashed 
	// line is wanted there will be a later call to pThis->dashtype() 
	gp_cairo_set_linestyle(&plot,  GP_CAIRO_SOLID);
	if(cairo_params->mono && lt >= -1)
		gp_cairo_set_color(&plot, gp_cairo_linetype2color(-1), 0.0);
	else
		gp_cairo_set_color(&plot, gp_cairo_linetype2color(lt), 0.0);
}

/* - fonts are selected as strings "name,size".
 * - _set_font("") restores the terminal's default font.*/
int cairotrm_set_font(GpTermEntry_Static * pThis, const char * font)
{
	char * fontname = NULL;
	float fontsize = 0.0f;
	if(!isempty(font)) {
		int sep = strcspn(font, ",");
		fontname = sstrdup(font);
		if(font[sep] == ',') {
			sscanf(&(font[sep+1]), "%f", &fontsize);
			fontname[sep] = '\0';
		}
	}
	else {
		fontname = sstrdup("");
	}
	if(isempty(fontname)) {
		SAlloc::F(fontname);
		fontname = isempty(cairo_params->fontname) ? sstrdup(CAIROTRM_DEFAULT_FONTNAME) : sstrdup(cairo_params->fontname);
	}
	if(fontsize == 0.0f)
		fontsize = (cairo_params->fontsize > 0.0f) ? cairo_params->fontsize : cairo_params_default->fontsize;
	// Reset the term variables (hchar, vchar, TicH, TicV). They may be taken into account in next plot commands 
	gp_cairo_set_font(&plot, fontname, fontsize * cairo_params->fontscale);
	gp_cairo_set_termvar(&plot, &(pThis->CV()), &(pThis->CH()));
	// Enhanced text processing needs to know the new font also 
	if(!isempty(fontname)) {
		SAlloc::F(cairo_enhanced_fontname);
		cairo_enhanced_fontname = sstrdup(fontname);
	}
	SAlloc::F(fontname);
	/* the returned int is not used anywhere */
	return 1;
}

int cairotrm_justify_text(GpTermEntry_Static * pThis, enum JUSTIFY mode)
{
	gp_cairo_set_justify(&plot, mode);
	return 1; /* we can justify */
}

void cairotrm_point(GpTermEntry_Static * pThis, uint x, uint y, int pointstyle)
{
	gp_cairo_draw_point(&plot, x, pThis->MaxY - y, pointstyle);
}

void cairotrm_pointsize(GpTermEntry_Static * pThis, double ptsize)
{
	ptsize = (ptsize < 0) ? cairo_params->ps : cairo_params->ps * ptsize;
	gp_cairo_set_pointsize(&plot, ptsize);
}

void cairotrm_linewidth(GpTermEntry_Static * pThis, double lw)
{
	lw *= cairo_params->lw * cairo_params->base_linewidth;
	gp_cairo_set_linewidth(&plot, lw);
}

int cairotrm_text_angle(GpTermEntry_Static * pThis, int angle)
{
	// a double is needed to compute cos, sin, etc. 
	gp_cairo_set_textangle(&plot, (double)angle);
	return 1; // 1 means we can rotate 
}

void cairotrm_fillbox(GpTermEntry_Static * pThis, int style, uint x, uint y, uint width, uint height)
{
	gp_cairo_draw_fillbox(&plot, x, pThis->MaxY - y, width, height, style);
}

int cairotrm_make_palette(GpTermEntry_Static * pThis, t_sm_palette * palette)
{
	return 0; // we can do continuous colors 
}

void cairotrm_set_color(GpTermEntry_Static * pThis, const t_colorspec * colorspec)
{
	GnuPlot * p_gp = pThis->P_Gp;
	rgb_color rgb1;
	double alpha = 0.0;
	if(colorspec->type == TC_LT) {
		rgb1 = gp_cairo_linetype2color(colorspec->lt);
	}
	else if(colorspec->type == TC_FRAC && cairo_params->mono) {
		int save_colorMode = p_gp->SmPltt.colorMode;
		p_gp->SmPltt.colorMode = SMPAL_COLOR_MODE_GRAY;
		p_gp->Rgb1MaxColorsFromGray(colorspec->value, &rgb1);
		p_gp->SmPltt.colorMode = (palette_color_mode)save_colorMode;
	}
	else if(colorspec->type == TC_FRAC) {
		p_gp->Rgb1MaxColorsFromGray(colorspec->value, &rgb1);
	}
	else if(colorspec->type == TC_RGB) {
		rgb1.r = (double)((colorspec->lt >> 16) & 0xff)/255;
		rgb1.g = (double)((colorspec->lt >> 8) & 0xff)/255;
		rgb1.b = (double)((colorspec->lt) & 0xff)/255;
		alpha = (double)((colorspec->lt >> 24) & 0xff)/255;
	}
	else return;
	gp_cairo_set_color(&plot, rgb1, alpha);
#ifdef PSLATEX_DRIVER
	PSLATEX_opacity = 1.0 - alpha; // Used by EPSLATEX_boxed_text 
#endif
}
//
// here we send the polygon command 
//
void cairotrm_filled_polygon(GpTermEntry_Static * pThis, int n, gpiPoint * corners)
{
	gpiPoint * mirrored_corners = (gpiPoint*)SAlloc::M(n*sizeof(gpiPoint));
	// can't use memcpy() here, as we have to mirror the y axis 
	gpiPoint * corners_copy = mirrored_corners;
	while(corners_copy < (mirrored_corners + n)) {
		*corners_copy = *corners++;
		corners_copy->y = pThis->MaxY - corners_copy->y;
		++corners_copy;
	}
	gp_cairo_draw_polygon(&plot, n, mirrored_corners);
	SAlloc::F(mirrored_corners);
}

void cairotrm_image(GpTermEntry_Static * pThis, uint M, uint N, coordval * image, gpiPoint * corner, t_imagecolor color_mode)
{
	/* This routine is to plot a pixel-based image on the display device.
	   'M' is the number of pixels along the y-dimension of the image and
	   'N' is the number of pixels along the x-dimension of the image.  The
	   coordval pointer 'image' is the pixel values normalized to the range
	   [0:1].  These values should be scaled accordingly for the output
	   device.  They 'image' data starts in the upper left corner and scans
	   along rows finishing in the lower right corner.  If 'color_mode' is
	   IC_PALETTE, the terminal is to use palette lookup to generate color
	   information.  In this scenario the size of 'image' is M*N.  If
	   'color_mode' is IC_RGB, the terminal is to use RGB components.  In
	   this scenario the size of 'image' is 3*M*N.  The data appears in RGB
	   tripples, i.e., image[0] = R(1,1), image[1] = G(1,1), image[2] =
	   B(1,1), image[3] = R(1,2), image[4] = G(1,2), ..., image[3*M*N-1] =
	   B(M,N).  The 'image' is actually an "input" image in the sense that
	   it must also be properly resampled for the output device.  Many output
	   mediums, e.g., PostScript, do this work via various driver functions.
	   To determine the appropriate rescaling, the 'corner' information
	   should be used.  There are four entries in the gpiPoint data array.
	   'corner[0]' is the upper left corner (in terms of plot location) of
	   the outer edge of the image.  Similarly, 'corner[1]' is the lower
	   right corner of the outer edge of the image.  (Outer edge means the
	   outer extent of the corner pixels, not the middle of the corner
	   pixels.)  'corner[2]' is the upper left corner of the visible part
	   of the image, and 'corner[3]' is the lower right corner of the visible
	   part of the image.  The information is provided in this way because
	   often it is necessary to clip a portion of the outer pixels of the
	   image. */
	// we will draw an image, scale and resize it 
	// FIXME add palette support ??? 
	uint * image255 = gp_cairo_helper_coordval_to_chars(pThis, image, M, N, color_mode);
	gp_cairo_draw_image(&plot, image255, corner[0].x, pThis->MaxY - corner[0].y, corner[1].x, pThis->MaxY - corner[1].y,
	    corner[2].x, pThis->MaxY - corner[2].y, corner[3].x, pThis->MaxY - corner[3].y, M, N);
	SAlloc::F(image255);
}

void cairotrm_boxed_text(GpTermEntry_Static * pThis, uint x, uint y, int option)
{
	if(option == TEXTBOX_INIT)
		y = pThis->MaxY - y;
	gp_cairo_boxed_text(&plot, x, y, option);
}

void cairotrm_dashtype(GpTermEntry_Static * pThis, int type, t_dashtype * custom_dash_pattern)
{
	gp_cairo_set_dashtype(&plot, type, custom_dash_pattern);
}

#endif /* TERM_BODY */

#ifdef TERM_TABLE
#ifdef HAVE_CAIROEPS
TERM_TABLE_START(epscairo_driver)
	"epscairo", 
	"eps terminal based on cairo",
	/* the following values are overridden by cairotrm_graphics */
	1 /* xmax */, 
	1 /* ymax */, 
	1 /* vchar */, 
	1 /* hchar */,
	1 /* vtic */, 
	1 /* htic */,
	cairotrm_options, 
	cairotrm_init, 
	cairotrm_reset, 
	cairotrm_text, 
	GnuPlot::NullScale, 
	cairotrm_graphics,
	cairotrm_move, 
	cairotrm_vector, 
	cairotrm_linetype, 
	cairotrm_put_text,
	cairotrm_text_angle, 
	cairotrm_justify_text,
	cairotrm_point, 
	GnuPlot::DoArrow, 
	cairotrm_set_font,
	cairotrm_pointsize,
	TERM_CAN_MULTIPLOT|TERM_BINARY|TERM_CAN_DASH|TERM_ALPHA_CHANNEL|TERM_LINEWIDTH|TERM_FONTSCALE|TERM_POINTSCALE,
	0 /* suspend */, 
	0 /* resume */, 
	cairotrm_fillbox, 
	cairotrm_linewidth,
	#ifdef USE_MOUSE
	0, 
	0, 
	0, 
	0, 
	0,
	#endif
	cairotrm_make_palette, 
	0 /* cairotrm_previous_palette */, 
	cairotrm_set_color, 
	cairotrm_filled_polygon,
	cairotrm_image,
	cairotrm_enhanced_open, 
	cairotrm_enhanced_flush, 
	cairotrm_enhanced_writec,
	0, 
	0, 
	1.0, 
	NULL, /* hypertext */
	cairotrm_boxed_text,
	NULL, /* modify_plots */
	cairotrm_dashtype 
TERM_TABLE_END(epscairo_driver)

#undef LAST_TERM
#define LAST_TERM epscairo_driver
#endif /* HAVE_CAIROEPS */

#ifdef PSLATEX_DRIVER
TERM_TABLE_START(cairolatex_driver)
	"cairolatex", 
	"LaTeX picture environment using graphicx package and Cairo backend",
	/* the following values are overridden by cairotrm_graphics */
	1 /* xmax */, 
	1 /* ymax */, 
	1 /* vchar */, 
	1 /* hchar */,
	1 /* vtic */, 
	1 /* htic */,
	cairotrm_options, 
	cairotrm_init, 
	cairotrm_reset, 
	cairotrm_text, 
	GnuPlot::NullScale, 
	cairotrm_graphics,
	cairotrm_move, 
	cairotrm_vector,
	EPSLATEX_linetype, 
	EPSLATEX_put_text,
	PS_text_angle, 
	PS_justify_text,
	cairotrm_point, 
	GnuPlot::DoArrow, 
	cairotrm_set_font,
	cairotrm_pointsize,
	TERM_CAN_MULTIPLOT|TERM_BINARY|TERM_CAN_DASH|TERM_ALPHA_CHANNEL|TERM_LINEWIDTH|TERM_FONTSCALE|TERM_POINTSCALE|TERM_IS_LATEX,
	0 /* suspend */, 
	0 /* resume */, 
	cairotrm_fillbox, 
	cairotrm_linewidth,
	#ifdef USE_MOUSE
	0, 
	0, 
	0, 
	0, 
	0,     /* no mouse support for postscript */
	#endif
	cairotrm_make_palette,
	0 /* cairotrm_previous_palette */,
	EPSLATEX_set_color, 
	cairotrm_filled_polygon,
	cairotrm_image,
	0, 
	0, 
	0,     /* Enhanced text mode not used */
	EPSLATEX_layer,     /* Used to signal front/back text */
	0, 
	1.0, 
	NULL, /* hypertext */
	EPSLATEX_boxed_text,
	NULL, /* modify_plots */
	cairotrm_dashtype 
TERM_TABLE_END(cairolatex_driver)

#undef LAST_TERM
#define LAST_TERM cairolatex_driver
#endif /* PSLATEX_DRIVER */

TERM_TABLE_START(pdfcairo_driver)
	"pdfcairo", 
	"pdf terminal based on cairo",
	/* the following values are overridden by cairotrm_graphics */
	1 /* xmax */, 
	1 /* ymax */, 
	1 /* vchar */, 
	1 /* hchar */,
	1 /* vtic */, 
	1 /* htic */,
	cairotrm_options, 
	cairotrm_init, 
	cairotrm_reset, 
	cairotrm_text, 
	GnuPlot::NullScale, 
	cairotrm_graphics,
	cairotrm_move, 
	cairotrm_vector, 
	cairotrm_linetype, 
	cairotrm_put_text,
	cairotrm_text_angle, 
	cairotrm_justify_text,
	cairotrm_point, 
	GnuPlot::DoArrow, 
	cairotrm_set_font,
	cairotrm_pointsize,
	TERM_CAN_MULTIPLOT|TERM_BINARY|TERM_CAN_DASH|TERM_ALPHA_CHANNEL|TERM_LINEWIDTH|TERM_FONTSCALE|TERM_POINTSCALE,
	0 /* suspend */, 
	0 /* resume */, 
	cairotrm_fillbox, 
	cairotrm_linewidth,
	#ifdef USE_MOUSE
	0, 
	0, 
	0, 
	0, 
	0,
	#endif
	cairotrm_make_palette, 
	0 /* cairotrm_previous_palette */, 
	cairotrm_set_color, 
	cairotrm_filled_polygon, 
	cairotrm_image, 
	cairotrm_enhanced_open, 
	cairotrm_enhanced_flush, 
	cairotrm_enhanced_writec, 
	0, 
	0, 
	1.0, 
	NULL, /* hypertext */
	cairotrm_boxed_text,
	NULL, /* modify_plots */
	cairotrm_dashtype
TERM_TABLE_END(pdfcairo_driver)

#undef LAST_TERM
#define LAST_TERM pdfcairo_driver

TERM_TABLE_START(pngcairo_driver)
	"pngcairo", 
	"png terminal based on cairo",
	/* the following values are overridden by cairotrm_graphics */
	1 /* xmax */, 
	1 /* ymax */, 
	1 /* vchar */, 
	1 /* hchar */,
	1 /* vtic */, 
	1 /* htic */,
	cairotrm_options, 
	cairotrm_init, 
	cairotrm_reset, 
	cairotrm_text, 
	GnuPlot::NullScale, 
	cairotrm_graphics,
	cairotrm_move, 
	cairotrm_vector, 
	cairotrm_linetype, 
	cairotrm_put_text,
	cairotrm_text_angle, 
	cairotrm_justify_text,
	cairotrm_point, 
	GnuPlot::DoArrow, 
	cairotrm_set_font,
	cairotrm_pointsize,
	TERM_BINARY|TERM_CAN_DASH|TERM_ALPHA_CHANNEL|TERM_LINEWIDTH|TERM_FONTSCALE|TERM_POINTSCALE,
	0 /* suspend */, 
	0 /* resume */, 
	cairotrm_fillbox, 
	cairotrm_linewidth,
	#ifdef USE_MOUSE
	0, 
	0, 
	0, 
	0, 
	0,
	#endif
	cairotrm_make_palette, 
	0 /* cairotrm_previous_palette */, 
	cairotrm_set_color, 
	cairotrm_filled_polygon, 
	cairotrm_image,
	cairotrm_enhanced_open, 
	cairotrm_enhanced_flush, 
	cairotrm_enhanced_writec, 
	0, 
	0, 
	1.0, 
	NULL, // hypertext 
	cairotrm_boxed_text,
	NULL, // modify_plots 
	cairotrm_dashtype 
TERM_TABLE_END(pngcairo_driver)

#undef LAST_TERM
#define LAST_TERM pngcairo_driver

#endif /* TERM_TABLE */
#endif /* TERM_PROTO_ONLY */

#ifdef TERM_HELP
START_HELP(epscairo)
"1 epscairo",
"?set terminal epscairo",
"?terminal epscairo",
"?set term epscairo",
"?term epscairo",
"?epscairo",
" The `epscairo` terminal device generates encapsulated PostScript (*.eps) using",
" the cairo and pango support libraries.  cairo version >= 1.6 is required.",
"",
" Please read the help for the `pdfcairo` terminal."
""
END_HELP(epscairo)
#endif /* TERM_HELP */

#ifdef TERM_HELP
START_HELP(cairolatex)
"1 cairolatex",
"?set terminal cairolatex",
"?terminal cairolatex",
"?set term cairolatex",
"?term cairolatex",
"?cairolatex",
" The `cairolatex` terminal device generates encapsulated PostScript (*.eps),",
" PDF, or PNG output using the cairo and pango support libraries and uses LaTeX",
" for text output using the same routines as the `epslatex` terminal.",
"",
" Syntax:",
"       set terminal cairolatex",
"                      {eps | pdf | png}",
"                      {standalone | input}",
"                      {blacktext | colortext | colourtext}",
"                      {header <header> | noheader}",
"                      {mono|color}",
"                      {{no}transparent} {{no}crop} {background <rgbcolor>}",
"                      {font <font>} {fontscale <scale>}",
"                      {linewidth <lw>} {rounded|butt|square} {dashlength <dl>}",
"                      {size <XX>{unit},<YY>{unit}}",
"                      {resolution <dpi>}",
"",
" The cairolatex terminal prints a plot like `terminal epscairo` or",
" `terminal pdfcairo` but transfers the texts to LaTeX instead of including",
" them in the graph. For reference of options not explained here see `pdfcairo`.",
"",
" `eps`, `pdf`, or `png` select the type of graphics output. Use `eps` with",
" latex/dvips and `pdf` for pdflatex.  If your plot has a huge number",
" of points use `png` to keep the filesize down.  When using the `png` option,",
" the terminal accepts an extra option `resolution` to control the pixel",
" density of the resulting PNG.  The argument of `resolution` is an integer",
" with the implied unit of DPI.",
"",
" `blacktext` forces all text to be written in black even in color mode;",
"",
" The `cairolatex` driver offers a special way of controlling text positioning:",
" (a) If any text string begins with '{', you also need to include a '}' at the",
" end of the text, and the whole text will be centered both horizontally",
" and vertically by LaTeX.  (b) If the text string begins with '[', you need",
" to continue it with: a position specification (up to two out of t,b,l,r,c),",
" ']{', the text itself, and finally, '}'. The text itself may be anything",
" LaTeX can typeset as an LR-box. \\rule{}{}'s may help for best positioning.",
" See also the documentation for the `pslatex` terminal driver.",
" To create multiline labels, use \\shortstack, for example",
"    set ylabel '[r]{\\shortstack{first line \\\\ second line}}'",
"",
" The `back` option of `set label` commands is handled slightly different",
" than in other terminals. Labels using 'back' are printed behind all other",
" elements of the plot while labels using 'front' are printed above",
" everything else.",
"",
" The driver produces two different files, one for the eps, pdf, or png part of",
" the figure and one for the LaTeX part. The name of the LaTeX file is taken",
" from the `set output` command. The name of the eps/pdf/png file is derived by",
" replacing the file extension (normally '.tex') with '.eps'/'.pdf'/'.png'",
" instead. There is no LaTeX output if no output file is given!  Remember to",
" close the `output file` before next plot unless in `multiplot` mode.",
"",
" In your LaTeX documents use '\\input{filename}' to include the figure.",
" The '.eps'/'.pdf'/'.png' file is included by the command",
" \\includegraphics{...}, so you must also include \\usepackage{graphicx} in",
" the LaTeX preamble.  If you want to use coloured text (option `colourtext`)",
" you also have to include \\usepackage{color} in the LaTeX preamble.",
"",
" The behaviour concerning font selection depends on the header mode.",
" In all cases, the given font size is used for the calculation of proper",
" spacing. When not using the `standalone` mode the actual LaTeX font and",
" font size at the point of inclusion is taken, so use LaTeX commands for",
" changing fonts. If you use e.g. 12pt as font size for your LaTeX",
" document, use '\", 12\"' as options. The font name is ignored. If using",
" `standalone` the given font and font size are used, see below for a",
" detailed description.",
"",
" If text is printed coloured is controlled by the TeX booleans \\ifGPcolor",
" and \\ifGPblacktext. Only if \\ifGPcolor is true and \\ifGPblacktext is",
" false, text is printed coloured. You may either change them in the",
" generated TeX file or provide them globally in your TeX file, for example",
" by using",
"    \\newif\\ifGPblacktext",
"    \\GPblacktexttrue",
" in the preamble of your document. The local assignment is only done if no",
" global value is given.",
"",
" When using the cairolatex terminal give the name of the TeX file in the",
" `set output` command including the file extension (normally \".tex\").",
" The graph filename is generated by replacing the extension.",
"",
" If using the `standalone` mode a complete LaTeX header is added to the",
" LaTeX file; and \"-inc\" is added to the filename of the gaph file.",
" The `standalone` mode generates a TeX file that produces",
" output with the correct size when using dvips, pdfTeX, or VTeX.",
" The default, `input`, generates a file that has to be included into a",
" LaTeX document using the \\input command.",
"",
" If a font other than \"\" or \"default\" is given it is interpreted as",
" LaTeX font name.  It contains up to three parts, separated by a comma:",
" 'fontname,fontseries,fontshape'.  If the default fontshape or fontseries",
" are requested, they can be omitted.  Thus, the real syntax for the fontname",
" is '{fontname}{,fontseries}{,fontshape}'.  The naming convention for all",
" parts is given by the LaTeX font scheme.  The fontname is 3 to 4 characters",
" long and is built as follows: One character for the font vendor, two",
" characters for the name of the font, and optionally one additional",
" character for special fonts, e.g., 'j' for fonts with old-style numerals",
" or 'x' for expert fonts. The names of many fonts is described in",
"^ <a href=\"http://www.tug.org/fontname/fontname.pdf\">",
"           http://www.tug.org/fontname/fontname.pdf",
"^ </a>",
" For example, 'cmr' stands for Computer Modern Roman, 'ptm' for Times-Roman,",
" and 'phv' for Helvetica.  The font series denotes the thickness of the",
" glyphs, in most cases 'm' for normal (\"medium\") and 'bx' or 'b' for bold",
" fonts.  The font shape is 'n' for upright, 'it' for italics, 'sl' for",
" slanted, or 'sc' for small caps, in general.  Some fonts may provide",
" different font series or shapes.",
"",
" Examples:",
"",
" Use Times-Roman boldface (with the same shape as in the surrounding text):",
"       set terminal cairolatex font 'ptm,bx'",
" Use Helvetica, boldface, italics:",
"       set terminal cairolatex font 'phv,bx,it'",
" Continue to use the surrounding font in slanted shape:",
"       set terminal cairolatex font ',,sl'",
" Use small capitals:",
"       set terminal cairolatex font ',,sc'",
"",
" By this method, only text fonts are changed. If you also want to change",
" the math fonts you have to use the \"gnuplot.cfg\" file or the `header`",
" option, described below.",
"",
" In `standalone` mode, the font size is taken from the given font size in the",
" `set terminal` command. To be able to use a specified font size, a file",
" \"size<size>.clo\" has to reside in the LaTeX search path.  By default,",
" 10pt, 11pt, and 12pt are supported.  If the package \"extsizes\" is",
" installed, 8pt, 9pt, 14pt, 17pt, and 20pt are added.",
"",
" The `header` option takes a string as argument.  This string is written",
" into the generated LaTeX file.  If using the `standalone` mode, it is",
" written into the preamble, directly before the \\begin{document} command.",
" In the `input` mode, it is placed directly after the \\begingroup command",
" to ensure that all settings are local to the plot.",
"",
" Examples:",
"",
" Use T1 fontencoding, change the text and math font to Times-Roman as well",
" as the sans-serif font to Helvetica:",
"     set terminal cairolatex standalone header \\",
"     \"\\\\usepackage[T1]{fontenc}\\n\\\\usepackage{mathptmx}\\n\\\\usepackage{helvet}\"",
" Use a boldface font in the plot, not influencing the text outside the plot:",
"     set terminal cairolatex input header \"\\\\bfseries\"",
"",
" If the file \"gnuplot.cfg\" is found by LaTeX it is input in the preamble",
" the LaTeX document, when using `standalone` mode.  It can be used for",
" further settings, e.g., changing the document font to Times-Roman,",
" Helvetica, and Courier, including math fonts (handled by \"mathptmx.sty\"):",
"       \\usepackage{mathptmx}",
"       \\usepackage[scaled=0.92]{helvet}",
"       \\usepackage{courier}",
" The file \"gnuplot.cfg\" is loaded before the header information given",
" by the `header` command.  Thus, you can use `header` to overwrite some of",
" settings performed using \"gnuplot.cfg\"",
""
END_HELP(cairolatex)
#endif /* TERM_HELP */

#ifdef TERM_HELP
START_HELP(pdfcairo)
"1 pdfcairo",
"?set terminal pdfcairo",
"?terminal pdfcairo",
"?set term pdfcairo",
"?term pdfcairo",
"?pdfcairo",
" The `pdfcairo` terminal device generates output in pdf. The actual",
" drawing is done via cairo, a 2D graphics library, and pango, a library for",
" laying out and rendering text.",
"",
" Syntax:",
"         set term pdfcairo",
"                      {{no}enhanced} {mono|color}",
"                      {font <font>} {fontscale <scale>}",
"                      {linewidth <lw>} {rounded|butt|square} {dashlength <dl>}",
"                      {background <rgbcolor>}",
"                      {size <XX>{unit},<YY>{unit}}",
"",
" This terminal supports an enhanced text mode, which allows font and other",
" formatting commands (subscripts, superscripts, etc.) to be embedded in labels",
" and other text strings. The enhanced text mode syntax is shared with other",
" gnuplot terminal types. See `enhanced` for more details.",
"",
" The width of all lines in the plot can be modified by the factor <lw>",
" specified in `linewidth`. The default linewidth is 0.5 points.",
" (1 \"PostScript\" point = 1/72 inch = 0.353 mm)",
"",
" `rounded` sets line caps and line joins to be rounded;",
" `butt` is the default, butt caps and mitered joins.",
"",
" The default size for the output is 5 inches x 3 inches. The `size` option",
" changes this to whatever the user requests. By default the X and Y sizes are",
" taken to be in inches, but other units are possible (currently only cm).",
" Screen coordinates always run from 0.0 to 1.0 along the full length of the",
" plot edges as specified by the `size` option.",
"",
" <font> is in the format \"FontFace,FontSize\", i.e. the face and the size",
" comma-separated in a single string. FontFace is a usual font face name, such",
" as \'Arial\'. If you do not provide FontFace, the pdfcairo terminal will use",
" \'Sans\'. FontSize is the font size, in points. If you do not provide it,",
" the pdfcairo terminal will use a nominal font size of 12 points.",
" However, the default fontscale parameter for this terminal is 0.5,",
" so the apparent font size is smaller than this if the pdf output is",
" viewed at full size.",
"    For example :",
"       set term pdfcairo font \"Arial,12\"",
"       set term pdfcairo font \"Arial\" # to change the font face only",
"       set term pdfcairo font \",12\" # to change the font size only",
"       set term pdfcairo font \"\" # to reset the font name and size",
"",
" The fonts are retrieved from the usual fonts subsystems. Under Windows,",
" those fonts are to be found and configured in the entry \"Fonts\" of the",
" control panel. Under UNIX, they are handled by \"fontconfig\".",
"",
" Pango, the library used to layout the text, is based on utf-8. Thus, the pdfcairo",
" terminal has to convert from your encoding to utf-8. The default input",
" encoding is based on your \'locale\'. If you want to use another encoding,",
" make sure gnuplot knows which one you are using. See `encoding` for more",
" details.",
"",
" Pango may give unexpected results with fonts that do not respect the unicode",
" mapping. With the Symbol font, for example, the pdfcairo terminal will use the map",
" provided by http://www.unicode.org/ to translate character codes to unicode.",
" Note that \"the Symbol font\" is to be understood as the Adobe",
" Symbol font, distributed with Acrobat Reader as \"SY______.PFB\".",
" Alternatively, the OpenSymbol font, distributed with OpenOffice.org as",
" \"opens___.ttf\", offers the same characters. Microsoft has distributed a",
" Symbol font (\"symbol.ttf\"), but it has a different character set with",
" several missing or moved mathematic characters. If you experience problems",
" with your default setup (if the demo enhancedtext.dem is not displayed",
" properly for example), you probably have to install one of the Adobe or",
" OpenOffice Symbol fonts, and remove the Microsoft one.",
" Other non-conform fonts, such as \"wingdings\" have been observed working.",
"",
" The rendering of the plot cannot be altered yet. To obtain the best output",
" possible, the rendering involves two mechanisms : antialiasing and",
" oversampling.",
" Antialiasing allows to display non-horizontal and non-vertical lines",
" smoother.",
" Oversampling combined with antialiasing provides subpixel accuracy,",
" so that gnuplot can draw a line from non-integer coordinates. This avoids",
" wobbling effects on diagonal lines ('plot x' for example).",
""
END_HELP(pdfcairo)
#endif /* TERM_HELP */

#ifdef TERM_HELP
START_HELP(pngcairo)
"1 pngcairo",
"?set terminal pngcairo",
"?terminal pngcairo",
"?set term pngcairo",
"?term pngcairo",
"?pngcairo",
" The `pngcairo` terminal device generates output in png. The actual",
" drawing is done via cairo, a 2D graphics library, and pango, a library for",
" laying out and rendering text.",
"",
" Syntax:",
"         set term pngcairo",
"                      {{no}enhanced} {mono|color}",
"                      {{no}transparent} {{no}crop} {background <rgbcolor>",
"                      {font <font>} {fontscale <scale>}",
"                      {linewidth <lw>} {rounded|butt|square} {dashlength <dl>}",
"                      {pointscale <ps>}",
"                      {size <XX>{unit},<YY>{unit}}",
"",
" This terminal supports an enhanced text mode, which allows font and other",
" formatting commands (subscripts, superscripts, etc.) to be embedded in labels",
" and other text strings. The enhanced text mode syntax is shared with other",
" gnuplot terminal types. See `enhanced` for more details.",
"",
" The width of all lines in the plot can be modified by the factor <lw>.",
"",
" `rounded` sets line caps and line joins to be rounded;",
" `butt` is the default, butt caps and mitered joins.",
"",
" The default size for the output is 640 x 480 pixels. The `size` option",
" changes this to whatever the user requests. By default the X and Y sizes are",
" taken to be in pixels, but other units are possible (currently cm and inch).",
" A size given in centimeters or inches will be converted into pixels assuming",
" a resolution of 72 dpi. Screen coordinates always run from 0.0 to 1.0 along",
" the full length of the plot edges as specified by the `size` option.",
"",
" <font> is in the format \"FontFace,FontSize\", i.e. the face and the size",
" comma-separated in a single string. FontFace is a usual font face name, such",
" as \'Arial\'. If you do not provide FontFace, the pngcairo terminal will use",
" \'Sans\'. FontSize is the font size, in points. If you do not provide it,",
" the pngcairo terminal will use a size of 12 points.",
"    For example :",
"       set term pngcairo font \"Arial,12\"",
"       set term pngcairo font \"Arial\" # to change the font face only",
"       set term pngcairo font \",12\" # to change the font size only",
"       set term pngcairo font \"\" # to reset the font name and size",
"",
" The fonts are retrieved from the usual fonts subsystems. Under Windows,",
" those fonts are to be found and configured in the entry \"Fonts\" of the",
" control panel. Under UNIX, they are handled by \"fontconfig\".",
"",
" Pango, the library used to layout the text, is based on utf-8. Thus, the pngcairo",
" terminal has to convert from your encoding to utf-8. The default input",
" encoding is based on your \'locale\'. If you want to use another encoding,",
" make sure gnuplot knows which one you are using. See `encoding` for more detail.",
"",
" Pango may give unexpected results with fonts that do not respect the unicode",
" mapping. With the Symbol font, for example, the pngcairo terminal will use the map",
" provided by http://www.unicode.org/ to translate character codes to unicode.",
" Note that \"the Symbol font\" is to be understood as the Adobe",
" Symbol font, distributed with Acrobat Reader as \"SY______.PFB\".",
" Alternatively, the OpenSymbol font, distributed with OpenOffice.org as",
" \"opens___.ttf\", offers the same characters. Microsoft has distributed a",
" Symbol font (\"symbol.ttf\"), but it has a different character set with",
" several missing or moved mathematic characters. If you experience problems",
" with your default setup (if the demo enhancedtext.dem is not displayed",
" properly for example), you probably have to install one of the Adobe or",
" OpenOffice Symbol fonts, and remove the Microsoft one.",
"",
" Rendering uses oversampling, antialiasing, and font hinting to the extent",
" supported by the cairo and pango libraries.",
""
END_HELP(pngcairo)
#endif // TERM_HELP 
#endif // } HAVE_CAIROPDF /
