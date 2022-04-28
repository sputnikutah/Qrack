/*
Copyright (C) 1997-2001 Id Software, Inc.

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
// gl_bloom.c: 2D lighting post process effect
//
//http://www.quakesrc.org/forums/viewtopic.php?t=4340&start=0

//#include "r_local.h"
#include "quakedef.h"

extern	cvar_t	vid_width;
extern	cvar_t	vid_height;

/* 
============================================================================== 
 
                        LIGHT BLOOMS
 
============================================================================== 
*/ 

static float Diamond8x[8][8] = 
{ 
        0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 
        0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, 
        0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, 
        0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, 
        0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, 
        0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, 
        0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, 
        0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f 
};

static float Diamond6x[6][6] = 
{ 
        0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 
        0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f,  
        0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, 
        0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, 
        0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f, 
        0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f 
};

static float Diamond4x[4][4] = 
{  
        0.3f, 0.4f, 0.4f, 0.3f,  
        0.4f, 0.9f, 0.9f, 0.4f, 
        0.4f, 0.9f, 0.9f, 0.4f, 
        0.3f, 0.4f, 0.4f, 0.3f 
};

static int      BLOOM_SIZE;

cvar_t	r_bloom					= {"r_bloom", "0", true};
cvar_t	r_bloom_alpha			= {"r_bloom_alpha", "0.3", true};
cvar_t	r_bloom_color			= {"r_bloom_color", "0.5", true};
cvar_t	r_bloom_diamond_size	= {"r_bloom_diamond_size", "8", true};
cvar_t	r_bloom_intensity		= {"r_bloom_intensity", "1", true};
cvar_t	r_bloom_darken			= {"r_bloom_darken", "4", true};
cvar_t	r_bloom_sample_size		= {"r_bloom_sample_size", "256", true};
cvar_t	r_bloom_fast_sample		= {"r_bloom_fast_sample", "0", true};

int r_bloomscreentexture;
int	r_bloomeffecttexture;
int r_bloombackuptexture;
int r_bloomdownsamplingtexture;

static int	r_screendownsamplingtexture_size;
static int  screen_texture_width, screen_texture_height;
//static int  r_screenbackuptexture_size;
static int  r_screenbackuptexture_width, r_screenbackuptexture_height; 

//current refdef size:
static int	curView_x;
static int  curView_y;
static int  curView_width;
static int  curView_height;

//texture coordinates of screen data inside screentexture
static float screenText_tcw;
static float screenText_tch;

static int  sample_width;
static int  sample_height;

//texture coordinates of adjusted textures
static float sampleText_tcw;
static float sampleText_tch;

//this macro is in sample size workspace coordinates
#define R_Bloom_SamplePass( xpos, ypos )                           \
    glBegin(GL_QUADS);                                             \
    glTexCoord2f(  0,                      sampleText_tch);        \
    glVertex2f(    xpos,                   ypos);                  \
    glTexCoord2f(  0,                      0);                     \
    glVertex2f(    xpos,                   ypos+sample_height);    \
    glTexCoord2f(  sampleText_tcw,         0);                     \
    glVertex2f(    xpos+sample_width,      ypos+sample_height);    \
    glTexCoord2f(  sampleText_tcw,         sampleText_tch);        \
    glVertex2f(    xpos+sample_width,      ypos);                  \
    glEnd();

#define R_Bloom_Quad( x, y, width, height, textwidth, textheight ) \
    glBegin(GL_QUADS);                                             \
    glTexCoord2f(  0,          textheight);                        \
    glVertex2f(    x,          y);                                 \
    glTexCoord2f(  0,          0);                                 \
    glVertex2f(    x,          y+height);                          \
    glTexCoord2f(  textwidth,  0);                                 \
    glVertex2f(    x+width,    y+height);                          \
    glTexCoord2f(  textwidth,  textheight);                        \
    glVertex2f(    x+width,    y);                                 \
    glEnd();



/*
=================
R_Bloom_InitBackUpTexture
=================
*/
void R_Bloom_InitBackUpTexture( int width, int height )
{
	unsigned char   *data;
    
    data = Q_malloc( width * height * 4 );
    memset( data, 0, width * height * 4 );
 
	r_screenbackuptexture_width  = width;
    r_screenbackuptexture_height = height;

  	r_bloombackuptexture =	GL_LoadTexture ( "bloomtex", width, height, data, TEX_LUMA, 4);
    Q_free ( data );
}

