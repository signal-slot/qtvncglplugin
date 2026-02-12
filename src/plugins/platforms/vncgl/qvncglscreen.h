// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QVncGlScreen_H
#define QVncGlScreen_H

#include <qpa/qplatformscreen.h>
#include <qpa/qplatformcursor.h>
#include <QtCore/QScopedPointer>
#include <memory>
#include <QtGui/QImage>
#include <QtGui/QRegion>
#include <QtGui/private/qt_egl_p.h>

QT_BEGIN_NAMESPACE

class QPlatformOpenGLContext;
class QOpenGLContext;
class QOpenGLFunctions;
class QVncGlServer;
class QVncGlClient;
class QVncGlClientCursor;
class QVncGlDirtyMap;

class QVncGlScreen : public QPlatformScreen
{
public:
    QVncGlScreen(const QStringList &args);
    ~QVncGlScreen();

    bool initialize();

    QRect geometry() const override { return m_geometry; }
    int depth() const override { return m_depth; }
    QImage::Format format() const override { return m_format; }
    QSizeF physicalSize() const override { return m_physicalSize; }
    QDpi logicalDpi() const override { return m_logicalDpi; }

    QImage *image() { return &m_framebuffer; }
    void clearDirty() { dirtyRegion = QRegion(); }

    void resize(const QSize &size);
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const;
    void updateFromOpenGL();
    EGLSurface surface() const { return m_surface; }
    bool swapBytes() const { return false; }
    void enableClientCursor(QVncGlClient *client);
    void disableClientCursor(QVncGlClient *client);
    QPlatformCursor *cursor() const override { return nullptr; }

    QStringList mArgs;
    QRegion dirtyRegion;
    QVncGlServer *vncServer = nullptr;
    QVncGlDirtyMap *dirty = nullptr;
#if QT_CONFIG(cursor)
    QVncGlClientCursor *clientCursor = nullptr;
#endif

private:
    void resizeFramebuffer();
    bool initializeEgl();

    QRect m_geometry;
    QSizeF m_physicalSize;
    QDpi m_logicalDpi = QDpi(96, 96);
    int m_depth = 32;
    QImage::Format m_format = QImage::Format_ARGB32_Premultiplied;
    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLConfig m_config = nullptr;
    mutable std::unique_ptr<QOpenGLFunctions> m_glFunctions;
    QImage m_framebuffer;
    QImage m_readbackBuffer;
};

QT_END_NAMESPACE

#endif // QVncGlScreen_H
