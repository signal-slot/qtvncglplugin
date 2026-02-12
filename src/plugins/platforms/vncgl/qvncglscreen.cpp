// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qvncglscreen.h"
#include "qvncgl_p.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QDebug>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QSurfaceFormat>
#include <QtGui/private/qeglconvenience_p.h>
#include <QtGui/private/qeglplatformcontext_p.h>
#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

class QVncGlEglContext : public QEGLPlatformContext
{
public:
    QVncGlEglContext(QVncGlScreen *screen, const QSurfaceFormat &format,
                   QPlatformOpenGLContext *share, EGLDisplay display, EGLConfig config)
        : QEGLPlatformContext(format, share, display, &config)
        , m_screen(screen)
    {
    }

    EGLSurface eglSurfaceForPlatformSurface(QPlatformSurface *surface) override
    {
        Q_UNUSED(surface);
        return m_screen->surface();
    }

    void swapBuffers(QPlatformSurface *surface) override
    {
        Q_UNUSED(surface);
        m_screen->updateFromOpenGL();
        QEGLPlatformContext::swapBuffers(surface);
    }

private:
    QVncGlScreen *m_screen = nullptr;
};

QVncGlScreen::QVncGlScreen(const QStringList &args)
    : mArgs(args)
{
}

QVncGlScreen::~QVncGlScreen()
{
    delete dirty;
#if QT_CONFIG(cursor)
    delete clientCursor;
    clientCursor = nullptr;
#endif
    if (m_surface != EGL_NO_SURFACE)
        eglDestroySurface(m_display, m_surface);
    if (m_display != EGL_NO_DISPLAY)
        eglTerminate(m_display);
}

bool QVncGlScreen::initialize()
{
    m_geometry = QRect(0, 0, 1024, 768);
    m_physicalSize = QSizeF(m_geometry.width() / 96.0 * 25.4,
                            m_geometry.height() / 96.0 * 25.4);

    QRegularExpression sizeRx("size=(\\d+)x(\\d+)"_L1);
    QRegularExpression mmSizeRx("mmsize=(?<width>(\\d*\\.)?\\d+)x(?<height>(\\d*\\.)?\\d+)"_L1);

    for (const QString &arg : mArgs) {
        QRegularExpressionMatch match;
        if (arg.contains(sizeRx, &match)) {
            m_geometry.setSize(QSize(match.captured(1).toInt(), match.captured(2).toInt()));
        } else if (arg.contains(mmSizeRx, &match)) {
            m_physicalSize = QSizeF(match.captured("width").toDouble(),
                                    match.captured("height").toDouble());
        }
    }

    if (!initializeEgl())
        return false;

    resizeFramebuffer();
    delete dirty;
    dirty = new QVncGlDirtyMapOptimized<quint32>(this);
    dirty->reset();
    return true;
}

void QVncGlScreen::resize(const QSize &size)
{
    if (size.isEmpty() || size == m_geometry.size())
        return;

    m_geometry = QRect(QPoint(0, 0), size);
    m_physicalSize = QSizeF(size.width() / 96.0 * 25.4,
                            size.height() / 96.0 * 25.4);

    // Recreate EGL pbuffer surface
    if (m_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(m_display, m_surface);
        m_surface = EGL_NO_SURFACE;
    }

    if (m_display != EGL_NO_DISPLAY && m_config) {
        const EGLint pbufferAttribs[] = {
            EGL_WIDTH, size.width(),
            EGL_HEIGHT, size.height(),
            EGL_NONE
        };
        m_surface = eglCreatePbufferSurface(m_display, m_config, pbufferAttribs);
        if (m_surface == EGL_NO_SURFACE)
            qWarning("vncgl: Failed to recreate EGL pbuffer surface for resize");
    }

    resizeFramebuffer();
    delete dirty;
    dirty = new QVncGlDirtyMapOptimized<quint32>(this);
    dirty->reset();

    if (QScreen *s = screen())
        QWindowSystemInterface::handleScreenGeometryChange(s, m_geometry, m_geometry);
}

bool QVncGlScreen::initializeEgl()
{
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        qWarning("vncgl: Failed to bind OpenGL ES API");
        return false;
    }

    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) {
        qWarning("vncgl: Failed to obtain EGL display");
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(m_display, &major, &minor)) {
        qWarning("vncgl: Failed to initialize EGL display");
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (!eglChooseConfig(m_display, configAttribs, &m_config, 1, &numConfigs) || numConfigs == 0) {
        qWarning("vncgl: Failed to choose EGL config");
        return false;
    }

    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, m_geometry.width(),
        EGL_HEIGHT, m_geometry.height(),
        EGL_NONE
    };

    m_surface = eglCreatePbufferSurface(m_display, m_config, pbufferAttribs);
    if (m_surface == EGL_NO_SURFACE) {
        qWarning("vncgl: Failed to create EGL pbuffer surface");
        return false;
    }

    return true;
}

void QVncGlScreen::resizeFramebuffer()
{
    const QSize size = m_geometry.size();
    if (size.isEmpty())
        return;
    m_framebuffer = QImage(size, QImage::Format_ARGB32);
    m_framebuffer.fill(Qt::black);
    m_readbackBuffer = QImage(size, QImage::Format_RGBA8888);
}

QPlatformOpenGLContext *QVncGlScreen::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    QSurfaceFormat format = context ? context->format() : QSurfaceFormat();
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(2, 0);
    return new QVncGlEglContext(const_cast<QVncGlScreen *>(this), format,
                              context ? context->shareHandle() : nullptr,
                              m_display, m_config);
}

void QVncGlScreen::updateFromOpenGL()
{
    if (m_surface == EGL_NO_SURFACE)
        return;

    const QSize size = m_geometry.size();
    if (size.isEmpty())
        return;

    if (m_readbackBuffer.size() != size)
        m_readbackBuffer = QImage(size, QImage::Format_RGBA8888);

    if (!m_glFunctions) {
        m_glFunctions = std::make_unique<QOpenGLFunctions>();
        m_glFunctions->initializeOpenGLFunctions();
    }

    m_glFunctions->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    m_glFunctions->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA,
                                GL_UNSIGNED_BYTE, m_readbackBuffer.bits());

    QImage converted = m_readbackBuffer.mirrored().convertToFormat(QImage::Format_ARGB32);
    m_framebuffer = converted;
    dirtyRegion = QRect(QPoint(0, 0), size);
    if (vncServer)
        vncServer->setDirty();
}

void QVncGlScreen::enableClientCursor(QVncGlClient *client)
{
#if QT_CONFIG(cursor)
    if (!clientCursor)
        clientCursor = new QVncGlClientCursor();
    clientCursor->addClient(client);
#else
    Q_UNUSED(client);
#endif
}

void QVncGlScreen::disableClientCursor(QVncGlClient *client)
{
#if QT_CONFIG(cursor)
    if (!clientCursor)
        return;
    const uint count = clientCursor->removeClient(client);
    if (count == 0) {
        delete clientCursor;
        clientCursor = nullptr;
    }
#else
    Q_UNUSED(client);
#endif
}

QT_END_NAMESPACE
