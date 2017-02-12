#include "quakedef.h"
#include <math.h>
#include "matrixlib.h"

const matrix4x4_t identitymatrix =
{
	{
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1}
	}
};

void Matrix4x4_CreateIdentity (matrix4x4_t *out)
{
	out->m[0][0]=1.0f;
	out->m[0][1]=0.0f;
	out->m[0][2]=0.0f;
	out->m[0][3]=0.0f;
	out->m[1][0]=0.0f;
	out->m[1][1]=1.0f;
	out->m[1][2]=0.0f;
	out->m[1][3]=0.0f;
	out->m[2][0]=0.0f;
	out->m[2][1]=0.0f;
	out->m[2][2]=1.0f;
	out->m[2][3]=0.0f;
	out->m[3][0]=0.0f;
	out->m[3][1]=0.0f;
	out->m[3][2]=0.0f;
	out->m[3][3]=1.0f;
}

void Matrix4x4_CreateFromQuakeEntity(matrix4x4_t *out, float x, float y, float z, float pitch, float yaw, float roll, float scale)
{
	double angle, sr, sp, sy, cr, cp, cy;

	if (roll)
	{
		angle = yaw * (M_PI*2 / 360);
		sy = sin(angle);
		cy = cos(angle);
		angle = pitch * (M_PI*2 / 360);
		sp = sin(angle);
		cp = cos(angle);
		angle = roll * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);
		out->m[0][0] = (float)((cp*cy) * scale);
		out->m[0][1] = (float)((sr*sp*cy+cr*-sy) * scale);
		out->m[0][2] = (float)((cr*sp*cy+-sr*-sy) * scale);
		out->m[0][3] = x;
		out->m[1][0] = (float)((cp*sy) * scale);
		out->m[1][1] = (float)((sr*sp*sy+cr*cy) * scale);
		out->m[1][2] = (float)((cr*sp*sy+-sr*cy) * scale);
		out->m[1][3] = y;
		out->m[2][0] = (float)((-sp) * scale);
		out->m[2][1] = (float)((sr*cp) * scale);
		out->m[2][2] = (float)((cr*cp) * scale);
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
	}
	else if (pitch)
	{
		angle = yaw * (M_PI*2 / 360);
		sy = sin(angle);
		cy = cos(angle);
		angle = pitch * (M_PI*2 / 360);
		sp = sin(angle);
		cp = cos(angle);
		out->m[0][0] = (float)((cp*cy) * scale);
		out->m[0][1] = (float)((-sy) * scale);
		out->m[0][2] = (float)((sp*cy) * scale);
		out->m[0][3] = x;
		out->m[1][0] = (float)((cp*sy) * scale);
		out->m[1][1] = (float)((cy) * scale);
		out->m[1][2] = (float)((sp*sy) * scale);
		out->m[1][3] = y;
		out->m[2][0] = (float)((-sp) * scale);
		out->m[2][1] = 0;
		out->m[2][2] = (float)((cp) * scale);
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
	}
	else if (yaw)
	{
		angle = yaw * (M_PI*2 / 360);
		sy = sin(angle);
		cy = cos(angle);
		out->m[0][0] = (float)((cy) * scale);
		out->m[0][1] = (float)((-sy) * scale);
		out->m[0][2] = 0;
		out->m[0][3] = x;
		out->m[1][0] = (float)((sy) * scale);
		out->m[1][1] = (float)((cy) * scale);
		out->m[1][2] = 0;
		out->m[1][3] = y;
		out->m[2][0] = 0;
		out->m[2][1] = 0;
		out->m[2][2] = scale;
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
	}
	else
	{
		out->m[0][0] = scale;
		out->m[0][1] = 0;
		out->m[0][2] = 0;
		out->m[0][3] = x;
		out->m[1][0] = 0;
		out->m[1][1] = scale;
		out->m[1][2] = 0;
		out->m[1][3] = y;
		out->m[2][0] = 0;
		out->m[2][1] = 0;
		out->m[2][2] = scale;
		out->m[2][3] = z;
		out->m[3][0] = 0;
		out->m[3][1] = 0;
		out->m[3][2] = 0;
		out->m[3][3] = 1;
	}
}

