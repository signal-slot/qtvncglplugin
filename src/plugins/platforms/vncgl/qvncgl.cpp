// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "qvncgl_p.h"
#include "qvncglscreen.h"
#include "qvncglclient.h"
#include "qvncglclipboard.h"
#include "QtNetwork/qtcpserver.h"
#include "QtNetwork/qtcpsocket.h"
#include <qendian.h>
#include <qthread.h>

#include <QtGui/qguiapplication.h>
#include <QtGui/QWindow>
#include <QtGui/qclipboard.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QBuffer>
#include <QMimeData>

#include <zlib.h>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcVncGl, "qt.qpa.vncgl");

QVncGlDirtyMap::QVncGlDirtyMap(QVncGlScreen *screen)
    : screen(screen), bytesPerPixel(0), numDirty(0)
{
    bytesPerPixel = (screen->depth() + 7) / 8;
    bufferWidth = screen->geometry().width();
    bufferHeight = screen->geometry().height();
    bufferStride = bufferWidth * bytesPerPixel;
    buffer = new uchar[bufferHeight * bufferStride];

    mapWidth = (bufferWidth + MAP_TILE_SIZE - 1) / MAP_TILE_SIZE;
    mapHeight = (bufferHeight + MAP_TILE_SIZE - 1) / MAP_TILE_SIZE;
    numTiles = mapWidth * mapHeight;
    map = new uchar[numTiles];
}

QVncGlDirtyMap::~QVncGlDirtyMap()
{
    delete[] map;
    delete[] buffer;
}

void QVncGlDirtyMap::reset()
{
    memset(map, 1, numTiles);
    memset(buffer, 0, bufferHeight * bufferStride);
    numDirty = numTiles;
}

inline bool QVncGlDirtyMap::dirty(int x, int y) const
{
    return map[y * mapWidth + x];
}

inline void QVncGlDirtyMap::setClean(int x, int y)
{
    map[y * mapWidth + x] = 0;
    --numDirty;
}

template <class T>
void QVncGlDirtyMapOptimized<T>::setDirty(int tileX, int tileY, bool force)
{
    static bool alwaysForce = qEnvironmentVariableIsSet("QT_VNC_NO_COMPAREBUFFER");
    if (alwaysForce)
        force = true;

    bool changed = false;

    if (!force) {
        const int lstep = bufferStride;
        const int startX = tileX * MAP_TILE_SIZE;
        const int startY = tileY * MAP_TILE_SIZE;
        const uchar *scrn = screen->image()->constBits()
                            + startY * lstep + startX * bytesPerPixel;
        uchar *old = buffer + startY * bufferStride + startX * sizeof(T);

        const int tileHeight = (startY + MAP_TILE_SIZE > bufferHeight ?
                                bufferHeight - startY : MAP_TILE_SIZE);
        const int tileWidth = (startX + MAP_TILE_SIZE > bufferWidth ?
                               bufferWidth - startX : MAP_TILE_SIZE);
        const bool doInlines = (tileWidth == MAP_TILE_SIZE);

        int y = tileHeight;

        if (doInlines) { // hw: memcmp/memcpy is inlined when using constants
            while (y) {
                if (memcmp(old, scrn, sizeof(T) * MAP_TILE_SIZE)) {
                    changed = true;
                    break;
                }
                scrn += lstep;
                old += bufferStride;
                --y;
            }

            while (y) {
                memcpy(old, scrn, sizeof(T) * MAP_TILE_SIZE);
                scrn += lstep;
                old += bufferStride;
                --y;
            }
        } else {
            while (y) {
                if (memcmp(old, scrn, sizeof(T) * tileWidth)) {
                    changed = true;
                    break;
                }
                scrn += lstep;
                old += bufferStride;
                --y;
            }

            while (y) {
                memcpy(old, scrn, sizeof(T) * tileWidth);
                scrn += lstep;
                old += bufferStride;
                --y;
            }
        }
    }

    const int mapIndex = tileY * mapWidth + tileX;
    if ((force || changed) && !map[mapIndex]) {
        map[mapIndex] = 1;
        ++numDirty;
    }
}

