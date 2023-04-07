#include <pthread.h>
#include <time.h>

#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

#include "base.h"
#include "hook.h"
#include "encoder.h"
#include "timing.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void capture_frame(Display* dpy)
{
    GLXDrawable drawable = glXGetCurrentDrawable();
    Window window = (Window)drawable;

    if (!window) return;

    XWindowAttributes attr = {0};
    XGetWindowAttributes(dpy, window, &attr);

    pthread_mutex_lock(&mutex);

    if (glXGetSwapIntervalMESA() > 0)
        glXSwapIntervalMESA(0);

    GLuint oldFramebuffer;
    GLenum oldReadBuffer;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, (GLint*) &oldFramebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    glGetIntegerv(GL_READ_BUFFER,(GLint*) &oldReadBuffer);
    glReadBuffer(GL_FRONT);

    u8* srcData =  malloc(attr.width * attr.height * 4);
    glReadPixels(0, 0, attr.width, attr.height, GL_RGBA, GL_UNSIGNED_BYTE, srcData);

    encoder_t* encoder = encoder_get_current();
    encoder_prepare_video(encoder, attr.width, attr.height);

    // Flip the image and add padding
    const size_t srcRowStride = attr.width * 4;
    const size_t dstRowStride = encoder->video_row_stride;

    u8* src = srcData;
    u8* dst = encoder->video_data + (attr.height - 1) * dstRowStride;

    memset(encoder->video_data, 0, dstRowStride);
    for (i32 i = 0; i < attr.height; i++)
    {
        memcpy(dst, src, srcRowStride);
        src += srcRowStride;
        dst -= dstRowStride;
    }

    free(srcData);

    encoder_flush_video(encoder);
    
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, oldFramebuffer);
    glReadBuffer(oldReadBuffer);

    pthread_mutex_unlock(&mutex);
}

SYM_HOOK(void, glXSwapBuffers, (Display* dpy, GLXDrawable drawable),
{
    if (timing_is_running())
    {
        timing_mark_video_ready();
        while (timing_is_running() && !timing_is_first_frame() && !timing_is_sound_done());

        timing_next_frame();
        capture_frame(dpy);
    }

    orig_glXSwapBuffers(dpy, drawable);
})

SYM_HOOK(void, glXDestroyWindow, (Display* dpy, GLXWindow window),
{
    shutdown_ldcapture();
    orig_glXDestroyWindow(dpy, window);
})

SYM_HOOK(__GLXextFuncPtr, glXGetProcAddressARB, (const GLubyte* procName),
{
    if (strcmp((const char*)procName, "glXSwapBuffers") == 0)
    {
        orig_glXSwapBuffers = (glXSwapBuffers_fn_t)orig_glXGetProcAddressARB((const GLubyte*)"glXSwapBuffers");
        return (__GLXextFuncPtr)glXSwapBuffers;
    }
    if (strcmp((const char*)procName, "glXDestroyWindow") == 0)
    {
        orig_glXDestroyWindow = (glXDestroyWindow_fn_t)orig_glXGetProcAddressARB((const GLubyte*)"glXDestroyWindow");
        return (__GLXextFuncPtr)glXDestroyWindow;
    }

    return orig_glXGetProcAddressARB(procName);
})

void init_video_opengl_x11()
{
    LOAD_SYM_HOOK(glXSwapBuffers);
    LOAD_SYM_HOOK(glXDestroyWindow);
    LOAD_SYM_HOOK(glXGetProcAddressARB);
}