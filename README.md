This is code for a USB arcade controller, meant to be used with RP2040-based boards.

It works with Flatbox [rev4](../hardware-rev4) and [rev5](../hardware-rev5). Precompiled binaries are included.

## Build (Linux/WSL)

Run:

```bash
just build
```

This runs `./build.sh` to build both firmware variants. You can also run the script directly.
On Nix hosts, missing build tools are supplied automatically through the pinned
`shell.nix` environment. The first build downloads the toolchain and requires network access.

The Pico SDK fetches its required `picotool` automatically, and the script checks for the host toolchain (`cmake`, `arm-none-eabi-gcc`, `g++`, and `ninja`/`make`) before configuring.
If `PICO_SDK_PATH` is not set, the script configures CMake with `-DPICO_SDK_FETCH_FROM_GIT=ON`.
Default build output directory is `build-local` (you can change it with `just build <build-dir>` or `./build.sh <build-dir>`).
This keeps new builds separate from the legacy build files tracked in this repository.
