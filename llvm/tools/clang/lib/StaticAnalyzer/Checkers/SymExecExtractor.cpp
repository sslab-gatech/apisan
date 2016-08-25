//== SymExecExtractor.cpp --------------------------------------------*- C++ -*--==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines SymExecExtractor, which prints out path conditions
// for each return code.
//===----------------------------------------------------------------------===//

#include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramStateTrait.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AsStmtPrinter.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"
#include <boost/algorithm/string/replace.hpp>

using namespace clang;
using namespace ento;

#define OP_CONSTRAINT "@="

typedef SmallVector<ExplodedNode*, 16> ExplodedNodeVectorTy;

namespace {
class SymExecEvent {
public:
  enum Kind {
    FN_CALL,
    ASSUME,
    EOP
  };

  SymExecEvent(Kind k);
  SymExecEvent(Kind k, const Stmt* s, CheckerContext &C);
  SymExecEvent(Kind k, std::string serialized);
  SymExecEvent(Kind k, SVal cond, bool assumption, const Stmt* s, CheckerContext &C);

  void Profile(llvm::FoldingSetNodeID &ID) const;
  std::string getAsString() const;
  std::string getKindAsXMLNode() const;
  std::string getCodeAsXMLNode() const;

private:
  Kind K;
  std::string Code;
  // For condition event
  std::string SV;
};

class SymExecExtractor : public Checker< eval::Assume,
                                         check::PostStmt<CallExpr>,
                                         check::EndFunction,
                                         check::EndAnalysis > {
public:
  SymExecExtractor();
  ProgramStateRef evalAssume(ProgramStateRef State,
                                 SVal Cond,
                                 bool Assumption) const;
  void checkPostStmt(const CallExpr *CE, CheckerContext &C) const;
  void checkEndFunction(CheckerContext &C) const;
  void checkEndAnalysis(ExplodedGraph &G, BugReporter &BR, ExprEngine &N) const;

private:
  // Bug type
  std::unique_ptr<BugType> SymExecExtractorReportType;
  mutable IdentifierInfo *II___builtin_expect;
  mutable std::string TypeInfo;
private:
  bool isInBlackList(CheckerContext &C, const FunctionDecl *FD) const;
};
} // end anonymous namespace

REGISTER_LIST_WITH_PROGRAMSTATE(EventList, SymExecEvent)

static std::string encodeToXML(std::string XML) {
  boost::replace_all(XML, "&", "&amp;");
  boost::replace_all(XML, ">", "&gt;");
  boost::replace_all(XML, "<", "&lt;");
  return XML;
}

static std::string getCodeAsString(CheckerContext &C, const Stmt *S) {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  S->getLocStart().printWithoutColumn(OS, C.getSourceManager());
  return OS.str();
}

std::string getCond(ProgramStateRef State, SymbolRef Symbol) {
  std::string Cond;
  llvm::raw_string_ostream CS(Cond);
  ProgramStateManager &Mgr = State->getStateManager();
  ConstraintManager &ConstMgr = Mgr.getConstraintManager();
  ConstMgr.printSymbolCond(State, Symbol, CS);
  CS.flush();

  if (!Cond.empty()) {
    std::string Result;
    llvm::raw_string_ostream RS(Result);
    // format : Symbol OP_CONSTRAINT Cond
    Symbol->dumpToStream(RS);
    RS << OP_CONSTRAINT << Cond;
    return RS.str();
  }
  else
    return std::string();
}

static void dumpTree(llvm::raw_ostream &OS,
                        ExplodedNodeVectorTy Nodes,
                        ExplodedNode *Cur,
                        ExplodedNode *Prev = nullptr,
                        unsigned indent = 0) {
  bool valid = false;

  // memoization
  for (ExplodedNodeVectorTy::iterator I = Nodes.begin(), E = Nodes.end();
      I != E; ++I) {
    if ((*I) == Cur)
      return;
  }

  Nodes.push_back(Cur);

  const EventListTy CurEvents  = Cur->getState()->get<EventList>();
  if (!CurEvents.isEmpty()) {
    if (Prev) {
      const EventListTy PrevEvents  = Prev->getState()->get<EventList>();
      if (!PrevEvents.isEqual(CurEvents))
        valid = true;
    }
    else
      valid = true;
  }

  if (valid) {
    indent += 1;
    OS << "<NODE>\n"
       << "<EVENT>\n"
       << CurEvents.getInternalPointer()->getHead().getAsString()
       << "\n" << "</EVENT>\n";
  }

  for (ExplodedNode::succ_iterator I = Cur->succ_begin(), E = Cur->succ_end();
      I != E; ++I) {
    dumpTree(OS, Nodes, (*I), Cur, indent);
  }

  if (valid)
    OS << "</NODE>\n";

  Nodes.pop_back();
}

// SymExecEvent
SymExecEvent::SymExecEvent(Kind k) : K(k) {}

