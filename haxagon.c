#define _GNU_SOURCE
#include <dlfcn.h>

#include <assert.h>
#include <math.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "vec2.h"

//#include "svg.h"
#include "svgfake.h"


#define VTXPTR_HIJACK_ADDR 0x462cea
#define DRAWARR_HIJACK_ADDR 0x462d8e

enum StepCounts {
	RSteps = 36,
	ThetaSteps = 20,
};
const float RStep = 8.0;
const float ThetaStep = 2.0 * M_PI / ThetaSteps;

enum BufferSizes {
	MaxObsLineCount = 400,
	DirHistoryCount = 10,
};

enum ScoringParams {
	HitPenalty = 1000,
	DangerDistPenaltyRadius = 60,
	MinDistPenaltyRadius = 100,
	MovePenaltyRadius = 100,
	
	MovePenaltyPct = 40,
	DangerDistPenaltyPct = 40,
	MinDistPenaltyPct = 20,
};

enum MagicNumbers {
	MagicPlayerSizeMax = 120,
	MagicModeSwitchCount = 90,
	MagicSmallnessLimit = 15,
	MagicFarnessLimit = 1000,
	MagicInnermostRPadding = 40,
};
const float MagicPlayerAngleTolerance = M_PI / 8.0;

enum ScreenDims {
	ScreenWidth = 768,
	ScreenHeight = 480,
};
static const float HalfWidth = ScreenWidth / 2.0;
static const float HalfHeight = ScreenHeight / 2.0;

enum Motion {
	Leftward,
	Rightward,
	Noneward,
	Keyboardward
};
 
typedef unsigned int    GLenum;
typedef void            GLvoid;
typedef int             GLint;
typedef int             GLsizei;
typedef float           GLfloat;

typedef void (*orig_glVertexPointer_f_type)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (*orig_glDrawArrays_f_type)(GLenum mode, GLint first, GLsizei count);


struct candidate {
	int i;
	int j;
	float cost;
};

struct visitee {
	char valid;
	int i;
	int j;
	int previ;
	int prevj;
	float cost;
};


static GLfloat *stolenvtxdata = 0;
static GLint stolensize;
static GLenum stolentype;
static GLsizei stolenstride;

static float playertheta, playerr;
static enum Motion playerdir = Noneward;

static vec2 obsLines[MaxObsLineCount][2];
static unsigned int obsLineCount;


static XKeyEvent
createKeyEvent (
	Display *display,
	Window win,
	Window winRoot,
	int press,
	int keycode,
	int modifiers)
{
	XKeyEvent event;
	
	event.display = display;
	event.window = win;
	event.root = winRoot;
	event.subwindow = None;
	event.time = CurrentTime;
	event.x = event.y = event.x_root = event.y_root = 1;
	event.same_screen = 1;
	event.keycode = XKeysymToKeycode(display, keycode);
	event.state = modifiers;
	event.type = press ? KeyPress : KeyRelease;
	
	return event;
}

static void
sendEvent (int kc, int press)
{
	static Display *display;
	static Window winRoot;
	XKeyEvent event;
	Window winFocus;
	int    revert;

	if (!display && !(display = XOpenDisplay(0)))
		printf("ERROR\n");
	if (!winRoot && !(winRoot = XDefaultRootWindow(display)))
		printf("ERROR\n");

	XGetInputFocus(display, &winFocus, &revert);
	event = createKeyEvent(display, winFocus, winRoot, press, kc, 0);
	XSendEvent(event.display, event.window, 1, KeyPressMask, (XEvent *) &event);
}

static struct candidate
makecandidate (int i, int j, float cost)
{
	struct candidate n;
	
	n.i = i;
	n.j = j;
	n.cost = cost;
	
	return n;
}

static struct visitee
makevisitee (struct candidate c, struct candidate prev)
{
	struct visitee v;

	v.valid = 1;
	v.i = c.i;
	v.j = c.j;
	v.previ = prev.i;
	v.prevj = prev.j;
	v.cost = c.cost;

	return v;
}

static inline vec2
getvec2forij (int i, int j)
{
	vec2 rv = {
		HalfWidth + RStep * j * sin(ThetaStep * i),
		HalfHeight + RStep * j * cos(ThetaStep * i)
	};
	return rv;
}