/*
=================
R_Bloom_InitEffectTexture
=================
*/
void R_Bloom_InitEffectTexture( void )
{
    unsigned char   *data;
    float   bloomsizecheck;
    
    if( r_bloom_sample_size.value < 32 )
        Cvar_SetValue ("r_bloom_sample_size", 32);

    //make sure bloom size is a power of 2
    BLOOM_SIZE = r_bloom_sample_size.value;
    bloomsizecheck = (float)BLOOM_SIZE;
    while(bloomsizecheck > 1.0f) bloomsizecheck /= 2.0f;
    if( bloomsizecheck != 1.0f )
    {
        BLOOM_SIZE = 32;
        while( BLOOM_SIZE < r_bloom_sample_size.value )
            BLOOM_SIZE *= 2;
    }

    //make sure bloom size doesn't have stupid values
    if( BLOOM_SIZE > screen_texture_width ||
        BLOOM_SIZE > screen_texture_height )
        BLOOM_SIZE = min( screen_texture_width, screen_texture_height );

    if( BLOOM_SIZE != r_bloom_sample_size.value )
        Cvar_SetValue ("r_bloom_sample_size", BLOOM_SIZE);

    data = Q_malloc( BLOOM_SIZE * BLOOM_SIZE * 4 );
    memset( data, 0, BLOOM_SIZE * BLOOM_SIZE * 4 );

  	r_bloomeffecttexture = GL_LoadTexture ( "***r_bloomeffecttexture***", BLOOM_SIZE, BLOOM_SIZE, data, TEX_LUMA, 4);
    
    Q_free ( data );
}

/*
=================
R_Bloom_InitTextures
=================
*/
void R_Bloom_InitTextures( void )
{
    unsigned char   *data;
    int     size;

    //find closer power of 2 to screen size 
    for (screen_texture_width = 1;screen_texture_width < vid_width.value;screen_texture_width *= 2);
	for (screen_texture_height = 1;screen_texture_height < vid_height.value;screen_texture_height *= 2);

    //disable blooms if we can't handle a texture of that size
    if( screen_texture_width > 4096 || screen_texture_height > 4096 ) 
	{
        screen_texture_width = screen_texture_height = 0;
        Cvar_SetValue ("r_bloom", 0);
        Con_Printf( "WARNING: 'R_InitBloomScreenTexture' too high resolution for Light Bloom. Effect disabled\n" );
        return;
    }

    //init the screen texture
    size = screen_texture_width * screen_texture_height * 4;
    data = Q_malloc( size );
    memset( data, 255, size );
	r_bloomscreentexture = GL_LoadTexture ( "***r_screenbackuptexture***", screen_texture_width, screen_texture_height, data, TEX_LUMA, 4);
    Q_free ( data );

    //validate bloom size and init the bloom effect texture
    R_Bloom_InitEffectTexture ();

    //if screensize is more than 2x the bloom effect texture, set up for stepped downsampling
    r_bloomdownsamplingtexture = 0;
    r_screendownsamplingtexture_size = 0;
    if( vid_width.value > (BLOOM_SIZE * 2) && !r_bloom_fast_sample.value )
    {
        r_screendownsamplingtexture_size = (int)(BLOOM_SIZE * 2);
        data = Q_malloc( r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4 );
        memset( data, 0, r_screendownsamplingtexture_size * r_screendownsamplingtexture_size * 4 );
		r_bloomdownsamplingtexture = GL_LoadTexture ( "***r_bloomdownsamplingtexture***", r_screendownsamplingtexture_size, r_screendownsamplingtexture_size, data, TEX_LUMA, 4);
        Q_free ( data );
    }

    //Init the screen backup texture
    if( r_screendownsamplingtexture_size )
        R_Bloom_InitBackUpTexture( r_screendownsamplingtexture_size, r_screendownsamplingtexture_size );
    else
        R_Bloom_InitBackUpTexture( BLOOM_SIZE, BLOOM_SIZE );
}

/*
=================
R_InitBloomTextures
=================
*/
void R_InitBloomTextures( void )
{
    BLOOM_SIZE = 0;
    if( !r_bloom.value )
        return;

	if (!qmb_initialized)
		return;	

    R_Bloom_InitTextures ();
}


