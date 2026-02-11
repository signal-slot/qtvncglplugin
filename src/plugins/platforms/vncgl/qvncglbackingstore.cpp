// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qvncglbackingstore.h"

#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLPaintDevice>

QT_BEGIN_NAMESPACE

QVncGlBackingStore::QVncGlBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
    , m_context(new QOpenGLContext)
{
    m_context->setFormat(window->requestedFormat());
    m_context->setScreen(window->screen());
    m_context->create();
}

QVncGlBackingStore::~QVncGlBackingStore()
{
    delete m_device;
    delete m_context;
}

QPaintDevice *QVncGlBackingStore::paintDevice()
{
    return m_device;
}

void QVncGlBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(region);
    Q_UNUSED(offset);
    if (!m_context)
        return;
    m_context->swapBuffers(window);
}

void QVncGlBackingStore::beginPaint(const QRegion &)
{
    if (!m_context)
        return;
    m_context->makeCurrent(window());
    delete m_device;
    m_device = new QOpenGLPaintDevice(window()->size());
}

void QVncGlBackingStore::endPaint()
{
    delete m_device;
    m_device = nullptr;
}

void QVncGlBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(size);
    Q_UNUSED(staticContents);
}

QT_END_NAMESPACE
