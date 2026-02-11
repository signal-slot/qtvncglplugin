// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QVNCGLBACKINGSTORE_H
#define QVNCGLBACKINGSTORE_H

#include <qpa/qplatformbackingstore.h>

QT_BEGIN_NAMESPACE

class QOpenGLContext;
class QOpenGLPaintDevice;

class QVncGlBackingStore : public QPlatformBackingStore
{
public:
    QVncGlBackingStore(QWindow *window);
    ~QVncGlBackingStore();

    QPaintDevice *paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void beginPaint(const QRegion &region) override;
    void endPaint() override;
    void resize(const QSize &size, const QRegion &staticContents) override;

private:
    QOpenGLContext *m_context = nullptr;
    QOpenGLPaintDevice *m_device = nullptr;
};

QT_END_NAMESPACE

#endif // QVNCGLBACKINGSTORE_H