//This function is GL stylie (use as 2nd arg to ML_MultMatrix4).
float *Matrix4_NewRotation(float a, float x, float y, float z)
{
	static float ret[16];
	float c = cos(a* M_PI / 180.0);
	float s = sin(a* M_PI / 180.0);

	ret[0] = x*x*(1-c)+c;
	ret[4] = x*y*(1-c)-z*s;
	ret[8] = x*z*(1-c)+y*s;
	ret[12] = 0;

	ret[1] = y*x*(1-c)+z*s;
    ret[5] = y*y*(1-c)+c;
	ret[9] = y*z*(1-c)-x*s;
	ret[13] = 0;

	ret[2] = x*z*(1-c)-y*s;
	ret[6] = y*z*(1-c)+x*s;
	ret[10] = z*z*(1-c)+c;
	ret[14] = 0;

	ret[3] = 0;
	ret[7] = 0;
	ret[11] = 0;
	ret[15] = 1;
	return ret;
}

//This function is GL stylie (use as 2nd arg to ML_MultMatrix4).
float *Matrix4_NewTranslation(float x, float y, float z)
{
	static float ret[16];
	ret[0] = 1;
	ret[4] = 0;
	ret[8] = 0;
	ret[12] = x;

	ret[1] = 0;
    ret[5] = 1;
	ret[9] = 0;
	ret[13] = y;

	ret[2] = 0;
	ret[6] = 0;
	ret[10] = 1;
	ret[14] = z;

	ret[3] = 0;
	ret[7] = 0;
	ret[11] = 0;
	ret[15] = 1;
	return ret;
}