// compute heuristic cost for a move to discourage things like hitting a wall or unnecessary movement
static float
getmovecost (struct candidate *src, struct candidate *dst, enum Motion dir)
{
	vec2 mid = {HalfWidth, HalfHeight};
	vec2 moveLine[2] = {
		getvec2forij(src->i, src->j),
		getvec2forij(dst->i, dst->j)
	};
	float mindist = INFINITY;

	// calcualte min distance for heuristic to use
	for (unsigned int i = 0; i < obsLineCount; i++) {		
		if (v2segintersect(obsLines[i][0], obsLines[i][1], moveLine[0], moveLine[1])) {
			mindist = 0.0;
			break;
		} else {
			// find distance of moveLine to this obstacle
			float dist1 = v2segdist(obsLines[i][0], obsLines[i][1], moveLine[0]);
			float dist2 = v2segdist(obsLines[i][0], obsLines[i][1], moveLine[1]);

			float Robs1 = v2len(v2sub(obsLines[i][0], mid));
			float Robs2 = v2len(v2sub(obsLines[i][1], mid));

			float Rmove1 = v2len(v2sub(moveLine[0], mid));
			float Rmove2 = v2len(v2sub(moveLine[1], mid));

			if (MIN(Robs1, Robs2) > MIN(Rmove1, Rmove2)) {
				// *LOOSELY*, obstacle is *AHEAD* of moveLine
				// and should be considered.
				
				mindist = MIN(mindist, dist1);
				mindist = MIN(mindist, dist2);
			}
		}
	}

	// compute the overall weighted cost for the move
	float calculatedCost;
	{
		float dangerDistPenalty, minDistPenalty, movePenalty;
		
		dangerDistPenalty = 1.0 - (MIN(mindist, DangerDistPenaltyRadius) / DangerDistPenaltyRadius);
		dangerDistPenalty *= dangerDistPenalty;

		minDistPenalty = 1.0 - (MIN(mindist, MinDistPenaltyRadius) / MinDistPenaltyRadius);
	
		movePenalty = dir != Noneward ? (dst->j / ((float) MAX(MovePenaltyRadius / RStep, dst->j))) : 0.0;
		movePenalty = sqrt(movePenalty);

		calculatedCost = (
			movePenalty * MovePenaltyPct +
			minDistPenalty * MinDistPenaltyPct +
			dangerDistPenalty * DangerDistPenaltyPct) / 100.0;

		if (mindist == 0.0) {
			calculatedCost = HitPenalty;
		}

		// svg stuff
		SvgColor col = {
			(mindist == 0.0) ? 255 : (unsigned char) (minDistPenalty * 255.0),
			0, 0, 255
		};
		svgline(moveLine, &col);
	}

	return calculatedCost;
}

// similar to strtok iterates through possible adjacencies until there are none
// left then it returns 1. Reset with NULL.
static int
getnextcandidate (struct candidate *node, struct candidate *nxt)
{
	static struct candidate *node0 = NULL;
	static struct candidate candidates[3];
	static unsigned int i = 0;
	
	float basecost;

	if (node == NULL) {
		i = 0;
		node0 = NULL;
		return 1;
	}
	
	// to calculate next cost, evaluate cost of each line to next candidate
	if (node0 == NULL) {
		basecost = node->cost;
		i = 0;
		node0 = node;

		candidates[0] = makecandidate((node0->i + 1) % ThetaSteps, node0->j + 1, basecost);
		candidates[0].cost += getmovecost(node0, &candidates[0], Leftward);
		
		candidates[1] = makecandidate(node0->i, node0->j + 1, basecost);
		candidates[1].cost += getmovecost(node0, &candidates[1], Noneward);
		
		candidates[2] = makecandidate((node0->i + ThetaSteps - 1) % ThetaSteps, node0->j + 1, basecost);
		candidates[2].cost += getmovecost(node0, &candidates[2], Rightward);
	}

	if (i < sizeof (candidates) / sizeof (candidates[0])) {
		*nxt = candidates[i++];
		return 0;
	} else {
		return 1;
	}
}

