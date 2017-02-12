//rww - automatic on-the-fly generation of normal maps
//adapted from the source code for nvidia's gimp plugin

#include "quakedef.h"

#ifndef min
#define min(a,b)  ((a)<(b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)  ((a)>(b) ? (a) : (b))
#endif

#define SQR(x)      ((x) * (x))
#define LERP(a,b,c) ((a) + ((b) - (a)) * (c))

#define NMAP_ASSERT

enum FILTER_TYPE
{
   FILTER_NONE = 0, FILTER_SOBEL_3x3, FILTER_SOBEL_5x5, FILTER_PREWITT_3x3,
   FILTER_PREWITT_5x5, FILTER_3x3, FILTER_5x5, FILTER_7x7, FILTER_9x9,
   MAX_FILTER_TYPE
};

enum ALPHA_TYPE
{
   ALPHA_NONE = 0, ALPHA_HEIGHT, ALPHA_INVERSE_HEIGHT, ALPHA_ZERO, ALPHA_ONE,
   ALPHA_INVERT, ALPHA_MAP, MAX_ALPHA_TYPE
};

enum CONVERSION_TYPE
{
   CONVERT_NONE = 0, CONVERT_BIASED_RGB, CONVERT_RED, CONVERT_GREEN, 
   CONVERT_BLUE, CONVERT_MAX_RGB, CONVERT_MIN_RGB, CONVERT_COLORSPACE,
   CONVERT_NORMALIZE_ONLY, CONVERT_DUDV_TO_NORMAL, CONVERT_HEIGHTMAP,
   MAX_CONVERSION_TYPE
};

enum DUDV_TYPE
{
   DUDV_NONE, DUDV_8BIT_SIGNED, DUDV_8BIT_UNSIGNED, DUDV_16BIT_SIGNED,
   DUDV_16BIT_UNSIGNED,
   MAX_DUDV_TYPE
};

float cubic_interpolate(float a, float b, float c, float d, float x)
{
   float v0, v1, v2, v3, x2;
   
   x2 = x * x;
   v0 = d - c - a + b;
   v1 = a - b - v0;
   v2 = c - a;
   v3 = b;
   
   return(v0 * x * x2 + v1 * x2 + v2 * x + v3);
}

void scale_pixels(unsigned char *dst, int dw, int dh,
                  unsigned char *src, int sw, int sh,
                  int bpp)
{
   int n, x, y;
   int ix, iy;
   float fx, fy;
   float dx, dy, val;
   float r0, r1, r2, r3;
   int srowbytes = sw * bpp;
   int drowbytes = dw * bpp;
   
#define VAL(x, y, c) \
   (float)src[((y) < 0 ? 0 : (y) >= sh ? sh - 1 : (y)) * srowbytes + \
              (((x) < 0 ? 0 : (x) >= sw ? sw - 1 : (x)) * bpp) + c]
      
   for(y = 0; y < dh; ++y)
   {
      fy = ((float)y / (float)dh) * (float)sh;
      iy = (int)fy;
      dy = fy - (float)iy;
      for(x = 0; x < dw; ++x)
      {
         fx = ((float)x / (float)dw) * (float)sw;
         ix = (int)fx;
         dx = fx - (float)ix;
         
         for(n = 0; n < bpp; ++n)
         {
            r0 = cubic_interpolate(VAL(ix - 1, iy - 1, n),
                                   VAL(ix,     iy - 1, n),
                                   VAL(ix + 1, iy - 1, n),
                                   VAL(ix + 2, iy - 1, n), dx);
            r1 = cubic_interpolate(VAL(ix - 1, iy,     n),
                                   VAL(ix,     iy,     n),
                                   VAL(ix + 1, iy,     n),
                                   VAL(ix + 2, iy,     n), dx);
            r2 = cubic_interpolate(VAL(ix - 1, iy + 1, n),
                                   VAL(ix,     iy + 1, n),
                                   VAL(ix + 1, iy + 1, n),
                                   VAL(ix + 2, iy + 1, n), dx);
            r3 = cubic_interpolate(VAL(ix - 1, iy + 2, n),
                                   VAL(ix,     iy + 2, n),
                                   VAL(ix + 1, iy + 2, n),
                                   VAL(ix + 2, iy + 2, n), dx);
            val = cubic_interpolate(r0, r1, r2, r3, dy);
            if(val <   0) val = 0;
            if(val > 255) val = 255;
            dst[y * drowbytes + (x * bpp) + n] = (unsigned char)val;
         }
      }
   }
#undef VAL   
}

int opt_filter = 0;//FILTER_SOBEL_3x3;
double opt_minz = 0.0;
double opt_scale = 8.0;//1.0;
int opt_wrap = 0;
int opt_height_source = 0;
int opt_alpha = ALPHA_NONE;
int opt_conversion = CONVERT_BIASED_RGB;//CONVERT_NONE;
int opt_dudv = DUDV_NONE;
int opt_xinvert = 0;
int opt_yinvert = 0;
int opt_swapRGB = 0;
double opt_contrast = 0.0;

static const float oneover255 = 1.0f / 255.0f; //i don't think this compiler sucks that much, but whatever.

static __inline void NORMALIZE(float *v)
{
   float len = (float)sqrt(SQR(v[0]) + SQR(v[1]) + SQR(v[2]));
   
   if(len > 1e-04f)
   {
      len = 1.0f / len;
      v[0] *= len;
      v[1] *= len;
      v[2] *= len;
   }
   else
      v[0] = v[1] = v[2] = 0;
}

typedef struct
{
   int x,y;
   float w;
} kernel_element;

