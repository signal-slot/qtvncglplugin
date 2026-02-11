// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QVNCGLWINDOW_H
#define QVNCGLWINDOW_H

#include <qpa/qplatformwindow.h>

QT_BEGIN_NAMESPACE

class QVncGlScreen;

class QVncGlWindow : public QPlatformWindow
{
public:
    QVncGlWindow(QWindow *window, QVncGlScreen *screen);

    void setGeometry(const QRect &rect) override;
    WId winId() const override { return m_winId; }

private:
    QVncGlScreen *m_screen = nullptr;
    WId m_winId = 0;
};

QT_END_NAMESPACE

#endif // QVNCGLWINDOW_H