template class QVncGlDirtyMapOptimized<unsigned char>;
template class QVncGlDirtyMapOptimized<unsigned short>;
template class QVncGlDirtyMapOptimized<unsigned int>;

static const struct {
    int keysym;
    int keycode;
} keyMap[] = {
    { 0xff08, Qt::Key_Backspace },
    { 0xff09, Qt::Key_Tab       },
    { 0xff0d, Qt::Key_Return    },
    { 0xff1b, Qt::Key_Escape    },
    { 0xff63, Qt::Key_Insert    },
    { 0xffff, Qt::Key_Delete    },
    { 0xff50, Qt::Key_Home      },
    { 0xff57, Qt::Key_End       },
    { 0xff55, Qt::Key_PageUp    },
    { 0xff56, Qt::Key_PageDown  },
    { 0xff51, Qt::Key_Left      },
    { 0xff52, Qt::Key_Up        },
    { 0xff53, Qt::Key_Right     },
    { 0xff54, Qt::Key_Down      },
    { 0xffbe, Qt::Key_F1        },
    { 0xffbf, Qt::Key_F2        },
    { 0xffc0, Qt::Key_F3        },
    { 0xffc1, Qt::Key_F4        },
    { 0xffc2, Qt::Key_F5        },
    { 0xffc3, Qt::Key_F6        },
    { 0xffc4, Qt::Key_F7        },
    { 0xffc5, Qt::Key_F8        },
    { 0xffc6, Qt::Key_F9        },
    { 0xffc7, Qt::Key_F10       },
    { 0xffc8, Qt::Key_F11       },
    { 0xffc9, Qt::Key_F12       },
    { 0xffe1, Qt::Key_Shift     },
    { 0xffe2, Qt::Key_Shift     },
    { 0xffe3, Qt::Key_Control   },
    { 0xffe4, Qt::Key_Control   },
    { 0xffe7, Qt::Key_Meta      },
    { 0xffe8, Qt::Key_Meta      },
    { 0xffe9, Qt::Key_Alt       },
    { 0xffea, Qt::Key_Alt       },

    { 0xffb0, Qt::Key_0         },
    { 0xffb1, Qt::Key_1         },
    { 0xffb2, Qt::Key_2         },
    { 0xffb3, Qt::Key_3         },
    { 0xffb4, Qt::Key_4         },
    { 0xffb5, Qt::Key_5         },
    { 0xffb6, Qt::Key_6         },
    { 0xffb7, Qt::Key_7         },
    { 0xffb8, Qt::Key_8         },
    { 0xffb9, Qt::Key_9         },

    { 0xff8d, Qt::Key_Return    },
    { 0xffaa, Qt::Key_Asterisk  },
    { 0xffab, Qt::Key_Plus      },
    { 0xffad, Qt::Key_Minus     },
    { 0xffae, Qt::Key_Period    },
    { 0xffaf, Qt::Key_Slash     },

    { 0xff95, Qt::Key_Home      },
    { 0xff96, Qt::Key_Left      },
    { 0xff97, Qt::Key_Up        },
    { 0xff98, Qt::Key_Right     },
    { 0xff99, Qt::Key_Down      },
    { 0xff9a, Qt::Key_PageUp    },
    { 0xff9b, Qt::Key_PageDown  },
    { 0xff9c, Qt::Key_End       },
    { 0xff9e, Qt::Key_Insert    },
    { 0xff9f, Qt::Key_Delete    },

    { 0, 0 }
};

void QRfbRect::read(QTcpSocket *s)
{
    quint16 buf[4];
    s->read(reinterpret_cast<char*>(buf), 8);
    x = ntohs(buf[0]);
    y = ntohs(buf[1]);
    w = ntohs(buf[2]);
    h = ntohs(buf[3]);
}

