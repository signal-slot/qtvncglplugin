// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qvncglintegration.h"
#include "qvncglclipboard.h"
#include "qvncglscreen.h"
#include "qvncglwindow.h"
#include "qvncglbackingstore.h"
#include "qvncgl_p.h"

#if defined(Q_OS_UNIX)
#include <QtGui/private/qgenericunixfontdatabase_p.h>
#include <QtGui/private/qgenericunixeventdispatcher_p.h>
#include <QtGui/private/qdesktopunixservices_p.h>
#elif defined(Q_OS_WIN)
#include <QtGui/private/qfreetypefontdatabase_p.h>
#include <QtCore/private/qeventdispatcher_win_p.h>
#endif
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qwindowsysteminterface.h>

#include <QtCore/QRegularExpression>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

QVncGlIntegration::QVncGlIntegration(const QStringList &paramList)
    : m_paramList(paramList)
#if defined(Q_OS_UNIX)
    , m_fontDb(new QGenericUnixFontDatabase)
#elif defined(Q_OS_WIN)
    , m_fontDb(new QFreeTypeFontDatabase)
#endif
{
    QRegularExpression portRx("port=(\\d+)"_L1);
    for (const QString &arg : paramList) {
        QRegularExpressionMatch match;
        if (arg.contains(portRx, &match)) {
            m_port = match.captured(1).toInt();
            m_portSpecified = true;
        }
    }

    bool ok;
    int envPort = qEnvironmentVariableIntValue("QT_VNC_PORT", &ok);
    if (ok) {
        m_port = envPort;
        m_portSpecified = true;
    }
}

QVncGlIntegration::~QVncGlIntegration()
{
    if (m_primaryScreen)
        QWindowSystemInterface::handleScreenRemoved(m_primaryScreen.get());
}

void QVncGlIntegration::initialize()
{
    m_primaryScreen.reset(new QVncGlScreen(m_paramList));
    if (m_primaryScreen->initialize()) {
        QWindowSystemInterface::handleScreenAdded(m_primaryScreen.get());
        m_server = std::make_unique<QVncGlServer>(m_primaryScreen.get(), m_port, !m_portSpecified);
        m_primaryScreen->vncServer = m_server.get();
    } else {
        qWarning("vncgl: Failed to initialize screen");
    }

    m_inputContext.reset(QPlatformInputContextFactory::create());
    m_nativeInterface.reset(new QPlatformNativeInterface);
}

bool QVncGlIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps:
    case OpenGL:
    case ThreadedOpenGL:
    case RhiBasedRendering:
        return true;
    default:
        return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *QVncGlIntegration::createPlatformWindow(QWindow *window) const
{
    return new QVncGlWindow(window, m_primaryScreen.get());
}

QPlatformBackingStore *QVncGlIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QVncGlBackingStore(window);
}

QPlatformOpenGLContext *QVncGlIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    return m_primaryScreen ? m_primaryScreen->createPlatformOpenGLContext(context) : nullptr;
}

QAbstractEventDispatcher *QVncGlIntegration::createEventDispatcher() const
{
#if defined(Q_OS_UNIX)
    return createUnixEventDispatcher();
#elif defined(Q_OS_WIN)
    return new QEventDispatcherWin32;
#else
    return nullptr;
#endif
}

QPlatformFontDatabase *QVncGlIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *QVncGlIntegration::services() const
{
    if (m_services.isNull()) {
#if defined(Q_OS_UNIX)
        m_services.reset(new QDesktopUnixServices);
#else
        m_services.reset(new QPlatformServices);
#endif
    }
    return m_services.data();
}

QPlatformNativeInterface *QVncGlIntegration::nativeInterface() const
{
    return m_nativeInterface.data();
}

QPlatformClipboard *QVncGlIntegration::clipboard() const
{
    if (!m_clipboard)
        m_clipboard = std::make_unique<QVncGlClipboard>(m_server.get());
    return m_clipboard.get();
}

QT_END_NAMESPACE
