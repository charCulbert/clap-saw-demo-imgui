# clap-saw-demo-imgui

This project is a port of the [clap-saw-demo](https://github.com/surge-synthesizer/clap-saw-demo)
VSTGUI example to have the same clap engine but use [Dear IMGUI](https://github.com/ocornut/imgui)
as the renderer for the clap gui. Currently it works on windows and macOS.

The project works by using the [clap-imgui-support](https://github.com/free-audio/clap-imgui-support)
library which provides an interface between the imgui rendering setup and the clap 
gui interface.

That library hides most of the details, providing a DirectX12 render setup on windows and
a Metal surface on macOS. Contributions from the linux community to make it work with
SDL/OpenGL or another appropriate imgui backend would be welcomed!

To use the imgui, make an editor class which subclasses `imgui_clap_editor` such as 
[the ClapSawDemoEditor here](https://github.com/free-audio/clap-saw-demo-imgui/blob/26bd59dd78dd8bf5f743d8fbe49ba2789ce30877/src/clap-saw-demo-editor.h#L14)
then implement the various mechanics to connect and render as shown in the cpp file. 


# Building the Example

Our CI pipeline shows the minimal build all paltforms which is

```shell
git clone --recurse-submodules https://github.com/free-audio/clap-saw-demo-imgui 
cd clap-saw-demo-imgui
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

To build, you will need visual studio installed on windows, and XCode and CMake 
installed on macOS.

As with all cmake projects you can integrate with your various IDE of choice. For instance
to use XCode directly you would do

```
cmake -B build -G Xcode
open build/clap-saw-demo-imgui.xcodeproj
```


## WCLAP

This fork also builds for WCLAP, using the same synth and ImGui editor in the
browser. The native Windows and macOS builds work as before.

### Build

You need Ninja, WASI SDK with pthread support, and Emscripten's `em++` on `PATH`.
Set `WASI_SDK_PATH` to your SDK directory, then run:

```sh
cmake -S . -B build/wclap -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$WASI_SDK_PATH/share/cmake/wasi-sdk-pthread.cmake"
cmake --build build/wclap
```

Load `build/wclap/artifacts/clap-saw-demo-imgui.wclap.tar.gz` in your host.

The build downloads pinned dependencies, including
[char-clap-utils](https://github.com/charCulbert/char-clap-utils).
Native dependencies are unchanged.

### Host support

Verified in WCLAP Browser DAW. The ImGui interface runs as Wasm with WebGL2;
the host does not need native ImGui support.

The host must support:

- WCLAP with WASI and shared memory.
- `clap.webview/3` resource loading and binary messages.
- `clap.gui` creation, attachment, showing and hiding via the `webview` API.
- Parameter flushing and main-thread callbacks.

Upstream's [browser-test-host at b42ade6](https://github.com/WebCLAP/browser-test-host/blob/b42ade615bef2c989f96400a7b0b2ef15cabd396/clap-audionode/clap-audioworkletprocessor.mjs#L282-L284)
does not yet implement that GUI lifecycle, so it needs changes to run this build.
Browser gamepad navigation is not supported.

For native builds with CMake 4, add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`
to the configure command for upstream's older readerwriterqueue dependency.