void QRfbRect::write(QTcpSocket *s) const
{
    quint16 buf[4];
    buf[0] = htons(x);
    buf[1] = htons(y);
    buf[2] = htons(w);
    buf[3] = htons(h);
    s->write(reinterpret_cast<char*>(buf) , 8);
}

void QRfbPixelFormat::read(QTcpSocket *s)
{
    char buf[16];
    s->read(buf, 16);
    bitsPerPixel = buf[0];
    depth = buf[1];
    bigEndian = buf[2];
    trueColor = buf[3];

    quint16 a = ntohs(*reinterpret_cast<quint16 *>(buf + 4));
    redBits = 0;
    while (a) { a >>= 1; redBits++; }

    a = ntohs(*reinterpret_cast<quint16 *>(buf + 6));
    greenBits = 0;
    while (a) { a >>= 1; greenBits++; }

    a = ntohs(*reinterpret_cast<quint16 *>(buf + 8));
    blueBits = 0;
    while (a) { a >>= 1; blueBits++; }

    redShift = buf[10];
    greenShift = buf[11];
    blueShift = buf[12];
}

void QRfbPixelFormat::write(QTcpSocket *s)
{
    char buf[16];
    buf[0] = bitsPerPixel;
    buf[1] = depth;
    buf[2] = bigEndian;
    buf[3] = trueColor;

    quint16 a = 0;
    for (int i = 0; i < redBits; i++) a = (a << 1) | 1;
    *reinterpret_cast<quint16 *>(buf + 4) = htons(a);

    a = 0;
    for (int i = 0; i < greenBits; i++) a = (a << 1) | 1;
    *reinterpret_cast<quint16 *>(buf + 6) = htons(a);

    a = 0;
    for (int i = 0; i < blueBits; i++) a = (a << 1) | 1;
    *reinterpret_cast<quint16 *>(buf + 8) = htons(a);

    buf[10] = redShift;
    buf[11] = greenShift;
    buf[12] = blueShift;
    s->write(buf, 16);
}


void QRfbServerInit::setName(const char *n)
{
    delete[] name;
    name = new char [strlen(n) + 1];
    strcpy(name, n);
}

void QRfbServerInit::read(QTcpSocket *s)
{
    s->read(reinterpret_cast<char *>(&width), 2);
    width = ntohs(width);
    s->read(reinterpret_cast<char *>(&height), 2);
    height = ntohs(height);
    format.read(s);

    quint32 len;
    s->read(reinterpret_cast<char *>(&len), 4);
    len = ntohl(len);

    name = new char [len + 1];
    s->read(name, len);
    name[len] = '\0';
}

void QRfbServerInit::write(QTcpSocket *s)
{
    quint16 t = htons(width);
    s->write(reinterpret_cast<char *>(&t), 2);
    t = htons(height);
    s->write(reinterpret_cast<char *>(&t), 2);
    format.write(s);
    quint32 len = static_cast<quint32>(strlen(name));
    len = htonl(len);
    s->write(reinterpret_cast<char *>(&len), 4);
    s->write(name, static_cast<qint64>(strlen(name)));
}

bool QRfbSetEncodings::read(QTcpSocket *s)
{
    if (s->bytesAvailable() < 3)
        return false;

    char tmp;
    s->read(&tmp, 1);        // padding
    s->read(reinterpret_cast<char *>(&count), 2);
    count = ntohs(count);

    return true;
}

bool QRfbFrameBufferUpdateRequest::read(QTcpSocket *s)
{
    if (s->bytesAvailable() < 9)
        return false;

    s->read(&incremental, 1);
    rect.read(s);

    return true;
}

