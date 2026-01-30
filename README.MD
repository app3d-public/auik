# App3D UI Kit

**Auik** is a comprehensive suite of class-oriented widgets for creating user interfaces within the App3D project using [Dear ImGui](https://github.com/ocornut/imgui). This component is dedicated to facilitating the development of visually appealing and functionally rich 3D interfaces, aiming to provide developers with the necessary tools for integrating sophisticated UI elements into their App3D applications seamlessly.

## Getting Started

Integrating auik into your App3D project involves a few straightforward steps:

1. **Integration with Your Project**
   - Ensure that your project is already set up with ImGui and the App3D enviroment.
   - Incorporate the `auik` component into your project to start leveraging its capabilities.

2. **Using the Widgets**
   - Utilize the widgets by following the structured approach provided by `auikk` to enhance your application's user interface.

## Limitations
- Currently auik widgets uses `awin` API for synchronization some events. Auik is not compatible with other window systems yet.
- The ImGui support is limited to ImGui v1.91.9 or less.

## Building

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
- [Dear ImGui](https://github.com/ocornut/imgui)
- [implot](https://github.com/epezent/implot)

## License
This project is licensed under the [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.