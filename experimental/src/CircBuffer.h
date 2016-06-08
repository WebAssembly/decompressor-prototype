#ifndef DECOMP_SRC_CIRCBUFFER_H
#define DECOMP_SRC_CIRCBUFFER_H

#include "DecodeDefs.h"
#include "StreamQueue.h"

#include <memory>

namespace wasm {

namespace decode {

// Defines a circular buffer of BaseType. Uses a fixed size array, with two
// indices: MinIndex and MexIndex. These indices represent the virtual address
// of the lowest/highest indices modeled in the queue.
//
// A Reader/Writer is used to deal with underflow/overflow conditions.
template<class BaseType>
class CircBuffer {
  CircBuffer(const CircBuffer&) = delete;
  CircBuffer &operator=(const CircBuffer&) = delete;
 public:

  ~CircBuffer() = default;

  size_t getMinIndex() const {
    return MinIndex;
  }

  size_t getMaxIndex() const {
    return MaxIndex;
  }

  size_t size() const {
    return MaxIndex - MinIndex;
  }

  // Fills buffer until Index can read 1 or more bytes. Returns true if
  // successful.
  bool fill(size_t Index);

  // Reads up to BufSize elements into Buf. Returns the number of elements
  // read. Returns zero if no more input. Automatically advances the Index with
  // the number of elements read.
  size_t read(size_t &Index, BaseType *Buf, size_t BufSize);

  // Skips up to N elements in the buffer. Returns the number of elements
  // skipped. Automatically advances the Indx with the number of elements read.
  size_t skip(size_t &Index, size_t N);

  // Writes BufSize elements in Buf. Returns true if successful. Automatically,
  // advances the Index with the number of elements read.
  bool write(size_t &Index, BaseType *Buf, size_t BufSize) {
    // TODO: implement this.
    (void) Index;
    (void) Buf;
    (void) BufSize;
    return false;
  }

  static std::shared_ptr<CircBuffer<BaseType>>
  create(std::unique_ptr<StreamQueue<BaseType>> Reader,
         std::unique_ptr<StreamQueue<BaseType>> Writer) {
    return std::make_shared<CircBuffer<BaseType>>(std::move(Reader),
                                                  std::move(Writer));
  }

  static std::shared_ptr<CircBuffer<BaseType>>
  createReader(std::unique_ptr<StreamQueue<BaseType>> Reader) {
    std::unique_ptr<StreamQueue<BaseType>> Writer;
    return std::make_shared<CircBuffer<BaseType>>(std::move(Reader),
                                                  std::move(Writer));
  }

  static std::shared_ptr<CircBuffer<BaseType>>
  createWriter(std::unique_ptr<StreamQueue<BaseType>> Writer) {
    std::unique_ptr<StreamQueue<BaseType>> Reader;
    return std::make_shared<CircBuffer<BaseType>>(std::move(Reader),
                                                  std::move(Writer));
  }

  CircBuffer(std::unique_ptr<StreamQueue<BaseType>> Reader,
             std::unique_ptr<StreamQueue<BaseType>> Writer)
      : Reader(std::move(Reader)), Writer(std::move(Writer)) {}

 private:
  std::unique_ptr<StreamQueue<BaseType>> Reader;
  std::unique_ptr<StreamQueue<BaseType>> Writer;
  static constexpr size_t kChunkSizeLog2 = 10;
  static constexpr size_t kChunkSize = 1 << kChunkSizeLog2;
  static constexpr size_t kBufChunksLog2 = 2;
  static constexpr size_t kBufChunks = 1 << kBufChunksLog2;
  static constexpr size_t kBufSize = kChunkSize * kBufChunks;
  static constexpr size_t kIndexMask =
              (1 << (kChunkSizeLog2 + kBufChunksLog2)) - 1;
  uint8_t Buffer[kBufSize];
  size_t MinIndex = 0;
  size_t MaxIndex = 0;
  bool EofReached = false;
};

} // end of namespace decode

} // end of namespace wasm

#endif // DECOMP_SRC_CIRCBUFFER_H