/*
=================
R_Bloom_DrawEffect
=================
*/
void R_Bloom_DrawEffect( void )
{
    //GL_Bind(0, r_bloomeffecttexture);
	glBindTexture(GL_TEXTURE_2D, r_bloomeffecttexture);
    glEnable(GL_BLEND);
//    glBlendFunc(GL_ONE, GL_ONE);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);

    glColor4f(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 0.8f);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBegin(GL_QUADS);                         

    glTexCoord2f(  0,                          sampleText_tch  );  
    glVertex2f(    curView_x,                  curView_y   );      
	
    glTexCoord2f(  0,                          0   );              
    glVertex2f(    curView_x,                  curView_y + curView_height  );  

    glTexCoord2f(  sampleText_tcw,             0   );              
    glVertex2f(    curView_x + curView_width,  curView_y + curView_height  );  

    glTexCoord2f(  sampleText_tcw,             sampleText_tch  );  
    glVertex2f(    curView_x + curView_width,  curView_y   );              

	glEnd();
    
    glDisable(GL_BLEND);
}

/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
void R_Bloom_GeneratexDiamonds( void )
{
    int				i, j;
    static float	intensity;
	extern cvar_t	gl_overbright;
	extern void		R_CalcRGB (float color, float intensity, float *rgb);
	float			rgb[3];//lxndr

    //set up sample size workspace
    glViewport( 0, 0, sample_width, sample_height );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity ();
    glOrtho(0, sample_width, sample_height, 0, -10, 100);
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity ();

    //copy small scene into r_bloomeffecttexture
    //GL_Bind(0, r_bloomeffecttexture);
	glBindTexture(GL_TEXTURE_2D, r_bloomeffecttexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

    //start modifying the small scene corner
    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    glEnable(GL_BLEND);

    //darkening passes
    if( r_bloom_darken.value )
    {
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        
        for(i=0; i<r_bloom_darken.value ;i++) 
		{
            R_Bloom_SamplePass( 0, 0 );
        }
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);
    }

    //bluring passes
    //glBlendFunc(GL_ONE, GL_ONE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);

	R_CalcRGB (r_bloom_color.value, 1, (float *) rgb);
    glColor4f (0.5f, 0.5f, 0.5f, 1.0); // lxndr: added

    if( r_bloom_diamond_size.value > 7 || r_bloom_diamond_size.value <= 3)
    {
        if( r_bloom_diamond_size.value != 8 ) Cvar_SetValue( "r_bloom_diamond_size", 8 );

        for(i=0; i<r_bloom_diamond_size.value; i++) 
		{
            for(j=0; j<r_bloom_diamond_size.value; j++) 
			{
				if ((chase_active.value)||(gl_overbright.value)||(cl.viewent.model->name == NULL))
					intensity = r_bloom_intensity.value * 0.1 * Diamond8x[i][j];
				else
					intensity = r_bloom_intensity.value * 0.3 * Diamond8x[i][j];
                if( intensity < 0.01f ) continue;
                glColor4f( intensity, intensity, intensity, 1.0);
                R_Bloom_SamplePass( i-4, j-4 );
            }
        }
    } 
	else 
	if( r_bloom_diamond_size.value > 5 ) 
	{
        
        if( r_bloom_diamond_size.value != 6 ) Cvar_SetValue( "r_bloom_diamond_size", 6 );

        for(i=0; i<r_bloom_diamond_size.value; i++) 
		{
            for(j=0; j<r_bloom_diamond_size.value; j++) 
			{
				if ((chase_active.value)||(gl_overbright.value)||(cl.viewent.model->name == NULL))
					intensity = r_bloom_intensity.value * 0.1 * Diamond6x[i][j];
				else
					intensity = r_bloom_intensity.value * 0.5 * Diamond6x[i][j];
                if( intensity < 0.01f ) continue;
                glColor4f( intensity, intensity, intensity, 1.0);
                R_Bloom_SamplePass( i-3, j-3 );
            }
        }
    } 
	else 
	if( r_bloom_diamond_size.value > 3 ) 
	{
		if( r_bloom_diamond_size.value != 4 ) Cvar_SetValue( "r_bloom_diamond_size", 4 );

        for(i=0; i<r_bloom_diamond_size.value; i++) {
            for(j=0; j<r_bloom_diamond_size.value; j++) {
				
				if ((chase_active.value)||(gl_overbright.value)||(cl.viewent.model->name == NULL))
					intensity = r_bloom_intensity.value * 0.1f * Diamond4x[i][j];
				else
					intensity = r_bloom_intensity.value * 0.8f * Diamond4x[i][j];
                if( intensity < 0.01f ) continue;
                glColor4f( intensity, intensity, intensity, 1.0);
                R_Bloom_SamplePass( i-2, j-2 );
            }
        }
    }
    
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height);

    //restore full screen workspace
    glViewport( 0, 0, vid_width.value, vid_height.value );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity ();
    glOrtho(0, vid_width.value, vid_height.value, 0, -10, 100);
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity ();
}                                           

