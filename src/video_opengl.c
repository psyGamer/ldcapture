#include "hook.h"
#include "encoder.h"

#include <stdio.h>
#include <string.h>
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

static void captureGLFrame(Display *dpy);
static void prepareSwap(Display *dpy, GLXDrawable drawable, GLXDrawable *oldDrawable, GLXContext *oldContext);
static void finishSwap(Display *dpy, GLXDrawable drawable, const GLXDrawable *oldDrawable, const GLXContext *oldContext);

static bool initialized = false;

static cmap_drawable contextFromDrawable;
static bool swapIntervalChecked = false;
static bool fboChecked = false;

static encoder_t *encoder;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void (*orig_glXSwapBuffers)(Display*, GLXDrawable);
static Bool (*orig_glXMakeCurrent)(Display*, GLXDrawable, GLXContext);
static __GLXextFuncPtr (*orig_glXGetProcAddressARB)(const GLubyte *);

// static void (*glBindFramebufferEXT)(GLenum target, GLuint framebuffer);
// static int (*glXSwapIntervalMESA)(unsigned int);
// static int (*glXGetSwapIntervalMESA)(void);

static void init_video_opengl()
{
    contextFromDrawable = cmap_drawable_init();
    encoder = encoder_create(ENCODER_TYPE_PNG);

    orig_glXSwapBuffers = load_orig_function("glXSwapBuffers");
    orig_glXMakeCurrent = load_orig_function("glXMakeCurrent");
    orig_glXGetProcAddressARB = load_orig_function("glXGetProcAddressARB");

    initialized = true;
}

static void hook_glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
    if (!initialized) init_video_opengl();

    GLXDrawable oldDrawable;
    GLXContext oldContext;

    prepareSwap(dpy, drawable, &oldDrawable, &oldContext);
    captureGLFrame(dpy);
    finishSwap(dpy, drawable, &oldDrawable, &oldContext);

    orig_glXSwapBuffers(dpy, drawable);
}

static Bool hook_glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext context)
{
    if (!initialized) init_video_opengl();

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

__GLXextFuncPtr hook_glXGetProcAddressARB(const GLubyte *procName)
{
    if (!initialized) init_video_opengl();

    printf("glXGetProcAddressARB: %s\n", procName);

    if (strcmp(procName, "glXSwapBuffers") == 0)
    {
        printf("HOOKED glXSwapBuffers\n");
        return hook_glXSwapBuffers;
    }
    if (strcmp(procName, "glXMakeCurrent") == 0)
    {
        printf("HOOKED glXMakeCurrent\n");
        return hook_glXMakeCurrent;
    }

    return orig_glXGetProcAddressARB(procName);
}

static void captureGLFrame(Display *dpy)
{
    GLXDrawable drawable = glXGetCurrentDrawable();
    Window window = (Window)drawable;

    if (!window) return;

    XWindowAttributes attr = {};
    XGetWindowAttributes(dpy, window, &attr);

    encoder_resize(encoder, attr.width, attr.height);

    pthread_mutex_lock(&mutex);

    if (glXGetSwapIntervalMESA() > 0)
        glXSwapIntervalMESA(0);

    GLuint oldFramebuffer;
    GLenum oldReadBuffer;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, (GLint*) &oldFramebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glGetIntegerv(GL_READ_BUFFER,(GLint*) &oldReadBuffer);
    glReadBuffer(GL_FRONT);

    // unsigned char *data = malloc(encoder->width * encoder->height * encoder->channels);

    glReadPixels(0, 0, attr.width, attr.height, GL_RGB, GL_UNSIGNED_BYTE, encoder->data);
    // memset(encoder->data, attr.width * attr.height * 3, 69);

    // GLsizei stride = 3 * width;
    // stride += (stride % 4) ? (4 - stride % 4) : 0;

    // stbi_flip_vertically_on_write(true);
    // stbi_write_png("output.png", width, height, 3, captureData, stride);
    encoder_save_frame(encoder);
    // printf("Frame done!\n");

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