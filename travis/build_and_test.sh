#!/bin/bash

set -x
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/.."
DEPS_DIR="${ROOT_DIR}/travis_deps"
export PATH=${DEPS_DIR}/bison/bin:${PATH}

cd ${ROOT_DIR}

make clean
make
