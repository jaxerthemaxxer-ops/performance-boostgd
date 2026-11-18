# Performance Boost

Geode mod for Geometry Dash 2.2081, built against Geode 5.11.2.

## Build locally (Windows)

1. Install the Geode CLI: https://docs.geode-sdk.org/getting-started/geode-cli
2. Run `geode sdk install` then `geode sdk install-binaries`
3. Make sure `GEODE_SDK` is set (the CLI does this) and CMake plus a C++ toolchain are installed
4. In this folder run `geode build`
5. Copy the generated `devassistant.performance-boost.geode` from `build/` into
   `%LOCALAPPDATA%\GeometryDash\geode\mods\` (or install it through the Geode menu)

## Build without installing anything

1. Push this folder to a GitHub repository
2. Open the Actions tab and run "Build mod"
3. Download the `.geode` from the build artifacts

## Change the mod ID

The ID `devassistant.performance-boost` and developer name are placeholders.
Edit `id` and `developer` in `mod.json` before publishing.
