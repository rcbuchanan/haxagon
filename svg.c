#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "svg.h"


static FILE *svgfile = NULL;


void
svgopen (const char *basepath, int width, int height)
{
	static char path[255];
	static int svgcount = 0;
	const char *s =
	    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
	    "<svg viewBox=\"%d %d %d %d\" version=\"1.1\" "
	    "xmlns=\"http://www.w3.org/2000/svg\">\n"
	    "<script type=\"text/javascript\"><![CDATA[\n\n"
	    "function onload () {"
	    "window.addEventListener('keydown', function(e) {\n"
	    "if (e.key == 'ArrowLeft') "
	    "window.location.href = 'file://%s%08d.svg';\n"
	    "else if (e.key == 'ArrowRight')\n"
	    "window.location.href = 'file://%s%08d.svg';\n"
	    "});\n"
	    "}\nonload();\n]]></script>\n";
	
	sprintf(path, "%s%08d.svg", basepath, svgcount++);
	svgfile = fopen(path, "w");
	fprintf(svgfile, s, 0, 0, width, height, basepath, svgcount - 2, basepath, svgcount);
}

void
svgclose ()
{
	fprintf(svgfile, "</svg>\n");
	fclose(svgfile);
	svgfile = NULL;
}

void
svgcircle (vec2 pos, float rad, SvgColor *color, float title, DrawMode mode)
{
	const char *s0 = "<circle cx=\"%f\" cy=\"%f\" r=\"%f\" text=\"%f\" ";
	const char *s1 = "fill=\"none\" stroke=\"rgb(%d, %d, %d)\" fill-opacity=\"%f\" />\n";
	const char *s2 = "fill=\"rgb(%d, %d, %d)\" fill-opacity=\"%f\" stroke=\"none\" />\n";
	float a = color->a / 255.0;

	fprintf(svgfile, s0, pos.x, pos.y, rad, title);
	if (mode == OutlineMode)
		fprintf(svgfile, s1, color->r, color->g, color->b, a);
	else
		fprintf(svgfile, s2, color->r, color->g, color->b, a);
}

void
svgline (vec2 *p, SvgColor *color)
{
	const char *s =
	    "<path d=\"M %f %f L %f %f\" "
	    "stroke=\"rgb(%d, %d, %d)\" stroke-opacity=\"%f\" "
	    "stroke-width=\"1\" fill=\"none\" "
	    //"marker-start=\"url(#Arrow1Lstart)\" />\n";
	    " />\n";
	float a = color->a / 255.0;
	fprintf(svgfile, s, p[0].x, p[0].y, p[1].x, p[1].y, color->r, color->g, color->b, a);

}

void
svgtriangle (vec2 *p, SvgColor *color, DrawMode mode)
{
	const char *s1 = "<path d=\"M %f %f L %f %f L %f %f Z\" ";
	const char *s2 =
	    "stroke=\"rgb(%d, %d, %d)\" fill-opacity=\"%f\" "
	    "stroke-width=\"1\" fill=\"none\" />\n";
	const char *s3 = "stroke=\"none\" fill=\"rgb(%d, %d, %d)\" fill-opacity=\"%f\" />\n";
	
	fprintf(svgfile, s1, p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
	if (mode == OutlineMode)
		fprintf(svgfile, s2, color->r, color->g, color->b, color->a / 256.0);
	else
		fprintf(svgfile, s3, color->r, color->g, color->b, color->a / 256.0);
}

void svgnexthue (SvgColor *color)
{
	float rgb0[3];
	float hsv0[3];
	float hsv1[3];
	float rgb1[3];
	float cmax, cmin;
	float crange;
	float rgbc, rgbx, rgbm;

	rgb0[0] = color->r / 255.0;
	rgb0[1] = color->g / 255.0;
	rgb0[2] = color->b / 255.0;

	cmin = MIN(MIN(rgb0[0], rgb0[1]), rgb0[2]);
	cmax = MAX(MAX(rgb0[0], rgb0[1]), rgb0[2]);
	crange = cmax - cmin;

	if (crange == 0.0)
		hsv0[0] = 0;
	else if (cmax == rgb0[0])
		hsv0[0] = 60.0 * fmod((rgb0[1] - rgb0[2]) / crange, 6.0);
	else if (cmax == rgb0[1])
		hsv0[0] = 60.0 * ((rgb0[2] - rgb0[0]) / crange + 2.0);
	else
		hsv0[0] = 60.0 * ((rgb0[0] - rgb0[1]) / crange + 4.0);
	hsv0[1] = cmax == 0.0 ? 0.0 : crange / cmax;
	hsv0[2] = cmax;
	
	hsv1[0] = fmod(hsv0[0] + 7.0 * 360.0 / 12.0, 360.0);
	hsv1[1] = hsv0[1];
	hsv1[2] = hsv0[2];

	rgbc = hsv1[2] * hsv1[1];
	rgbx = rgbc * (1 - fabs(fmod(hsv1[0] / 60.0, 2.0) - 1));
	rgbm = hsv0[2] - rgbc;

	if (hsv1[0] < 60.0) {
		rgb1[0] = rgbc;
		rgb1[1] = rgbx;
		rgb1[2] = 0;
	} else if (hsv1[0] < 120.0) {
		rgb1[0] = rgbx;
		rgb1[1] = rgbc;
		rgb1[2] = 0;
	} else if (hsv1[0] < 180.0) {
		rgb1[0] = 0;
		rgb1[1] = rgbc;
		rgb1[2] = rgbx;
	} else if (hsv1[0] < 240.0) {
		rgb1[0] = 0;
		rgb1[1] = rgbx;
		rgb1[2] = rgbc;
	} else if (hsv1[0] < 300.0) {
		rgb1[0] = rgbx;
		rgb1[1] = 0;
		rgb1[2] = rgbc;
	} else {
		rgb1[0] = rgbc;
		rgb1[1] = 0;
		rgb1[2] = rgbx;
	}

	for (unsigned int i = 0; i < sizeof (rgb1) / sizeof (rgb1[0]); i++)
		rgb1[i] = (rgb1[i] + rgbm) * 255.0;

	/* printf("RGB0: %f %f %f\n", rgb0[0], rgb0[1], rgb0[2]); */
	/* printf("HSV0: %f %f %f\n", hsv0[0], hsv0[1], hsv0[2]); */
	/* printf("HSV1: %f %f %f\n", hsv0[0], hsv0[1], hsv0[2]); */
	/* printf("RGB1: %f %f %f\n", rgb1[0], rgb1[1], rgb1[2]); */

	color->r = (int) rgb1[0];
	color->g = (int) rgb1[1];
	color->b = (int) rgb1[2];
}
