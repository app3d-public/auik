# App3D UI Kit Library

**Auik** is a GPU-oriented, event-driven UI runtime designed for high-performance applications.
It is not a windowing framework and does not own your main loop. Instead, it provides a rendering and state runtime designed to integrate into existing engines and toolchains.

## Features

- **Retained UI Architecture**  
  Persistent widget tree owned by the library with explicit lifecycle and state management.
- **GPU-Centric Rendering Pipeline**  
  Draw commands are encoded into structured GPU streams designed for minimal CPU overhead and predictable memory layout.
- **SDI (Single Draw Instancing)**  
  Batches UI primitives into unified instance streams to reduce draw calls and pipeline switches.
- **Bindless Texture Model**  
  Texture sampling is handled via bindless descriptors, eliminating per-draw descriptor rebinding and reducing CPU overhead.
- **SDF-Based Rendering**  
  Resolution-independent vector shapes, borders, and effects using signed distance fields.
- **Advanced Typography**  
  Full Unicode-aware text shaping and layout.

## Data stream modes
Auik supports two stream recording models:
- **Cached**: Event-driven recording model. GPU data remains stable between frames and is updated only when widget state changes.
- **Transient**: Frame-driven recording model. GPU data is refreshed every frame from the widget cache regardless of state changes.  


## Theme and styling
Auik provides a declarative theme system inspired by CSS-like cascading, resolved explicitly during build/update stages.

Styles can be linked by:
- widget tag
- widget ID
- parent hierarchy
- global defaults

### Supported properties
| Property         | Description                         |
|------------------|-------------------------------------|
| padding          | The padding of the widget.          |
| margin           | The margin of the widget.           |
| background_color | The background color of the widget. |
| border_color     | The border color of the widget.     |
| border_radius    | The border radius of the widget.    |
| border_thickness | The border thickness of the widget. |
| corner_mask      | The corner mask of the widget.      |
| text_color       | The text color of the widget.       |
| text_size        | The text size of the widget.        |
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
### Initializating auik resources
```cpp
acul::vector<auik::v2::DrawPipeline> pipelines(1);
acul::vector<auik::v2::DrawStream> streams(1);

auto pool = agrb::descriptor_pool::builder()
                .set_max_sets(100)
                .add_pool_size(vk::DescriptorType::eStorageBuffer, 10)
                .build(device);

auik::v2::CreateInfo create_info;
create_info.set_ed(&env::get_event_dispatcher())
    .set_disposal_queue(&env::get_disposal_queue())
    .set_draw_streams(streams.data(), static_cast<u32>(streams.size()))
    .set_gpu_backend(auik::v2::create_agrb_backend(device, pool.get()))
    .set_window_backend(auik::v2::create_awin_backend(env::get_main_window(), env::get_event_dispatcher()))
    .set_frames_in_flight(FRAMES_IN_FLIGHT)
    .set_window_size(env::get_main_window().dimensions());
auik::v2::init_library(create_info);

auto &cquads_stream = streams[0];
auik::v2::create_quads_stream_cached(cquads_stream);
auik::v2::set_primary_quad_stream(&cquads_stream);
auto &quads_pipeline = pipelines[0];
auik::v2::construct_quads_pipeline(quads_pipeline, device);
cquads_stream.pipeline = &quads_pipeline;

agrb::graphics_pipeline_batch batch;
auto &cquads_artifact = batch.artifacts.emplace_back();

auik::v2::construct_pipeline_artifact(cquads_artifact, 0, &quads_pipeline);
auik::v2::configure_quads_pipeline(cquads_artifact, render_pass, quads_pipeline, device);
if (!batch.allocate_pipelines(env::get_device(), 1))
    throw acul::runtime_error("Failed to allocate ui pipelines");
auik::v2::clear_shader_cache(device);
```

### Adding widgets
```cpp
const f32 dpi = awin::get_dpi(env::get_main_window());
auik::v2::set_theme(auik::v2::create_default_theme(dpi));
auto* demo_window = auik::v2::create_demo_window();
auik::v2::add_widget_to_root(demo_window);
```

### Render loop
```cpp
if (auik::v2::is_dirty_layout()) auik::v2::record_all_commands();
for (auto &stream : streams) auik::v2::render_stream(stream, &secondary);
auik::v2::next_frame();
```

## License
This project is licensed under the  [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.
