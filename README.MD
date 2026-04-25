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
- **Animations and Delayed UI Tasks**  
  Supports host-paced UI animations and delayed tasks for time-based widget updates.
- **Post Effects Pipeline**  
  Supports slot-based post effects between CPU draw data and GPU streams.

## Data streams
Auik uses a single stream recording model:
- GPU data remains stable between frames and is updated only when widget state changes.
- Widgets that require per-frame refresh can be registered in the library transient cache and are re-synced during `sync_frame()`.

### Limitations

Auik supports alpha blending, including composition between UI and non-UI content. It does not guarantee
order-independent transparency (OIT).

Draw commands are grouped into multiple streams and pipelines. Transparent primitives are blended in the order in which their streams and draw commands are rendered. This means alpha blending is reliable for simple cases with a well-defined draw order, but transparency between overlapping widgets from different streams is not guaranteed to match strict painter-order composition.

Widgets and themes should not rely on semi-transparent container backgrounds blending correctly with all descendants when those descendants are rendered through different streams.


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
- [amal](https://github.com/app3d-public/amal)
- [umbf](https://github.com/app3d-public/umbf)

The following dependencies must be installed on your system:
- [fontconfig](https://www.freedesktop.org/wiki/Software/fontconfig/)
- [freetype](https://www.freetype.org/)
- [harfbuzz](https://github.com/harfbuzz/harfbuzz)

## Basic Initialization Example
The example below uses the `awin` window backend and the `agrb` GPU backend.
It is configured for a `waitEvents` - based update model. 
A host refresh request flag is used to wake the main loop when UI updates are required,
while a pending filter is used to limit the effective frame rate.

### Initializing auik resources
```cpp
acul::vector<auik::v2::DrawPipeline> pipelines;
acul::vector<auik::v2::DrawStream> streams;

u32 service_pipelines_count = auik::v2::get_service_pipelines_count();
u32 stream_pipelines_count = auik::v2::get_default_streams_pipelines_count();
pipelines.resize(service_pipelines_count + stream_pipelines_count);
streams.resize(auik::v2::get_default_streams_count());

auto pool = agrb::descriptor_pool::builder()
                .set_max_sets(100)
                .add_pool_size(vk::DescriptorType::eStorageBuffer, 10)
                .add_pool_size(vk::DescriptorType::eCombinedImageSampler, 32)
                .build(device);

auik::v2::CreateInfo create_info;
auik::v2::SyncOptions sync_options;
sync_options.set_host_refresh_request(&env::get_window_refresh_request())
            .set_pending_filter(&env::get_pending_filter());
create_info.set_event_dispatcher(&env::get_event_dispatcher())
    .set_draw_streams(streams.data(), static_cast<u32>(streams.size()))
    .set_gpu_backend(auik::v2::create_agrb_backend(device, pool.get()))
    .set_window_backend(auik::v2::create_awin_backend(env::get_main_window()))
    .set_frames_in_flight(FRAMES_IN_FLIGHT)
    .set_max_textures_size(32)
    .set_sync_options(sync_options);
auik::v2::init_library(create_info);

agrb::graphics_pipeline_batch batch;
auik::v2::configure_service_pipelines(batch, pipelines.data());
auto *streams_pipelines = pipelines.data() + service_pipelines_count;
auik::v2::configure_default_streams(batch, streams_pipelines, streams.data(), 0, render_pass);
if (!batch.allocate_pipelines(env::get_device(), pipelines.size()))
    throw acul::runtime_error("Failed to allocate ui pipelines");
auik::v2::clear_shader_cache(device);
auik::v2::create_default_post_effects();
```

### Theme configuration
```cpp
auik::v2::FontRegistry fonts;
auik::v2::Font default_font;
if (!auik::v2::load_fonts(fonts)) throw acul::runtime_error("Failed to load UI fonts");
const f32 dpi = awin::get_dpi(env::get_main_window());
if (!auik::v2::load_win32_icons(fonts, dpi)) throw acul::runtime_error("Failed to load OS icons");
if (!auik::v2::load_font(fonts, default_font, "Segoe UI")) throw acul::runtime_error("Failed to load UI font");
auik::v2::set_theme(auik::v2::create_default_theme(&default_font, dpi));
```
> [!NOTE]
> For MS Windows, we provide support for loading icons from `Segoe Fluent Icons` / `Segoe MDL2 Assets`.     
> For other supported platforms, we provide support for `Material Design Icons` (by Google).
> To load them, specify the directory containing the font and call
> `auik::v2::load_material_icons(const FontRegistry&, f32)`.

### Adding widgets
```cpp
auto* demo_window = auik::v2::create_demo_window();
auik::v2::add_widget_to_root(demo_window);
```

### Render loop
```cpp
auik::v2::sync_frame();
if (auik::v2::is_dirty_render())
{
   auik::v2::sync_gpu_cache();
   for (auto &stream : streams) auik::v2::render_stream(stream, &cmd);
}
auik::v2::next_frame(&cmd);
if (has_window_refresh_request()) awin::push_empty_event();
```

## Recommended Host Lifecycle
Auik does not own the application loop, so the host is responsible for deciding when to sleep,
when to wake, and when a UI update may actually be rendered.

The recommended integration model is:

1. The host owns a latched `host_refresh_request` flag.
2. Auik raises this flag when UI state requires another host iteration.
3. The host computes the next sleep timeout from its own pacing policy plus the nearest auik delayed task.
4. The host waits for OS events or timeout expiration.
5. After wakeup, the host applies its own frame pacing / pending-filter policy.
6. Only in an allowed frame does the host dispatch delayed auik tasks, process resize work, and render UI.
7. After rendering, the host clears its latched refresh request.

In practice, it is useful to treat the host loop and auik as two different lifetimes:

- The host loop owns sleeping, wakeup, window visibility, and frame pacing.
- Auik owns UI dirtiness, transient redraw requests, and delayed UI tasks such as tooltip reveal.

These two lifetimes are intentionally connected through only a small contract:

- `host_refresh_request`
- `PendingFilter`
- `next_delayed_task_in(...)`
- `dispatch_delayed_tasks(...)`

### Integration notes
- `host_refresh_request` is a request/response latch between auik and the host application. Auik may raise it many times, but the host should clear it only after it has completed an iteration that can observe and process the requested work.
- `PendingFilter` belongs to the host pacing policy. Auik can participate in it, but should not replace it.
- Delayed auik tasks should normally be dispatched only in a frame that is allowed by the host pacing policy. This avoids running UI work more often than the renderer can present it.
- The timeout used by `wait_events_timeout()` should be resolved from the nearest valid wake source, not from sentinel values such as "infinite timeout".
- Minimized windows should usually bypass normal render work and fall back to plain event waiting until the window becomes renderable again.

### Pseudocode
```cpp
while (!window.ready_to_close())
{
    const bool has_ui_delay = auik::v2::has_delayed_tasks();
    f64 timeout = host_refresh_request ? pending_filter.remaining(awin::get_time()) : AWIN_TIMEOUT_INF;
    if (has_ui_delay)
    {
        const f64 ui_delay = auik::v2::next_delayed_task_in(awin::get_time());
        if (ui_delay > 0.0 && (timeout < 0.0 || ui_delay < timeout)) timeout = ui_delay;
    }

    awin::set_timeout(timeout);
    awin::wait_events_timeout();
    if (!window.focused()) continue;

    pending_filter.sync(awin::get_time());
    if (!pending_filter.allow()) continue;

    if (has_ui_delay) auik::v2::dispatch_delayed_tasks(awin::get_time());
    renderer.draw();
    pending_filter.reset();
    host_refresh_request = false;
}
```

## License
This project is licensed under the  [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.
