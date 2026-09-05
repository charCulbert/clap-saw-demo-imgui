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


## WCLAP with a Wasm ImGui interface

The native build above keeps upstream's native renderer and pinned dependencies.
The same editor controls and synth sources also build for the browser. Native
window attachment lives in `src/clap-saw-demo-gui.cpp`; `web/` adapts the existing
editor queues to WebView messages between the separate DSP and GUI Wasm modules.
Resource serving, message transport and GUI lifecycle use `char_clap::WebUI`,
with a local Wasm MIME override for the pinned helper version.

Install WASI SDK with pthread support and put Emscripten's `em++` on `PATH`, then:

```sh
cmake -S . -B build/wclap -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$WASI_SDK_PATH/share/cmake/wasi-sdk-pthread.cmake"
cmake --build build/wclap
```

Load `build/wclap/artifacts/clap-saw-demo-imgui.wclap.tar.gz` in a WCLAP host.
The host needs WASI/shared-memory WCLAP support, `clap.webview/3` resource loading
and binary messaging, the `clap.gui` `webview` lifecycle, parameter flushing and
main-thread callbacks. The browser must support Wasm and WebGL2; the interface
is a web UI and needs no native ImGui support from the host.
Verified in WCLAP Browser DAW. Upstream's [browser-test-host at b42ade6](https://github.com/WebCLAP/browser-test-host/blob/b42ade615bef2c989f96400a7b0b2ef15cabd396/clap-audionode/clap-audioworkletprocessor.mjs#L282-L284)
leaves the GUI lifecycle unimplemented and needs host-side changes for this build.

The web build downloads pinned CLAP, clap-helpers and char-clap-utils dependencies;
it does not change the native dependency pins. Browser gamepad navigation is not supported.
For native builds with CMake 4, also pass `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`
for upstream's older readerwriterqueue CMake file.
