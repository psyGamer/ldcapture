#include "hook.h"

#include <stdio.h>
#include <pthread.h>

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

#define i_key GLXDrawable
#define i_val GLXContext
#define i_tag drawable
#include <stc/cmap.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static void captureGLFrame(Display *dpy);
static void prepareSwap(Display *dpy, GLXDrawable drawable, GLXDrawable *oldDrawable, GLXContext *oldContext);
static void finishSwap(Display *dpy, GLXDrawable drawable, const GLXDrawable *oldDrawable, const GLXContext *oldContext);

static bool initialized = false;

static cmap_drawable contextFromDrawable;
static bool swapIntervalChecked = false;
static bool fboChecked = false;
static unsigned char *captureData = NULL; // TEMP

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void (*orig_glXSwapBuffers)(Display*, GLXDrawable);
static Bool (*orig_glXMakeCurrent)(Display*, GLXDrawable, GLXContext);

// static void (*glBindFramebufferEXT)(GLenum target, GLuint framebuffer);
// static int (*glXSwapIntervalMESA)(unsigned int);
// static int (*glXGetSwapIntervalMESA)(void);

void initVideoOpenGL()
{
    contextFromDrawable = cmap_drawable_init();

    orig_glXSwapBuffers = load_original_function("glXSwapBuffers");
    orig_glXMakeCurrent = load_original_function("glXMakeCurrent");

    initialized = true;
}

void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
    if (!initialized) initVideoOpenGL();

    GLXDrawable oldDrawable;
    GLXContext oldContext;

    prepareSwap(dpy, drawable, &oldDrawable, &oldContext);
    captureGLFrame(dpy);
    finishSwap(dpy, drawable, &oldDrawable, &oldContext);

    orig_glXSwapBuffers(dpy, drawable);
}

Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext context)
{
    if (!initialized) initVideoOpenGL();

    Bool result = orig_glXMakeCurrent(dpy, drawable, context);

    if (result)
    {
        cmap_drawable_insert(&contextFromDrawable, drawable, context);

        // if (!swapIntervalChecked && (!glXSwapIntervalMESA || !glXGetSwapIntervalMESA))
        // {
        //     // glXSwapIntervalMESA = glXGetProcAddress("glXSwapIntervalMESA");
        //     // glXGetSwapIntervalMESA = glXGetProcAddress("glXGetSwapIntervalMESA");
        //     if(glXSwapIntervalMESA)
        //         fprintf(stdout, "video/opengl: wglSwapIntervalEXT supported\n");

        //     swapIntervalChecked = true;
        // }

        // if (!fboChecked && !glBindFramebufferEXT)
        // {
        //     glBindFramebufferEXT = glXGetProcAddress("glBindFramebufferEXT");
        //     if(glBindFramebufferEXT)
        //         fprintf(stdout, "video/opengl: FBOs supported\n");

        //     fboChecked = true;
        // }
    }

    return result;
}

static void captureGLFrame(Display *dpy)
{
    GLXDrawable drawable = glXGetCurrentDrawable();
    Window window = (Window)drawable;

    if (window == NULL) return;

    XWindowAttributes attr = {};
    XGetWindowAttributes(dpy, window, &attr);
    // printf("Window size: %d, %d", attr.width, attr.height);

    free(captureData);
    captureData = malloc(attr.width * attr.height * 3);

    pthread_mutex_lock(&mutex);

    if (glXGetSwapIntervalMESA() > 0)
        glXSwapIntervalMESA(0);

    GLuint oldFramebuffer;
    GLenum oldReadBuffer;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, (GLint*) &oldFramebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glGetIntegerv(GL_READ_BUFFER,(GLint*) &oldReadBuffer);
    glReadBuffer(GL_FRONT);

    glReadPixels(0, 0, attr.width, attr.height, GL_RGB, GL_UNSIGNED_BYTE, captureData);

    // GLsizei stride = 3 * width;
    // stride += (stride % 4) ? (4 - stride % 4) : 0;

    // stbi_flip_vertically_on_write(true);
    // stbi_write_png("output.png", width, height, 3, captureData, stride);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, oldFramebuffer);
    glReadBuffer(oldReadBuffer);

    pthread_mutex_unlock(&mutex);
}

static void prepareSwap(Display *dpy, GLXDrawable drawable, GLXDrawable *oldDrawable, GLXContext *oldContext)
{
    *oldDrawable = glXGetCurrentDrawable();
    *oldContext = glXGetCurrentContext();
    
    if (*oldDrawable != drawable)
    {
        GLXContext context = *(GLXContext*)cmap_drawable_get(&contextFromDrawable, drawable);
        if (context != NULL)
        {
            glXMakeCurrent(dpy, drawable, context);
        }
        else
        {
            fprintf(stderr, "video/opengl: SwapBuffers called on drawable with no active rendering context. This is potentially bad.\n");
        }
    }
}

static void finishSwap(Display *dpy, GLXDrawable drawable, const GLXDrawable *oldDrawable, const GLXContext *oldContext)
{
    if (*oldDrawable != drawable)
    {
        glXMakeCurrent(dpy, *oldDrawable, *oldContext);
    }
}