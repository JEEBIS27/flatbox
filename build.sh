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

if command -v c++ >/dev/null 2>&1; then
  CXX_BIN="$(command -v c++)"
elif command -v g++ >/dev/null 2>&1; then
  CXX_BIN="$(command -v g++)"
else
  echo "Error: C++ コンパイラが見つかりません。" >&2
  echo "       sudo apt update && sudo apt install build-essential g++" >&2
  exit 1
fi
export CXX="${CXX_BIN}"

if command -v ninja >/dev/null 2>&1; then
  NINJA_BIN="$(command -v ninja)"
  GENERATOR="Ninja"
elif [[ -x "${HOME}/.local/bin/ninja" ]]; then
  export PATH="${HOME}/.local/bin:${PATH}"
  NINJA_BIN="${HOME}/.local/bin/ninja"
  GENERATOR="Ninja"
elif command -v make >/dev/null 2>&1; then
  GENERATOR="Unix Makefiles"
else
  echo "Error: ninja か make のどちらかが必要です。" >&2
  exit 1
fi

export PICOTOOL_FORCE_FETCH_FROM_GIT=ON

CMAKE_CONFIGURE_ARGS=()
if [[ -n "${NINJA_BIN:-}" && "${GENERATOR}" == "Ninja" ]]; then
  CMAKE_CONFIGURE_ARGS+=("-DCMAKE_MAKE_PROGRAM=${NINJA_BIN}")
fi
if [[ -z "${PICO_SDK_PATH:-}" ]]; then
  CMAKE_CONFIGURE_ARGS+=("-DPICO_SDK_FETCH_FROM_GIT=ON")
fi

if [[ -f "${BUILD_DIR_PATH}/CMakeCache.txt" ]] && grep -q "CMAKE_MAKE_PROGRAM:FILEPATH=.*ninja" "${BUILD_DIR_PATH}/CMakeCache.txt" 2>/dev/null; then
  if [[ -n "${NINJA_BIN:-}" ]] && [[ "$(grep -E "^CMAKE_MAKE_PROGRAM:FILEPATH=" "${BUILD_DIR_PATH}/CMakeCache.txt" | head -n 1 | cut -d= -f2-)" != "${NINJA_BIN}" ]]; then
    echo "Notice: stale CMake ninja cache detected; forcing ${NINJA_BIN} for this build."
  fi
fi

echo "Configuring (${GENERATOR})..."
"${CMAKE_BIN}" -S "${ROOT_DIR}" -B "${BUILD_DIR_PATH}" -G "${GENERATOR}" "${CMAKE_CONFIGURE_ARGS[@]}"

echo "Building..."
"${CMAKE_BIN}" --build "${BUILD_DIR_PATH}" -j

echo "Done."
echo "Generated:"
echo "  ${BUILD_DIR_PATH}/flatbox-rev4.uf2"
echo "  ${BUILD_DIR_PATH}/flatbox-rev5.uf2"
