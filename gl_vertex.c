/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske

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
#ifdef USESHADERS

#include "quakedef.h"

#define MAX_VERTEX_ARRAYS 16
#define MAX_BUFFERS 1024
extern gl_brokendrivers_t gl_brokendrivers;
GLuint gl_vertexbufferobjects[MAX_BUFFERS];

int gl_currentvertexbuffer = 0;

void GL_CreateBuffers (void)
{
	// get a batch of vertex buffers
	qglDeleteBuffers (MAX_BUFFERS, gl_vertexbufferobjects);
	glFinish ();
	qglGenBuffers (MAX_BUFFERS, gl_vertexbufferobjects);
}

void GL_BeginBuffers (void)
{
	gl_currentvertexbuffer = 0;
}

GLuint GL_GetBuffer (void)
{
	GLuint vbo = 0;

	if (gl_currentvertexbuffer >= MAX_BUFFERS)
	{
		Sys_Error ("GL_GetBuffer : gl_currentvertexbuffer >= MAX_BUFFERS");
	}

	vbo = gl_vertexbufferobjects[gl_currentvertexbuffer];
	gl_currentvertexbuffer++;

	return vbo;
}


GLuint gl_current_array_buffer = 0;
GLuint gl_current_element_array_buffer = 0;
unsigned int gl_currentvertexarrays = 0;

typedef struct gl_vertexarray_s
{
	GLboolean dirty;
	GLboolean enabled;
	GLuint buffer;
	GLint size;
	GLenum type;
	GLboolean normalized;
	GLsizei stride;
	GLvoid *pointer;
} gl_vertexarray_t;

gl_vertexarray_t gl_vertexarrays[MAX_VERTEX_ARRAYS];

qboolean gl_varraydirty = false;

void GL_EnableVertexArrays (unsigned int arraymask)
{
	if (arraymask != gl_currentvertexarrays)
	{
		gl_varraydirty = true;
		gl_currentvertexarrays = arraymask;
	}
}


#if 0
// gcc chokes on this
#define GL_CheckAndSetDirtyVAState(prop) \
	if (gl_vertexarrays[varray].##prop != ##prop) \
	{ \
		gl_vertexarrays[varray].##prop = ##prop; \
		gl_vertexarrays[varray].dirty = GL_TRUE; \
	}
#endif

void GL_VertexArrayPointer (GLuint buffer, int varray, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLvoid *pointer)
{
	if (gl_vertexarrays[varray].buffer != buffer)
	{
		gl_vertexarrays[varray].buffer = buffer;
		gl_vertexarrays[varray].dirty = GL_TRUE;
	}

	if (gl_vertexarrays[varray].size != size)
	{
		gl_vertexarrays[varray].size = size;
		gl_vertexarrays[varray].dirty = GL_TRUE;
	}

	if (gl_vertexarrays[varray].type != type)
	{
		gl_vertexarrays[varray].type = type;
		gl_vertexarrays[varray].dirty = GL_TRUE;
	}

	if (gl_vertexarrays[varray].normalized != normalized)
	{
		gl_vertexarrays[varray].normalized = normalized;
		gl_vertexarrays[varray].dirty = GL_TRUE;
	}

	if (gl_vertexarrays[varray].stride != stride)
	{
		gl_vertexarrays[varray].stride = stride;
		gl_vertexarrays[varray].dirty = GL_TRUE;
	}

	if (gl_vertexarrays[varray].pointer != pointer)
	{
		gl_vertexarrays[varray].pointer = pointer;
		gl_vertexarrays[varray].dirty = GL_TRUE;
	}

#if 0
// gcc chokes on this
	GL_CheckAndSetDirtyVAState (buffer);
	GL_CheckAndSetDirtyVAState (size);
	GL_CheckAndSetDirtyVAState (type);
	GL_CheckAndSetDirtyVAState (normalized);
	GL_CheckAndSetDirtyVAState (stride);
	GL_CheckAndSetDirtyVAState (pointer);
#endif

	if (gl_vertexarrays[varray].dirty)
		gl_varraydirty = true;
}


void GL_UncacheArrays (void)
{
	int i;

	// shut down all vertex arrays that were enabled
	for (i = 0; i < MAX_VERTEX_ARRAYS; i++)
	{
		if (gl_vertexarrays[i].enabled)
			qglDisableVertexAttribArrayARB (i);

		gl_vertexarrays[i].buffer = 0;
		gl_vertexarrays[i].enabled = GL_FALSE;
		gl_vertexarrays[i].dirty = GL_TRUE;
	}

	// clear all cached states
	qglBindBuffer (GL_ARRAY_BUFFER, 0);
	qglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

	gl_current_array_buffer = 0;
	gl_current_element_array_buffer = 0;

	gl_currentvertexarrays = 0;
	gl_varraydirty = true;
}


void GL_CheckVertexArrays (void)
{
	if (gl_varraydirty)
	{
		int i;

		for (i = 0; i < MAX_VERTEX_ARRAYS; i++)
		{
			if (gl_currentvertexarrays & (1 << i))
			{
				if (gl_vertexarrays[i].buffer != gl_current_array_buffer)
				{
					qglBindBuffer (GL_ARRAY_BUFFER, gl_vertexarrays[i].buffer);
					gl_current_array_buffer = gl_vertexarrays[i].buffer;
					gl_vertexarrays[i].dirty = GL_TRUE;
				}

				if (!gl_vertexarrays[i].enabled)
				{
					qglEnableVertexAttribArrayARB (i);
					gl_vertexarrays[i].enabled = GL_TRUE;
					gl_vertexarrays[i].dirty = GL_TRUE;
				}

				if (gl_vertexarrays[i].dirty)
				{
					qglVertexAttribPointerARB (i, gl_vertexarrays[i].size,
						gl_vertexarrays[i].type,
						gl_vertexarrays[i].normalized,
						gl_vertexarrays[i].stride,
						gl_vertexarrays[i].pointer);

					gl_vertexarrays[i].dirty = GL_FALSE;
				}
			}
			else if (gl_vertexarrays[i].enabled)
			{
				qglDisableVertexAttribArrayARB (i);
				gl_vertexarrays[i].enabled = GL_FALSE;
				gl_vertexarrays[i].dirty = GL_TRUE;
			}
		}

		gl_varraydirty = false;
	}
}

void GL_DrawElements (GLuint buffer, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLvoid *indices)
{
	GL_CheckVertexArrays ();

	if (buffer != gl_current_element_array_buffer)
	{
		qglBindBuffer (GL_ELEMENT_ARRAY_BUFFER, buffer);
		gl_current_element_array_buffer = buffer;
	}
	if (gl_brokendrivers.IntelBroken)
	{
		glPushAttrib (0);
		qglDrawRangeElements (mode, start, end, count, type, indices);
		glPopAttrib ();
	}
	else qglDrawRangeElements (mode, start, end, count, type, indices);

//	rs_drawelements++;// this is just for debug output.. add later?
}


void GL_DrawArrays (GLenum mode, GLint first, GLsizei count)
{
	GL_CheckVertexArrays ();

	if (gl_brokendrivers.IntelBroken)
	{
		glPushAttrib (0);
		glDrawArrays (mode, first, count);
		glPopAttrib ();
	}
	else glDrawArrays (mode, first, count);

//	rs_drawelements++;
}
#endif



