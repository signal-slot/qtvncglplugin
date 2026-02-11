# Qt VNC GL Platform Plugin

A Qt 6 platform plugin that provides GPU-accelerated VNC server functionality using EGL/OpenGL ES offscreen rendering.

Unlike the software-only VNC plugin in Qt, this plugin uses EGL pbuffer surfaces and OpenGL ES for rendering, enabling hardware-accelerated graphics over VNC. All Qt rendering (including Qt Quick/QML scenes) is performed via OpenGL, with the framebuffer read back and served over the VNC protocol.

## Features

- GPU-accelerated rendering via EGL/OpenGL ES pbuffer surfaces
- Full VNC server (RFB protocol 3.3) with keyboard and mouse input
- Supports Qt Quick, Qt Widgets, and any Qt application using the OpenGL rendering backend
- Configurable resolution and VNC port
- Client-side cursor support

## Requirements

- Qt 6 (qtbase with OpenGL support)
- EGL and OpenGL ES 2.0 capable GPU/driver
- Network access for VNC client connections

## Building

```bash
mkdir build && cd build
qt-cmake ..
cmake --build .
```

The plugin will be built as `libqvncgl.so` (or platform equivalent).

## Usage

Run any Qt application with the `vncgl` platform:

```bash
./your_app -platform vncgl:port=5900
```

### Platform Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `port=N` | VNC server listen port | 5900 |
| `size=WxH` | Screen resolution | 1024x768 |
| `mmsize=WxH` | Physical screen size in mm | auto |

### Examples

```bash
# Run with custom port and resolution
./your_app -platform vncgl:port=5910:size=1920x1080

# Force OpenGL backend for apps that default to Vulkan
./your_app -platform vncgl:port=5910 --opengl
```

Connect with any VNC client (e.g., TigerVNC, RealVNC) to `localhost:5910`.

## License

This project is licensed under the terms of the GNU General Public License v3.0 (GPL-3.0).
See [LICENSE.GPL3](LICENSE.GPL3) for details.
