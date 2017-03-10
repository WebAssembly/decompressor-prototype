#!/bin/bash

set -x
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/.."
DEPS_DIR="${ROOT_DIR}/travis_deps"
export PATH=${DEPS_DIR}/bison/bin:${PATH}
export NODE=`which node`

cd ${ROOT_DIR}

# TODO(kschimpf): Fix me.
# Build for Wasm.
#make clean-all
#make -j8 PLATFORM=Travis WASM=1 RELEASE=1

# Build for Wasm Vanilla.
make clean-all
make -j8 PLATFORM=Travis -db --no-builtin-rules GEN=1
make -j8 PLATFORM=Travis WASM=1 WASM_VANILLA=1 RELEASE=1

# Build for host.
make -j8 PLATFORM=Travis presubmit
