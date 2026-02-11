// Copyright (C) 2026 Signal Slot, Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <qpa/qplatformintegrationplugin.h>
#include "qvncglintegration.h"
#include "qvncgl_p.h"

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

class QVncGlIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformIntegrationFactoryInterface_iid FILE "vncgl.json")
public:
    QPlatformIntegration *create(const QString&, const QStringList&) override;
};

QPlatformIntegration* QVncGlIntegrationPlugin::create(const QString& system, const QStringList& paramList)
{
    if (!system.compare("vncgl"_L1, Qt::CaseInsensitive))
        return new QVncGlIntegration(paramList);

    return nullptr;
}

QT_END_NAMESPACE

#include "main.moc"
