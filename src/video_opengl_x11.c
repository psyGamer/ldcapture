#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

#include "hook.h"
#include "encoder.h"
#include "timing.h"

static encoder_t *encoder;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void capture_frame(Display *dpy)
{
    GLXDrawable drawable = glXGetCurrentDrawable();
    Window window = (Window)drawable;

    if (!window) return;

    XWindowAttributes attr = {0};
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

    glReadPixels(0, 0, attr.width, attr.height, GL_RGBA, GL_UNSIGNED_BYTE, encoder->data);
    
    // Flip the image (https://codereview.stackexchange.com/q/29618)
    const size_t rowStride = attr.width * 4;
    unsigned char *tempRow = malloc(rowStride);
    unsigned char *low  = encoder->data;
    unsigned char *high = encoder->data + (attr.height - 1) * rowStride;

    for (; low < high; low += rowStride, high -= rowStride) {
        memcpy(tempRow, low, rowStride);
        memcpy(low, high, rowStride);
        memcpy(high, tempRow, rowStride);
    }

    free(tempRow);

    encoder_save_frame(encoder);
    
    timing_video_done();
    while (!timing_is_sound_done()); // Wait for sound

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, oldFramebuffer);
    glReadBuffer(oldReadBuffer);

    pthread_mutex_unlock(&mutex);
}

SYM_HOOK(void, glXSwapBuffers, (Display *dpy, GLXDrawable drawable),
{
    if (timing_is_running())
    {
        timing_next_frame();
        capture_frame(dpy);
    }

    orig_glXSwapBuffers(dpy, drawable);
})

SYM_HOOK(void, glXDestroyWindow, (Display *dpy, GLXWindow window),
{
    shutdown_ldcapture();
    orig_glXDestroyWindow(dpy, window);
})

SYM_HOOK(__GLXextFuncPtr, glXGetProcAddressARB, (const GLubyte *procName),
{
    if (strcmp((const char *)procName, "glXSwapBuffers") == 0)
    {
        orig_glXSwapBuffers = (glXSwapBuffers_fn_t)orig_glXGetProcAddressARB((const GLubyte *)"glXSwapBuffers");
        return (__GLXextFuncPtr)glXSwapBuffers;
    }
    if (strcmp((const char *)procName, "glXDestroyWindow") == 0)
    {
        orig_glXDestroyWindow = (glXDestroyWindow_fn_t)orig_glXGetProcAddressARB((const GLubyte *)"glXDestroyWindow");
        return (__GLXextFuncPtr)glXDestroyWindow;
    }

    return orig_glXGetProcAddressARB(procName);
})

void init_video_opengl_x11()
{
    encoder = encoder_create(ENCODER_TYPE_QOI);

    LOAD_SYM_HOOK(glXSwapBuffers);
    LOAD_SYM_HOOK(glXDestroyWindow);
    LOAD_SYM_HOOK(glXGetProcAddressARB);
}