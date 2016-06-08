#ifndef DECOMP_SRC_STREAMOFBYTES_H
#define DECOMP_SRC_STREAMOFBYTES_H

#include <cstddef>
#include <cstdint>

namespace wasm {

namespace decode {

// Interface modeling queue's of base. Reads remove from the front,
// and writes append to the back.
template<class Base>
class StreamQueue {
  StreamQueue(const StreamQueue &M) = delete;
  StreamQueue &operator=(const StreamQueue&) = delete;
public:

  StreamQueue() {}

  virtual ~StreamQueue() = default;

  // Reads a contiguous range of elements.
  //
  // @param Buf     - A pointer to a buffer to be filled.
  // @param BufSize - The size of the buffer to read into.
  // @result        - The number of bytes read, or 0 if unable
  //                  to read anymore bytes.
  virtual int read(uint8_t *Buf, size_t Size=1) = 0;

  // Writes a contigues range of elements to the given address.
  //
  // @param Buf     - A pointer to a buffer to write from.
  // @param BufSize - The size of the buffer to write from.
  // @param Address - The logical address where writing should start
  //                  in the stream of bytes.
  // @result        - True if successful.
  virtual bool write(uint8_t *Buf, size_t Size = 1) = 0;

  // Communicates that the stream can no longer be modified. Returns
  // false if unable to freeze.
  virtual bool freeze() = 0;
#if 0
  // Returns true if the logical address is valid for the stream of bytes.
  virtual bool isValidAddress(size_t Address) = 0;
#endif

  // Returns true if Address is at the end of stream (i.e. equal to the size
  // of the stream).
  virtual bool atEof() = 0;


};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMP_SRC_STREAMOFBYTES_H