bool QRfbKeyEvent::read(QTcpSocket *s)
{
    if (s->bytesAvailable() < 7)
        return false;

    s->read(&down, 1);
    quint16 tmp;
    s->read(reinterpret_cast<char *>(&tmp), 2);  // padding

    quint32 key;
    s->read(reinterpret_cast<char *>(&key), 4);
    key = ntohl(key);

    unicode = 0;
    keycode = 0;
    int i = 0;
    while (keyMap[i].keysym && !keycode) {
        if (keyMap[i].keysym == static_cast<int>(key))
            keycode = keyMap[i].keycode;
        i++;
    }

    if (keycode >= ' ' && keycode <= '~')
        unicode = keycode;

    if (!keycode) {
        if (key <= 0xff) {
            unicode = key;
            if (key >= 'a' && key <= 'z')
                keycode = Qt::Key_A + key - 'a';
            else if (key >= ' ' && key <= '~')
                keycode = Qt::Key_Space + key - ' ';
        }
    }

    return true;
}

bool QRfbPointerEvent::read(QTcpSocket *s)
{
    if (s->bytesAvailable() < 5)
        return false;

    char buttonMask;
    s->read(&buttonMask, 1);
    buttons = Qt::NoButton;
    if (buttonMask & 1)
        buttons |= Qt::LeftButton;
    if (buttonMask & 2)
        buttons |= Qt::MiddleButton;
    if (buttonMask & 4)
        buttons |= Qt::RightButton;

    quint16 tmp;
    s->read(reinterpret_cast<char *>(&tmp), 2);
    x = ntohs(tmp);
    s->read(reinterpret_cast<char *>(&tmp), 2);
    y = ntohs(tmp);

    return true;
}

bool QRfbClientCutText::read(QTcpSocket *s)
{
    if (s->bytesAvailable() < 7)
        return false;

    char tmp[3];
    s->read(tmp, 3);        // padding
    quint32 rawLength;
    s->read(reinterpret_cast<char *>(&rawLength), 4);
    length = static_cast<qint32>(ntohl(rawLength));

    return true;
}

void QRfbRawEncoder::write()
{
//    QVncGlDirtyMap *map = server->dirtyMap();
    QTcpSocket *socket = client->clientSocket();

    const int bytesPerPixel = client->clientBytesPerPixel();

    // create a region from the dirty rects and send the region's merged rects.
    // ### use the tile map again
    QRegion rgn = client->dirtyRegion();
    qCDebug(lcVncGl) << "QRfbRawEncoder::write()" << rgn;

    const QImage screenImage = client->server()->screenImage();
    rgn &= screenImage.rect();

    const auto rectsInRegion = rgn.rectCount();

    {
        const char tmp[2] = { 0, 0 }; // msg type, padding
        socket->write(tmp, sizeof(tmp));
    }

    {
        const quint16 count = htons(rectsInRegion);
        socket->write(reinterpret_cast<const char *>(&count), sizeof(count));
    }

    if (rectsInRegion <= 0)
        return;

    for (const QRect &tileRect: rgn) {
        const QRfbRect rect(tileRect.x(), tileRect.y(),
                            tileRect.width(), tileRect.height());
        rect.write(socket);

        const quint32 encoding = htonl(0); // raw encoding
        socket->write(reinterpret_cast<const char *>(&encoding), sizeof(encoding));

        qsizetype linestep = screenImage.bytesPerLine();
        const uchar *screendata = screenImage.scanLine(rect.y)
                                  + rect.x * screenImage.depth() / 8;

        if (client->doPixelConversion()) {
            const int bufferSize = rect.w * rect.h * bytesPerPixel;
            if (bufferSize > buffer.size())
                buffer.resize(bufferSize);

            // convert pixels
            char *b = buffer.data();
            const int bstep = rect.w * bytesPerPixel;
            const int depth = screenImage.depth();
            for (int i = 0; i < rect.h; ++i) {
                client->convertPixels(b, reinterpret_cast<const char*>(screendata), rect.w, depth);
                screendata += linestep;
                b += bstep;
            }
            socket->write(buffer.constData(), bufferSize);
        } else {
            for (int i = 0; i < rect.h; ++i) {
                socket->write(reinterpret_cast<const char*>(screendata), rect.w * bytesPerPixel);
                screendata += linestep;
            }
        }
        if (socket->state() == QAbstractSocket::UnconnectedState)
            break;
    }
    socket->flush();
}

