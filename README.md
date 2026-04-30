This is code for a USB arcade controller, meant to be used with RP2040-based boards.

It works with Flatbox [rev4](../hardware-rev4) and [rev5](../hardware-rev5). Precompiled binaries are included.

## Build (Linux/WSL)

Run:

```bash
./build.sh
```

If `PICO_SDK_PATH` is not set, the script configures CMake with `-DPICO_SDK_FETCH_FROM_GIT=ON`.
Default build output directory is `build-cli` (you can change it with `./build.sh <build-dir>`).
