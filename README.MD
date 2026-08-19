# App3D UI Kit Library

Auik is a GPU-oriented, event-driven UI runtime designed for high-performance applications.
It is not a windowing framework and does not own your main loop. Instead, it provides a rendering and state runtime designed to integrate into existing engines and toolchains.

## Features
- Retained-mode UI architecture
- Event-driven update model
- Model bindings for data-driven widgets
- Multi-pipeline GPU rendering
- Unicode-aware typography
- Animations and delayed UI tasks
- Post-processing effects
- CSS-inspired theme system
- Audio support for UI interactions and modal events
- Binary UI state snapshots
- i18n integration API

## Architecture
Auik uses an event-driven retained UI model with a multi-pipeline rendering architecture.
Unlike traditional UI systems that generate a single vertex stream, Auik organizes rendering into multiple specialized draw streams and pipelines.

## Styling
Auik includes a CSS-inspired theme system. Themes can be created either through the native C++ API or through the CSS transpiler.

## Icons
Auik provides an API for default icons using [Remix Icon](https://github.com/Remix-Design/RemixIcon). Icons and fonts are not bundled.

## Limitations
* Auik supports standard alpha blending but does not provide order-independent transparency (OIT).
Transparent widgets rendered through different streams or pipelines may not preserve strict painter-order composition.

* Auik assigns depth ranges to widgets and child layers, but visual painter order is not guaranteed across different draw streams or GPU pipelines inside the same widget subtree. Background and foreground children are intended for non-layout attachments and overlays, not for strict mixed-pipeline overlap composition.

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
- [acbt](https://repos.wusikijeronii.me/app3d/acbt)
- [acul](https://repos.wusikijeronii.me/app3d/acul)
- [amal](https://repos.wusikijeronii.me/app3d/amal)
- [umbf](https://repos.wusikijeronii.me/app3d/umbf)

The following dependencies must be installed on your system:
- [Python 3](https://www.python.org/)
- [Jinja2](https://pypi.org/project/Jinja2/)
- [tinycss2](https://pypi.org/project/tinycss2/)
- [fontconfig](https://www.freedesktop.org/wiki/Software/fontconfig/)
- [freetype](https://www.freetype.org/)
- [harfbuzz](https://github.com/harfbuzz/harfbuzz)

### CSS Theme Generation

Auik itself generates the built-in style and variable ids required by the public widget headers. Optional modules can generate their own public style id headers and register them with the theme compiler. Host applications generate concrete CSS theme sources explicitly with `compile_auik_css_theme()`.

```cmake
add_subdirectory(modules/auik)
```

Call `compile_auik_css_theme()` even when the host application only wants the default theme:

```cmake
compile_auik_css_theme("${AUIK_BASE_THEME}")
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
- [auik-gbe-agrb](https://repos.wusikijeronii.me/app3d/auik-gbe-agrb) - Agrb (Vulkan) rendering backend

### Window bindings
- [auik-wbe-awin](https://repos.wusikijeronii.me/app3d/auik-wbe-awin) - Awin window system integration

## Demo
Auik provides an optional [auik-demo](https://repos.wusikijeronii.me/app3d/auik-demo) module containing a ready-to-use demo window built entirely from standard Auik widgets.

## License
This project is licensed under the  [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.
