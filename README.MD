# App3D UI Kit Library

Auik is a GPU-oriented, event-driven UI runtime designed for high-performance applications.
It is not a windowing framework and does not own your main loop. Instead, it provides a rendering and state runtime designed to integrate into existing engines and toolchains.

## Features
- Retained-mode UI architecture
- Event-driven update model
- Multi-pipeline GPU rendering
- Single Draw Instancing (SDI)
- Bindless texture model
- SDF-based rendering
- Unicode-aware typography
- Animations and delayed UI tasks
- Post-processing effects
- CSS-inspired theme system
- Audio support for UI interactions and modal events
- Configuration and session history support

## Architecture
Auik uses an event-driven retained UI model with a multi-pipeline rendering architecture.
Unlike traditional UI systems that generate a single vertex stream, Auik organizes rendering into multiple specialized draw streams and pipelines.

## Styling
Auik includes a CSS-inspired theme system. Themes can be created either through the native C++ API or through the CSS transpiler.

## Limitations
Auik supports standard alpha blending but does not provide OIT order-independent transparency.
Transparent widgets rendered through different streams or pipelines may not preserve strict painter-order composition.

## Building
Auik is distributed as a CMake project and is intended to be integrated as a submodule.

### Supported compilers:
- GNU GCC
- Clang

### Supported OS:
- Linux
- Microsoft Windows

### Bundled submodules
The following dependencies are included as git submodules and must be checked out when cloning:
- [acbt](../acbt)
- [acul](../acul)
- [amal](../amal)
- [umbf](../umbf)

The following dependencies must be installed on your system:
- [Python 3](https://www.python.org/)
- [Jinja2](https://pypi.org/project/Jinja2/)
- [tinycss2](https://pypi.org/project/tinycss2/)
- [fontconfig](https://www.freedesktop.org/wiki/Software/fontconfig/)
- [freetype](https://www.freetype.org/)
- [harfbuzz](https://github.com/harfbuzz/harfbuzz)

## CSS Theme Generation

If `AUIK_CSS_DIR` is set before adding Auik, CSS files from that directory are parsed and converted into generated C++ theme sources.

```cmake
set(AUIK_CSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ui")
add_subdirectory(modules/auik)
```

Generated files are exported through:
- `AUIK_CSS_SRC`
- `AUIK_CSS_HEADER`
- `AUIK_CSS_INCLUDE_DIR`
- `AUIK_CSS_TARGET`

The host application is responsible for compiling the generated files into its own target.

## Bindings
Auik uses external platform and rendering bindings.

### GPU bindings
- [auik-vulkan](../auik-vulkan) - Vulkan rendering backend

### Window bindings
- [auik-awin](../auik-awin) - Awin window system integration

## License
This project is licensed under the  [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.
