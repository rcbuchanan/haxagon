#pragma once

#define svgopen(basepath, width, height)
#define svgclose()
#define svgcircle(pos, rad, color, title, mode)
#define svgline(p, color)
#define svgtriangle(p, color, mode)
#define svgnexthue(color);
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
