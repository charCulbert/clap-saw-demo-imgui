include(FetchContent)

# The webview draft requires newer headers; native builds keep upstream's pins.
FetchContent_Declare(web_clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG a47f6badb49d948fd009998f28309cdab78979c9
    GIT_SUBMODULES "")
FetchContent_Declare(web_clap_helpers
    GIT_REPOSITORY https://github.com/free-audio/clap-helpers.git
    GIT_TAG 629ae7526740e3d0ee4d64c230df183ecdb2ff5a
    GIT_SUBMODULES "")
FetchContent_Declare(char_clap_utils
    GIT_REPOSITORY https://github.com/charCulbert/char-clap-utils.git
    GIT_TAG ff3e152bf46a5c07c747b0dea63081fcca10b002
    GIT_SUBMODULES "")
FetchContent_MakeAvailable(web_clap web_clap_helpers char_clap_utils)
