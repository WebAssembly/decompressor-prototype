A Framework For Structural Compression On Web Assembly Binary Files
===================================================================

# Introduction

This document is a design document describing a formal framework for _Layer 1_
structural compression tools for
[binary encoded WASM modules](https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md).
The concept of compression (and its inverse decompression) can be thought of as
writing a filter that converts one bitstream to another.  The framework proposed
here is based on the structure of WASM modules, and provides a way of including
filter methods into the module. These filter methods describe how to decompress
the file.

This framework does this is by introducing new filter sections into the binary
WASM module. Each filter section defines a set of methods, via s-expressions,
that define how to decompress WASM modules.

Typically, only a single filter section is introduced as the first section in
the WASM module, to introduce the methods to decompress the WASM
module. However, a WASM module may include several filter sections. The filter
methods defined in a filter section apply until the next filter section is
reached. Succeeding filter sections allows one to update the filter
methods. These updated filter methods are then applied until the next filter
section.

It should be noted here that we are not describing how to build compression
filters, and the corresponding filter sections. That is left to individual
vendors to supply. By encoding the filter within the binary WASM module,
decompression works independently of how the WASM module was compressed.
