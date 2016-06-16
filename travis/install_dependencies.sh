#!/bin/bash

set -x
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="${SCRIPT_DIR}/.."
DEPS_DIR="${ROOT_DIR}/travis_deps"

# Install newer bison.
rm -rf ${DEPS_DIR}
mkdir -p ${DEPS_DIR}
cd ${DEPS_DIR}
wget --no-check-certificate http://ftp.gnu.org/gnu/bison/bison-3.0.2.tar.gz
tar -xf bison-3.0.2.tar.gz
cd bison-3.0.2
./configure --prefix=${DEPS_DIR}/bison
mkdir ${DEPS_DIR}/bison
make -j 4 install
