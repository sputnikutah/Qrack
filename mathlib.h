/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// mathlib.h

typedef	float	vec_t;
typedef	vec_t	vec3_t[3];
typedef	vec_t	vec5_t[5];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

struct mplane_s;

#define RAD2DEG(a)((float)(a)*(float)(180.0f / M_PI))
#define DEG2RAD(a)((float)(a)*(float)(M_PI/180.0f))

#define M_PI_DIV_180 (M_PI / 180.0f) //johnfitz

#define NANMASK (255 << 23)
#define	IS_NAN(x) (((*(int *) & x) & NANMASK) == NANMASK)

#define Q_rint(x) ((x) >= 0 ? (int)((x) + 0.5) : (int)((x) - 0.5))

#define DotProduct(x, y)	((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])

#define FloatInterpolate(f1, _frac, f2)			\
	(_mathlib_temp_float1 = _frac,			\
	(f1) + _mathlib_temp_float1 * ((f2) - (f1)))

#define lhrandom(MIN,MAX) ((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))//LordHavoc
#define BoxesOverlap(a,b,c,d) ((a)[0] <= (d)[0] && (b)[0] >= (c)[0] && (a)[1] <= (d)[1] && (b)[1] >= (c)[1] && (a)[2] <= (d)[2] && (b)[2] >= (c)[2])//LordHavoc
#define VectorSubtract(a, b, c)	((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define VectorAdd(a, b, c)	((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define VectorCopy(a, b)	((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define VectorClear(a)		((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a, b)	((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define VectorSet(v, x, y, z)	((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
#define VectorMultiply(a, b, c) ((c)[0]=(a)[0]*(b)[0],(c)[1]=(a)[1]*(b)[1],(c)[2]=(a)[2]*(b)[2])
#define VectorLerp(v1,lerp,v2,c) ((c)[0] = (v1)[0] + (lerp) * ((v2)[0] - (v1)[0]), (c)[1] = (v1)[1] + (lerp) * ((v2)[1] - (v1)[1]), (c)[2] = (v1)[2] + (lerp) * ((v2)[2] - (v1)[2]))
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
#define VectorRandom(v) {do{(v)[0] = lhrandom(-1, 1);(v)[1] = lhrandom(-1, 1);(v)[2] = lhrandom(-1, 1);}while(DotProduct(v, v) > 1);}
#define VectorMAMAM(scale1, b1, scale2, b2, scale3, b3, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2])

#define CLAMP(min, x, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x)) //johnfitz

#define CrossProduct(v1, v2, x)					\
	((x)[0] = (v1)[1] * (v2)[2] - (v1)[2] * (v2)[1],	\
	(x)[1] = (v1)[2] * (v2)[0] - (v1)[0] * (v2)[2],		\
	(x)[2] = (v1)[0] * (v2)[1] - (v1)[1] * (v2)[0])

#define VectorInverse(v)									\
(															\
	v[0]	= -v[0],										\
	v[1]	= -v[1],										\
	v[2]	= -v[2]											\
)

#define VectorInterpolate(v1, _frac, v2, v)				\
do {									\
	_mathlib_temp_float1 = _frac;					\
									\
	(v)[0] = (v1)[0] + _mathlib_temp_float1 * ((v2)[0] - (v1)[0]);	\
	(v)[1] = (v1)[1] + _mathlib_temp_float1 * ((v2)[1] - (v1)[1]);	\
	(v)[2] = (v1)[2] + _mathlib_temp_float1 * ((v2)[2] - (v1)[2]);	\
} while(0)

#define VectorSupCompare(v, w, m)								\
	(_mathlib_temp_float1 = m,								\
	(v)[0] - (w)[0] > -_mathlib_temp_float1 && (v)[0] - (w)[0] < _mathlib_temp_float1 &&	\
	(v)[1] - (w)[1] > -_mathlib_temp_float1 && (v)[1] - (w)[1] < _mathlib_temp_float1 &&	\
	(v)[2] - (w)[2] > -_mathlib_temp_float1 && (v)[2] - (w)[2] < _mathlib_temp_float1)

#define VectorL2Compare(v, w, m)				\
	(_mathlib_temp_float1 = (m) * (m),			\
	_mathlib_temp_vec1[0] = (v)[0] - (w)[0], _mathlib_temp_vec1[1] = (v)[1] - (w)[1], _mathlib_temp_vec1[2] = (v)[2] - (w)[2],\
	_mathlib_temp_vec1[0] * _mathlib_temp_vec1[0] +		\
	_mathlib_temp_vec1[1] * _mathlib_temp_vec1[1] +		\
	_mathlib_temp_vec1[2] * _mathlib_temp_vec1[2] < _mathlib_temp_float1)

