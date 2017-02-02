/* -*- C++ -*- */
//
// Copyright 2016 WebAssembly Community Group participants
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implements a binary tree encoding huffman values.

#include "utils/HuffmanEncoding.h"

#include "utils/heap.h"

#include <algorithm>

namespace wasm {

using namespace decode;

namespace utils {

HuffmanEncoder::Node::Node(NodeType Type, WeightType Weight)
    : Type(Type), Weight(Weight) {
}

HuffmanEncoder::Node::~Node() {
}

int HuffmanEncoder::Node::compare(Node* Nd) const {
  if (getWeight() < Nd->getWeight())
    return -1;
  if (getWeight() > Nd->getWeight())
    return 1;
  return int(Type) - int(Nd->Type);
}

void HuffmanEncoder::Node::indentTo(FILE* Out, size_t Indent) {
  for (size_t i = 0; i < Indent; ++i)
    fputs("  ", Out);
}

HuffmanEncoder::Symbol::Symbol(size_t Id, WeightType Weight)
    : Node(NodeType::Symbol, Weight), Id(Id), Path(0), NumBits(0) {
  // TODO(karlschimpf): Remove hack of next assignment once compile-int
  // has been ported!
  Path = Id;
}

HuffmanEncoder::Symbol::~Symbol() {
}

int HuffmanEncoder::Symbol::compare(Node* Nd) const {
  int diff = Node::compare(Nd);
  if (diff != 0)
    return diff;
  assert(isa<Symbol>(Nd));
  Symbol* Sym = cast<Symbol>(Nd);
  if (Id < Sym->Id)
    return -1;
  if (Id > Sym->Id)
    return 1;
  return 0;
}

void HuffmanEncoder::Symbol::describe(FILE* Out, bool Brief, size_t Indent) {
  (void)Brief;
  indentTo(Out, Indent);
  fprintf(Out, "Sym(%" PRIuMAX " %" PRIuMAX "", uintmax_t(Id),
          uintmax_t(Weight));
  if (!Brief) {
    fprintf(Out, " 0x%" PRIxMAX ":%" PRIuMAX "", uintmax_t(Path),
            uintmax_t(NumBits));
  }
  fprintf(Out, ")\n");
}

HuffmanEncoder::NodePtr HuffmanEncoder::Symbol::installPaths(
    NodePtr Self,
    HuffmanEncoder& Encoder,
    PathType Path,
    unsigned NumBits) {
  if (NumBits > Encoder.getMaxPathLength())
    return NodePtr();
  this->Path = Path;
  this->NumBits = NumBits;
  return Self;
}

size_t HuffmanEncoder::Symbol::nodeSize() const {
  return 1;
}

HuffmanEncoder::Selector::Selector(size_t Id, NodePtr Kid1, NodePtr Kid2)
    : Node(NodeType::Selector, Kid1->getWeight() + Kid2->getWeight()),
      Id(Id),
      Kid1(Kid1),
      Kid2(Kid2),
      Size(Kid1->nodeSize() + Kid2->nodeSize()) {
}

HuffmanEncoder::Selector::~Selector() {
}

int HuffmanEncoder::Selector::compare(Node* Nd) const {
  int Diff = Node::compare(Nd);
  if (Diff != 0)
    return Diff;
  assert(isa<Selector>(Nd));
  Selector* Sel = cast<Selector>(Nd);
  if (Size < Sel->Size)
    return -1;
  if (Size > Sel->Size)
    return 1;
  if (Id < Sel->Id)
    return -1;
  if (Id > Sel->Id)
    return 1;
  return 0;
}

void HuffmanEncoder::Selector::describe(FILE* Out, bool Brief, size_t Indent) {
  indentTo(Out, Indent);
  fprintf(Out, "sel(%" PRIuMAX ")\n", uintmax_t(Id));
  if (Brief)
    return;
  ++Indent;
  Kid1->describe(Out, Brief, Indent);
  Kid2->describe(Out, Brief, Indent);
}

void HuffmanEncoder::Selector::fixFields() {
  Weight = Kid1->getWeight() + Kid2->getWeight();
  Size = Kid1->nodeSize() + Kid2->nodeSize();
}

HuffmanEncoder::NodePtr HuffmanEncoder::Selector::installPaths(
    NodePtr Self,
    HuffmanEncoder& Encoder,
    PathType Path,
    unsigned NumBits) {
  unsigned KidBits = NumBits + 1;
  for (int NumTries = 1; NumTries <= 2; NumTries++) {
    auto Sel = cast<Selector>(Self.get());
    NodePtr K1 = Sel->getKid1();
    K1 = K1->installPaths(K1, Encoder, Path, KidBits);
    NodePtr K2 = Sel->getKid2();
    K2 =
        K2->installPaths(K2, Encoder, Path | (PathType(1) << NumBits), KidBits);
    if (K1 && K2) {
      Sel->Kid1 = K1;
      Sel->Kid2 = K2;
      Sel->fixFields();
      return Self;
    }
    if (NumTries == 2)
      // Flattening did not work, fail to parent
      break;

    // Tree is too deep. See if flattened binary tree will fit.
    unsigned BitsNeeded = 0;
    size_t Count = Sel->nodeSize();
    while (Count > 0) {
      ++BitsNeeded;
      Count >>= 1;
    }

    if (NumBits + BitsNeeded > Encoder.getMaxPathLength())
      // Can't fix at this node, go to parent.
      break;

    // Can flatten tree at this depth. Flatten and assign paths.
    std::vector<NodePtr> Symbols;
    std::vector<NodePtr> ToVisit;
    ToVisit.push_back(Sel->Kid1);
    ToVisit.push_back(Sel->Kid2);
    while (!ToVisit.empty()) {
      NodePtr Nd = ToVisit.back();
      ToVisit.pop_back();
      if (isa<Symbol>(Nd.get())) {
        Symbols.push_back(Nd);
        continue;
      }
      assert(isa<Selector>(Nd.get()));
      auto Sel = cast<Selector>(Nd.get());
      ToVisit.push_back(Sel->getKid1());
      ToVisit.push_back(Sel->getKid2());
    }
    // Sort so that if tree is not full, nodes with maximal weight will be
    // on shorter paths.
    std::sort(Symbols.begin(), Symbols.end(), Encoder.getNodePtrLtFcn());
    std::vector<NodePtr>* Ply1 = &Symbols;
    std::vector<NodePtr>* Ply2 = &ToVisit;
    while (Ply1->size() > 1) {
      std::reverse(Ply1->begin(), Ply1->end());
      while (Ply1->size() >= 2) {
        NodePtr N1 = Ply1->back();
        Ply1->pop_back();
        NodePtr N2 = Ply1->back();
        Ply1->pop_back();
        Ply2->push_back(
            std::make_shared<Selector>(Encoder.getNextSelectorId(), N2, N1));
      }
      if (!Ply1->empty()) {
        Ply2->push_back(Ply1->back());
        Ply1->pop_back();
      }
      std::swap(Ply1, Ply2);
    }
    // Loop again to verify if flattening actually worked, as well
    // as update paths for symbols in subtree.
    Self = Ply1->front();
  }
  return NodePtr();
}

size_t HuffmanEncoder::Selector::nodeSize() const {
  return Size;
}

HuffmanEncoder::HuffmanEncoder()
    : MaxAllowedPath(MaxPathLength),
      NextSelectorId(0),
      NodePtrLtFcn(
          [](NodePtr N1, NodePtr N2) { return N1->compare(N2.get()) < 0; }) {
}

HuffmanEncoder::~HuffmanEncoder() {
}

void HuffmanEncoder::setMaxPathLength(unsigned NewSize) {
  assert(NewSize <= MaxPathLength);
  MaxAllowedPath = NewSize;
}

HuffmanEncoder::SymbolPtr HuffmanEncoder::createSymbol(WeightType Weight) {
  auto Sym = std::make_shared<Symbol>(Alphabet.size(), Weight);
  Alphabet.push_back(Sym);
  return Sym;
}

HuffmanEncoder::NodePtr HuffmanEncoder::getSymbol(size_t Id) const {
  assert(Id < Alphabet.size());
  return Alphabet.at(Id);
}

HuffmanEncoder::NodePtr HuffmanEncoder::encodeSymbols() {
  if (Alphabet.empty())
    return NodePtr();
  std::shared_ptr<heap<NodePtr>> Heap =
      std::make_shared<heap<NodePtr>>(getNodePtrLtFcn());
  for (NodePtr& Sym : Alphabet) {
    NodePtr Nd = Sym;
    auto V = Heap->push(Nd);
  }
  while (Heap->size() >= 2) {
    NodePtr N1 = Heap->top()->getValue();
    Heap->pop();
    NodePtr N2 = Heap->top()->getValue();
    Heap->pop();
    NodePtr Sel = std::make_shared<Selector>(getNextSelectorId(), N2, N1);
    Heap->push(Sel);
  }
  NodePtr Root = Heap->top()->getValue();
  Root = Root->installPaths(Root, *this, 0, 0);
  if (!Root)
    fatal("Can't build Huffman encoding for alphabet!");
  return Root;
}

}  // end of namespace utils

}  // end of namespace wasm
