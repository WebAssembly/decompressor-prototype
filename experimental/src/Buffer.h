#ifndef DECOMP_SRC_BUFFER_H
#define DECOMP_SRC_BUFFER_H

// Defines a circular buffer of BaseType. Uses a fixed size array, with two
// indices: MinIndex and MexIndex. These indices represent the virtual address
// of the lowest/highest indices modeled in the queue.
//
// Optionally, allows providing a reader/writer to deal with underflow/overflow
// conditions.

template<class ByteType>
class CircBuffer {
  CircBuffer(const CircBuffer&) = delete;
  CircBuffer &operator=(const CircBuffer&) = delete;
 public:

};

#endif // DECOMP_SRC_BUFFER_H
