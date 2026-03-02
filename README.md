# Qt VNC GL Platform Plugin

A Qt 6 platform plugin that provides GPU-accelerated VNC server functionality using EGL/OpenGL ES offscreen rendering.

Unlike the software-only VNC plugin in Qt, this plugin uses EGL pbuffer surfaces and OpenGL ES for rendering, enabling hardware-accelerated graphics over VNC. All Qt rendering (including Qt Quick/QML scenes) is performed via OpenGL, with the framebuffer read back and served over the VNC protocol.

## Features

- GPU-accelerated rendering via EGL/OpenGL ES pbuffer surfaces
- Full VNC server (RFB protocol 3.3) with keyboard and mouse input
- Supports Qt Quick, Qt Widgets, and any Qt application using the OpenGL rendering backend
- Configurable VNC port and physical screen size
- Screen automatically resizes to match the application window
- Client-side cursor support
- Cross-platform: Linux (native EGL) and Windows (EGL via ANGLE)

## Supported Platforms

| Platform | GPU Backend |
|----------|-------------|
| Linux | EGL with native OpenGL ES drivers |
| Windows | EGL via ANGLE (OpenGL ES over Direct3D) |

## Requirements

- Qt 6 (qtbase with OpenGL and EGL support)
- EGL and OpenGL ES 2.0 capable GPU/driver
  - Linux: Mesa or vendor EGL driver
  - Windows: ANGLE (bundled with Qt)
- Network access for VNC client connections

## Building

```bash
mkdir build && cd build
qt-cmake ..
cmake --build .
```

The plugin will be built as `libqvncgl.so` (Linux) or `qvncgl.dll` (Windows).

## Usage

Run any Qt application with the `vncgl` platform:

```bash
./your_app -platform vncgl:port=5900
```

### Platform Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `port=N` | VNC server listen port | 15900 (auto-detect) |
| `size=WxH` | Initial screen resolution (resizes to match window) | 1024x768 |
| `mmsize=WxH` | Physical screen size in mm | auto (96 dpi) |

### Environment Variables

| Variable | Description |
|----------|-------------|
| `QT_VNC_PORT` | Override VNC server listen port (takes precedence over `port=N` parameter) |

### Port Selection

When no port is specified (neither `port=N` parameter nor `QT_VNC_PORT`), the server automatically scans for an available port starting from 15900. When a port is explicitly specified, only that port is used.

### Examples

```bash
# Run with default auto-detect port (starting from 15900)
./your_app -platform vncgl

# Run with custom port via platform parameter
./your_app -platform vncgl:port=5910

# Run with custom port via environment variable
QT_VNC_PORT=5910 ./your_app -platform vncgl

# Run with explicit initial resolution
./your_app -platform vncgl:port=5910:size=1920x1080

# Force OpenGL backend for apps that default to Vulkan
./your_app -platform vncgl:port=5910 --opengl
```

Connect with any VNC client (e.g., TigerVNC, RealVNC) to `localhost:<port>`.

## License

This project is licensed under the terms of the GNU General Public License v3.0 (GPL-3.0).
See [LICENSE.GPL3](LICENSE.GPL3) for details.