static void make_kernel(kernel_element *k, float *weights, int size)
{
   int x, y, idx;
   
   for(y = 0; y < size; ++y)
   {
      for(x = 0; x < size; ++x)
      {
         idx = x + y * size;
         k[idx].x = x - (size / 2);
         k[idx].y = (size / 2) - y;
         k[idx].w = weights[idx];
      }
   }
}

static void rotate_array(float *dst, float *src, int size)
{
   int x, y, newx, newy;
   
   for(y = 0; y < size; ++y)
   {
      for(x = 0; x < size; ++x)
      {
         newy = size - x - 1;
         newx = y;
         dst[newx + newy * size] = src[x + y * size];
      }
   }
}

static void make_heightmap(unsigned char *image, int w, int h, int bpp)
{
   unsigned int i, num_pixels = w * h;
   int x, y;
   float v, hmin, hmax;
   float *s, *r;
   
   s = (float*)malloc(w * h * 3 * sizeof(float));
   if(s == 0)
   {
      NMAP_ASSERT(!"Memory allocation error!");
      return;
   }
   r = (float*)malloc(w * h * 4 * sizeof(float));
   if(r == 0)
   {
      free(s);
      NMAP_ASSERT(!"Memory allocation error!");
      return;
   }
   
   /* scale into 0 to 1 range, make signed -1 to 1 */
   for(i = 0; i < num_pixels; ++i)
   {
      s[3 * i + 0] = (((float)image[bpp * i + 0] / 255.0f) - 0.5) * 2.0f;
      s[3 * i + 1] = (((float)image[bpp * i + 1] / 255.0f) - 0.5) * 2.0f;
      s[3 * i + 2] = (((float)image[bpp * i + 2] / 255.0f) - 0.5) * 2.0f;
   }
   
   memset(r, 0, w * h * 4 * sizeof(float));

#define S(x, y, n) s[(y) * (w * 3) + ((x) * 3) + (n)]
#define R(x, y, n) r[(y) * (w * 4) + ((x) * 4) + (n)]
   
   /* top-left to bottom-right */
   for(x = 1; x < w; ++x)
      R(x, 0, 0) = R(x - 1, 0, 0) + S(x - 1, 0, 0);
   for(y = 1; y < h; ++y)
      R(0, y, 0) = R(0, y - 1, 0) + S(0, y - 1, 1);
   for(y = 1; y < h; ++y)
   {
      for(x = 1; x < w; ++x)
      {
         R(x, y, 0) = (R(x, y - 1, 0) + R(x - 1, y, 0) +
                       S(x - 1, y, 0) + S(x, y - 1, 1)) * 0.5f;
      }
   }

   /* top-right to bottom-left */
   for(x = w - 2; x >= 0; --x)
      R(x, 0, 1) = R(x + 1, 0, 1) - S(x + 1, 0, 0);
   for(y = 1; y < h; ++y)
      R(0, y, 1) = R(0, y - 1, 1) + S(0, y - 1, 1);
   for(y = 1; y < h; ++y)
   {
      for(x = w - 2; x >= 0; --x)
      {
         R(x, y, 1) = (R(x, y - 1, 1) + R(x + 1, y, 1) -
                       S(x + 1, y, 0) + S(x, y - 1, 1)) * 0.5f;
      }
   }

   /* bottom-left to top-right */
   for(x = 1; x < w; ++x)
      R(x, 0, 2) = R(x - 1, 0, 2) + S(x - 1, 0, 0);
   for(y = h - 2; y >= 0; --y)
      R(0, y, 2) = R(0, y + 1, 2) - S(0, y + 1, 1);
   for(y = h - 2; y >= 0; --y)
   {
      for(x = 1; x < w; ++x)
      {
         R(x, y, 2) = (R(x, y + 1, 2) + R(x - 1, y, 2) +
                       S(x - 1, y, 0) - S(x, y + 1, 1)) * 0.5f;
      }
   }

   /* bottom-right to top-left */
   for(x = w - 2; x >= 0; --x)
      R(x, 0, 3) = R(x + 1, 0, 3) - S(x + 1, 0, 0);
   for(y = h - 2; y >= 0; --y)
      R(0, y, 3) = R(0, y + 1, 3) - S(0, y + 1, 1);
   for(y = h - 2; y >= 0; --y)
   {
      for(x = w - 2; x >= 0; --x)
      {
         R(x, y, 3) = (R(x, y + 1, 3) + R(x + 1, y, 3) -
                       S(x + 1, y, 0) - S(x, y + 1, 1)) * 0.5f;
      }
   }

#undef S
#undef R

   /* accumulate, find min/max */
   hmin =  1e10f;
   hmax = -1e10f;
   for(i = 0; i < num_pixels; ++i)
   {
      r[4 * i] += r[4 * i + 1] + r[4 * i + 2] + r[4 * i + 3];
      if(r[4 * i] < hmin) hmin = r[4 * i];
      if(r[4 * i] > hmax) hmax = r[4 * i];
   }

   /* scale into 0 - 1 range */
   for(i = 0; i < num_pixels; ++i)
   {   
      v = (r[4 * i] - hmin) / (hmax - hmin);
      /* adjust contrast */
      v = (v - 0.5f) * opt_contrast + v;
      if(v < 0) v = 0;
      if(v > 1) v = 1;
      r[4 * i] = v;
   }

   /* write out results */
   for(i = 0; i < num_pixels; ++i)
   {
      v = r[4 * i] * 255.0f;
      image[bpp * i + 0] = (unsigned char)v;
      image[bpp * i + 1] = (unsigned char)v;
      image[bpp * i + 2] = (unsigned char)v;
   }

   free(s);
   free(r);
}