#define VectorCompare(v, w)	((v)[0] == (w)[0] && (v)[1] == (w)[1] && (v)[2] == (w)[2])

#define VectorMA(a, _f, b, c)					\
do {								\
	_mathlib_temp_float1 = (_f);				\
	(c)[0] = (a)[0] + _mathlib_temp_float1 * (b)[0];	\
	(c)[1] = (a)[1] + _mathlib_temp_float1 * (b)[1];	\
	(c)[2] = (a)[2] + _mathlib_temp_float1 * (b)[2];	\
} while(0)

#define VectorScale(in, _scale, out)					\
do {									\
	float	scale = (_scale);					\
	(out)[0] = (in)[0] * (scale); (out)[1] = (in)[1] * (scale); (out)[2] = (in)[2] * (scale);\
} while(0)

#define anglemod(a)	((360.0 / 65536) * ((int)((a) * (65536 / 360.0)) & 65535))

#define VectorNormalizeFast(_v)							\
do {										\
	_mathlib_temp_float1 = DotProduct((_v), (_v));				\
	if (_mathlib_temp_float1) {						\
		_mathlib_temp_float2 = 0.5f * _mathlib_temp_float1;		\
		_mathlib_temp_int1 = *((int *) &_mathlib_temp_float1);		\
		_mathlib_temp_int1 = 0x5f375a86 - (_mathlib_temp_int1 >> 1);	\
		_mathlib_temp_float1 = *((float *) &_mathlib_temp_int1);	\
		_mathlib_temp_float1 = _mathlib_temp_float1 * (1.5f - _mathlib_temp_float2 * _mathlib_temp_float1 * _mathlib_temp_float1);	\
		VectorScale((_v), _mathlib_temp_float1, (_v));			\
	}									\
} while(0)

#define FloatInterpolate(f1, _frac, f2)			\
	(_mathlib_temp_float1 = _frac,			\
	(f1) + _mathlib_temp_float1 * ((f2) - (f1)))

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)			\
	(((p)->type < 3)?					\
	(							\
		((p)->dist <= (emins)[(p)->type])?		\
			1					\
		:						\
		(						\
			((p)->dist >= (emaxs)[(p)->type])?	\
				2				\
			:					\
				3				\
		)						\
	)							\
	:							\
		BoxOnPlaneSide ((emins), (emaxs), (p)))

#define PlaneDist(point, plane) ((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal))
#define PlaneDiff(point, plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

void PlaneEquation(float *x, float *y, float *z, float *plane); //rww

void VectorVectors (vec3_t forward, vec3_t right, vec3_t up);
void Angle2Vector (vec3_t angles, vec3_t vec);
vec_t VectorLength (vec3_t v);
vec_t VectorDistance (vec3_t x,vec3_t y);
float VecLength2(vec3_t v1, vec3_t v2);
float VectorNormalize (vec3_t v);		// returns vector length
#define VectorNormalize2(v,dest) {float ilength = 1.0f / (float) sqrt(DotProduct(v,v));dest[0] = v[0] * ilength;dest[1] = v[1] * ilength;dest[2] = v[2] * ilength;}

void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

void FloorDivMod (double numer, double denom, int *quotient, int *rem);
fixed16_t Invert24To16 (fixed16_t val);
int GreatestCommonDivisor (int i1, int i2);

void vectoangles (vec3_t vec, vec3_t ang);
void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct mplane_s *plane);

void RotatePointAroundVector (vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);

extern	vec3_t	vec3_origin;
extern	int	_mathlib_temp_int1, _mathlib_temp_int2;
extern	float	_mathlib_temp_float1, _mathlib_temp_float2;
extern	vec3_t	_mathlib_temp_vec1;

extern long int lrint(double flt);
extern long int lrintf(float flt);

#define Q_ROUND_POWER2(in, out) {						\
	_mathlib_temp_int1 = in;							\
	for (out = 1; out < _mathlib_temp_int1; out <<= 1)	\
	;												\
}

void LerpVector (const vec3_t from, const vec3_t to, float frac, vec3_t out);
void LerpAngles (const vec3_t from, const vec3_t to, float frac, vec3_t out);

typedef struct matrix4x4_s
{
	float m[4][4];
}
matrix4x4_t;

extern const matrix4x4_t identitymatrix;

void Matrix4x4_Transform (const matrix4x4_t *in, const float v[3], float out[3]);

void MatrixInverse4x4(float *mat, float *dst); //rww
//void TranslateMatrix4x4(float *mat, float *v); //rww
//void MatrixMultiply4x4(float *in, float *in2, float *out); //rww
void TransformPointByMatrix4x4(float *matrix, float *in, float *out); //rww
//void TransformV4ByMatrix4x4(float *matrix, float *in, float *out); //rww

void PlaneClassify(struct mplane_s *p);