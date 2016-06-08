#ifndef DECOMP_SRC_FILTER_AST_H
#define DECOMP_SRC_FILTER_AST_H

#include "DecodeDefs.h"

#include <string>

namespace wasm {

namespace filter {

enum class ValueType {
  Integer,
  Symbol,
  Uint8
};

class Node {
public:
  ValueType getType() const {
    return Type;
  }
  size_t getNumKids() const {
    if (NumKids)
      return NumKids;
    return NumKids = countKids();
  }
  Node *getFirstKid() const {
    return KidsFirst;
  }
  Node *getNextSibling() const {
    return NextSibling;
  }
  void prependKid(Node *NewKid) {
    NewKid->NextSibling = KidsFirst;
    KidsFirst = NewKid;
    if (KidsLast == nullptr)
      KidsLast = NewKid;
    NumKids = 0;
  }
  void appendKid(Node *NewKid) {
    if (KidsLast) {
      NewKid->NextSibling = NewKid;
    } else {
      KidsFirst = NewKid;
    }
    KidsLast = NewKid;
    NumKids = 0;
  }
protected:
  ValueType Type;
  mutable size_t NumKids; // if zero, assume not computed.
  Node *NextSibling = nullptr;
  Node *KidsFirst = nullptr;
  Node *KidsLast = nullptr;

  Node(ValueType Type) : Type(Type) {}
  size_t countKids() const;
};

class Integer : public Node {
  Integer(const Integer&) = delete;
  Integer &operator=(const Integer&) = delete;
public:
  uint64_t getValue() const {
    return Value;
  }
  const std::string &getName() const {
    return Name;
  }
  Node *createInteger(uint64_t Value, std::string Name) {
    return new Integer(Value, Name);
  }
private:
  uint64_t Value;
  std::string Name;
  Integer(uint64_t Value, std::string &Name)
      : Node(ValueType::Integer), Value(Value), Name(Name) {}
};


} // end of namespace filter

} // end of namespace wasm

#endif // DECOMP_SRC_FILTER_AST_H