int GenerateNormalMap(unsigned char *src, unsigned char *dst, int dstWidth, int dstHeight, int dstBpp)
{
   int x, y;
   int width, height, bpp, rowbytes, amap_w = 0, amap_h = 0;
   unsigned char *d, *s, *tmp, *amap = 0;
   float *heights;
   float val, du, dv, n[3], weight;
   float rgb_bias[3];
   int i, num_elements = 0;
   kernel_element *kernel_du = 0;
   kernel_element *kernel_dv = 0;

   if(opt_filter < 0 || opt_filter >= MAX_FILTER_TYPE)
      opt_filter = FILTER_NONE;
   if(dstBpp != 4) opt_height_source = 0;
   if(dstBpp != 4 && (opt_dudv == DUDV_16BIT_SIGNED ||
                             opt_dudv == DUDV_16BIT_UNSIGNED))
      opt_dudv = DUDV_NONE;
   
   width = dstWidth;
   height = dstHeight;
   bpp = dstBpp;
   rowbytes = width * bpp;

   heights = malloc((width*height)*sizeof(float));//g_new(float, width * height);
   if(heights == 0)
   {
      NMAP_ASSERT(!"Memory allocation error!");
      return(-1);
   }

   switch(opt_filter)
   {
      case FILTER_NONE:
         num_elements = 2;
         kernel_du = (kernel_element*)malloc(2 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(2 * sizeof(kernel_element));
         
         kernel_du[0].x = -1; kernel_du[0].y = 0; kernel_du[0].w = -0.5f;
         kernel_du[1].x =  1; kernel_du[1].y = 0; kernel_du[1].w =  0.5f;
         
         kernel_dv[0].x = 0; kernel_dv[0].y =  1; kernel_dv[0].w =  0.5f;
         kernel_dv[1].x = 0; kernel_dv[1].y = -1; kernel_dv[1].w = -0.5f;
         
         break;
      case FILTER_SOBEL_3x3:
         num_elements = 6;
         kernel_du = (kernel_element*)malloc(6 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(6 * sizeof(kernel_element));
      
         kernel_du[0].x = -1; kernel_du[0].y =  1; kernel_du[0].w = -1.0f;
         kernel_du[1].x = -1; kernel_du[1].y =  0; kernel_du[1].w = -2.0f;
         kernel_du[2].x = -1; kernel_du[2].y = -1; kernel_du[2].w = -1.0f;
         kernel_du[3].x =  1; kernel_du[3].y =  1; kernel_du[3].w =  1.0f;
         kernel_du[4].x =  1; kernel_du[4].y =  0; kernel_du[4].w =  2.0f;
         kernel_du[5].x =  1; kernel_du[5].y = -1; kernel_du[5].w =  1.0f;
         
         kernel_dv[0].x = -1; kernel_dv[0].y =  1; kernel_dv[0].w =  1.0f;
         kernel_dv[1].x =  0; kernel_dv[1].y =  1; kernel_dv[1].w =  2.0f;
         kernel_dv[2].x =  1; kernel_dv[2].y =  1; kernel_dv[2].w =  1.0f;
         kernel_dv[3].x = -1; kernel_dv[3].y = -1; kernel_dv[3].w = -1.0f;
         kernel_dv[4].x =  0; kernel_dv[4].y = -1; kernel_dv[4].w = -2.0f;
         kernel_dv[5].x =  1; kernel_dv[5].y = -1; kernel_dv[5].w = -1.0f;
         
         break;
      case FILTER_SOBEL_5x5:
         num_elements = 20;
         kernel_du = (kernel_element*)malloc(20 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(20 * sizeof(kernel_element));

         kernel_du[ 0].x = -2; kernel_du[ 0].y =  2; kernel_du[ 0].w =  -1.0f;
         kernel_du[ 1].x = -2; kernel_du[ 1].y =  1; kernel_du[ 1].w =  -4.0f;
         kernel_du[ 2].x = -2; kernel_du[ 2].y =  0; kernel_du[ 2].w =  -6.0f;
         kernel_du[ 3].x = -2; kernel_du[ 3].y = -1; kernel_du[ 3].w =  -4.0f;
         kernel_du[ 4].x = -2; kernel_du[ 4].y = -2; kernel_du[ 4].w =  -1.0f;
         kernel_du[ 5].x = -1; kernel_du[ 5].y =  2; kernel_du[ 5].w =  -2.0f;
         kernel_du[ 6].x = -1; kernel_du[ 6].y =  1; kernel_du[ 6].w =  -8.0f;
         kernel_du[ 7].x = -1; kernel_du[ 7].y =  0; kernel_du[ 7].w = -12.0f;
         kernel_du[ 8].x = -1; kernel_du[ 8].y = -1; kernel_du[ 8].w =  -8.0f;
         kernel_du[ 9].x = -1; kernel_du[ 9].y = -2; kernel_du[ 9].w =  -2.0f;
         kernel_du[10].x =  1; kernel_du[10].y =  2; kernel_du[10].w =   2.0f;
         kernel_du[11].x =  1; kernel_du[11].y =  1; kernel_du[11].w =   8.0f;
         kernel_du[12].x =  1; kernel_du[12].y =  0; kernel_du[12].w =  12.0f;
         kernel_du[13].x =  1; kernel_du[13].y = -1; kernel_du[13].w =   8.0f;
         kernel_du[14].x =  1; kernel_du[14].y = -2; kernel_du[14].w =   2.0f;
         kernel_du[15].x =  2; kernel_du[15].y =  2; kernel_du[15].w =   1.0f;
         kernel_du[16].x =  2; kernel_du[16].y =  1; kernel_du[16].w =   4.0f;
         kernel_du[17].x =  2; kernel_du[17].y =  0; kernel_du[17].w =   6.0f;
         kernel_du[18].x =  2; kernel_du[18].y = -1; kernel_du[18].w =   4.0f;
         kernel_du[19].x =  2; kernel_du[19].y = -2; kernel_du[19].w =   1.0f;
      
         kernel_dv[ 0].x = -2; kernel_dv[ 0].y =  2; kernel_dv[ 0].w =   1.0f;
         kernel_dv[ 1].x = -1; kernel_dv[ 1].y =  2; kernel_dv[ 1].w =   4.0f;
         kernel_dv[ 2].x =  0; kernel_dv[ 2].y =  2; kernel_dv[ 2].w =   6.0f;
         kernel_dv[ 3].x =  1; kernel_dv[ 3].y =  2; kernel_dv[ 3].w =   4.0f;
         kernel_dv[ 4].x =  2; kernel_dv[ 4].y =  2; kernel_dv[ 4].w =   1.0f;
         kernel_dv[ 5].x = -2; kernel_dv[ 5].y =  1; kernel_dv[ 5].w =   2.0f;
         kernel_dv[ 6].x = -1; kernel_dv[ 6].y =  1; kernel_dv[ 6].w =   8.0f;
         kernel_dv[ 7].x =  0; kernel_dv[ 7].y =  1; kernel_dv[ 7].w =  12.0f;
         kernel_dv[ 8].x =  1; kernel_dv[ 8].y =  1; kernel_dv[ 8].w =   8.0f;
         kernel_dv[ 9].x =  2; kernel_dv[ 9].y =  1; kernel_dv[ 9].w =   2.0f;
         kernel_dv[10].x = -2; kernel_dv[10].y = -1; kernel_dv[10].w =  -2.0f;
         kernel_dv[11].x = -1; kernel_dv[11].y = -1; kernel_dv[11].w =  -8.0f;
         kernel_dv[12].x =  0; kernel_dv[12].y = -1; kernel_dv[12].w = -12.0f;
         kernel_dv[13].x =  1; kernel_dv[13].y = -1; kernel_dv[13].w =  -8.0f;
         kernel_dv[14].x =  2; kernel_dv[14].y = -1; kernel_dv[14].w =  -2.0f;
         kernel_dv[15].x = -2; kernel_dv[15].y = -2; kernel_dv[15].w =  -1.0f;
         kernel_dv[16].x = -1; kernel_dv[16].y = -2; kernel_dv[16].w =  -4.0f;
         kernel_dv[17].x =  0; kernel_dv[17].y = -2; kernel_dv[17].w =  -6.0f;
         kernel_dv[18].x =  1; kernel_dv[18].y = -2; kernel_dv[18].w =  -4.0f;
         kernel_dv[19].x =  2; kernel_dv[19].y = -2; kernel_dv[19].w =  -1.0f;
      
         break;
      case FILTER_PREWITT_3x3:
         num_elements = 6;
         kernel_du = (kernel_element*)malloc(6 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(6 * sizeof(kernel_element));
      
         kernel_du[0].x = -1; kernel_du[0].y =  1; kernel_du[0].w = -1.0f;
         kernel_du[1].x = -1; kernel_du[1].y =  0; kernel_du[1].w = -1.0f;
         kernel_du[2].x = -1; kernel_du[2].y = -1; kernel_du[2].w = -1.0f;
         kernel_du[3].x =  1; kernel_du[3].y =  1; kernel_du[3].w =  1.0f;
         kernel_du[4].x =  1; kernel_du[4].y =  0; kernel_du[4].w =  1.0f;
         kernel_du[5].x =  1; kernel_du[5].y = -1; kernel_du[5].w =  1.0f;
         
         kernel_dv[0].x = -1; kernel_dv[0].y =  1; kernel_dv[0].w =  1.0f;
         kernel_dv[1].x =  0; kernel_dv[1].y =  1; kernel_dv[1].w =  1.0f;
         kernel_dv[2].x =  1; kernel_dv[2].y =  1; kernel_dv[2].w =  1.0f;
         kernel_dv[3].x = -1; kernel_dv[3].y = -1; kernel_dv[3].w = -1.0f;
         kernel_dv[4].x =  0; kernel_dv[4].y = -1; kernel_dv[4].w = -1.0f;
         kernel_dv[5].x =  1; kernel_dv[5].y = -1; kernel_dv[5].w = -1.0f;
         
         break;      
      case FILTER_PREWITT_5x5:
         num_elements = 20;
         kernel_du = (kernel_element*)malloc(20 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(20 * sizeof(kernel_element));

         kernel_du[ 0].x = -2; kernel_du[ 0].y =  2; kernel_du[ 0].w = -1.0f;
         kernel_du[ 1].x = -2; kernel_du[ 1].y =  1; kernel_du[ 1].w = -1.0f;
         kernel_du[ 2].x = -2; kernel_du[ 2].y =  0; kernel_du[ 2].w = -1.0f;
         kernel_du[ 3].x = -2; kernel_du[ 3].y = -1; kernel_du[ 3].w = -1.0f;
         kernel_du[ 4].x = -2; kernel_du[ 4].y = -2; kernel_du[ 4].w = -1.0f;
         kernel_du[ 5].x = -1; kernel_du[ 5].y =  2; kernel_du[ 5].w = -2.0f;
         kernel_du[ 6].x = -1; kernel_du[ 6].y =  1; kernel_du[ 6].w = -2.0f;
         kernel_du[ 7].x = -1; kernel_du[ 7].y =  0; kernel_du[ 7].w = -2.0f;
         kernel_du[ 8].x = -1; kernel_du[ 8].y = -1; kernel_du[ 8].w = -2.0f;
         kernel_du[ 9].x = -1; kernel_du[ 9].y = -2; kernel_du[ 9].w = -2.0f;
         kernel_du[10].x =  1; kernel_du[10].y =  2; kernel_du[10].w =  2.0f;
         kernel_du[11].x =  1; kernel_du[11].y =  1; kernel_du[11].w =  2.0f;
         kernel_du[12].x =  1; kernel_du[12].y =  0; kernel_du[12].w =  2.0f;
         kernel_du[13].x =  1; kernel_du[13].y = -1; kernel_du[13].w =  2.0f;
         kernel_du[14].x =  1; kernel_du[14].y = -2; kernel_du[14].w =  2.0f;
         kernel_du[15].x =  2; kernel_du[15].y =  2; kernel_du[15].w =  1.0f;
         kernel_du[16].x =  2; kernel_du[16].y =  1; kernel_du[16].w =  1.0f;
         kernel_du[17].x =  2; kernel_du[17].y =  0; kernel_du[17].w =  1.0f;
         kernel_du[18].x =  2; kernel_du[18].y = -1; kernel_du[18].w =  1.0f;
         kernel_du[19].x =  2; kernel_du[19].y = -2; kernel_du[19].w =  1.0f;
      
         kernel_dv[ 0].x = -2; kernel_dv[ 0].y =  2; kernel_dv[ 0].w =  1.0f;
         kernel_dv[ 1].x = -1; kernel_dv[ 1].y =  2; kernel_dv[ 1].w =  1.0f;
         kernel_dv[ 2].x =  0; kernel_dv[ 2].y =  2; kernel_dv[ 2].w =  1.0f;
         kernel_dv[ 3].x =  1; kernel_dv[ 3].y =  2; kernel_dv[ 3].w =  1.0f;
         kernel_dv[ 4].x =  2; kernel_dv[ 4].y =  2; kernel_dv[ 4].w =  1.0f;
         kernel_dv[ 5].x = -2; kernel_dv[ 5].y =  1; kernel_dv[ 5].w =  2.0f;
         kernel_dv[ 6].x = -1; kernel_dv[ 6].y =  1; kernel_dv[ 6].w =  2.0f;
         kernel_dv[ 7].x =  0; kernel_dv[ 7].y =  1; kernel_dv[ 7].w =  2.0f;
         kernel_dv[ 8].x =  1; kernel_dv[ 8].y =  1; kernel_dv[ 8].w =  2.0f;
         kernel_dv[ 9].x =  2; kernel_dv[ 9].y =  1; kernel_dv[ 9].w =  2.0f;
         kernel_dv[10].x = -2; kernel_dv[10].y = -1; kernel_dv[10].w = -2.0f;
         kernel_dv[11].x = -1; kernel_dv[11].y = -1; kernel_dv[11].w = -2.0f;
         kernel_dv[12].x =  0; kernel_dv[12].y = -1; kernel_dv[12].w = -2.0f;
         kernel_dv[13].x =  1; kernel_dv[13].y = -1; kernel_dv[13].w = -2.0f;
         kernel_dv[14].x =  2; kernel_dv[14].y = -1; kernel_dv[14].w = -2.0f;
         kernel_dv[15].x = -2; kernel_dv[15].y = -2; kernel_dv[15].w = -1.0f;
         kernel_dv[16].x = -1; kernel_dv[16].y = -2; kernel_dv[16].w = -1.0f;
         kernel_dv[17].x =  0; kernel_dv[17].y = -2; kernel_dv[17].w = -1.0f;
         kernel_dv[18].x =  1; kernel_dv[18].y = -2; kernel_dv[18].w = -1.0f;
         kernel_dv[19].x =  2; kernel_dv[19].y = -2; kernel_dv[19].w = -1.0f;
      
         break;
      case FILTER_3x3:
         num_elements = 6;
         kernel_du = (kernel_element*)malloc(6 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(6 * sizeof(kernel_element));
      
         weight = 1.0f / 6.0f;
      
         kernel_du[0].x = -1; kernel_du[0].y =  1; kernel_du[0].w = -weight;
         kernel_du[1].x = -1; kernel_du[1].y =  0; kernel_du[1].w = -weight;
         kernel_du[2].x = -1; kernel_du[2].y = -1; kernel_du[2].w = -weight;
         kernel_du[3].x =  1; kernel_du[3].y =  1; kernel_du[3].w =  weight;
         kernel_du[4].x =  1; kernel_du[4].y =  0; kernel_du[4].w =  weight;
         kernel_du[5].x =  1; kernel_du[5].y = -1; kernel_du[5].w =  weight;
         
         kernel_dv[0].x = -1; kernel_dv[0].y =  1; kernel_dv[0].w =  weight;
         kernel_dv[1].x =  0; kernel_dv[1].y =  1; kernel_dv[1].w =  weight;
         kernel_dv[2].x =  1; kernel_dv[2].y =  1; kernel_dv[2].w =  weight;
         kernel_dv[3].x = -1; kernel_dv[3].y = -1; kernel_dv[3].w = -weight;
         kernel_dv[4].x =  0; kernel_dv[4].y = -1; kernel_dv[4].w = -weight;
         kernel_dv[5].x =  1; kernel_dv[5].y = -1; kernel_dv[5].w = -weight;
         break;
      case FILTER_5x5:
      {
         int n;
         float usum = 0, vsum = 0;
         float wt22 = 1.0f / 16.0f;
         float wt12 = 1.0f / 10.0f;
         float wt02 = 1.0f / 8.0f;
         float wt11 = 1.0f / 2.8f;
         num_elements = 20;
         kernel_du = (kernel_element*)malloc(20 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(20 * sizeof(kernel_element));
         
         kernel_du[0 ].x = -2; kernel_du[0 ].y =  2; kernel_du[0 ].w = -wt22;
         kernel_du[1 ].x = -1; kernel_du[1 ].y =  2; kernel_du[1 ].w = -wt12;
         kernel_du[2 ].x =  1; kernel_du[2 ].y =  2; kernel_du[2 ].w =  wt12;
         kernel_du[3 ].x =  2; kernel_du[3 ].y =  2; kernel_du[3 ].w =  wt22;
         kernel_du[4 ].x = -2; kernel_du[4 ].y =  1; kernel_du[4 ].w = -wt12;
         kernel_du[5 ].x = -1; kernel_du[5 ].y =  1; kernel_du[5 ].w = -wt11;
         kernel_du[6 ].x =  1; kernel_du[6 ].y =  1; kernel_du[6 ].w =  wt11;
         kernel_du[7 ].x =  2; kernel_du[7 ].y =  1; kernel_du[7 ].w =  wt12;
         kernel_du[8 ].x = -2; kernel_du[8 ].y =  0; kernel_du[8 ].w = -wt02;
         kernel_du[9 ].x = -1; kernel_du[9 ].y =  0; kernel_du[9 ].w = -0.5f;
         kernel_du[10].x =  1; kernel_du[10].y =  0; kernel_du[10].w =  0.5f;
         kernel_du[11].x =  2; kernel_du[11].y =  0; kernel_du[11].w =  wt02;
         kernel_du[12].x = -2; kernel_du[12].y = -1; kernel_du[12].w = -wt12;
         kernel_du[13].x = -1; kernel_du[13].y = -1; kernel_du[13].w = -wt11;
         kernel_du[14].x =  1; kernel_du[14].y = -1; kernel_du[14].w =  wt11;
         kernel_du[15].x =  2; kernel_du[15].y = -1; kernel_du[15].w =  wt12;
         kernel_du[16].x = -2; kernel_du[16].y = -2; kernel_du[16].w = -wt22;
         kernel_du[17].x = -1; kernel_du[17].y = -2; kernel_du[17].w = -wt12;
         kernel_du[18].x =  1; kernel_du[18].y = -2; kernel_du[18].w =  wt12;
         kernel_du[19].x =  2; kernel_du[19].y = -2; kernel_du[19].w =  wt22;
         
         kernel_dv[0 ].x = -2; kernel_dv[0 ].y =  2; kernel_dv[0 ].w =  wt22;
         kernel_dv[1 ].x = -1; kernel_dv[1 ].y =  2; kernel_dv[1 ].w =  wt12;
         kernel_dv[2 ].x =  0; kernel_dv[2 ].y =  2; kernel_dv[2 ].w =  0.25f;
         kernel_dv[3 ].x =  1; kernel_dv[3 ].y =  2; kernel_dv[3 ].w =  wt12;
         kernel_dv[4 ].x =  2; kernel_dv[4 ].y =  2; kernel_dv[4 ].w =  wt22;
         kernel_dv[5 ].x = -2; kernel_dv[5 ].y =  1; kernel_dv[5 ].w =  wt12;
         kernel_dv[6 ].x = -1; kernel_dv[6 ].y =  1; kernel_dv[6 ].w =  wt11;
         kernel_dv[7 ].x =  0; kernel_dv[7 ].y =  1; kernel_dv[7 ].w =  0.5f;
         kernel_dv[8 ].x =  1; kernel_dv[8 ].y =  1; kernel_dv[8 ].w =  wt11;
         kernel_dv[9 ].x =  2; kernel_dv[9 ].y =  1; kernel_dv[9 ].w =  wt22;
         kernel_dv[10].x = -2; kernel_dv[10].y = -1; kernel_dv[10].w = -wt22;
         kernel_dv[11].x = -1; kernel_dv[11].y = -1; kernel_dv[11].w = -wt11;
         kernel_dv[12].x =  0; kernel_dv[12].y = -1; kernel_dv[12].w = -0.5f;
         kernel_dv[13].x =  1; kernel_dv[13].y = -1; kernel_dv[13].w = -wt11;
         kernel_dv[14].x =  2; kernel_dv[14].y = -1; kernel_dv[14].w = -wt12;
         kernel_dv[15].x = -2; kernel_dv[15].y = -2; kernel_dv[15].w = -wt22;
         kernel_dv[16].x = -1; kernel_dv[16].y = -2; kernel_dv[16].w = -wt12;
         kernel_dv[17].x =  0; kernel_dv[17].y = -2; kernel_dv[17].w = -0.25f;
         kernel_dv[18].x =  1; kernel_dv[18].y = -2; kernel_dv[18].w = -wt12;
         kernel_dv[19].x =  2; kernel_dv[19].y = -2; kernel_dv[19].w = -wt22;

         for(n = 0; n < 20; ++n)
         {
            usum += (float)fabs(kernel_du[n].w);
            vsum += (float)fabs(kernel_dv[n].w);
         }
         for(n = 0; n < 20; ++n)
         {
            kernel_du[n].w /= usum;
            kernel_dv[n].w /= vsum;
         }
         
         break;
      }
      case FILTER_7x7:
      {
         float du_weights[]=
         {
            -1, -2, -3, 0, 3, 2, 1,
            -2, -3, -4, 0, 4, 3, 2,
            -3, -4, -5, 0, 5, 4, 3,
            -4, -5, -6, 0, 6, 5, 4,
            -3, -4, -5, 0, 5, 4, 3,
            -2, -3, -4, 0, 4, 3, 2,
            -1, -2, -3, 0, 3, 2, 1   
         };
         float dv_weights[49];
         int n;
         float usum = 0, vsum = 0;
         
         num_elements = 49;
         kernel_du = (kernel_element*)malloc(49 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(49 * sizeof(kernel_element));
         
         make_kernel(kernel_du, du_weights, 7);
         rotate_array(dv_weights, du_weights, 7);
         make_kernel(kernel_dv, dv_weights, 7);
         
         for(n = 0; n < 49; ++n)
         {
            usum += (float)fabs(kernel_du[n].w);
            vsum += (float)fabs(kernel_dv[n].w);
         }
         for(n = 0; n < 49; ++n)
         {
            kernel_du[n].w /= usum;
            kernel_dv[n].w /= vsum;
         }
         
         break;
      }
      case FILTER_9x9:
      {
         float du_weights[]=
         {
            -1, -2, -3, -4, 0, 4, 3, 2, 1,
            -2, -3, -4, -5, 0, 5, 4, 3, 2,
            -3, -4, -5, -6, 0, 6, 5, 4, 3,
            -4, -5, -6, -7, 0, 7, 6, 5, 4,
            -5, -6, -7, -8, 0, 8, 7, 6, 5,
            -4, -5, -6, -7, 0, 7, 6, 5, 4,
            -3, -4, -5, -6, 0, 6, 5, 4, 3,
            -2, -3, -4, -5, 0, 5, 4, 3, 2,
            -1, -2, -3, -4, 0, 4, 3, 2, 1     
         };
         float dv_weights[81];
         int n;
         float usum = 0, vsum = 0;
         
         num_elements = 81;
         kernel_du = (kernel_element*)malloc(81 * sizeof(kernel_element));
         kernel_dv = (kernel_element*)malloc(81 * sizeof(kernel_element));
         
         make_kernel(kernel_du, du_weights, 9);
         rotate_array(dv_weights, du_weights, 9);
         make_kernel(kernel_dv, dv_weights, 9);
         
         for(n = 0; n < 81; ++n)
         {
            usum += (float)fabs(kernel_du[n].w);
            vsum += (float)fabs(kernel_dv[n].w);
         }
         for(n = 0; n < 81; ++n)
         {
            kernel_du[n].w /= usum;
            kernel_dv[n].w /= vsum;
         }
         
         break;
      }
   }

   if(opt_conversion == CONVERT_BIASED_RGB)
   {
      /* approximated average color of the image
       * scale to 16x16, accumulate the pixels and average */
      unsigned int sum[3];

      tmp = malloc(16 * 16 * bpp);
      scale_pixels(tmp, 16, 16, src, width, height, bpp);

      sum[0] = sum[1] = sum[2] = 0;
      
      s = src;
      for(y = 0; y < 16; ++y)
      {
         for(x = 0; x < 16; ++x)
         {
            sum[0] += *s++;
            sum[1] += *s++;
            sum[2] += *s++;
            if(bpp == 4) s++;
         }
      }
         
      rgb_bias[0] = (float)sum[0] / 256.0f;
      rgb_bias[1] = (float)sum[1] / 256.0f;
      rgb_bias[2] = (float)sum[2] / 256.0f;
      
      free(tmp);
   }
   else
   {
      rgb_bias[0] = 0;
      rgb_bias[1] = 0;
      rgb_bias[2] = 0;
   }

   if(opt_conversion != CONVERT_NORMALIZE_ONLY &&
      opt_conversion != CONVERT_DUDV_TO_NORMAL &&
      opt_conversion != CONVERT_HEIGHTMAP)
   {
      s = src;
      for(y = 0; y < height; ++y)
      {
         for(x = 0; x < width; ++x)
         {
            if(!opt_height_source)
            {
               switch(opt_conversion)
               {
                  case CONVERT_NONE:
                     val = (float)s[0] * 0.3f +
                           (float)s[1] * 0.59f +
                           (float)s[2] * 0.11f;
                     break;
                  case CONVERT_BIASED_RGB:
                     val = (((float)max(0, s[0] - rgb_bias[0])) * 0.3f ) +
                           (((float)max(0, s[1] - rgb_bias[1])) * 0.59f) +
                           (((float)max(0, s[2] - rgb_bias[2])) * 0.11f);
                     break;
                  case CONVERT_RED:
                     val = (float)s[0];
                     break;
                  case CONVERT_GREEN:
                     val = (float)s[1];
                     break;
                  case CONVERT_BLUE:
                     val = (float)s[2];
                     break;
                  case CONVERT_MAX_RGB:
                     val = (float)max(s[0], max(s[1], s[2]));
                     break;
                  case CONVERT_MIN_RGB:
                     val = (float)min(s[0], min(s[1], s[2]));
                     break;
                  case CONVERT_COLORSPACE:
                     val = (1.0f - ((1.0f - ((float)s[0] / 255.0f)) *
                                    (1.0f - ((float)s[1] / 255.0f)) *
                                    (1.0f - ((float)s[2] / 255.0f)))) * 255.0f;
                     break;
                  default:
                     val = 255.0f;
                     break;
               }
            }
            else
               val = (float)s[3];
         
            heights[x + y * width] = val * oneover255;
         
            s += bpp;
         }
      }
   }

#define HEIGHT(x,y) \
   (heights[(max(0, min(width - 1, (x)))) + (max(0, min(height - 1, (y)))) * width])
#define HEIGHT_WRAP(x,y) \
   (heights[((x) < 0 ? (width + (x)) : ((x) >= width ? ((x) - width) : (x)))+ \
            (((y) < 0 ? (height + (y)) : ((y) >= height ? ((y) - height) : (y))) * width)])

  
   for(y = 0; y < height; ++y)
   {
      for(x = 0; x < width; ++x)
      {
         d = dst + ((y * rowbytes) + (x * bpp));
         s = src + ((y * rowbytes) + (x * bpp));

         if(opt_conversion == CONVERT_NORMALIZE_ONLY ||
            opt_conversion == CONVERT_HEIGHTMAP)
         {
            n[0] = (((float)s[0] * oneover255) - 0.5f) * 2.0f;
            n[1] = (((float)s[1] * oneover255) - 0.5f) * 2.0f;
            n[2] = (((float)s[2] * oneover255) - 0.5f) * 2.0f;
            n[0] *= opt_scale;
            n[1] *= opt_scale;
         }
         else if(opt_conversion == CONVERT_DUDV_TO_NORMAL)
         {
            n[0] = (((float)s[0] * oneover255) - 0.5f) * 2.0f;
            n[1] = (((float)s[1] * oneover255) - 0.5f) * 2.0f;
            n[2] = (float)sqrt(1.0f - (n[0] * n[0] - n[1] * n[1]));
            n[0] *= opt_scale;
            n[1] *= opt_scale;
         }
         else
         {
            du = 0; dv = 0;
            if(!opt_wrap)
            {
               for(i = 0; i < num_elements; ++i)
                  du += HEIGHT(x + kernel_du[i].x,
                               y + kernel_du[i].y) * kernel_du[i].w;
               for(i = 0; i < num_elements; ++i)
                  dv += HEIGHT(x + kernel_dv[i].x,
                               y + kernel_dv[i].y) * kernel_dv[i].w;
            }
            else
            {
               for(i = 0; i < num_elements; ++i)
                  du += HEIGHT_WRAP(x + kernel_du[i].x,
                                    y + kernel_du[i].y) * kernel_du[i].w;
               for(i = 0; i < num_elements; ++i)
                  dv += HEIGHT_WRAP(x + kernel_dv[i].x,
                                    y + kernel_dv[i].y) * kernel_dv[i].w;
            }
            
            n[0] = -du * opt_scale;
            n[1] = -dv * opt_scale;
            n[2] = 1.0f;
         }
         
         NORMALIZE(n);
         
         if(n[2] < opt_minz)
         {
            n[2] = opt_minz;
            NORMALIZE(n);
         }
         
         if(opt_xinvert) n[0] = -n[0];
         if(opt_yinvert) n[1] = -n[1];
         if(opt_swapRGB)
         {
            val = n[0];
            n[0] = n[2];
            n[2] = val;
         }
         
         if(!opt_dudv)
         {
            *d++ = (unsigned char)((n[0] + 1.0f) * 127.5f);
            *d++ = (unsigned char)((n[1] + 1.0f) * 127.5f);
            *d++ = (unsigned char)((n[2] + 1.0f) * 127.5f);
         
            if(dstBpp == 4)
            {
               switch(opt_alpha)
               {
                  case ALPHA_NONE:
                     *d++ = s[3]; break;
                  case ALPHA_HEIGHT:
                     *d++ = (unsigned char)(heights[x + y * width] * 255.0f); break;
                  case ALPHA_INVERSE_HEIGHT:
                     *d++ = 255 - (unsigned char)(heights[x + y * width] * 255.0f); break;
                  case ALPHA_ZERO:
                     *d++ = 0; break;
                  case ALPHA_ONE:
                     *d++ = 255; break;
                  case ALPHA_INVERT:
                     *d++ = 255 - s[3]; break;
                  case ALPHA_MAP:
                     *d++ = 0;/*sample_alpha_map(amap, x, y, amap_w, amap_h,
                                             width, height); break;*/
                  default:
                     *d++ = s[3]; break;
               }
            }
         }
         else
         {
            if(opt_dudv == DUDV_8BIT_SIGNED ||
               opt_dudv == DUDV_8BIT_UNSIGNED)
            {
               if(opt_dudv == DUDV_8BIT_UNSIGNED)
               {
                  n[0] += 1.0f;
                  n[1] += 1.0f;
               }
               *d++ = (unsigned char)(n[0] * 127.5f);
               *d++ = (unsigned char)(n[1] * 127.5f);
               *d++ = 0;
               if(dstBpp == 4) *d++ = 255;
            }
            else if(opt_dudv == DUDV_16BIT_SIGNED ||
                    opt_dudv == DUDV_16BIT_UNSIGNED)
            {
               unsigned short *d16 = (unsigned short*)d;
               if(opt_dudv == DUDV_16BIT_UNSIGNED)
               {
                  n[0] += 1.0f;
                  n[1] += 1.0f;
               }
               *d16++ = (unsigned short)(n[0] * 32767.5f);
               *d16++ = (unsigned short)(n[1] * 32767.5f);
            }
         }
      }
   }
   
  
#undef HEIGHT
#undef HEIGHT_WRAP
  
   free(heights);
   free(kernel_du);
   free(kernel_dv);
   if(amap) free(amap);
   
   return(0);
}
