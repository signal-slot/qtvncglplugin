// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qvncglwindow.h"
#include "qvncglscreen.h"

#include <qpa/qwindowsysteminterface.h>
#include <QtGui/QSurface>

QT_BEGIN_NAMESPACE

QVncGlWindow::QVncGlWindow(QWindow *window, QVncGlScreen *screen)
    : QPlatformWindow(window)
    , m_screen(screen)
{
    static int serialNo = 0;
    m_winId = ++serialNo;
    if (window)
        window->setSurfaceType(QSurface::OpenGLSurface);
    setGeometry(screen ? screen->geometry() : window->geometry());
}

void QVncGlWindow::setGeometry(const QRect &rect)
{
    if (!m_screen)
        return;
    m_screen->resize(rect.size());
    const QRect screenRect = m_screen->geometry();
    QWindowSystemInterface::handleGeometryChange(window(), screenRect);
    QPlatformWindow::setGeometry(screenRect);
}

QT_END_NAMESPACE