#if QT_CONFIG(cursor)
QVncGlClientCursor::QVncGlClientCursor()
{
    QWindow *w = QGuiApplication::focusWindow();
    QCursor c = w ? w->cursor() : QCursor(Qt::ArrowCursor);
    changeCursor(&c, nullptr);
}

QVncGlClientCursor::~QVncGlClientCursor()
{
}

void QVncGlClientCursor::write(QVncGlClient *client) const
{
    QTcpSocket *socket = client->clientSocket();

    // FramebufferUpdate header
    {
        const quint16 tmp[6] = { htons(0),
                                 htons(1),
                                 htons(static_cast<uint16_t>(hotspot.x())), htons(static_cast<uint16_t>(hotspot.y())),
                                 htons(static_cast<uint16_t>(cursor.width())),
                                 htons(static_cast<uint16_t>(cursor.height())) };
        socket->write(reinterpret_cast<const char*>(tmp), sizeof(tmp));

        const qint32 encoding = qToBigEndian(-239);
        socket->write(reinterpret_cast<const char*>(&encoding), sizeof(encoding));
    }

    if (cursor.isNull())
        return;

    // write pixels
    Q_ASSERT(cursor.hasAlphaChannel());
    const QImage img = cursor.convertToFormat(client->server()->screen()->format());
    const int n = client->clientBytesPerPixel() * img.width();
    const int depth = img.depth();
    char *buffer = new char[n];
    for (int i = 0; i < img.height(); ++i) {
        client->convertPixels(buffer, (const char*)img.scanLine(i), img.width(), depth);
        socket->write(buffer, n);
    }
    delete[] buffer;

    // write mask
    const QImage bitmap = cursor.createAlphaMask().convertToFormat(QImage::Format_Mono);
    Q_ASSERT(bitmap.depth() == 1);
    Q_ASSERT(bitmap.size() == img.size());
    const int width = (bitmap.width() + 7) / 8;
    for (int i = 0; i < bitmap.height(); ++i)
        socket->write(reinterpret_cast<const char*>(bitmap.scanLine(i)), width);
}

void QVncGlClientCursor::changeCursor(QCursor *widgetCursor, QWindow *window)
{
    Q_UNUSED(window);
    const Qt::CursorShape shape = widgetCursor ? widgetCursor->shape() : Qt::ArrowCursor;

    if (shape == Qt::BitmapCursor) {
        // application supplied cursor
        hotspot = widgetCursor->hotSpot();
        cursor = widgetCursor->pixmap().toImage();
    } else {
        // system cursor
        QPlatformCursorImage platformImage(nullptr, nullptr, 0, 0, 0, 0);
        platformImage.set(shape);
        cursor = *platformImage.image();
        hotspot = platformImage.hotspot();
    }
    for (auto client : std::as_const(clients))
        client->setDirtyCursor();
}

void QVncGlClientCursor::addClient(QVncGlClient *client)
{
    if (!clients.contains(client)) {
        clients.append(client);
        // Force a cursor update when the client connects.
        client->setDirtyCursor();
    }
}

uint QVncGlClientCursor::removeClient(QVncGlClient *client)
{
    clients.removeOne(client);
    return clients.size();
}
#endif // QT_CONFIG(cursor)

QVncGlServer::QVncGlServer(QVncGlScreen *screen, quint16 port)
    : qvnc_screen(screen)
    , m_port(port)
{
    QMetaObject::invokeMethod(this, "init", Qt::QueuedConnection);
}