//be aware that this generates two sorts of matricies depending on order of a+b
void Matrix4_Multiply(float *a, float *b, float *out)
{
	out[0]  = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
	out[1]  = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
	out[2]  = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
	out[3]  = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];

	out[4]  = a[0] * b[4] + a[4] * b[5] + a[8] * b[6] + a[12] * b[7];
	out[5]  = a[1] * b[4] + a[5] * b[5] + a[9] * b[6] + a[13] * b[7];
	out[6]  = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
	out[7]  = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];

	out[8]  = a[0] * b[8] + a[4] * b[9] + a[8] * b[10] + a[12] * b[11];
	out[9]  = a[1] * b[8] + a[5] * b[9] + a[9] * b[10] + a[13] * b[11];
	out[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
	out[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];

	out[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15];
	out[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15];
	out[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];
	out[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

void ML_ModelViewMatrix(float *modelview, vec3_t viewangles, vec3_t vieworg)
{
	float tempmat[16];
	//load identity.
	memset(modelview, 0, sizeof(*modelview)*16);
//#if FULLYGL
	modelview[0] = 1;
	modelview[5] = 1;
	modelview[10] = 1;
	modelview[15] = 1;

	Matrix4_Multiply(modelview, Matrix4_NewRotation(-90,  1, 0, 0), tempmat);	    // put Z going up
	Matrix4_Multiply(tempmat, Matrix4_NewRotation(90,  0, 0, 1), modelview);	    // put Z going up
/*#else
	//use this lame wierd and crazy identity matrix..
	modelview[2] = -1;
	modelview[4] = -1;
	modelview[9] = 1;
	modelview[15] = 1;
#endif*/
	//figure out the current modelview matrix

	//I would if some of these, but then I'd still need a couple of copys
	Matrix4_Multiply(modelview, Matrix4_NewRotation(-viewangles[2],  1, 0, 0), tempmat);
	Matrix4_Multiply(tempmat, Matrix4_NewRotation(-viewangles[0],  0, 1, 0), modelview);
	Matrix4_Multiply(modelview, Matrix4_NewRotation(-viewangles[1],  0, 0, 1), tempmat);

	Matrix4_Multiply(tempmat, Matrix4_NewTranslation(-vieworg[0],  -vieworg[1],  -vieworg[2]), modelview);	    // put Z going up
}

void ML_ProjectionMatrix(float *proj, float wdivh, float fovy)
{
	float xmin, xmax, ymin, ymax;
	float nudge = 1;

	//proj
	ymax = 4 * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;

	xmin = ymin * wdivh;
	xmax = ymax * wdivh;

	proj[0] = (2*4) / (xmax - xmin);
	proj[4] = 0;
	proj[8] = (xmax + xmin) / (xmax - xmin);
	proj[12] = 0;

	proj[1] = 0;
	proj[5] = (2*4) / (ymax - ymin);
	proj[9] = (ymax + ymin) / (ymax - ymin);
	proj[13] = 0;

	proj[2] = 0;
	proj[6] = 0;
	proj[10] = -1  * nudge;
	proj[14] = -2*4 * nudge;
	
	proj[3] = 0;
	proj[7] = 0;
	proj[11] = -1;
	proj[15] = 0;
}

//transform 4d vector by a 4d matrix.
void Matrix4_Transform4(float *matrix, float *vector, float *product)
{
	product[0] = matrix[0]*vector[0] + matrix[4]*vector[1] + matrix[8]*vector[2] + matrix[12]*vector[3];
	product[1] = matrix[1]*vector[0] + matrix[5]*vector[1] + matrix[9]*vector[2] + matrix[13]*vector[3];
	product[2] = matrix[2]*vector[0] + matrix[6]*vector[1] + matrix[10]*vector[2] + matrix[14]*vector[3];
	product[3] = matrix[3]*vector[0] + matrix[7]*vector[1] + matrix[11]*vector[2] + matrix[15]*vector[3];
}

void Matrix4x4_Transpose (matrix4x4_t *out, const matrix4x4_t *in1)
{
	out->m[0][0] = in1->m[0][0];
	out->m[0][1] = in1->m[1][0];
	out->m[0][2] = in1->m[2][0];
	out->m[0][3] = in1->m[3][0];
	out->m[1][0] = in1->m[0][1];
	out->m[1][1] = in1->m[1][1];
	out->m[1][2] = in1->m[2][1];
	out->m[1][3] = in1->m[3][1];
	out->m[2][0] = in1->m[0][2];
	out->m[2][1] = in1->m[1][2];
	out->m[2][2] = in1->m[2][2];
	out->m[2][3] = in1->m[3][2];
	out->m[3][0] = in1->m[0][3];
	out->m[3][1] = in1->m[1][3];
	out->m[3][2] = in1->m[2][3];
	out->m[3][3] = in1->m[3][3];
}

void Matrix4x4_Transform3x3 (const matrix4x4_t *in, const float v[3], float out[3])
{
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2];
}

void Matrix4x4_Transform (const matrix4x4_t *in, const float v[3], float out[3])
{
	out[0] = v[0] * in->m[0][0] + v[1] * in->m[0][1] + v[2] * in->m[0][2] + in->m[0][3];
	out[1] = v[0] * in->m[1][0] + v[1] * in->m[1][1] + v[2] * in->m[1][2] + in->m[1][3];
	out[2] = v[0] * in->m[2][0] + v[1] * in->m[2][1] + v[2] * in->m[2][2] + in->m[2][3];
}

void Matrix4x4_OriginFromMatrix (const matrix4x4_t *in, float *out)
{
	out[0] = in->m[0][3];
	out[1] = in->m[1][3];
	out[2] = in->m[2][3];
}

//returns fractions of screen.
//uses GL style rotations and translations and stuff.
//3d -> screen (fixme: offscreen return values needed)
void ML_Project (vec3_t in, vec3_t out, vec3_t viewangles, vec3_t vieworg, float wdivh, float fovy)
{
	float modelview[16];
	float proj[16];

	ML_ModelViewMatrix(modelview, viewangles, vieworg);
	ML_ProjectionMatrix(proj, wdivh, fovy);

	{
		float v[4], tempv[4];
		v[0] = in[0];
		v[1] = in[1];
		v[2] = in[2];
		v[3] = 1;

		Matrix4_Transform4(modelview, v, tempv); 
		Matrix4_Transform4(proj, tempv, v);

		v[0] /= v[3];
		v[1] /= v[3];
		v[2] /= v[3];

		out[0] = (1+v[0])/2;
		out[1] = (1+v[1])/2;
		out[2] = (1+v[2])/2;
	}
}

