// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qvncglclipboard.h"
#include "qvncgl_p.h"

#include <QMimeData>

QT_BEGIN_NAMESPACE

QVncGlClipboard::QVncGlClipboard(QVncGlServer *server)
    : m_server(server)
{
}

QVncGlClipboard::~QVncGlClipboard()
{
    delete m_data;
}

QMimeData *QVncGlClipboard::mimeData(QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return nullptr;
    if (!m_data)
        m_data = new QMimeData;
    return m_data;
}

void QVncGlClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    if (data == m_data)
        return;

    delete m_data;
    m_data = data;

    if (m_data)
        m_server->sendClipboardToClients(m_data);

    emitChanged(mode);
}

void QVncGlClipboard::setFromVncClient(QMimeData *data, QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    if (data == m_data)
        return;

    delete m_data;
    m_data = data;

    emitChanged(mode);
}

bool QVncGlClipboard::supportsMode(QClipboard::Mode mode) const
{
    return mode == QClipboard::Clipboard;
}

bool QVncGlClipboard::ownsMode(QClipboard::Mode mode) const
{
    Q_UNUSED(mode);
    return false;
}

QT_END_NAMESPACE
