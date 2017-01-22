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

// Define ordering over nodes, so that we can use a heap to extract smallest
// weighted subtrees.
bool operator<(HuffmanEncoder::NodePtr T1, HuffmanEncoder::NodePtr T2) {
  return T1->getWeight() < T2->getWeight();
}

HuffmanEncoder::Node::Node(NodeType Type, WeightType Weight)
    : Type(Type), Weight(Weight) {
}

HuffmanEncoder::Node::~Node() {
}

HuffmanEncoder::Symbol::Symbol(WeightType Weight)
    : Node(NodeType::Symbol, Weight), Path(0) {
}

HuffmanEncoder::Symbol::~Symbol() {
}

HuffmanEncoder::NodePtr HuffmanEncoder::Symbol::installPaths(NodePtr Self,
                                                             PathType Path,
                                                             unsigned NumBits) {
  if (NumBits > MaxPathLength)
    return NodePtr();
  this->Path = Path;
  this->NumBits = NumBits;
  return Self;
}

size_t HuffmanEncoder::Symbol::nodeSize() const {
  return 1;
}

HuffmanEncoder::Selector::Selector(NodePtr Kid1, NodePtr Kid2)
    : Node(NodeType::Selector, Kid1->getWeight() + Kid2->getWeight()),
      Kid1(Kid1),
      Kid2(Kid2),
      Size(Kid1->nodeSize() + Kid2->nodeSize()) {
}

void HuffmanEncoder::Selector::fixFields() {
  Weight = Kid1->getWeight() + Kid2->getWeight();
  Size = Kid1->nodeSize() + Kid2->nodeSize();
}

HuffmanEncoder::Selector::~Selector() {
}

HuffmanEncoder::NodePtr HuffmanEncoder::Selector::installPaths(
    NodePtr Self,
    PathType Path,
    unsigned NumBits) {
  PathType KidPath = Path << 1;
  unsigned KidBits = NumBits + 1;
  for (int NumTries = 1; NumTries <= 2; NumTries++) {
    assert(isa<Selector>(Self.get()));
    auto Sel = cast<Selector>(Self);
    NodePtr K1 = Sel->getKid1();
    K1 = K1->installPaths(K1, KidPath, KidBits);
    NodePtr K2 = Sel->getKid2();
    K2 = K2->installPaths(K2, KidPath + 1, KidBits);
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

    if (NumBits + BitsNeeded > MaxPathLength)
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
    std::vector<NodePtr>* Ply1 = &Symbols;
    std::vector<NodePtr>* Ply2 = &ToVisit;
    while (Ply1->size() > 1) {
      while (Ply1->size() >= 2) {
        NodePtr N1 = Ply1->back();
        Ply1->pop_back();
        NodePtr N2 = Ply1->back();
        Ply1->pop_back();
        Ply2->push_back(std::make_shared<Selector>(N1, N2));
      }
      if (!Ply1->empty()) {
        Ply2->push_back(Ply1->back());
        Ply1->pop_back();
      }
      std::reverse(Ply2->begin(), Ply2->end());
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

HuffmanEncoder::HuffmanEncoder() {
}

HuffmanEncoder::~HuffmanEncoder() {
}

void HuffmanEncoder::add(SymbolPtr Sym) {
  Alphabet.push_back(Sym);
}

HuffmanEncoder::NodePtr HuffmanEncoder::encodeSymbols() {
  if (Alphabet.empty())
    return NodePtr();
  heap<NodePtr> Heap((std::less<NodePtr>()));
  for (NodePtr& Sym : Alphabet)
    Heap.push(Sym);
  while (Heap.size() >= 2) {
    NodePtr N1 = Heap.top()->getValue();
    Heap.pop();
    NodePtr N2 = Heap.top()->getValue();
    Heap.pop();
    NodePtr Sel = std::make_shared<Selector>(N1, N2);
    Heap.push(Sel);
  }
  NodePtr Root = Heap.top()->getValue();
  Root = Root->installPaths(Root, 0, 0);
  if (!Root)
    fatal("Can't build Huffman encoding for alphabet!");
  return Root;
}

}  // end of namespace utils

}  // end of namespace wasm