void QVncGlServer::init()
{
    serverSocket = new QTcpServer(this);
    if (!serverSocket->listen(QHostAddress::Any, m_port))
        qWarning() << "QVncGlServer could not connect:" << serverSocket->errorString();
    else
        qWarning("QVncGlServer created on port %d", m_port);

    connect(serverSocket, SIGNAL(newConnection()), this, SLOT(newConnection()));

}

QVncGlServer::~QVncGlServer()
{
    qDeleteAll(clients);
}

void QVncGlServer::setDirty()
{
    for (auto client : std::as_const(clients))
        client->setDirty(qvnc_screen->dirtyRegion);

    qvnc_screen->clearDirty();
}


void QVncGlServer::newConnection()
{
    auto clientSocket = serverSocket->nextPendingConnection();
    clients.append(new QVncGlClient(clientSocket, this));

    dirtyMap()->reset();

    qCDebug(lcVncGl) << "new Connection from: " << clientSocket->localAddress();

    qvnc_screen->setPowerState(QPlatformScreen::PowerStateOn);
}

void QVncGlServer::discardClient(QVncGlClient *client)
{
    clients.removeOne(client);
    qvnc_screen->disableClientCursor(client);
    client->deleteLater();
    if (clients.isEmpty())
        qvnc_screen->setPowerState(QPlatformScreen::PowerStateOff);
}

void QVncGlServer::sendCutText(const QString &text)
{
    const QByteArray latin1 = text.toLatin1();
    const quint32 length = latin1.size();

    QByteArray message(8 + length, Qt::Uninitialized);
    char *data = message.data();
    data[0] = ServerCutText;
    data[1] = data[2] = data[3] = 0; // padding
    qToBigEndian(length, data + 4);
    memcpy(data + 8, latin1.constData(), length);

    for (QVncGlClient *client : std::as_const(clients)) {
        if (client->isConnected())
            client->clientSocket()->write(message);
    }
}

static void writeExtClipMessage(QTcpSocket *socket, const QByteArray &payload)
{
    char header[8];
    header[0] = QVncGlServer::ServerCutText;
    header[1] = header[2] = header[3] = 0; // padding
    // Negative length signals extended clipboard
    const qint32 negLen = -static_cast<qint32>(payload.size());
    qToBigEndian(negLen, header + 4);
    socket->write(header, 8);
    socket->write(payload);
}

void QVncGlServer::sendExtClipCaps(QVncGlClient *client)
{
    // Caps message: flags(4) + [size(4) per format bit]
    const quint32 formats = ExtClipFormat::Text | ExtClipFormat::HTML | ExtClipFormat::DIB;
    const quint32 actions = ExtClipAction::Caps | ExtClipAction::Request
                          | ExtClipAction::Peek | ExtClipAction::Notify
                          | ExtClipAction::Provide;
    const quint32 flags = actions | formats;

    // Count format bits to know how many size fields to send
    int numFormats = 0;
    for (int bit = 0; bit < 16; ++bit) {
        if (formats & (1u << bit))
            ++numFormats;
    }

    QByteArray payload(4 + numFormats * 4, Qt::Uninitialized);
    char *p = payload.data();
    qToBigEndian(flags, p);
    p += 4;
    // Max size 0 for each format = force notify/request/provide flow
    for (int bit = 0; bit < 16; ++bit) {
        if (formats & (1u << bit)) {
            qToBigEndian(quint32(0), p);
            p += 4;
        }
    }

    writeExtClipMessage(client->clientSocket(), payload);
}

void QVncGlServer::sendExtClipNotify(QVncGlClient *client, quint32 formats)
{
    QByteArray payload(4, Qt::Uninitialized);
    const quint32 flags = ExtClipAction::Notify | (formats & ExtClipFormat::Mask);
    qToBigEndian(flags, payload.data());
    writeExtClipMessage(client->clientSocket(), payload);
}

void QVncGlServer::sendExtClipRequest(QVncGlClient *client, quint32 formats)
{
    QByteArray payload(4, Qt::Uninitialized);
    const quint32 flags = ExtClipAction::Request | (formats & ExtClipFormat::Mask);
    qToBigEndian(flags, payload.data());
    writeExtClipMessage(client->clientSocket(), payload);
}

