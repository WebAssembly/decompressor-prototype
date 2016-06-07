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

# Background

Structural compression comes in many forms. The simplest is to recognize that
WASM modules byte align all of its data. Byte alignment, in general, isn't
minimal.  For example, if a field contains a boolean value, it can be
represented using 1 bit rather than a byte. Storing the field as a bit would
reduce the space requirements (for that field) by 7/8. Hence, using smaller
fixed-width storage can decrease file size.

Beside fixed-length compressions, there are also many forms of
[variable-length encodings](https://en.wikipedia.org/wiki/Variable-length_code)
of data that can compress data further. An example is form is
[Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding). More
specifically, integers are frequently encoded using a
[variable-length quantity](https://en.wikipedia.org/wiki/Variable-length_quantity).
These Compressions typically use
[binary octects](https://en.wikipedia.org/wiki/Binary_code) that encode
arbitrary large integers as a sequence of eight-bit bytes.

[DWARF](https://en.wikipedia.org/wiki/DWARF) files introduces the notions of
[LEB128](https://en.wikipedia.org/wiki/LEB128) for unsigned integers, and
[Signed LEB128](https://en.wikipedia.org/wiki/LEB128) for signed integers. Both
are forms of variable-length quantity. Encoding is done by chunking the number
into a sequence of 7-bit values, sorted from least significant bits to most
significant bits. Then, write out each 7-bit value as a byte, by setting the
most significant bit on each 7-bit value, except the last.

Note that the most significant bit can be used to determine if more data is
needed, and allows one to encode the number back together.

Similarly, Signed LEB128 is a generalization of LEB128. Non-negative numbers
encode well using LEB128. On the other hand, negative numbers do not, since the
most significant bit of a negative number is always 1. This results is all bits
of the number being written out in the encoding, resulting in a larger sequence
of bits than you started with.

Therefore, the signed LEB128 sign extend negative numbers to the smallest number
of 7-bit chunks where the most significant bit of the most significant chunk
is 1. The resulting sequence of chunks is then written out using the same format
as LEB128.

[LLVM bitcode files](http://llvm.org/docs/BitCodeFormat.html) use a variant on
the LEB128 encoding, called variable bit rate that allows the chunk size to be
any size. not just 7. They use the notation vbr(N) to denote encodings that use
chunks of size N-1, generating sequences of N bits. The advantage of this scheme
is that numbers can frequently be compressed better than LEB128. The
disadvantage is that the written data is no longer byte aligned.

Similarly, LLVM bitcode files use a different technique to encode signed
integers. It first encodes the number as an absolute value, and then left shifts
it by one, and then sets the least significant bit to 1 if negative.

Another place for compression is to realize that code sections of WASM modules
are basically a list of instructions. These instructions are encoded using byte
opcodes. These opcodes are used to dispatch the reader to read the remaining
bytes of the instruction, based on the opcode value. This framework introduces a
select instruction to perform this task.

It should be noted that using single byte width opcodes may not be the best
choice. First, while some opcodes may require all 8 bits, there may be less that
256 different opcode values. In such cases the opcode can be compressed into a
smaller fixed size.

Although the current WASM instruction only uses one-byte opcodes, future
extensions may require the use of multiple byte opcodes. This framework allows
for this possibility by allowing select instructions to be nested.

Another common form of structural compression is to realize that many
instructions (or even sequences of instructions) have common fields. In such
cases, one can create a new opcode that automatically fills in the common
components. This removes the automatically filled in portions from having to be
explicitly encoded as data.

Note that these new opcodes do not extend the WASM code section opcodes. Those
opcodes are predefined, and built into the decoder of a WASM module. Rather,
they are opcodes only added for decompression. Further, what new opcodes are
introduced may be (dynamically) dependent on the data for the specific WASM
module being decompressed.

The minimally viable framework introduced by this document allows for the types
of compression discussed above. Other compressions are possible, but are not
proposed here. Rather, we assume that this framework can (and should) be
extended with corresponding compelling examples.

Note that we do not have any experience on how well one can structurally
compress WASM modules.  However, it should also be noted that the framework
proposed here are generalizations of the compression techniques used in
[LLVM bitcode files](http://llvm.org/docs/BitCodeFormat.html). The LLVM bitcode
file format is also used in
[PNaCl bitcode files](http://www.gonacl.com). Experience with PNaCl bitcode
files, and its compression tool pnacl-compress, has shown that compression rates
of at least 40% is achievable (from that of byte-aligning all data). Further,
these compressed files are based on structural compressions, and can be further
compressed using common byte-level compression tools (such as gzip) by an
additional 10 to 20 percent.
