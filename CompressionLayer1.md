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

# Motivation

This section is intended to motivate ways to compress WASM modules, and the
corresponding filter algorithms that will allow the decompression tool to
decompress the generated compressed file.

The following subsections try to introduce the concepts in a tutorial format.

## Compression is applied to sections

A decompressed WASM module, at the top level, introduces a header, followed by a
simple framework to define a sequence of sections.  Each section header contains
a value that defines the length of the section, defining where the section
ends. The next section immediately follows the previous section.  No more
sections are in the module once the end of file is reached.

Compression, within this framework, is only defined on sections. Conceptually, a
section is processed as a bitstream that only contains the bits of that
section. Therefore the algorithm need not know what appears before or after the
section.

Decompression algorithms are associated with the type of section they apply
to. The association is made using a map from the section ID, to the
corresponding algorithm to apply. If that map doesn't define a corresponding
decompression algorithm, no decompression is applied to that section. Note: to
simplify readability, decompression algorithms will simply be called algorithms.

Algorithms to decode sections are explicitly placed inside the compressed file
as 'filter' sections. Filter sections can appear anywhere within the WASM
module.

Typically, there is only one filter section, and occurs as the first section in
the compressed WASM module. However, multiple filter sections are
allowed. Succeeding filter sections can replace algorithms already defined. When
filter sections appear in a compressed WASM module, the algorithms defined by
that filter section applies to all sections between it and the next filter
section (or end of module if no next filter sections exists).

A filter section only needs to define algorithms for the sections that have been
compressed. All other sections are assumed to not be compressed and the
decompression tool will automatically copy them.

An algorithm is modeled as s-expressions. These s-expressions have operators
that correspond to a programming language that describes how to decompress a
compressed WASM module. In general, we will not discuss the details of these
s-expressions in this motivation section. They will be deferred to section
[Proposed Framework](#proposed-framework).  Rather, only when the motivation
warrants it, will we introduce specific operators (i.e. functions) that support
the motivated example.

Decompression can be thought of as a filter from the compressed bitstream
(i.e. sequence of zeros and ones) to the uncompressed bitstream (a sequence of
bytes). While it is true that decompressed WASM modules are byte based, it is
sufficient to use a bitstream semantic model, since it simplifies defining the
semantics of decompression by only having one model for input and output files.

It is important to note that the decompression tool defined here is based on
structural compression. What differentiates structural compression (WASM layer
1) and non-structural compression (WASM layer 2) is that non-structural
compression doesn't know anything about the structure of the input. Therefore
the compression can't use that knowledge to improve compression. Examples of a
non-structural compressor is zip a file.

Structural compression, on the other hand, can use structural knowledge of the
data within the file to reduce the size of the corresponding compressed file.

The following subsections of this section introduce ways structural compression
can be used to compress (and hence decompress) WASM modules.

## Structure of algorithms

A decompression algorithm consists instructions. Each _instruction_ can be
either a _definintion_, an _expression_, or a _statement_. Definitions define
methods, and associates them with names.  Statements define
control-flow. Expressions represent actions that process values. Values can be
read and/or written. Reads are from an input stream while writes are to the
output stream.

In addition, an expression can appear anywhere a statement is allowed.

Input (and output) streams are either bits, bytes, integers, or abstract syntax
trees. All expressions (i.e. reads and writes) have different meanings,
depending on the type of stream. This is intentional. This allows the same
algorithm that reads compressed WASM modules to also write out the decompressed
form. That is, even though the bit stream may use fewer bits to represent
numbers, the same expression will generate byte-aligned encodings of integers on
byte streams.

In addition, the basic structural model used by algorithms is that WASM modules
are simply a sequences of integers.

Decompression algorithms can directly map from an input (bit stream) to the
decompressed output (byte stream). However, they can also be a sequence of
filters. In such cases, the sequence of filters are applied in the order
specified. The output of the previous filter is then feed in as the input for
the next filter.

## Example s-expressions

Before going into motivation in more detail, let's step back for a minute and
focus on how algorithms are written in compressed WASM modules. A simple example
of an s-expression is the following:

```
(define 'type'
  (bit.to.byte
    (loop (varuint32)
          (varuint7)
          (loop (varuint32) (uint8))
          (if (varuint1) (uint8) (void)))))
```

The above s-expression defines how to parse the
[type section of a WASM Module](https://github.com/WebAssembly/design/blob/master/BinaryEncoding.md#type-section).
The S-expression _define_ assigns its second argument (the bit.to.byte
s-expression) as the algorithm for the section with ID 'type'.

Before going into any detail, it is important to understand the philosophy of
algorithms. An algorithm is used to both parse and generate WASM modules. All
instructions, except control flow (i.e. statements), do both a read and a
write. Conceptually, filters read in a sequence of integers. Control flow
constructs (statements) walk the sequence of integers. The formatting
expressions, when reached, reads the input stream using the specified format,
and then (immediately) writes the read value to the output stream.

Unless special logic is added, algorithms do nothing more than copy the input to
the output, since formatting expressions decode (i.e. read) an integer, and then
immediately uses the same format to encode the value and write it to the output
stream.

This pattern is intentional. It allows us to use the same algorithms for
multiple contexts, as we will see in later sections.

As we will se later, some type of formatting have different effects, depending
on the type of stream. In other cases, expressions can have modifiers that
change from the default behavior of doing both a read and a write. This allows
more interesting effects than simply copying the input to the output.
   
The _bit.to.byte_ statement defines the structure of the input and output
streams of the filter. _bit_ defines the input stream as a sequence of bits
while _byte_ defines the output stream as a sequence of bytes. That is, the name
before the _to_ (between _bit_ and _byte_) defines the structure of the input
stream while the name after the _to_ defines the structure of the output stream.

The _loop_ statement is iterated N times. N is defined using its first argument
(an expression). The remaining arguments are a sequence of statements that
define the body of the loop. Note that in this case, the size of the loop is
read from the bitstream to decompress.

It should be noted that when the loop reads value _N_, it also writes it
out. This guarantees that the loop size is copied into the output file. This
copying is also true for condition tests as well. Again, the key thing to
remember is that most expressions read the value in, and then write it back out.

Formatting expressions _varuint32_, _varuint7_, etc. read/write an encoded
integer using the encoding defined by its name. The same format is used for both
reading and writing.  Expression _void_ does not consume any memory, nor does it
represent any (defined) value.

The nested loop statement reads the list of parameter types for a function. It
is then followed by an if statement that reads (and writes) the return type if
the function signature has one.

As noted earlier, an algorithm not only describes how to parse, but also
describes how to regenerate the WASM module. This concept is built into the
framework. By default two translation functions read and write are defined by
the framework, and operate on expressions. The read function is used to parse
numbers in, and the write function is used to print them back out.

Algorithms are written with respect to decompression. Hence parsing is reading
from the compressed file while printing is writing to the corresponding
decompressed file.

<a name="proposed-framework">
# Proposed Framework

## Types

