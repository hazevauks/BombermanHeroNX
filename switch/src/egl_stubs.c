/*
 * EGL stubs.
 *
 * SDL's video driver for the Switch is built on EGL, so libSDL2.a wants these symbols. The game never opens an
 * SDL window: RT64 presents through Vulkan on the libnx window, and SDL is kept for input, events and audio.
 * Answering EGL here keeps devkitPro's OpenGL Mesa (an older copy of the code the Vulkan driver is built from)
 * out of the NRO. Every call fails, so an SDL window can never be created by mistake.
 */
#include <stddef.h>
#include <stdint.h>

typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef int32_t EGLint;
typedef intptr_t EGLAttrib;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLNativeDisplayType;
typedef void *EGLNativeWindowType;
typedef void (*EGLFuncPtr)(void);

#define EGL_FALSE 0
#define EGL_NOT_INITIALIZED 0x3001

EGLBoolean eglBindAPI(EGLenum api) { (void)api; return EGL_FALSE; }
EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint *a, EGLConfig *c, EGLint n, EGLint *count) { (void)d; (void)a; (void)c; (void)n; if (count) *count = 0; return EGL_FALSE; }
EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext s, const EGLint *a) { (void)d; (void)c; (void)s; (void)a; return NULL; }
EGLSurface eglCreatePbufferSurface(EGLDisplay d, EGLConfig c, const EGLint *a) { (void)d; (void)c; (void)a; return NULL; }
EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a) { (void)d; (void)c; (void)w; (void)a; return NULL; }
EGLBoolean eglDestroyContext(EGLDisplay d, EGLContext c) { (void)d; (void)c; return EGL_FALSE; }
EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s) { (void)d; (void)s; return EGL_FALSE; }
EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint a, EGLint *v) { (void)d; (void)c; (void)a; (void)v; return EGL_FALSE; }
EGLDisplay eglGetDisplay(EGLNativeDisplayType n) { (void)n; return NULL; }
EGLint eglGetError(void) { return EGL_NOT_INITIALIZED; }
EGLDisplay eglGetPlatformDisplay(EGLenum p, void *n, const EGLAttrib *a) { (void)p; (void)n; (void)a; return NULL; }
EGLFuncPtr eglGetProcAddress(const char *name) { (void)name; return NULL; }
EGLBoolean eglInitialize(EGLDisplay d, EGLint *major, EGLint *minor) { (void)d; (void)major; (void)minor; return EGL_FALSE; }
EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext c) { (void)d; (void)draw; (void)read; (void)c; return EGL_FALSE; }
EGLenum eglQueryAPI(void) { return 0; }
const char *eglQueryString(EGLDisplay d, EGLint name) { (void)d; (void)name; return NULL; }
EGLBoolean eglSwapBuffers(EGLDisplay d, EGLSurface s) { (void)d; (void)s; return EGL_FALSE; }
EGLBoolean eglSwapInterval(EGLDisplay d, EGLint interval) { (void)d; (void)interval; return EGL_FALSE; }
EGLBoolean eglTerminate(EGLDisplay d) { (void)d; return EGL_FALSE; }
EGLBoolean eglWaitGL(void) { return EGL_FALSE; }
EGLBoolean eglWaitNative(EGLint engine) { (void)engine; return EGL_FALSE; }
