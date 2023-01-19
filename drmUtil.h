#pragma once

#include <drm.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <libcamera/libcamera.h>

int setupDRM(char const *name, int x, int y, int width, int height);
void gbmSwapBuffers(EGLDisplay *display, EGLSurface *surface);
void makeBufferDRM(int fd, libcamera::StreamConfiguration const &cfg, libcamera::FrameBuffer *buffer, int camera_num);
void displayDRM(int width, int height);
//void gbmSwapBuffers(EGLDisplay *display, EGLSurface *surface);
void page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
void exit_drm(void);
void drm_gl_setup();
//static int matchConfigToVisual(EGLDisplay display, EGLint visualId, EGLConfig *configs, int count);
//static drmModeConnector *getConnector(drmModeRes *resources);
//static drmModeEncoder *findEncoder(drmModeConnector *connector);
