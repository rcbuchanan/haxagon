#ifndef VEC2_H_
#define VEC2_H_

#include <math.h>

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

typedef struct vec2_t {
	float x;
	float y;
} vec2;

static inline vec2
v2create (float x, float y)
{
	vec2 v;
	
	v.x = x;
	v.y = y;
	
	return v;
}

static inline vec2
v2polar (float theta, float r)
{
	vec2 v;
	
	v.x = sin(theta) * r;
	v.y = cos(theta) * r;
	
	return v;
}

static inline float
v2dot (vec2 v, vec2 w)
{
	return v.x * w.x + v.y * w.y;
}

static inline float
v2crossz (vec2 v, vec2 w)
{
	return v.x * w.y - v.y * w.x;
}

static inline float
v2len (vec2 v)
{
	return sqrt(v.x * v.x + v.y * v.y);
}

static inline float
v2angle (vec2 v, vec2 w)
{
	return atan2f(v2crossz(v, w), v2dot(v, w));
}

static inline vec2
v2rotate (vec2 v, float theta)
{
	float s = sinf(theta);
	float c = cosf(theta);
	
	vec2 rv = {
		c * v.x - s * v.y,
		s * v.x + c * v.y};
	return rv;
}

static inline vec2
v2sub (vec2 v, vec2 w)
{
	vec2 rv = {v.x - w.x, v.y - w.y};
	return rv;
}

static inline vec2
v2add (vec2 v, vec2 w)
{
	vec2 rv = {v.x + w.x, v.y + w.y};
	return rv;
}

static inline vec2
v2scale (vec2 v, float k)
{
	vec2 rv = {k * v.x, k * v.y};
	return rv;
}

static inline vec2
v2perp (vec2 v)
{
	vec2 rv = {v.y, -v.x};
	return rv;
}

static inline vec2
v2norm (vec2 v)
{
	vec2 rv = {v.x / v2len(v), v.y / v2len(v)};
	return rv;
}

static inline vec2
v2mid (vec2 v, vec2 w)
{
	vec2 rv = {(v.x + w.x) / 2, (v.y + w.y) / 2};
	return rv;
}

static inline int
v2tricircumscribespoint (vec2 *p, vec2 cp)
{
	vec2 side0 = v2sub(p[0], p[1]);
	vec2 side1 = v2sub(p[1], p[2]);
	
	vec2 lo0 = v2add(v2scale(side0, 0.5), p[1]);
	vec2 lo1 = v2add(v2scale(side1, 0.5), p[2]);

	vec2 lp0 = v2norm(side0);
	vec2 ld1 = v2norm(v2perp(side1));

	float t = v2dot(lp0, v2sub(lo0, lo1)) / v2dot(lp0, ld1);
	vec2 mid = v2add(lo1, v2scale(ld1, t));
	
	return v2len(v2sub(mid, cp)) < v2len(v2sub(mid, p[0])) ? 1 : 0;
}

static inline vec2
v2centroid (vec2 org, vec2 v, vec2 w)
{
	vec2 vd = v2sub(v, org);
	vec2 wd = v2sub(w, org);
	float rd3 = 1 / 3.0f;

	return v2add(org, v2add(v2scale(vd, rd3), v2scale(wd, rd3)));
}

static inline void
v2sortccw (vec2 *a, vec2 *b, vec2 *c)
{
	vec2 tmp;
	if (v2angle(v2sub(*c, *b), v2sub(*a, *b)) < 0) {
		tmp = *c;
		*c = *a;
		*a = tmp;
	}
}

static inline int
v2segintersectfinder (vec2 sa0, vec2 sa1, vec2 sb0, vec2 sb1, vec2 *at)
{
	// segments w/ origin at 0
	vec2 sad = v2norm(v2sub(sa1, sa0));
	vec2 sbd = v2norm(v2sub(sb1, sb0));

	float sal = v2len(v2sub(sa1, sa0));
	float sbl = v2len(v2sub(sb1, sb0));

	// parametric eqn to solve:
	// 	t_a * sal * sad + sa0 = t_b * sbl * sbd + sb0
	// 	t_a * sal * (sad . perp to b) = (sb0 - sa0) . perp to b
	// 	t_a = [(sb0 - sa0) . perp to b] / [sal * (sad . perp to b)]
	
	float ta = v2crossz(v2sub(sb0, sa0), sbd) / v2crossz(sad, sbd) / sal;
	float tb = v2crossz(v2sub(sa0, sb0), sad) / v2crossz(sbd, sad) / sbl;

	if (ta <= 0.0 || ta >= 1.0 || isnan(ta)) {
		return 0;
	} else if (tb <= 0.0 || tb >= 1.0 || isnan(tb)) {
		return 0;
	} else {
		*at = v2add(sa0, v2scale(sad, ta * sal));
		return 1;
	}
}