/*
=================
R_Bloom_DownsampleView
=================
*/
void R_Bloom_DownsampleView( void )
{
    glDisable( GL_BLEND );
    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

    //stepped downsample
    if( r_screendownsamplingtexture_size )
    {
        int     midsample_width = r_screendownsamplingtexture_size * sampleText_tcw;
        int     midsample_height = r_screendownsamplingtexture_size * sampleText_tch;
        
        //copy the screen and draw resized
		glBindTexture(GL_TEXTURE_2D, r_bloomscreentexture);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, vid_height.value - (curView_y + curView_height), curView_width, curView_height);
        R_Bloom_Quad( 0,  vid_height.value-midsample_height, midsample_width, midsample_height, screenText_tcw, screenText_tch  );
        
        //now copy into Downsampling (mid-sized) texture
		glBindTexture(GL_TEXTURE_2D, r_bloomdownsamplingtexture);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height);

        //now draw again in bloom size
        glColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
        R_Bloom_Quad( 0,  vid_height.value-sample_height, sample_width, sample_height, sampleText_tcw, sampleText_tch );
        
        //now blend the big screen texture into the bloom generation space (hoping it adds some blur)
        glEnable( GL_BLEND );
        glBlendFunc(GL_ONE, GL_ONE);
        glColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		glBindTexture(GL_TEXTURE_2D, r_bloomscreentexture);
        R_Bloom_Quad( 0,  vid_height.value-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
        glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
        glDisable( GL_BLEND );

    } 
	else 
	{    //downsample simple
		glBindTexture(GL_TEXTURE_2D, r_bloomscreentexture);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, curView_x, vid_height.value - (curView_y + curView_height), curView_width, curView_height);
        R_Bloom_Quad( 0, vid_height.value-sample_height, sample_width, sample_height, screenText_tcw, screenText_tch );
    }
}

/*
=================
R_BloomBlend
=================
*/
//void R_BloomBlend ( refdef_t *fd, meshlist_t *meshlist )
void R_BloomBlend (int bloom)
{
    if( !bloom || !r_bloom.value )
        return;

    if( !BLOOM_SIZE )
        R_Bloom_InitTextures();

    if( screen_texture_width < BLOOM_SIZE ||
        screen_texture_height < BLOOM_SIZE )
        return;

    //set up full screen workspace
    glViewport( 0, 0, vid_width.value, vid_height.value );
    glDisable( GL_DEPTH_TEST );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity ();
    glOrtho(0, vid_width.value, vid_height.value, 0, -10, 100);
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity ();
    glDisable(GL_CULL_FACE);

    glDisable( GL_BLEND );
    glEnable( GL_TEXTURE_2D );

    glColor4f( 1, 1, 1, 1 );

    //set up current sizes
//  curView_x = fd->x;
//  curView_y = fd->y;
	curView_x = 0;
	curView_y = 0;
    curView_width = vid_width.value;
    curView_height = vid_height.value;
    screenText_tcw = ((float)vid_width.value / (float)screen_texture_width);
    screenText_tch = ((float)vid_height.value / (float)screen_texture_height);

    if( vid_height.value > vid_width.value ) 
	{
        //sampleText_tcw = ((float)vid_width.value / (float)vid_height.value);
	    screenText_tcw = ((float)vid_width.value / (float)screen_texture_width);
		screenText_tch = ((float)vid_height.value / (float)screen_texture_height);
        sampleText_tch = 1.0f;
    } 
	else 
	{
        sampleText_tcw = 1.0f;
        sampleText_tch = ((float)vid_height.value / (float)vid_width.value);
    }

    sample_width = BLOOM_SIZE * sampleText_tcw;
    sample_height = BLOOM_SIZE * sampleText_tch;
    
    //copy the screen space we'll use to work into the backup texture
	glBindTexture(GL_TEXTURE_2D, r_bloombackuptexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_width, r_screenbackuptexture_height );  
	
    //create the bloom image
	R_Bloom_DownsampleView();
    R_Bloom_GeneratexDiamonds();

    //restore the screen-backup to the screen
    glDisable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, r_bloombackuptexture);
    glColor4f( 1, 1, 1, 1 );

    R_Bloom_Quad( 0, vid_height.value - r_screenbackuptexture_height, r_screenbackuptexture_width, r_screenbackuptexture_height, 1.0, 1.0);
	R_Bloom_DrawEffect();
	
	glColor4f (1,1,1,1);
    glDisable (GL_BLEND);   
    glEnable (GL_TEXTURE_2D);   
	glEnable(GL_DEPTH_TEST);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);   
	glViewport (glx, gly, glwidth, glheight);
}
