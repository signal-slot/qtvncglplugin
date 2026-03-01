// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QVNCGLCLIENT_H
#define QVNCGLCLIENT_H

#include <QObject>

#include "qvncgl_p.h"

QT_BEGIN_NAMESPACE

class QTcpSocket;
class QVncGlServer;

class QVncGlClient : public QObject
{
    Q_OBJECT
public:
    enum ClientMsg {
        SetPixelFormat = 0,
        FixColourMapEntries = 1,
        SetEncodings = 2,
        FramebufferUpdateRequest = 3,
        KeyEvent = 4,
        PointerEvent = 5,
        ClientCutText = 6
    };

    explicit QVncGlClient(QTcpSocket *clientSocket, QVncGlServer *server);
    ~QVncGlClient();
    QTcpSocket *clientSocket() const;
    QVncGlServer *server() const { return m_server; }

    void setDirty(const QRegion &region);
    void setDirtyCursor() { m_dirtyCursor = true; scheduleUpdate(); }
    QRegion dirtyRegion() const { return m_dirtyRegion; }
    inline bool isConnected() const { return m_state == Connected; }
    inline bool supportsExtClipboard() const { return m_supportsExtClipboard; }
    inline quint32 clientClipFormats() const { return m_clientClipFormats; }

    inline int clientBytesPerPixel() const {
        return m_pixelFormat.bitsPerPixel / 8;
    }

    void convertPixels(char *dst, const char *src, int count, int depth) const;
    inline bool doPixelConversion() const { return m_needConversion; }

signals:

private slots:
    void readClient();
    void discardClient();
    void checkUpdate();
    void scheduleUpdate();

protected:
    bool event(QEvent *event) override;

private:
    enum ClientState {
        Disconnected,
        Protocol,
        Authentication,
        Init,
        Connected
    };
    enum ProtocolVersion {
        V3_3,
        V3_7,
        V3_8
    };

    void setPixelFormat();
    void setEncodings();
    void frameBufferUpdateRequest();
    void pointerEvent();
    void keyEvent();
    void clientCutText();
    bool pixelConversionNeeded() const;

    void handleExtClipboardMessage(const QByteArray &payload);
    void handleExtClipCaps(quint32 flags, const QByteArray &payload);
    void handleExtClipRequest(quint32 flags);
    void handleExtClipPeek();
    void handleExtClipNotify(quint32 flags);
    void handleExtClipProvide(quint32 flags, const QByteArray &payload);

    QVncGlServer *m_server;
    QTcpSocket *m_clientSocket;
    QRfbEncoder *m_encoder;

    // Client State
    ClientState m_state;
    quint8 m_msgType;
    bool m_handleMsg;
    QRfbPixelFormat m_pixelFormat;
    bool m_sameEndian;
    bool m_needConversion;
    int m_encodingsPending;
    int m_cutTextPending;
    uint m_supportCopyRect : 1;
    uint m_supportRRE : 1;
    uint m_supportCoRRE : 1;
    uint m_supportHextile : 1;
    uint m_supportZRLE : 1;
    uint m_supportCursor : 1;
    uint m_supportDesktopSize : 1;
    bool m_wantUpdate;
    Qt::KeyboardModifiers m_keymod;
    bool m_dirtyCursor;
    bool m_updatePending;
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    bool m_swapBytes;
#endif
    QRegion m_dirtyRegion;
    ProtocolVersion m_protocolVersion;

    // Extended Clipboard
    bool m_supportsExtClipboard = false;
    quint32 m_clientClipFormats = 0;
    quint32 m_clientClipActions = 0;
    qint32 m_extClipPayloadPending = 0;
};

QT_END_NAMESPACE

#endif // QVNCGLCLIENT_H
