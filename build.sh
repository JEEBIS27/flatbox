#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR_NAME="${1:-build-cli}"
BUILD_DIR_PATH="${ROOT_DIR}/${BUILD_DIR_NAME}"

if command -v cmake >/dev/null 2>&1; then
  CMAKE_BIN="cmake"
elif [[ -x "${HOME}/.local/bin/cmake" ]]; then
  CMAKE_BIN="${HOME}/.local/bin/cmake"
else
  echo "Error: cmake が見つかりません。" >&2
  echo "       apt install cmake または pip --user で導入してください。" >&2
  exit 1
fi

if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  :
else
  echo "Error: arm-none-eabi-gcc が見つかりません。" >&2
  echo "       Arm GCC ツールチェーンを導入してください。" >&2
  exit 1
fi

if command -v ninja >/dev/null 2>&1; then
  GENERATOR="Ninja"
elif [[ -x "${HOME}/.local/bin/ninja" ]]; then
  export PATH="${HOME}/.local/bin:${PATH}"
  GENERATOR="Ninja"
elif command -v make >/dev/null 2>&1; then
  GENERATOR="Unix Makefiles"
else
  echo "Error: ninja か make のどちらかが必要です。" >&2
  exit 1
fi

CMAKE_CONFIGURE_ARGS=()
if [[ -z "${PICO_SDK_PATH:-}" ]]; then
  CMAKE_CONFIGURE_ARGS+=("-DPICO_SDK_FETCH_FROM_GIT=ON")
fi

echo "Configuring (${GENERATOR})..."
"${CMAKE_BIN}" -S "${ROOT_DIR}" -B "${BUILD_DIR_PATH}" -G "${GENERATOR}" "${CMAKE_CONFIGURE_ARGS[@]}"

echo "Building..."
"${CMAKE_BIN}" --build "${BUILD_DIR_PATH}" -j

echo "Done."
echo "Generated:"
echo "  ${BUILD_DIR_PATH}/flatbox-rev4.uf2"
echo "  ${BUILD_DIR_PATH}/flatbox-rev5.uf2"