SymExecEvent::SymExecEvent(Kind k, const Stmt* s, CheckerContext &C)
  : K(k), SV() {
    Code = getCodeAsString(C, s);

    switch (K) {
      case FN_CALL: {
        std::string Result;
        llvm::raw_string_ostream OS(Result);
        const CallExpr *CE = dyn_cast<CallExpr>(s);
        const SymExpr *SE = C.getSVal(CE).getAsSymbol(true);
        if (SE) {
          SE->dumpToStream(OS);
          SV = OS.str();
        }
        else {
          assert(CE != nullptr);
          AsStmtPrinter Printer(OS, C.getLocationContext(), C.getState(), 0, true);
          Printer.Visit(const_cast<CallExpr*>(CE));
          SV = OS.str();
        }
        break;
      }
      default:
        break;
    }
}

SymExecEvent::SymExecEvent(Kind k, std::string serialized)
  : K(k) {
    SV = serialized;
}

std::string SymExecEvent::getKindAsXMLNode() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  OS << "<KIND>";

  switch (K){
    case FN_CALL:
      OS << "@LOG_CALL";
      break;
    case ASSUME:
      OS << "@LOG_ASSUME";
      break;
    case EOP:
      OS << "@LOG_EOP";
      break;
  }

  OS << "</KIND>";
  return OS.str();
}

std::string SymExecEvent::getCodeAsXMLNode() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  OS << "<CODE>" << Code << "</CODE>";
  return OS.str();
}

std::string SymExecEvent::getAsString() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  LangOptions LO;

  OS << getKindAsXMLNode();

  switch (K) {
    case FN_CALL:
      OS << getCodeAsXMLNode();
      OS << "<CALL>" << encodeToXML(SV) << "</CALL>";
      break;

    case ASSUME:
      OS << "<COND>" << encodeToXML(SV) << "</COND>";
      break;

    case EOP:
      break;

      llvm_unreachable("Unexpected symbolic execution event kind");
  }

  return OS.str();
}

void SymExecEvent::Profile(llvm::FoldingSetNodeID &ID) const {
  ID.AddInteger(K);
  ID.AddString(SV);
}

// SymExecExtractor
SymExecExtractor::SymExecExtractor() : II___builtin_expect(nullptr) {
  SymExecExtractorReportType.reset(
      new BugType(this,
        "Return symbolic execution abstractions",
        "Symbolic execution extractor"));
}

ProgramStateRef SymExecExtractor::evalAssume(ProgramStateRef State,
    SVal Cond,
    bool Assumption) const {
  if (SymbolRef S = Cond.getAsSymbol()) {
    if (const SymIntExpr *SIE = dyn_cast<SymIntExpr>(S)) {
      std::string serialized = getCond(State, SIE->getLHS());
      if (!serialized.empty()) {
        ProgramStateRef NewState = State->add<EventList>(SymExecEvent(SymExecEvent::ASSUME, serialized));
        return NewState;
      }
    }
  }
  return State;
}

void SymExecExtractor::checkPostStmt(const CallExpr *CE,
                                     CheckerContext &C) const {
  if (isInBlackList(C, C.getCalleeDecl(CE)))
    return;

  ProgramStateRef State = C.getState();
  ProgramStateRef NewState = State->add<EventList>(SymExecEvent(SymExecEvent::FN_CALL, CE, C));
  C.addTransition(NewState);
}

void SymExecExtractor::checkEndFunction(CheckerContext &C) const {
  if (!C.getLocationContext()->inTopFrame())
    return;

  ProgramStateRef State = C.getState();
  ProgramStateRef NewState = State->add<EventList>(SymExecEvent(SymExecEvent::EOP));
  C.addTransition(NewState);
}

void SymExecExtractor::checkEndAnalysis(ExplodedGraph &G,
                                         BugReporter &BR,
                                         ExprEngine &N) const {
  const ExplodedNode *GraphRoot = *G.roots_begin();
  const LocationContext *LC = GraphRoot->getLocation().getLocationContext();
  const Decl *D = LC->getDecl();
  const SourceManager &SM = BR.getSourceManager();

  std::string Report;
  llvm::raw_string_ostream OS(Report);
  ExplodedNodeVectorTy Nodes;

  OS << "\n@SYM_EXEC_EXTRACTOR_BEGIN\n";
  for (ExplodedGraph::roots_iterator I = G.roots_begin(), E = G.roots_end();
      I != E; ++I) {
    OS << "<TREE>\n";
    dumpTree(OS, Nodes, (*I));
    OS << "</TREE>\n";
  }
  OS << "\n@SYM_EXEC_EXTRACTOR_END\n";

  BugReport *R = new BugReport(*SymExecExtractorReportType, OS.str(),
                                PathDiagnosticLocation(D, SM));
  llvm::errs() << "###: " << OS.str() << "\n";
  BR.emitReport(R);
}

bool SymExecExtractor::isInBlackList(CheckerContext &C,
    const FunctionDecl *FD) const {
  if (!FD) return false;

  const IdentifierInfo *II = FD->getIdentifier();
  ASTContext &Ctx = C.getASTContext();

  if (!II___builtin_expect)
    II___builtin_expect = &Ctx.Idents.get("__builtin_expect");
  if (II___builtin_expect == II)
    return true;

  return false;
}

void ento::registerSymExecExtractor(CheckerManager &mgr) {
  mgr.registerChecker<SymExecExtractor>();
}
