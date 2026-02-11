# Copyright (C) 2026 Signal Slot, Inc.
# SPDX-License-Identifier: BSD-3-Clause

qt_find_package(EGL MODULE)

#####################################################################
## QVncGlIntegrationPlugin Plugin:
#####################################################################

qt_internal_add_plugin(QVncGlIntegrationPlugin
    OUTPUT_NAME qvncgl
    PLUGIN_TYPE platforms
    DEFAULT_IF "vncgl" IN_LIST QT_QPA_PLATFORMS
    SOURCES
        main.cpp
        qvncgl.cpp qvncgl_p.h
        qvncglclient.cpp qvncglclient.h
        qvncglintegration.cpp qvncglintegration.h
        qvncglscreen.cpp qvncglscreen.h
        qvncglbackingstore.cpp qvncglbackingstore.h
        qvncglwindow.cpp qvncglwindow.h
    DEFINES
        QT_EGL_NO_X11
    LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::Gui
        Qt::GuiPrivate
        Qt::Network
        Qt::OpenGL
        EGL::EGL
)

qt_internal_extend_target(QVncGlIntegrationPlugin CONDITION QT_FEATURE_opengl
    LIBRARIES
        Qt::OpenGLPrivate
)

## Scopes:
#####################################################################