static enum Motion
findPath (int i0, int j0)
{
	int ncandidates = 0;
	struct candidate cur, nxt;
	struct candidate candidates[ThetaSteps * RSteps];
	
	struct visitee visited[ThetaSteps][RSteps];
	

	memset(visited, 0, sizeof (visited));

	// seed loop with first candidate.
	cur = makecandidate(i0, j0, 0.0);
	visited[cur.i][cur.j] = makevisitee(cur, cur);

	// process candidates until we run out or reach the end.
	do {
		getnextcandidate(NULL, NULL);
		while (!getnextcandidate(&cur, &nxt)) {
			if (!visited[nxt.i][nxt.j].valid || nxt.cost < visited[nxt.i][nxt.j].cost) {
				// this candidate is either unvisited or lower
				// cost that previous attempt to location
				
				candidates[ncandidates++] = nxt;
				visited[nxt.i][nxt.j] = makevisitee(nxt, cur);
			}
		}

		// should never run out of candidates without reaching solution
		assert(ncandidates != 0);

		// grab next best candidate
		// TODO: get heap. do less stupid
		{
			float mincost = INFINITY;
			int besti = 0;
			for (int i = 0; i < ncandidates; i++) {
				if (candidates[i].cost < mincost) {
					mincost = candidates[i].cost;
					besti = i;
				}
			}

			cur = candidates[besti];
			candidates[besti] = candidates[--ncandidates];
		}
	} while (cur.j < RSteps - 1);


	enum Motion returndir = Noneward;
	{
		SvgColor col = {255, 255, 0, 255};
		
		vec2 drawLine[2];
		struct visitee *n = &visited[cur.i][cur.j];

		// step backwards through found path
		drawLine[0] = getvec2forij(n->i, n->j);
		while (n->j > (j0 + 1)) {
			n = &visited[n->previ][n->prevj];

			drawLine[1] = drawLine[0];
			drawLine[0] = getvec2forij(n->i, n->j);
			svgline(drawLine, &col);

		}

		// use 1st step to figure out which way to move
		int dif = (ThetaSteps + n->i - n->previ) % ThetaSteps;
		if (dif == 0) {
			returndir = Noneward;
		} else if (dif < ThetaSteps / 2) {
			returndir = Leftward;
		} else if (dif > ThetaSteps / 2) {
			returndir = Rightward;
		}
	}

	return returndir;
}

static void
handleGlDrawArrays (GLint first, GLsizei count)
{
	vec2 p[3];
	vec2 mid = {HalfWidth, HalfHeight};
	int isplayer, isobs;
	float innermostR;

	GLfloat nearpoints[10];
	int nearpointcount = 0;

	FILE *debugfile;
	static int debugfilenum = 0;
	char debugfilenamebuf[100];

	sprintf(debugfilenamebuf, "/tmp/debug%08d.txt", debugfilenum++);
	debugfile = fopen(debugfilenamebuf, "w");
	svgopen("/tmp/", ScreenWidth, ScreenHeight);

	// first pass to scope out frame specific metrics
	innermostR = INFINITY;
	for (int i = first; i < count / 3; i++) {
		for (int j = 0; j < 3; j++) {
			p[j].x = stolenvtxdata[(9 * i) + (3 * j) + 0];
			p[j].y = stolenvtxdata[(9 * i) + (3 * j) + 1];
			fprintf(debugfile, "%f %f\n", p[j].x, p[j].y);

			float r = v2len(v2sub(p[j], mid));
			innermostR = MIN(innermostR, r);
		}
	}
	
	// find the player. Draw stuff.
	playertheta = 0;
	obsLineCount = 0;
	
	for (int i = first; i < count / 3; i++) {
		for (int j = 0; j < 3; j++) {
			p[j].x = stolenvtxdata[(9 * i) + (3 * j) + 0];
			p[j].y = stolenvtxdata[(9 * i) + (3 * j) + 1];
		}


		// check if is player
		{
			int i;
			vec2 v[3];
			float a[3];
			isplayer = 1;
			
			for (i = 0; isplayer && i < 3; i++) {
				v[i] = v2sub(p[i], p[(i + 1) % 3]);
				if (v2len(v[i]) > MagicPlayerSizeMax) {
					isplayer = 0;
				}
			}

			for (i = 0; isplayer && i < 3; i++) {
				a[i] = M_PI - fabs(v2angle(v[i], v[(i + 1) % 3]));
				if (isnan(a[i]) || fabs(a[i] - M_PI / 3) > MagicPlayerAngleTolerance) {
					isplayer = 0;
				}
			}
			
		}


		// check if is obstacle
		isobs = 1;
		{
			int i = 0;
			float min = INFINITY, max = 0;
			float near = INFINITY, far = 0;
			for (i = 0; i < 3; i++) {
				max = MAX(v2len(v2sub(p[i], p[(i + 1) % 3])), max);
				min = MIN(v2len(v2sub(p[i], p[(i + 1) % 3])), min);
				near = MIN(near, v2len(v2sub(p[i], mid)));
				far = MAX(far, v2len(v2sub(p[i], mid)));
			}

			if (isplayer) {
				// can't also be player!
				isobs = 0;
			} else if (min < MagicSmallnessLimit) {
				// too small (necessary?)
				isobs = 0;
			} else if (near < MagicInnermostRPadding + innermostR && far > MagicFarnessLimit) {
				// one of the BIG triangles
				nearpoints[nearpointcount++] = near;
				isobs = 0;
			}

			for (i = 0; i < 3; i++) {
				if (p[i].y == 0.0) {
					// special case to eliminate score panels
					isobs = 0;
				}
			}
		}

		SvgColor col = {isplayer ? 255 : 0, isobs ? 255 : 0, 0, 255};
		svgtriangle(p, &col, OutlineMode);

		if (isplayer) {
			//TODO: wtf "math"?
			playertheta = M_PI / 2.0 - atan2(p[0].y - mid.y, p[1].x - mid.x);
			playerr = v2len(v2sub(p[0], mid));
		} else if (isobs) {
			obsLines[obsLineCount][0] = p[0];
			obsLines[obsLineCount++][1] = p[1];
			obsLines[obsLineCount][0] = p[1];
			obsLines[obsLineCount++][1] = p[2];
			obsLines[obsLineCount][0] = p[2];
			obsLines[obsLineCount++][1] = p[0];
			assert(obsLineCount < MaxObsLineCount);
		}
	}

	//printf("nearpoints: ");
	//for (int i = 0; i < nearpointcount; i++) {
	//	printf("%f ", nearpoints[i]);
	//}
	//printf("\n");

	int pindex = (int) roundf(playertheta / M_PI / 2.0 * ThetaSteps);
	pindex = (pindex + ThetaSteps) % ThetaSteps;
	int pindexr = (int) roundf(playerr / RStep);

	playerdir = findPath(pindex, pindexr);

	// keyboard stuff
	{
		SvgColor col = {255, 0, 0, 255};
		switch (playerdir) {
		case Leftward:
			svgcircle(v2create(HalfWidth / 4, HalfHeight / 4), 20.0, &col, 0.0, OutlineMode);
			sendEvent(XK_Left, 1);
			sendEvent(XK_Right, 0);
			break;
		case Noneward:
			svgcircle(v2create(HalfWidth, HalfHeight / 4), 20.0, &col, 0.0, OutlineMode);
			sendEvent(XK_Left, 0);
			sendEvent(XK_Right, 0);
			break;
		case Rightward:
			svgcircle(v2create(3 * HalfWidth / 2, HalfHeight / 4), 20.0, &col, 0.0, OutlineMode);
			sendEvent(XK_Left, 0);
			sendEvent(XK_Right, 1);
			break;
		case Keyboardward:
			break;
		}
	}

	fclose(debugfile);
	svgclose();


	// transform the view
	/* for (int i = first; i < count / 3; i++) { */
	/* 	for (int j = 0; j < 3; j++) { */
	/* 		p[j].x = stolenvtxdata[(9 * i) + (3 * j) + 0]; */
	/* 		p[j].y = stolenvtxdata[(9 * i) + (3 * j) + 1]; */
	/* 		p[j] = v2add(v2rotate(v2sub(p[j], mid), playertheta + M_PI), mid); */
	/* 		stolenvtxdata[(9 * i) + (3 * j) + 0] = p[j].x; */
	/* 		stolenvtxdata[(9 * i) + (3 * j) + 1] = p[j].y; */
	/* 	} */
	/* } */

}


