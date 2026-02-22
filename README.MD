# AUIK

**Auik** is a GPU-oriented, event-driven UI runtime.
It combines Retained and Immediate execution models inside a deterministic, pipeline-oriented architecture fully controlled by the host application.

Auik is not a windowing framework and does not own your main loop.
It is a rendering and state runtime that integrates into existing engines and toolchains.

## Features

- **Hybrid Retained + Immediate Runtime**  
  Both execution models coexist and share the same rendering and caching infrastructure.
- **GPU-Centric Rendering Pipeline**  
  Draw data is prepared as structured GPU streams designed for minimal state changes.
- **SDI (Single Draw Instancing)**  
  Optimized to reduce draw calls and pipeline switches across complex UI hierarchies.
- **Hierarchical Draw Layers**  
  Explicit multi-layer composition with deterministic ordering and subpass control.
- **SDF-Based Rendering**  
  Resolution-independent rendering for shapes and borders.
- **Professional Typography**  
  True text shaping and layout powered by HarfBuzz.

## Architecture Overview

Auik implements a decoupled, pipeline-oriented architecture separating:

- **Control Flow** (events, interaction, widget state)
- **Data Preparation** (draw streams, layout, style resolution)
- **Execution** (GPU pipelines, render queue submission)

The system avoids implicit redraw models and instead prepares structured GPU-ready data.

![Architecture Flow](./images/arch.png)

### Execution model
1. Host injects events.
2. UI updates state.
3. Layout/style/command recording runs on demand.
4. Draw streams are submitted by the selected GPU backend.

### Supported stream modes
- Retained: persistent recorded data, updated on state changes.
- Immediate: frame-local command generation with internal caches.

## Theme and styling
Auik provides a declarative theme system inspired by CSS-like cascading, resolved explicitly during build/update stages.

Styles can be linked by:
- widget tag
- widget ID
- parent hierarchy
- global defaults

Style properties are divided into inherited and non-inherited groups.

---

### Non-Inherited properties
| Property         | Description                         |
|------------------|-------------------------------------|
| padding          | The padding of the widget.          |
| margin           | The margin of the widget.           |
| background_color | The background color of the widget. |
| border_color     | The border color of the widget.     |
| border_radius    | The border radius of the widget.    |
| border_thickness | The border thickness of the widget. |
| corner_mask      | The corner mask of the widget.      |

### Inherited properties:
| Property   | Description                   |
|------------|-------------------------------|
| text_color | The text color of the widget. |

---

The Theme API supports named style values stored in a typed key-value registry.

## Configuration and session history
Auik supports:

- persistent window layout
- widget configuration
- enabled/disabled states

Configuration is stored as a UMBF data block and can be embedded into UMBF files.

## Building

Auik is distributed as a CMake project and is intended to be integrated as a submodule.

Before adding the subdirectory, define:

| Option                    | Description                  | Supported values |
|---------------------------|------------------------------|------------------|
| `AUIK_GPU_BACKEND`        | GPU backend                  | `VULKAN`         |
| `AUIK_WINDOW_BACKEND`     | Window backend               | `AWIN`           |
| `AUIK_SHADERS_OUTPUT_DIR` | Output directory for shaders | `<path>`         |

> [!NOTE]
> The Vulkan backend is built on top of the **agrb** abstraction layer.  
> If you need to work with native Vulkan handles, use the Vulkan-to-agrb adapter provided by the agrb library.
>
> Auik also provides a helper function (`initialize_agrb_adopted_device`) that configures agrb with UI-specific defaults (descriptor pools, resource counts, etc.) for seamless integration. To enable this mode, define `AUIK_BACKEND_AGRB_ADOPTED`.

### Supported compilers:
- GNU GCC
- Clang

### Supported OS:
- Linux
- Microsoft Windows

### Bundled submodules
The following dependencies are included as git submodules and must be checked out when cloning:
- [acbt](https://github.com/app3d-public/acbt)
- [acul](https://github.com/app3d-public/acul)
- [awin](https://github.com/app3d-public/awin)
- [amal](https://github.com/app3d-public/amal)
- [agrb](https://github.com/app3d-public/agrb)
- [umbf](https://github.com/app3d-public/umbf)

## Minimal Initialization Example

```cpp
auik::v2::init_agrb_dispatcher();

auik::v2::CreateInfo create_info;
create_info.set_ed(&dispatcher)
    .set_disposal_queue(&disposal_queue)
    .set_gpu_backend(&device)
    .set_frames_in_flight(FRAMES_IN_FLIGHT);
auik::v2::init_library(create_info);

auto *layers = acul::alloc_n<auik::v2::DrawLayer>(1);
layers[0].subpass = 1;
layers[0].stream_count = 1;
layers[0].streams = acul::alloc_n<auik::v2::DrawStream>(1);

auik::v2::create_quads_stream_retained(layers[0].streams[0]);
auik::v2::set_default_quad_stream(&layers[0].streams[0]);

auik::v2::set_theme(auik::v2::create_default_theme());
// build widget tree...
// run initial style/layout/record pass...
```

## License
This project is licensed under the  [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.