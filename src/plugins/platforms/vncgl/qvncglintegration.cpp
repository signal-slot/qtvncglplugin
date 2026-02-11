// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qvncglintegration.h"
#include "qvncglscreen.h"
#include "qvncglwindow.h"
#include "qvncglbackingstore.h"
#include "qvncgl_p.h"

#include <QtGui/private/qgenericunixfontdatabase_p.h>
#include <QtGui/private/qgenericunixeventdispatcher_p.h>
#include <QtGui/private/qdesktopunixservices_p.h>
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
    , m_fontDb(new QGenericUnixFontDatabase)
{
    QRegularExpression portRx("port=(\\d+)"_L1);
    for (const QString &arg : paramList) {
        QRegularExpressionMatch match;
        if (arg.contains(portRx, &match))
            m_port = match.captured(1).toInt();
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
        m_server = std::make_unique<QVncGlServer>(m_primaryScreen.get(), m_port);
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
    return createUnixEventDispatcher();
}

QPlatformFontDatabase *QVncGlIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *QVncGlIntegration::services() const
{
    if (m_services.isNull())
        m_services.reset(new QDesktopUnixServices);
    return m_services.data();
}

QPlatformNativeInterface *QVncGlIntegration::nativeInterface() const
{
    return m_nativeInterface.data();
}

QT_END_NAMESPACE
