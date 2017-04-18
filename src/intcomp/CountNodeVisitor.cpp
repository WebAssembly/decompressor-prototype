// -*- C++ -*- */
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

// Implements a visitor class that visits nodes in a count node trie.

#include "intcomp/CountNodeVisitor.h"

namespace wasm {

namespace intcomp {

namespace {

const char* StateName[]{"enter", "visiting", "Exit"};

}  // end of anonymous namespace

const char* CountNodeVisitor::getName(State St) {
  return StateName[unsigned(St)];
}

CountNodeVisitor::Frame::~Frame() {}

void CountNodeVisitor::Frame::describe(FILE* Out) const {
  describePrefix(Out);
  fprintf(stderr, "  %" PRIuMAX "..%" PRIuMAX " [%" PRIuMAX "] %s\n",
          uintmax_t(FirstKid), uintmax_t(LastKid), uintmax_t(CurKid),
          getName(CurState));
  getNode()->describe(Out);
  describeSuffix(Out);
}

void CountNodeVisitor::Frame::describePrefix(FILE* Out) const {
  fputs("<frame", Out);
}

void CountNodeVisitor::Frame::describeSuffix(FILE* Out) const {
  fputs(">\n", Out);
}

CountNodeVisitor::CountNodeVisitor(CountNode::RootPtr Root) : Root(Root) {}

CountNodeVisitor::FramePtr CountNodeVisitor::getFrame(size_t FirstKid,
                                                      size_t LastKid) {
  return std::make_shared<Frame>(*this, FirstKid, LastKid);
}

CountNodeVisitor::FramePtr CountNodeVisitor::getFrame(CountNode::IntPtr Nd,
                                                      size_t FirstKid,
                                                      size_t LastKid) {
  return std::make_shared<Frame>(*this, Nd, FirstKid, LastKid);
}

void CountNodeVisitor::describe(FILE* Out) const {
  fprintf(stderr, "*** Stack ***\n");
  for (auto Frame : Stack) {
    Frame->describe(Out);
  }
  fprintf(stderr, "*************\n");
}

void CountNodeVisitor::walk() {
  callRoot();
  walkOther();
  while (!Stack.empty()) {
    FramePtr Frame = Stack.back();
    switch (Frame->CurState) {
      case State::Enter: {
        if (Frame->CurKid >= Frame->LastKid) {
          Frame->CurState = State::Visiting;
          break;
        }
        callNode(ToVisit[Frame->CurKid++]);
        break;
      }
      case State::Visiting: {
        Frame->CurState = State::Exit;
        visit(Frame);
        break;
      }
      case State::Exit: {
        while (ToVisit.size() > Frame->FirstKid)
          ToVisit.pop_back();
        Stack.pop_back();
        visitReturn(Frame);
        break;
      }
    }
  }
}

void CountNodeVisitor::walkOther() {
  CountNode::PtrVector L;
  Root->getOthers(L);
  for (CountNode::Ptr Nd : L)
    visitOther(Nd);
}

void CountNodeVisitor::callRoot() {
  size_t FirstKid = ToVisit.size();
  for (auto& Pair : *Root.get())
    ToVisit.push_back(Pair.second);
  Stack.push_back(getFrame(FirstKid, ToVisit.size()));
}

void CountNodeVisitor::callNode(CountNode::IntPtr Nd) {
  size_t FirstKid = ToVisit.size();
  for (auto& Pair : *Nd.get())
    ToVisit.push_back(Pair.second);
  Stack.push_back(getFrame(Nd, FirstKid, ToVisit.size()));
}

void CountNodeVisitor::visit(FramePtr Frame) {}

void CountNodeVisitor::visitReturn(FramePtr Frame) {}

void CountNodeVisitor::visitOther(CountNode::Ptr Nd) {}

}  // end of namespace intcomp

}  // end of namespace wasm
