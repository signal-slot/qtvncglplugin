// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QVNCGLCLIPBOARD_H
#define QVNCGLCLIPBOARD_H

#include <qpa/qplatformclipboard.h>

QT_BEGIN_NAMESPACE

class QVncGlServer;

class QVncGlClipboard : public QPlatformClipboard
{
public:
    QVncGlClipboard(QVncGlServer *server);
    ~QVncGlClipboard();

    QMimeData *mimeData(QClipboard::Mode mode) override;
    void setMimeData(QMimeData *data, QClipboard::Mode mode) override;
    void setFromVncClient(QMimeData *data, QClipboard::Mode mode);
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;

private:
    QVncGlServer *m_server;
    QMimeData *m_data = nullptr;
};

QT_END_NAMESPACE

#endif // QVNCGLCLIPBOARD_H
