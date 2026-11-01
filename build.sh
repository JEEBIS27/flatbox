#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR_NAME="${1:-build-local}"
BUILD_DIR_PATH="${ROOT_DIR}/${BUILD_DIR_NAME}"

# Supply the toolchain automatically on Nix hosts.
if [[ -z "${FLATBOX_NIX_BUILD:-}" ]] && command -v nix-shell >/dev/null 2>&1 &&
   { ! command -v cmake >/dev/null 2>&1 || ! command -v arm-none-eabi-gcc >/dev/null 2>&1; }; then
  export FLATBOX_NIX_BUILD=1
  export FLATBOX_BUILD_SCRIPT="${ROOT_DIR}/build.sh"
  export FLATBOX_BUILD_DIR="${BUILD_DIR_NAME}"
  export NIX_BUILD_SHELL="$(command -v bash)"
  exec nix-shell "${ROOT_DIR}/shell.nix" --run 'exec bash "$FLATBOX_BUILD_SCRIPT" "$FLATBOX_BUILD_DIR"'
fi

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
# Preserve relocated build trees, including nested dependency caches.
if [[ -d "${BUILD_DIR_PATH}" ]]; then
  while IFS= read -r -d '' cache; do
    if ! grep -Fxq "CMAKE_CACHEFILE_DIR:INTERNAL=$(dirname "${cache}")" "${cache}"; then
      backup_dir="$(mktemp -d /tmp/flatbox-build-backup.XXXXXX)"
      mv -- "${BUILD_DIR_PATH}" "${backup_dir}/build"
      echo "Notice: relocated CMake cache; previous build saved to ${backup_dir}/build"
      break
    fi
  done < <(find "${BUILD_DIR_PATH}" -name CMakeCache.txt -print0)
fi
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