// prevent the bad functions from making GCC sad
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

void
glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer, ...)
{
	void **rbp;
	orig_glVertexPointer_f_type orig_glVertexPointer;

	asm("\t mov %%rbp,%0" : "=r"(rbp));
	if (*(rbp + 1) == (void *) VTXPTR_HIJACK_ADDR) {
		// steal data for later
		stolensize = size;
		stolentype = type;
		stolenstride = stride;
		stolenvtxdata = (GLfloat *) pointer;
	}
	orig_glVertexPointer = (orig_glVertexPointer_f_type) dlsym(RTLD_NEXT, "glVertexPointer");
	return orig_glVertexPointer(size, type, stride, pointer);
}

void
glDrawArrays (GLenum mode, GLint first, GLsizei count, ...)
{
	void **rbp;
	orig_glDrawArrays_f_type orig_glDrawArrays;
	static int isready = 0;

	// count triangles to see if the game has actually started
	if (count == MagicModeSwitchCount) isready = 1;

	asm("\t mov %%rbp,%0" : "=r"(rbp));
	if (*(rbp + 1) == (void *) DRAWARR_HIJACK_ADDR && stolenvtxdata && isready) {
		// actually process the data obtains in glVertexPointer
		handleGlDrawArrays(first, count);
		stolenvtxdata = 0;
	}

	orig_glDrawArrays = (orig_glDrawArrays_f_type) dlsym(RTLD_NEXT, "glDrawArrays");
	return orig_glDrawArrays(mode, first, count);
}

#pragma GCC diagnostic pop