static inline int
v2segintersect (vec2 sa0, vec2 sa1, vec2 sb0, vec2 sb1)
{
	// segments w/ origin at 0
	vec2 sad = v2norm(v2sub(sa1, sa0));
	vec2 sbd = v2norm(v2sub(sb1, sb0));

	float sal = v2len(v2sub(sa1, sa0));
	float sbl = v2len(v2sub(sb1, sb0));

	// parametric eqn to solve:
	// 	t_a * sal * sad + sa0 = t_b * sbl * sbd + sb0
	// 	t_a * sal * (sad . perp to b) = (sb0 - sa0) . perp to b
	// 	t_a = [(sb0 - sa0) . perp to b] / [sal * (sad . perp to b)]
	
	float ta = v2crossz(v2sub(sb0, sa0), sbd) / v2crossz(sad, sbd) / sal;
	float tb = v2crossz(v2sub(sa0, sb0), sad) / v2crossz(sbd, sad) / sbl;

	if (ta <= 0.0 || ta >= 1.0 || isnan(ta))
		return 0;
	else if (tb <= 0.0 || tb >= 1.0 || isnan(tb))
		return 0;
	else
		return 1;
}

static inline int
v2segrayintersect (vec2 s0, vec2 s1, vec2 ro, vec2 rd)
{
	// segments w/ origin at 0
	vec2 sad = v2norm(v2sub(s1, s0));
	vec2 sbd = v2norm(rd);

	float sal = v2len(v2sub(s1, s0));
	float sbl = 1;

	// parametric eqn to solve:
	// 	t_a * sal * sad + sa0 = t_b * sbl * sbd + sb0
	// 	t_a * sal * (sad . perp to b) = (sb0 - sa0) . perp to b
	// 	t_a = [(sb0 - sa0) . perp to b] / [sal * (sad . perp to b)]
	
	float ta = v2crossz(v2sub(ro, s0), sbd) / v2crossz(sad, sbd) / sal;
	float tb = v2crossz(v2sub(s0, ro), sad) / v2crossz(sbd, sad) / sbl;

	if (ta <= 0.0 || ta >= 1.0 || isnan(ta))
		return 0;
	else if (tb <= 0.0 || isnan(tb))
		return 0;
	else
		return 1;
}

static inline int
v2segrayintersectfinder (vec2 sa0, vec2 sa1, vec2 ro, vec2 rd, vec2 *at)
{
	// segments w/ origin at 0
	vec2 sad = v2norm(v2sub(sa1, sa0));
	vec2 sbd = v2norm(rd);

	float sal = v2len(v2sub(sa1, sa0));
	float sbl = 1;

	// parametric eqn to solve:
	// 	t_a * sal * sad + sa0 = t_b * sbl * sbd + sb0
	// 	t_a * sal * (sad . perp to b) = (sb0 - sa0) . perp to b
	// 	t_a = [(sb0 - sa0) . perp to b] / [sal * (sad . perp to b)]
	
	float ta = v2crossz(v2sub(ro, sa0), sbd) / v2crossz(sad, sbd) / sal;
	float tb = v2crossz(v2sub(sa0, ro), sad) / v2crossz(sbd, sad) / sbl;


	if (ta <= 0.0 || ta >= 1.0 || isnan(ta)) {
		return 0;
	} else if (tb <= 0.0 || isnan(tb)) {
		return 0;
	} else {
		*at = v2add(sa0, v2scale(sad, ta * sal));
		return 1;
	}
}

// adapted from http://stackoverflow.com/questions/849211
static inline float
v2segdist(vec2 v, vec2 w, vec2 p)
{
	float l2 = v.x * w.x + v.y * w.y;
	
	float t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
	
	t = MAX(0, MIN(1, t));
	
	vec2 hit = {
		v.x + t * (w.x - v.x),
		v.y + t * (w.y - v.y)
	};
	return v2len(v2sub(p, hit));
}

#endif
