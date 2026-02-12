# Copyright (C) 2026 Signal Slot, Inc.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## QVncGlIntegrationPlugin Plugin:
#####################################################################

qt_add_plugin(QVncGlIntegrationPlugin
    SHARED
    PLUGIN_TYPE platforms
    OUTPUT_NAME qvncgl
    CLASS_NAME QVncGlIntegrationPlugin
)

target_sources(QVncGlIntegrationPlugin PRIVATE
    main.cpp
    qvncgl.cpp qvncgl_p.h
    qvncglclient.cpp qvncglclient.h
    qvncglintegration.cpp qvncglintegration.h
    qvncglscreen.cpp qvncglscreen.h
    qvncglbackingstore.cpp qvncglbackingstore.h
    qvncglwindow.cpp qvncglwindow.h
)

target_compile_definitions(QVncGlIntegrationPlugin PRIVATE
    QT_EGL_NO_X11
)

target_link_libraries(QVncGlIntegrationPlugin PRIVATE
    Qt::Core
    Qt::CorePrivate
    Qt::Gui
    Qt::GuiPrivate
    Qt::Network
    Qt::OpenGL
    Qt::OpenGLPrivate
    EGL::EGL
)

install(TARGETS QVncGlIntegrationPlugin
    LIBRARY DESTINATION "${QT6_INSTALL_PLUGINS}/platforms"
)
