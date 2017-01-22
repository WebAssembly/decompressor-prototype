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

// Defines a binary tree encoding huffman values.
//
// Initially, the alphabet is defined by creating instances of class Symbol.
// Each symbol in the alphabet is given a probability that is used to define the
// corresponding Huffman encoding for that value.
//
// Note: Probabilities are not used. Rather, weighted values are. The
// probability for any symbol is it's weigth divided by the sum of weights of
// all symbols in the alphabet.
//
// Note: This implementation limits the binary (Huffman) encodings to 64 bits.
// This is done to guarantee that each path can be represented as an integer.
// If necessary, symbols with path length greater than 64 bits are "balanced"
// with their parents until all paths meet the 64 bit limitation.
//
// In addition, to make sure that path values are unique, independent of the
// number of bits used for the encoding, they are encoded from leaf to root
// (i.e. the least significant bit always represents the first bit of the path).

#ifndef DECOMPRESSOR_SRc_UTILS_HEAP_H
#define DECOMPRESSOR_SRc_UTILS_HEAP_H

#include "utils/Defs.h"
#include "utils/Casting.h"

#include <vector>
#include <memory>

namespace wasm {

namespace utils {

class HuffmanEncoder : public std::enable_shared_from_this<HuffmanEncoder> {
 public:
  class Node;
  class Symbol;
  class Selector;
  typedef uint64_t PathType;
  typedef uint64_t WeightType;
  typedef std::shared_ptr<Node> NodePtr;
  typedef std::shared_ptr<Symbol> SymbolPtr;
  typedef std::shared_ptr<Selector> SelectorPtr;

  static constexpr unsigned MaxPathLength = sizeof(PathType) * CHAR_BIT;
  enum class NodeType { Selector, Symbol };

  // Node used to encode binary paths of Huffman encoding.
  class Node : public std::enable_shared_from_this<Node> {
    Node() = delete;
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    friend class HuffmanEncoder;

   public:
    explicit Node(NodeType Type, WeightType Weight, unsigned NumBits);
    virtual ~Node();
    WeightType getWeight() const { return Weight; }
    unsigned getNumBits() const { return NumBits; }

    NodeType getRtClassId() const { return Type; }

   protected:
    const NodeType Type;
    // The weight of all symbols in the subtree.
    WeightType Weight;
    // The number of bits needed to represent all paths in subtree.
    unsigned NumBits;
    // Note: Installs Huffman encoding values into leaves, based on path and
    // number of bits. Returns false if parent needs to rebalance because
    // it was unable to install with path limit.
    virtual NodePtr installPaths(NodePtr Self,
                                 PathType Path,
                                 unsigned NumBits) = 0;
    // Returns number of nodes in tree.
    virtual size_t nodeSize() const = 0;
  };

  // Defines a symbol in the alphabet being Huffman encoded.  Note: Only
  // construct using std::make_shared<>.
  class Symbol : public Node {
    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;

   public:
    // Note: Path and number of bits are not defined until installed.
    Symbol(WeightType Weight);
    ~Symbol() OVERRIDE;

    PathType getPath() const { return Path; }

    static bool implementsClass(NodeType Type) {
      return Type == NodeType::Symbol;
    }

   protected:
    NodePtr installPaths(NodePtr Self,
                         PathType Path,
                         unsigned NumBits) OVERRIDE;
    size_t nodeSize() const OVERRIDE;

   private:
    PathType Path;
  };

  // Defines (binary) selector tree node.
  class Selector : public Node {
    Selector() = delete;
    Selector(const Selector&) = delete;
    Selector& operator=(const Selector&) = delete;

   public:
    Selector(NodePtr Kid1, NodePtr Kid2);
    ~Selector() OVERRIDE;

    NodePtr getKid1() const { return Kid1; }
    NodePtr getKid2() const { return Kid2; }
    NodePtr getKid(unsigned i) const;

    static bool implementsClass(NodeType Type) {
      return Type == NodeType::Selector;
    }

   protected:
    void FixFields();
    NodePtr installPaths(NodePtr Self,
                         PathType Path,
                         unsigned NumBits) OVERRIDE;
    size_t nodeSize() const OVERRIDE;

   private:
    NodePtr Kid1;
    NodePtr Kid2;
    size_t Size;
  };

  HuffmanEncoder();
  ~HuffmanEncoder();

  // Add the given symbol to the alphabet to be encoded.
  void add(SymbolPtr Sym);

  // Define the Huffman encodings for each symbol in the alphabet.
  // Returns the root of the corresponding tree defining the encoded
  // symbols.
  NodePtr encodeSymbols();

 protected:
  std::vector<NodePtr> Alphabet;
};

}  // end of namespace utils

}  // end of namespace wasm

#endif  // DECOMPRESSOR_SRc_UTILS_HEAP_H
