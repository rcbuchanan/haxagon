#ifndef SVG_H_
#define SVG_H_

#include <stdint.h>

#include "vec2.h"


typedef enum DrawMode_t {
	FillMode,
	OutlineMode
} DrawMode;

typedef struct SvgColor_t {
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
} SvgColor;

void svgopen (const char *basepath, int width, int height);

void svgclose ();

void svgcircle (vec2 pos, float rad, SvgColor *color, float title, DrawMode mode);

void svgline (vec2 *p, SvgColor *color);

void svgtriangle (vec2 *p, SvgColor *color, DrawMode mode);

void svgnexthue (SvgColor *color);

#endif