void QVncGlServer::sendExtClipProvide(QVncGlClient *client, quint32 formats, const QMimeData *mimeData)
{
    // Build uncompressed data: for each format bit (low to high), U32 size + data
    QByteArray uncompressed;
    for (int bit = 0; bit < 16; ++bit) {
        const quint32 fmt = 1u << bit;
        if (!(formats & fmt))
            continue;

        QByteArray fmtData;
        if (fmt == ExtClipFormat::Text && mimeData->hasText()) {
            fmtData = mimeData->text().toUtf8();
        } else if (fmt == ExtClipFormat::HTML && mimeData->hasHtml()) {
            fmtData = mimeData->html().toUtf8();
        } else if (fmt == ExtClipFormat::DIB && mimeData->hasImage()) {
            QImage img = qvariant_cast<QImage>(mimeData->imageData());
            if (img.isNull())
                continue;
            QBuffer buf(&fmtData);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "BMP");
            buf.close();
            // Strip 14-byte BITMAPFILEHEADER to get DIB
            if (fmtData.size() <= 14)
                continue;
            fmtData.remove(0, 14);
        } else {
            continue;
        }

        // Append U32 size + data
        char sizeBuf[4];
        qToBigEndian(static_cast<quint32>(fmtData.size()), sizeBuf);
        uncompressed.append(sizeBuf, 4);
        uncompressed.append(fmtData);
    }

    if (uncompressed.isEmpty())
        return;

    // zlib compress
    uLongf compressedSize = compressBound(uncompressed.size());
    QByteArray compressed(4 + static_cast<int>(compressedSize), Qt::Uninitialized);
    char *p = compressed.data();

    const quint32 flags = ExtClipAction::Provide | (formats & ExtClipFormat::Mask);
    qToBigEndian(flags, p);

    if (compress2(reinterpret_cast<Bytef *>(p + 4), &compressedSize,
                  reinterpret_cast<const Bytef *>(uncompressed.constData()),
                  uncompressed.size(), Z_DEFAULT_COMPRESSION) != Z_OK) {
        qWarning("QVncGlServer: zlib compress failed for extended clipboard provide");
        return;
    }

    compressed.resize(4 + static_cast<int>(compressedSize));
    writeExtClipMessage(client->clientSocket(), compressed);
}

void QVncGlServer::sendClipboardToClients(const QMimeData *mimeData)
{
    if (!mimeData)
        return;

    // Prepare legacy message once (Latin-1 text)
    QByteArray legacyMessage;
    if (mimeData->hasText()) {
        const QByteArray latin1 = mimeData->text().toLatin1();
        const quint32 length = latin1.size();
        legacyMessage.resize(8 + length);
        char *data = legacyMessage.data();
        data[0] = ServerCutText;
        data[1] = data[2] = data[3] = 0;
        qToBigEndian(length, data + 4);
        memcpy(data + 8, latin1.constData(), length);
    }

    for (QVncGlClient *client : std::as_const(clients)) {
        if (!client->isConnected())
            continue;

        if (client->supportsExtClipboard()) {
            // Determine available formats
            quint32 formats = 0;
            if (mimeData->hasText())
                formats |= ExtClipFormat::Text;
            if (mimeData->hasHtml())
                formats |= ExtClipFormat::HTML;
            if (mimeData->hasImage())
                formats |= ExtClipFormat::DIB;
            // Filter by what client supports
            formats &= client->clientClipFormats();
            if (formats)
                sendExtClipNotify(client, formats);
        } else if (!legacyMessage.isEmpty()) {
            client->clientSocket()->write(legacyMessage);
        }
    }
}

inline QImage QVncGlServer::screenImage() const
{
    return *qvnc_screen->image();
}

QT_END_NAMESPACE

#include "moc_qvncgl_p.cpp"
