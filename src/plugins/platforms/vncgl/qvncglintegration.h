// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QVNCGLINTEGRATION_H
#define QVNCGLINTEGRATION_H

#include <qpa/qplatformintegration.h>
#include <memory>
#include <qscopedpointer.h>

QT_BEGIN_NAMESPACE

class QAbstractEventDispatcher;
class QPlatformNativeInterface;
class QVncGlClipboard;
class QVncGlScreen;
class QVncGlServer;

class QVncGlIntegration : public QPlatformIntegration
{
public:
    QVncGlIntegration(const QStringList &paramList);
    ~QVncGlIntegration();

    void initialize() override;
    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;

    QAbstractEventDispatcher *createEventDispatcher() const override;

    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;
    QPlatformInputContext *inputContext() const override { return m_inputContext.data(); }

    QPlatformNativeInterface *nativeInterface() const override;
    QPlatformClipboard *clipboard() const override;

private:
    QStringList m_paramList;
    int m_port = 5900;
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformInputContext> m_inputContext;
    mutable QScopedPointer<QPlatformServices> m_services;
    mutable QScopedPointer<QPlatformNativeInterface> m_nativeInterface;
    std::unique_ptr<QVncGlScreen> m_primaryScreen;
    std::unique_ptr<QVncGlServer> m_server;
    mutable std::unique_ptr<QVncGlClipboard> m_clipboard;
};

QT_END_NAMESPACE

#endif // QVNCGLINTEGRATION_H
