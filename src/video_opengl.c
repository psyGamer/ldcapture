#include "hook.h"

#include <stdio.h>
#include <GL/glx.h>
#include <uthash.h>

static void prepareSwap(Display *dpy, GLXDrawable drawable, GLXDrawable *oldDrawable, GLXContext *oldContext);



static void (*orig_glXSwapBuffers)(Display*, GLXDrawable);

void initVideoOpenGL()
{
    orig_glXSwapBuffers = load_original_function("glXSwapBuffers");
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
    
}

static void prepareSwap(Display *dpy, GLXDrawable drawable, GLXDrawable *oldDrawable, GLXContext *oldContext)
{
    *oldDrawable = glXGetCurrentDrawable();
    *oldContext = glXGetCurrentContext();
    
    if (*oldDrawable != drawable)
    {

    }
}