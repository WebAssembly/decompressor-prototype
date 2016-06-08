#include "CircBuffer.h"

namespace wasm {

namespace decode {

template<class BaseType>
size_t CircBuffer<BaseType>::read(size_t &Index, BaseType *Buf,
                                  size_t BufSize) {
  if (!fill(Index))
    return 0;
  size_t Count = std::min(BufSize, (MaxIndex - Index));
  for (size_t i = 0; i < Count; ++i)
    *(Buf++) = Buffer[(Index++) & kIndexMask];
  return Count;
}

template<class BaseType>
bool CircBuffer<BaseType>::fill(size_t Index) {
  while (Index >= MaxIndex) {
    if (EofReached)
      return false;
    // Make sure there is room to read more bytes, make room for more.
    if (size() == kBufSize)
      MinIndex += kChunkSize;
    size_t NumBytes = Reader->read(&Buffer[MaxIndex & kIndexMask], kChunkSize);
    MaxIndex += NumBytes;
    if (NumBytes == 0) {
      EofReached = true;
      return false;
    }
  }
  return Index >= MinIndex;
}

template class CircBuffer<uint8_t>;

}; // end of namespace decode

} // end of namespace wasm
