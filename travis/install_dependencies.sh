#!/bin/bash

set -x
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/.."
DEPS_DIR="${ROOT_DIR}/travis_deps"

# Cleanup.
rm -rf ${DEPS_DIR}
mkdir -p ${DEPS_DIR}
cd ${DEPS_DIR}

# Install pinned wasm toolchain.
WASM_ARCHIVE=https://storage.googleapis.com/wasm-llvm/builds/git
WASM_FILENAME=wasm-binaries-10995.tbz2
WASM_URL=${WASM_ARCHIVE}/${WASM_FILENAME}
curl ${WASM_URL} -O
tar xf ${WASM_FILENAME}
sed -e s:/b/build/slave/linux/build/src/src/work/:$PWD/: \
  < wasm-install/emscripten_config > wasm_emscripten_config
sed -e s:/b/build/slave/linux/build/src/src/work/:$PWD/: \
  < wasm-install/emscripten_config_vanilla > wasm_emscripten_config_vanilla

if [ "${TRAVIS}" = true ]; then
  # Install newer bison.
  wget --no-check-certificate http://ftp.gnu.org/gnu/bison/bison-3.0.2.tar.gz
  tar -xf bison-3.0.2.tar.gz
  cd bison-3.0.2
  ./configure --prefix=${DEPS_DIR}/bison
  mkdir ${DEPS_DIR}/bison
  make -j 4 install
  cd ..
fi
