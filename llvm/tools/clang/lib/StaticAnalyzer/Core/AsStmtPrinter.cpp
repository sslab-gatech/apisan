//===--- AsStmtPrinter.cpp - Printing implementation for Stmt ASTs ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Stmt::dumpPretty/Stmt::printPretty methods, which
// pretty print the AST back out to C code.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Core/PathSensitive/AsStmtPrinter.h"

using namespace clang;
using namespace ento;

#define TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(__x) do {       \
    const Stmt *__s = dyn_cast<Stmt>(__x);              \
    if (__s != nullptr && tryToEvalSymExprOrSVal(__s))  \
      return;                                           \
  } while(0)

bool AsStmtPrinter::tryToEvalSymExprOrSVal(const Stmt *S) {
  const Expr *E = dyn_cast<const Expr>(S);
  if (E == nullptr)
    return false;

  SVal SV = PS->getSVal(S, LCtx);
  if (SV.isUnknownOrUndef())
    return false;

  if (IsLValue) {
    const SymExpr *SE = SV.getAsSymExpr();
    if (SE) {
      if (dyn_cast<SymbolConjured>(SE))
        return false;
      ++Level;
      SE->dumpToStream(OS, Level);
      --Level;
      return true;
    }
  }

#ifndef FSS_DISABLE_ADOHC_WORKAROUND_FOR_CLANG_BUG
    if (SV.getBaseKind() == SVal::LocKind &&
        SV.getSubKind() == loc::MemRegionKind)
      return false;
#endif
    ++Level;
    SV.dumpToStream(OS, Level);
    --Level;
  return true;
}

// API_SANITIZER
void AsStmtPrinter::PrintCallee(raw_ostream &OS,
                                          CheckerContext&C, const CallExpr* CE) {
  AsStmtPrinter P(OS, C.getLocationContext(), C.getState(), 0, true);
  P.Visit(const_cast<Expr*>(CE->getCallee()));
}

//===----------------------------------------------------------------------===//
//  Stmt printing methods.
//===----------------------------------------------------------------------===//

/// PrintRawCompoundStmt - Print a compound stmt without indenting the {, and
/// with no newline after the }.
void AsStmtPrinter::PrintRawCompoundStmt(CompoundStmt *Node) {
  OS << "{\n";
  for (auto *I : Node->body())
    PrintStmt(I);

  Indent() << "}";
}

void AsStmtPrinter::PrintRawDecl(Decl *D) {
  D->print(OS, Policy, IndentLevel);
}

void AsStmtPrinter::PrintRawDeclStmt(const DeclStmt *S) {
  SmallVector<Decl*, 2> Decls(S->decls());
  Decl::printGroup(Decls.data(), Decls.size(), OS, Policy, IndentLevel);
}

void AsStmtPrinter::VisitNullStmt(NullStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << ";\n";
}

void AsStmtPrinter::VisitDeclStmt(DeclStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent();
  PrintRawDeclStmt(Node);
  OS << ";\n";
}

void AsStmtPrinter::VisitCompoundStmt(CompoundStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent();
  PrintRawCompoundStmt(Node);
  OS << "\n";
}

void AsStmtPrinter::VisitCaseStmt(CaseStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent(-1) << "case ";
  PrintExpr(Node->getLHS());
  if (Node->getRHS()) {
    OS << " ... ";
    PrintExpr(Node->getRHS());
  }
  OS << ":\n";

  PrintStmt(Node->getSubStmt(), 0);
}

void AsStmtPrinter::VisitDefaultStmt(DefaultStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);


  Indent(-1) << "default:\n";
  PrintStmt(Node->getSubStmt(), 0);
}

void AsStmtPrinter::VisitLabelStmt(LabelStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);


  Indent(-1) << Node->getName() << ":\n";
  PrintStmt(Node->getSubStmt(), 0);
}

void AsStmtPrinter::VisitAttributedStmt(AttributedStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);


  for (const auto *Attr : Node->getAttrs()) {
    Attr->printPretty(OS, Policy);
  }

  PrintStmt(Node->getSubStmt(), 0);
}

void AsStmtPrinter::PrintRawIfStmt(IfStmt *If) {
  OS << "if (";
  if (const DeclStmt *DS = If->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(If->getCond());
  OS << ')';

  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(If->getThen())) {
    OS << ' ';
    PrintRawCompoundStmt(CS);
    OS << (If->getElse() ? ' ' : '\n');
  } else {
    OS << '\n';
    PrintStmt(If->getThen());
    if (If->getElse()) Indent();
  }

  if (Stmt *Else = If->getElse()) {
    OS << "else";

    if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Else)) {
      OS << ' ';
      PrintRawCompoundStmt(CS);
      OS << '\n';
    } else if (IfStmt *ElseIf = dyn_cast<IfStmt>(Else)) {
      OS << ' ';
      PrintRawIfStmt(ElseIf);
    } else {
      OS << '\n';
      PrintStmt(If->getElse());
    }
  }
}

void AsStmtPrinter::VisitIfStmt(IfStmt *If) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(If);

  Indent();
  PrintRawIfStmt(If);
}

void AsStmtPrinter::VisitSwitchStmt(SwitchStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "switch (";
  if (const DeclStmt *DS = Node->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(Node->getCond());
  OS << ")";

  // Pretty print compoundstmt bodies (very common).
  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    OS << " ";
    PrintRawCompoundStmt(CS);
    OS << "\n";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
  }
}

void AsStmtPrinter::VisitWhileStmt(WhileStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "while (";
  if (const DeclStmt *DS = Node->getConditionVariableDeclStmt())
    PrintRawDeclStmt(DS);
  else
    PrintExpr(Node->getCond());
  OS << ")\n";
  PrintStmt(Node->getBody());
}

void AsStmtPrinter::VisitDoStmt(DoStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "do ";
  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    PrintRawCompoundStmt(CS);
    OS << " ";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
    Indent();
  }

  OS << "while (";
  PrintExpr(Node->getCond());
  OS << ");\n";
}

void AsStmtPrinter::VisitForStmt(ForStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "for (";
  if (Node->getInit()) {
    if (DeclStmt *DS = dyn_cast<DeclStmt>(Node->getInit()))
      PrintRawDeclStmt(DS);
    else
      PrintExpr(cast<Expr>(Node->getInit()));
  }
  OS << ";";
  if (Node->getCond()) {
    OS << " ";
    PrintExpr(Node->getCond());
  }
  OS << ";";
  if (Node->getInc()) {
    OS << " ";
    PrintExpr(Node->getInc());
  }
  OS << ") ";

  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    PrintRawCompoundStmt(CS);
    OS << "\n";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
  }
}

void AsStmtPrinter::VisitObjCForCollectionStmt(ObjCForCollectionStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "for (";
  if (DeclStmt *DS = dyn_cast<DeclStmt>(Node->getElement()))
    PrintRawDeclStmt(DS);
  else
    PrintExpr(cast<Expr>(Node->getElement()));
  OS << " in ";
  PrintExpr(Node->getCollection());
  OS << ") ";

  if (CompoundStmt *CS = dyn_cast<CompoundStmt>(Node->getBody())) {
    PrintRawCompoundStmt(CS);
    OS << "\n";
  } else {
    OS << "\n";
    PrintStmt(Node->getBody());
  }
}

void AsStmtPrinter::VisitCXXForRangeStmt(CXXForRangeStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "for (";
  PrintingPolicy SubPolicy(Policy);
  SubPolicy.SuppressInitializers = true;
  Node->getLoopVariable()->print(OS, SubPolicy, IndentLevel);
  OS << " : ";
  PrintExpr(Node->getRangeInit());
  OS << ") {\n";
  PrintStmt(Node->getBody());
  Indent() << "}";
  if (Policy.IncludeNewlines) OS << "\n";
}

void AsStmtPrinter::VisitMSDependentExistsStmt(MSDependentExistsStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent();
  if (Node->isIfExists())
    OS << "__if_exists (";
  else
    OS << "__if_not_exists (";

  if (NestedNameSpecifier *Qualifier
      = Node->getQualifierLoc().getNestedNameSpecifier())
    Qualifier->print(OS, Policy);

  OS << Node->getNameInfo() << ") ";

  PrintRawCompoundStmt(Node->getSubStmt());
}

void AsStmtPrinter::VisitGotoStmt(GotoStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "goto " << Node->getLabel()->getName() << ";";
  if (Policy.IncludeNewlines) OS << "\n";
}

void AsStmtPrinter::VisitIndirectGotoStmt(IndirectGotoStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "goto *";
  PrintExpr(Node->getTarget());
  OS << ";";
  if (Policy.IncludeNewlines) OS << "\n";
}

void AsStmtPrinter::VisitContinueStmt(ContinueStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "continue;";
  if (Policy.IncludeNewlines) OS << "\n";
}

void AsStmtPrinter::VisitBreakStmt(BreakStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "break;";
  if (Policy.IncludeNewlines) OS << "\n";
}


void AsStmtPrinter::VisitReturnStmt(ReturnStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "return";
  if (Node->getRetValue()) {
    OS << " ";
    PrintExpr(Node->getRetValue());
  }
  OS << ";";
  if (Policy.IncludeNewlines) OS << "\n";
}


void AsStmtPrinter::VisitGCCAsmStmt(GCCAsmStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "asm ";

  if (Node->isVolatile())
    OS << "volatile ";

  OS << "(";
  VisitStringLiteral(Node->getAsmString());

  // Outputs
  if (Node->getNumOutputs() != 0 || Node->getNumInputs() != 0 ||
      Node->getNumClobbers() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumOutputs(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    if (!Node->getOutputName(i).empty()) {
      OS << '[';
      OS << Node->getOutputName(i);
      OS << "] ";
    }

    VisitStringLiteral(Node->getOutputConstraintLiteral(i));
    OS << " ";
    Visit(Node->getOutputExpr(i));
  }

  // Inputs
  if (Node->getNumInputs() != 0 || Node->getNumClobbers() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumInputs(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    if (!Node->getInputName(i).empty()) {
      OS << '[';
      OS << Node->getInputName(i);
      OS << "] ";
    }

    VisitStringLiteral(Node->getInputConstraintLiteral(i));
    OS << " ";
    Visit(Node->getInputExpr(i));
  }

  // Clobbers
  if (Node->getNumClobbers() != 0)
    OS << " : ";

  for (unsigned i = 0, e = Node->getNumClobbers(); i != e; ++i) {
    if (i != 0)
      OS << ", ";

    VisitStringLiteral(Node->getClobberStringLiteral(i));
  }

  OS << ");";
  if (Policy.IncludeNewlines) OS << "\n";
}

void AsStmtPrinter::VisitMSAsmStmt(MSAsmStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  // FIXME: Implement MS style inline asm statement printer.
  Indent() << "__asm ";
  if (Node->hasBraces())
    OS << "{\n";
  OS << Node->getAsmString() << "\n";
  if (Node->hasBraces())
    Indent() << "}\n";
}

void AsStmtPrinter::VisitCapturedStmt(CapturedStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintStmt(Node->getCapturedDecl()->getBody());
}

void AsStmtPrinter::VisitObjCAtTryStmt(ObjCAtTryStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "@try";
  if (CompoundStmt *TS = dyn_cast<CompoundStmt>(Node->getTryBody())) {
    PrintRawCompoundStmt(TS);
    OS << "\n";
  }

  for (unsigned I = 0, N = Node->getNumCatchStmts(); I != N; ++I) {
    ObjCAtCatchStmt *catchStmt = Node->getCatchStmt(I);
    Indent() << "@catch(";
    if (catchStmt->getCatchParamDecl()) {
      if (Decl *DS = catchStmt->getCatchParamDecl())
        PrintRawDecl(DS);
    }
    OS << ")";
    if (CompoundStmt *CS = dyn_cast<CompoundStmt>(catchStmt->getCatchBody())) {
      PrintRawCompoundStmt(CS);
      OS << "\n";
    }
  }

  if (ObjCAtFinallyStmt *FS = static_cast<ObjCAtFinallyStmt *>(
        Node->getFinallyStmt())) {
    Indent() << "@finally";
    PrintRawCompoundStmt(dyn_cast<CompoundStmt>(FS->getFinallyBody()));
    OS << "\n";
  }
}

void AsStmtPrinter::VisitObjCAtFinallyStmt(ObjCAtFinallyStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

}

void AsStmtPrinter::VisitObjCAtCatchStmt (ObjCAtCatchStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "@catch (...) { /* todo */ } \n";
}

void AsStmtPrinter::VisitObjCAtThrowStmt(ObjCAtThrowStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "@throw";
  if (Node->getThrowExpr()) {
    OS << " ";
    PrintExpr(Node->getThrowExpr());
  }
  OS << ";\n";
}

void AsStmtPrinter::VisitObjCAtSynchronizedStmt(ObjCAtSynchronizedStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "@synchronized (";
  PrintExpr(Node->getSynchExpr());
  OS << ")";
  PrintRawCompoundStmt(Node->getSynchBody());
  OS << "\n";
}

void AsStmtPrinter::VisitObjCAutoreleasePoolStmt(ObjCAutoreleasePoolStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "@autoreleasepool";
  PrintRawCompoundStmt(dyn_cast<CompoundStmt>(Node->getSubStmt()));
  OS << "\n";
}

void AsStmtPrinter::PrintRawCXXCatchStmt(CXXCatchStmt *Node) {
  OS << "catch (";
  if (Decl *ExDecl = Node->getExceptionDecl())
    PrintRawDecl(ExDecl);
  else
    OS << "...";
  OS << ") ";
  PrintRawCompoundStmt(cast<CompoundStmt>(Node->getHandlerBlock()));
}

void AsStmtPrinter::VisitCXXCatchStmt(CXXCatchStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent();
  PrintRawCXXCatchStmt(Node);
  OS << "\n";
}

void AsStmtPrinter::VisitCXXTryStmt(CXXTryStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "try ";
  PrintRawCompoundStmt(Node->getTryBlock());
  for (unsigned i = 0, e = Node->getNumHandlers(); i < e; ++i) {
    OS << " ";
    PrintRawCXXCatchStmt(Node->getHandler(i));
  }
  OS << "\n";
}

void AsStmtPrinter::VisitSEHTryStmt(SEHTryStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << (Node->getIsCXXTry() ? "try " : "__try ");
  PrintRawCompoundStmt(Node->getTryBlock());
  SEHExceptStmt *E = Node->getExceptHandler();
  SEHFinallyStmt *F = Node->getFinallyHandler();
  if(E)
    PrintRawSEHExceptHandler(E);
  else {
    assert(F && "Must have a finally block...");
    PrintRawSEHFinallyStmt(F);
  }
  OS << "\n";
}

void AsStmtPrinter::PrintRawSEHFinallyStmt(SEHFinallyStmt *Node) {
  OS << "__finally ";
  PrintRawCompoundStmt(Node->getBlock());
  OS << "\n";
}

void AsStmtPrinter::PrintRawSEHExceptHandler(SEHExceptStmt *Node) {
  OS << "__except (";
  VisitExpr(Node->getFilterExpr());
  OS << ")\n";
  PrintRawCompoundStmt(Node->getBlock());
  OS << "\n";
}

void AsStmtPrinter::VisitSEHExceptStmt(SEHExceptStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent();
  PrintRawSEHExceptHandler(Node);
  OS << "\n";
}

void AsStmtPrinter::VisitSEHFinallyStmt(SEHFinallyStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent();
  PrintRawSEHFinallyStmt(Node);
  OS << "\n";
}

void AsStmtPrinter::VisitSEHLeaveStmt(SEHLeaveStmt *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "__leave;";
  if (Policy.IncludeNewlines) OS << "\n";
}

//===----------------------------------------------------------------------===//
//  OpenMP clauses printing methods
//===----------------------------------------------------------------------===//

namespace {
class OMPClausePrinter : public OMPClauseVisitor<OMPClausePrinter> {
  raw_ostream &OS;
  const PrintingPolicy &Policy;
  /// \brief Process clauses with list of variables.
  template <typename T>
  void VisitOMPClauseList(T *Node, char StartSym);
public:
  OMPClausePrinter(raw_ostream &OS, const PrintingPolicy &Policy)
    : OS(OS), Policy(Policy) { }
#define OPENMP_CLAUSE(Name, Class)              \
  void Visit##Class(Class *S);
#include "clang/Basic/OpenMPKinds.def"
};

void OMPClausePrinter::VisitOMPIfClause(OMPIfClause *Node) {
  OS << "if(";
  Node->getCondition()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPFinalClause(OMPFinalClause *Node) {
  OS << "final(";
  Node->getCondition()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPNumThreadsClause(OMPNumThreadsClause *Node) {
  OS << "num_threads(";
  Node->getNumThreads()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPSafelenClause(OMPSafelenClause *Node) {
  OS << "safelen(";
  Node->getSafelen()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPCollapseClause(OMPCollapseClause *Node) {
  OS << "collapse(";
  Node->getNumForLoops()->printPretty(OS, nullptr, Policy, 0);
  OS << ")";
}

void OMPClausePrinter::VisitOMPDefaultClause(OMPDefaultClause *Node) {
  OS << "default("
     << getOpenMPSimpleClauseTypeName(OMPC_default, Node->getDefaultKind())
     << ")";
}

void OMPClausePrinter::VisitOMPProcBindClause(OMPProcBindClause *Node) {
  OS << "proc_bind("
     << getOpenMPSimpleClauseTypeName(OMPC_proc_bind, Node->getProcBindKind())
     << ")";
}

void OMPClausePrinter::VisitOMPScheduleClause(OMPScheduleClause *Node) {
  OS << "schedule("
     << getOpenMPSimpleClauseTypeName(OMPC_schedule, Node->getScheduleKind());
  if (Node->getChunkSize()) {
    OS << ", ";
    Node->getChunkSize()->printPretty(OS, nullptr, Policy);
  }
  OS << ")";
}

void OMPClausePrinter::VisitOMPOrderedClause(OMPOrderedClause *) {
  OS << "ordered";
}

void OMPClausePrinter::VisitOMPNowaitClause(OMPNowaitClause *) {
  OS << "nowait";
}

void OMPClausePrinter::VisitOMPUntiedClause(OMPUntiedClause *) {
  OS << "untied";
}

void OMPClausePrinter::VisitOMPMergeableClause(OMPMergeableClause *) {
  OS << "mergeable";
}

void OMPClausePrinter::VisitOMPReadClause(OMPReadClause *) {
  OS << "read";
}


void OMPClausePrinter::VisitOMPWriteClause(OMPWriteClause *) {
  OS << "write";
}

void OMPClausePrinter::VisitOMPUpdateClause(OMPUpdateClause *) {
  OS << "update";
}

void OMPClausePrinter::VisitOMPCaptureClause(OMPCaptureClause *) {
  OS << "capture";

}

void OMPClausePrinter::VisitOMPSeqCstClause(OMPSeqCstClause *) {
  OS << "seq_cst";
}

template<typename T>
void OMPClausePrinter::VisitOMPClauseList(T *Node, char StartSym) {
  for (typename T::varlist_iterator I = Node->varlist_begin(),
         E = Node->varlist_end();
       I != E; ++I) {
    assert(*I && "Expected non-null Stmt");
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*I)) {
      OS << (I == Node->varlist_begin() ? StartSym : ',');
      cast<NamedDecl>(DRE->getDecl())->printQualifiedName(OS);
    } else {
      OS << (I == Node->varlist_begin() ? StartSym : ',');
      (*I)->printPretty(OS, nullptr, Policy, 0);
    }
  }
}

void OMPClausePrinter::VisitOMPPrivateClause(OMPPrivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "private";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPFirstprivateClause(OMPFirstprivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "firstprivate";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPLastprivateClause(OMPLastprivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "lastprivate";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPSharedClause(OMPSharedClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "shared";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPReductionClause(OMPReductionClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "reduction(";
    NestedNameSpecifier *QualifierLoc =
      Node->getQualifierLoc().getNestedNameSpecifier();
    OverloadedOperatorKind OOK =
      Node->getNameInfo().getName().getCXXOverloadedOperator();
    if (QualifierLoc == nullptr && OOK != OO_None) {
      // Print reduction identifier in C format
      OS << getOperatorSpelling(OOK);
    } else {
      // Use C++ format
      if (QualifierLoc != nullptr)
        QualifierLoc->print(OS, Policy);
      OS << Node->getNameInfo();
    }
    OS << ":";
    VisitOMPClauseList(Node, ' ');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPLinearClause(OMPLinearClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "linear";
    VisitOMPClauseList(Node, '(');
    if (Node->getStep() != nullptr) {
      OS << ": ";
      Node->getStep()->printPretty(OS, nullptr, Policy, 0);
    }
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPAlignedClause(OMPAlignedClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "aligned";
    VisitOMPClauseList(Node, '(');
    if (Node->getAlignment() != nullptr) {
      OS << ": ";
      Node->getAlignment()->printPretty(OS, nullptr, Policy, 0);
    }
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPCopyinClause(OMPCopyinClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "copyin";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPCopyprivateClause(OMPCopyprivateClause *Node) {
  if (!Node->varlist_empty()) {
    OS << "copyprivate";
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}

void OMPClausePrinter::VisitOMPFlushClause(OMPFlushClause *Node) {
  if (!Node->varlist_empty()) {
    VisitOMPClauseList(Node, '(');
    OS << ")";
  }
}
}

//===----------------------------------------------------------------------===//
//  OpenMP directives printing methods
//===----------------------------------------------------------------------===//

void AsStmtPrinter::PrintOMPExecutableDirective(OMPExecutableDirective *S) {
  OMPClausePrinter Printer(OS, Policy);
  ArrayRef<OMPClause *> Clauses = S->clauses();
  for (ArrayRef<OMPClause *>::iterator I = Clauses.begin(), E = Clauses.end();
       I != E; ++I)
    if (*I && !(*I)->isImplicit()) {
      Printer.Visit(*I);
      OS << ' ';
    }
  OS << "\n";
  if (S->hasAssociatedStmt() && S->getAssociatedStmt()) {
    assert(isa<CapturedStmt>(S->getAssociatedStmt()) &&
           "Expected captured statement!");
    Stmt *CS = cast<CapturedStmt>(S->getAssociatedStmt())->getCapturedStmt();
    PrintStmt(CS);
  }
}

void AsStmtPrinter::VisitOMPParallelDirective(OMPParallelDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp parallel ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPSimdDirective(OMPSimdDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp simd ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPForDirective(OMPForDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp for ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPForSimdDirective(OMPForSimdDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp for simd ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPSectionsDirective(OMPSectionsDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp sections ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPSectionDirective(OMPSectionDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp section";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPSingleDirective(OMPSingleDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp single ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPMasterDirective(OMPMasterDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp master";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPCriticalDirective(OMPCriticalDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp critical";
  if (Node->getDirectiveName().getName()) {
    OS << " (";
    Node->getDirectiveName().printName(OS);
    OS << ")";
  }
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPParallelForDirective(OMPParallelForDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp parallel for ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPParallelForSimdDirective(
  OMPParallelForSimdDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp parallel for simd ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPParallelSectionsDirective(
  OMPParallelSectionsDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp parallel sections ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPTaskDirective(OMPTaskDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp task ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPTaskyieldDirective(OMPTaskyieldDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp taskyield";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPBarrierDirective(OMPBarrierDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp barrier";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPTaskwaitDirective(OMPTaskwaitDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp taskwait";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPFlushDirective(OMPFlushDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp flush ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPOrderedDirective(OMPOrderedDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp ordered";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPAtomicDirective(OMPAtomicDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp atomic ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPTargetDirective(OMPTargetDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp target ";
  PrintOMPExecutableDirective(Node);
}

void AsStmtPrinter::VisitOMPTeamsDirective(OMPTeamsDirective *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Indent() << "#pragma omp teams ";
  PrintOMPExecutableDirective(Node);
}

//===----------------------------------------------------------------------===//
//  Expr printing methods.
//===----------------------------------------------------------------------===//

void AsStmtPrinter::VisitDeclRefExpr(DeclRefExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
      OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void AsStmtPrinter::VisitDependentScopeDeclRefExpr(
  DependentScopeDeclRefExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
      OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void AsStmtPrinter::VisitUnresolvedLookupExpr(UnresolvedLookupExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (Node->getQualifier())
    Node->getQualifier()->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
      OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void AsStmtPrinter::VisitObjCIvarRefExpr(ObjCIvarRefExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (Node->getBase()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  OS << *Node->getDecl();
}

void AsStmtPrinter::VisitObjCPropertyRefExpr(ObjCPropertyRefExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (Node->isSuperReceiver())
    OS << "super.";
  else if (Node->isObjectReceiver() && Node->getBase()) {
    PrintExpr(Node->getBase());
    OS << ".";
  } else if (Node->isClassReceiver() && Node->getClassReceiver()) {
    OS << Node->getClassReceiver()->getName() << ".";
  }

  if (Node->isImplicitProperty())
    Node->getImplicitPropertyGetter()->getSelector().print(OS);
  else
    OS << Node->getExplicitProperty()->getName();
}

void AsStmtPrinter::VisitObjCSubscriptRefExpr(ObjCSubscriptRefExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getBaseExpr());
  OS << "[";
  PrintExpr(Node->getKeyExpr());
  OS << "]";
}

void AsStmtPrinter::VisitPredefinedExpr(PredefinedExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << PredefinedExpr::getIdentTypeName(Node->getIdentType());
}

void AsStmtPrinter::VisitCharacterLiteral(CharacterLiteral *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  unsigned value = Node->getValue();

  switch (Node->getKind()) {
  case CharacterLiteral::Ascii: break; // no prefix.
  case CharacterLiteral::Wide:  OS << 'L'; break;
  case CharacterLiteral::UTF16: OS << 'u'; break;
  case CharacterLiteral::UTF32: OS << 'U'; break;
  }

  switch (value) {
  case '\\':
    OS << "'\\\\'";
    break;
  case '\'':
    OS << "'\\''";
    break;
  case '\a':
    // TODO: K&R: the meaning of '\\a' is different in traditional C
    OS << "'\\a'";
    break;
  case '\b':
    OS << "'\\b'";
    break;
    // Nonstandard escape sequence.
    /*case '\e':
      OS << "'\\e'";
      break;*/
  case '\f':
    OS << "'\\f'";
    break;
  case '\n':
    OS << "'\\n'";
    break;
  case '\r':
    OS << "'\\r'";
    break;
  case '\t':
    OS << "'\\t'";
    break;
  case '\v':
    OS << "'\\v'";
    break;
  default:
    if (value < 256 && isPrintable((unsigned char)value))
      OS << "'" << (char)value << "'";
    else if (value < 256)
      OS << "'\\x" << llvm::format("%02x", value) << "'";
    else if (value <= 0xFFFF)
      OS << "'\\u" << llvm::format("%04x", value) << "'";
    else
      OS << "'\\U" << llvm::format("%08x", value) << "'";
  }
}

void AsStmtPrinter::VisitIntegerLiteral(IntegerLiteral *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  bool isSigned = Node->getType()->isSignedIntegerType();
  OS << Node->getValue().toString(10, isSigned);

  // Emit suffixes.  Integer literals are always a builtin integer type.
  switch (Node->getType()->getAs<BuiltinType>()->getKind()) {
  default: llvm_unreachable("Unexpected type for integer literal!");
  case BuiltinType::SChar:     OS << "i8"; break;
  case BuiltinType::UChar:     OS << "Ui8"; break;
  case BuiltinType::Short:     OS << "i16"; break;
  case BuiltinType::UShort:    OS << "Ui16"; break;
  case BuiltinType::Int:       break; // no suffix.
  case BuiltinType::UInt:      OS << 'U'; break;
  case BuiltinType::Long:      OS << 'L'; break;
  case BuiltinType::ULong:     OS << "UL"; break;
  case BuiltinType::LongLong:  OS << "LL"; break;
  case BuiltinType::ULongLong: OS << "ULL"; break;
  case BuiltinType::Int128:    OS << "i128"; break;
  case BuiltinType::UInt128:   OS << "Ui128"; break;
  }
}

static void PrintFloatingLiteral(raw_ostream &OS, FloatingLiteral *Node,
                                 bool PrintSuffix) {
  SmallString<16> Str;
  Node->getValue().toString(Str);
  OS << Str;
  if (Str.find_first_not_of("-0123456789") == StringRef::npos)
    OS << '.'; // Trailing dot in order to separate from ints.

  if (!PrintSuffix)
    return;

  // Emit suffixes.  Float literals are always a builtin float type.
  switch (Node->getType()->getAs<BuiltinType>()->getKind()) {
  default: llvm_unreachable("Unexpected type for float literal!");
  case BuiltinType::Half:       break; // FIXME: suffix?
  case BuiltinType::Double:     break; // no suffix.
  case BuiltinType::Float:      OS << 'F'; break;
  case BuiltinType::LongDouble: OS << 'L'; break;
  }
}

void AsStmtPrinter::VisitFloatingLiteral(FloatingLiteral *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintFloatingLiteral(OS, Node, /*PrintSuffix=*/true);
}

void AsStmtPrinter::VisitImaginaryLiteral(ImaginaryLiteral *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getSubExpr());
  OS << "i";
}

void AsStmtPrinter::VisitStringLiteral(StringLiteral *Str) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Str);

  Str->outputString(OS);
}
void AsStmtPrinter::VisitParenExpr(ParenExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}
void AsStmtPrinter::VisitUnaryOperator(UnaryOperator *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (!Node->isPostfix()) {
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());

    // Print a space if this is an "identifier operator" like __real, or if
    // it might be concatenated incorrectly like '+'.
    switch (Node->getOpcode()) {
    default: break;
    case UO_Real:
    case UO_Imag:
    case UO_Extension:
      OS << ' ';
      break;
    case UO_Plus:
    case UO_Minus:
      if (isa<UnaryOperator>(Node->getSubExpr()))
        OS << ' ';
      break;
    }
  }

  PrintExpr(Node->getSubExpr());

  if (Node->isPostfix())
    OS << UnaryOperator::getOpcodeStr(Node->getOpcode());
}

void AsStmtPrinter::VisitOffsetOfExpr(OffsetOfExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__builtin_offsetof(";
  Node->getTypeSourceInfo()->getType().print(OS, Policy);
  OS << ", ";
  bool PrintedSomething = false;
  for (unsigned i = 0, n = Node->getNumComponents(); i < n; ++i) {
    OffsetOfExpr::OffsetOfNode ON = Node->getComponent(i);
    if (ON.getKind() == OffsetOfExpr::OffsetOfNode::Array) {
      // Array node
      OS << "[";
      PrintExpr(Node->getIndexExpr(ON.getArrayExprIndex()));
      OS << "]";
      PrintedSomething = true;
      continue;
    }

    // Skip implicit base indirections.
    if (ON.getKind() == OffsetOfExpr::OffsetOfNode::Base)
      continue;

    // Field or identifier node.
    IdentifierInfo *Id = ON.getFieldName();
    if (!Id)
      continue;

    if (PrintedSomething)
      OS << ".";
    else
      PrintedSomething = true;
    OS << Id->getName();
  }
  OS << ")";
}

void AsStmtPrinter::VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *Node){
  // want to print siezof(buf), not a just integer
  // TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  switch(Node->getKind()) {
  case UETT_SizeOf:
    OS << "sizeof";
    break;
  case UETT_AlignOf:
    if (Policy.LangOpts.CPlusPlus)
      OS << "alignof";
    else if (Policy.LangOpts.C11)
      OS << "_Alignof";
    else
      OS << "__alignof";
    break;
  case UETT_VecStep:
    OS << "vec_step";
    break;
  }
  if (Node->isArgumentType()) {
    OS << '(';
    Node->getArgumentType().print(OS, Policy);
    OS << ')';
  } else {
    OS << " ";
    PrintExpr(Node->getArgumentExpr());
  }
}

void AsStmtPrinter::VisitGenericSelectionExpr(GenericSelectionExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "_Generic(";
  PrintExpr(Node->getControllingExpr());
  for (unsigned i = 0; i != Node->getNumAssocs(); ++i) {
    OS << ", ";
    QualType T = Node->getAssocType(i);
    if (T.isNull())
      OS << "default";
    else
      T.print(OS, Policy);
    OS << ": ";
    PrintExpr(Node->getAssocExpr(i));
  }
  OS << ")";
}

void AsStmtPrinter::VisitArraySubscriptExpr(ArraySubscriptExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getLHS());
  OS << "[";
  PrintExpr(Node->getRHS());
  OS << "]";
}

void AsStmtPrinter::PrintCallArgs(CallExpr *Call) {
  for (unsigned i = 0, e = Call->getNumArgs(); i != e; ++i) {
    if (isa<CXXDefaultArgExpr>(Call->getArg(i))) {
      // Don't print any defaulted arguments
      break;
    }

    if (i) OS << ", ";
    PrintExpr(Call->getArg(i));
  }
}

void AsStmtPrinter::VisitCallExpr(CallExpr *Call) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Call);

  FunctionDecl *FD = Call->getDirectCallee();

  if (FD) {
    OS << FD->getNameInfo().getAsString();
    OS << "(";
    PrintCallArgs(Call);
    OS << ")";
  }
}
void AsStmtPrinter::VisitMemberExpr(MemberExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  // FIXME: Suppress printing implicit bases (like "this")
  PrintExpr(Node->getBase());

  MemberExpr *ParentMember = dyn_cast<MemberExpr>(Node->getBase());
  FieldDecl  *ParentDecl   = ParentMember
    ? dyn_cast<FieldDecl>(ParentMember->getMemberDecl()) : nullptr;

  if (!ParentDecl || !ParentDecl->isAnonymousStructOrUnion())
    OS << (Node->isArrow() ? "->" : ".");

  if (FieldDecl *FD = dyn_cast<FieldDecl>(Node->getMemberDecl()))
    if (FD->isAnonymousStructOrUnion())
      return;

  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
      OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}
void AsStmtPrinter::VisitObjCIsaExpr(ObjCIsaExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getBase());
  OS << (Node->isArrow() ? "->isa" : ".isa");
}

void AsStmtPrinter::VisitExtVectorElementExpr(ExtVectorElementExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getBase());
  OS << ".";
  OS << Node->getAccessor().getName();
}
void AsStmtPrinter::VisitCStyleCastExpr(CStyleCastExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);
  PrintExpr(Node->getSubExpr());
}
void AsStmtPrinter::VisitCompoundLiteralExpr(CompoundLiteralExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << '(';
  Node->getType().print(OS, Policy);
  OS << ')';
  PrintExpr(Node->getInitializer());
}
void AsStmtPrinter::VisitImplicitCastExpr(ImplicitCastExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  // No need to print anything, simply forward to the subexpression.
  PrintExpr(Node->getSubExpr());
}
void AsStmtPrinter::VisitBinaryOperator(BinaryOperator *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}
void AsStmtPrinter::VisitCompoundAssignOperator(CompoundAssignOperator *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getLHS());
  OS << " " << BinaryOperator::getOpcodeStr(Node->getOpcode()) << " ";
  PrintExpr(Node->getRHS());
}
void AsStmtPrinter::VisitConditionalOperator(ConditionalOperator *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getCond());
  OS << " ? ";
  PrintExpr(Node->getLHS());
  OS << " : ";
  PrintExpr(Node->getRHS());
}

// GNU extensions.

void
AsStmtPrinter::VisitBinaryConditionalOperator(BinaryConditionalOperator *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getCommon());
  OS << " ?: ";
  PrintExpr(Node->getFalseExpr());
}
void AsStmtPrinter::VisitAddrLabelExpr(AddrLabelExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "&&" << Node->getLabel()->getName();
}

void AsStmtPrinter::VisitStmtExpr(StmtExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << "(";
  PrintRawCompoundStmt(E->getSubStmt());
  OS << ")";
}

void AsStmtPrinter::VisitChooseExpr(ChooseExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__builtin_choose_expr(";
  PrintExpr(Node->getCond());
  OS << ", ";
  PrintExpr(Node->getLHS());
  OS << ", ";
  PrintExpr(Node->getRHS());
  OS << ")";
}

void AsStmtPrinter::VisitGNUNullExpr(GNUNullExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__null";
}

void AsStmtPrinter::VisitShuffleVectorExpr(ShuffleVectorExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__builtin_shufflevector(";
  for (unsigned i = 0, e = Node->getNumSubExprs(); i != e; ++i) {
    if (i) OS << ", ";
    PrintExpr(Node->getExpr(i));
  }
  OS << ")";
}

void AsStmtPrinter::VisitConvertVectorExpr(ConvertVectorExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__builtin_convertvector(";
  PrintExpr(Node->getSrcExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

void AsStmtPrinter::VisitInitListExpr(InitListExpr* Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (Node->getSyntacticForm()) {
    Visit(Node->getSyntacticForm());
    return;
  }

  OS << "{ ";
  for (unsigned i = 0, e = Node->getNumInits(); i != e; ++i) {
    if (i) OS << ", ";
    if (Node->getInit(i))
      PrintExpr(Node->getInit(i));
    else
      OS << "0";
  }
  OS << " }";
}

void AsStmtPrinter::VisitParenListExpr(ParenListExpr* Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "( ";
  for (unsigned i = 0, e = Node->getNumExprs(); i != e; ++i) {
    if (i) OS << ", ";
    PrintExpr(Node->getExpr(i));
  }
  OS << " )";
}

void AsStmtPrinter::VisitDesignatedInitExpr(DesignatedInitExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  for (DesignatedInitExpr::designators_iterator D = Node->designators_begin(),
         DEnd = Node->designators_end();
       D != DEnd; ++D) {
    if (D->isFieldDesignator()) {
      if (D->getDotLoc().isInvalid()) {
        if (IdentifierInfo *II = D->getFieldName())
          OS << II->getName() << ":";
      } else {
        OS << "." << D->getFieldName()->getName();
      }
    } else {
      OS << "[";
      if (D->isArrayDesignator()) {
        PrintExpr(Node->getArrayIndex(*D));
      } else {
        PrintExpr(Node->getArrayRangeStart(*D));
        OS << " ... ";
        PrintExpr(Node->getArrayRangeEnd(*D));
      }
      OS << "]";
    }
  }

  OS << " = ";
  PrintExpr(Node->getInit());
}

void AsStmtPrinter::VisitImplicitValueInitExpr(ImplicitValueInitExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (Policy.LangOpts.CPlusPlus) {
    OS << "/*implicit*/";
    Node->getType().print(OS, Policy);
    OS << "()";
  } else {
    OS << "/*implicit*/(";
    Node->getType().print(OS, Policy);
    OS << ')';
    if (Node->getType()->isRecordType())
      OS << "{}";
    else
      OS << 0;
  }
}

void AsStmtPrinter::VisitVAArgExpr(VAArgExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__builtin_va_arg(";
  PrintExpr(Node->getSubExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}

void AsStmtPrinter::VisitPseudoObjectExpr(PseudoObjectExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getSyntacticForm());
}

void AsStmtPrinter::VisitAtomicExpr(AtomicExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  const char *Name = nullptr;
  switch (Node->getOp()) {
#define BUILTIN(ID, TYPE, ATTRS)
#define ATOMIC_BUILTIN(ID, TYPE, ATTRS)         \
    case AtomicExpr::AO ## ID:                  \
      Name = #ID "(";                           \
        break;
#include "clang/Basic/Builtins.def"
  }
  OS << Name;

  // AtomicExpr stores its subexpressions in a permuted order.
  PrintExpr(Node->getPtr());
  if (Node->getOp() != AtomicExpr::AO__c11_atomic_load &&
      Node->getOp() != AtomicExpr::AO__atomic_load_n) {
    OS << ", ";
    PrintExpr(Node->getVal1());
  }
  if (Node->getOp() == AtomicExpr::AO__atomic_exchange ||
      Node->isCmpXChg()) {
    OS << ", ";
    PrintExpr(Node->getVal2());
  }
  if (Node->getOp() == AtomicExpr::AO__atomic_compare_exchange ||
      Node->getOp() == AtomicExpr::AO__atomic_compare_exchange_n) {
    OS << ", ";
    PrintExpr(Node->getWeak());
  }
  if (Node->getOp() != AtomicExpr::AO__c11_atomic_init) {
    OS << ", ";
    PrintExpr(Node->getOrder());
  }
  if (Node->isCmpXChg()) {
    OS << ", ";
    PrintExpr(Node->getOrderFail());
  }
  OS << ")";
}

// C++
void AsStmtPrinter::VisitCXXOperatorCallExpr(CXXOperatorCallExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  const char *OpStrings[NUM_OVERLOADED_OPERATORS] = {
    "",
#define OVERLOADED_OPERATOR(Name,Spelling,Token,Unary,Binary,MemberOnly) \
    Spelling,
#include "clang/Basic/OperatorKinds.def"
  };

  OverloadedOperatorKind Kind = Node->getOperator();
  if (Kind == OO_PlusPlus || Kind == OO_MinusMinus) {
    if (Node->getNumArgs() == 1) {
      OS << OpStrings[Kind] << ' ';
      PrintExpr(Node->getArg(0));
    } else {
      PrintExpr(Node->getArg(0));
      OS << ' ' << OpStrings[Kind];
    }
  } else if (Kind == OO_Arrow) {
    PrintExpr(Node->getArg(0));
  } else if (Kind == OO_Call) {
    PrintExpr(Node->getArg(0));
    OS << '(';
    for (unsigned ArgIdx = 1; ArgIdx < Node->getNumArgs(); ++ArgIdx) {
      if (ArgIdx > 1)
        OS << ", ";
      if (!isa<CXXDefaultArgExpr>(Node->getArg(ArgIdx)))
        PrintExpr(Node->getArg(ArgIdx));
    }
    OS << ')';
  } else if (Kind == OO_Subscript) {
    PrintExpr(Node->getArg(0));
    OS << '[';
    PrintExpr(Node->getArg(1));
    OS << ']';
  } else if (Node->getNumArgs() == 1) {
    OS << OpStrings[Kind] << ' ';
    PrintExpr(Node->getArg(0));
  } else if (Node->getNumArgs() == 2) {
    PrintExpr(Node->getArg(0));
    OS << ' ' << OpStrings[Kind] << ' ';
    PrintExpr(Node->getArg(1));
  } else {
    llvm_unreachable("unknown overloaded operator");
  }
}

void AsStmtPrinter::VisitCXXMemberCallExpr(CXXMemberCallExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  // If we have a conversion operator call only print the argument.
  CXXMethodDecl *MD = Node->getMethodDecl();
  if (MD && isa<CXXConversionDecl>(MD)) {
    PrintExpr(Node->getImplicitObjectArgument());
    return;
  }
  VisitCallExpr(cast<CallExpr>(Node));
}

void AsStmtPrinter::VisitCUDAKernelCallExpr(CUDAKernelCallExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getCallee());
  OS << "<<<";
  PrintCallArgs(Node->getConfig());
  OS << ">>>(";
  PrintCallArgs(Node);
  OS << ")";
}

void AsStmtPrinter::VisitCXXNamedCastExpr(CXXNamedCastExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << Node->getCastName() << '<';
  Node->getTypeAsWritten().print(OS, Policy);
  OS << ">(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}

void AsStmtPrinter::VisitCXXStaticCastExpr(CXXStaticCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void AsStmtPrinter::VisitCXXDynamicCastExpr(CXXDynamicCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void AsStmtPrinter::VisitCXXReinterpretCastExpr(CXXReinterpretCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void AsStmtPrinter::VisitCXXConstCastExpr(CXXConstCastExpr *Node) {
  VisitCXXNamedCastExpr(Node);
}

void AsStmtPrinter::VisitCXXTypeidExpr(CXXTypeidExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "typeid(";
  if (Node->isTypeOperand()) {
    Node->getTypeOperandSourceInfo()->getType().print(OS, Policy);
  } else {
    PrintExpr(Node->getExprOperand());
  }
  OS << ")";
}

void AsStmtPrinter::VisitCXXUuidofExpr(CXXUuidofExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__uuidof(";
  if (Node->isTypeOperand()) {
    Node->getTypeOperandSourceInfo()->getType().print(OS, Policy);
  } else {
    PrintExpr(Node->getExprOperand());
  }
  OS << ")";
}

void AsStmtPrinter::VisitMSPropertyRefExpr(MSPropertyRefExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getBaseExpr());
  if (Node->isArrow())
    OS << "->";
  else
    OS << ".";
  if (NestedNameSpecifier *Qualifier =
      Node->getQualifierLoc().getNestedNameSpecifier())
    Qualifier->print(OS, Policy);
  OS << Node->getPropertyDecl()->getDeclName();
}

void AsStmtPrinter::VisitUserDefinedLiteral(UserDefinedLiteral *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  switch (Node->getLiteralOperatorKind()) {
  case UserDefinedLiteral::LOK_Raw:
    OS << cast<StringLiteral>(Node->getArg(0)->IgnoreImpCasts())->getString();
    break;
  case UserDefinedLiteral::LOK_Template: {
    DeclRefExpr *DRE = cast<DeclRefExpr>(Node->getCallee()->IgnoreImpCasts());
    const TemplateArgumentList *Args =
      cast<FunctionDecl>(DRE->getDecl())->getTemplateSpecializationArgs();
    assert(Args);
    const TemplateArgument &Pack = Args->get(0);
    for (const auto &P : Pack.pack_elements()) {
      char C = (char)P.getAsIntegral().getZExtValue();
      OS << C;
    }
    break;
  }
  case UserDefinedLiteral::LOK_Integer: {
    // Print integer literal without suffix.
    IntegerLiteral *Int = cast<IntegerLiteral>(Node->getCookedLiteral());
    OS << Int->getValue().toString(10, /*isSigned*/false);
    break;
  }
  case UserDefinedLiteral::LOK_Floating: {
    // Print floating literal without suffix.
    FloatingLiteral *Float = cast<FloatingLiteral>(Node->getCookedLiteral());
    PrintFloatingLiteral(OS, Float, /*PrintSuffix=*/false);
    break;
  }
  case UserDefinedLiteral::LOK_String:
  case UserDefinedLiteral::LOK_Character:
    PrintExpr(Node->getCookedLiteral());
    break;
  }
  OS << Node->getUDSuffix()->getName();
}

void AsStmtPrinter::VisitCXXBoolLiteralExpr(CXXBoolLiteralExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << (Node->getValue() ? "true" : "false");
}

void AsStmtPrinter::VisitCXXNullPtrLiteralExpr(CXXNullPtrLiteralExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "nullptr";
}

void AsStmtPrinter::VisitCXXThisExpr(CXXThisExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "this";
}

void AsStmtPrinter::VisitCXXThrowExpr(CXXThrowExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (!Node->getSubExpr())
    OS << "throw";
  else {
    OS << "throw ";
    PrintExpr(Node->getSubExpr());
  }
}

void AsStmtPrinter::VisitCXXDefaultArgExpr(CXXDefaultArgExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  // Nothing to print: we picked up the default argument.
}

void AsStmtPrinter::VisitCXXDefaultInitExpr(CXXDefaultInitExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  // Nothing to print: we picked up the default initializer.
}

void AsStmtPrinter::VisitCXXFunctionalCastExpr(CXXFunctionalCastExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Node->getType().print(OS, Policy);
  OS << "(";
  PrintExpr(Node->getSubExpr());
  OS << ")";
}

void AsStmtPrinter::VisitCXXBindTemporaryExpr(CXXBindTemporaryExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getSubExpr());
}

void AsStmtPrinter::VisitCXXTemporaryObjectExpr(CXXTemporaryObjectExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Node->getType().print(OS, Policy);
  OS << "(";
  for (CXXTemporaryObjectExpr::arg_iterator Arg = Node->arg_begin(),
         ArgEnd = Node->arg_end();
       Arg != ArgEnd; ++Arg) {
    if (Arg->isDefaultArgument())
      break;
    if (Arg != Node->arg_begin())
      OS << ", ";
    PrintExpr(*Arg);
  }
  OS << ")";
}

void AsStmtPrinter::VisitLambdaExpr(LambdaExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << '[';
  bool NeedComma = false;
  switch (Node->getCaptureDefault()) {
  case LCD_None:
    break;

  case LCD_ByCopy:
    OS << '=';
    NeedComma = true;
    break;

  case LCD_ByRef:
    OS << '&';
    NeedComma = true;
    break;
  }
  for (LambdaExpr::capture_iterator C = Node->explicit_capture_begin(),
         CEnd = Node->explicit_capture_end();
       C != CEnd;
       ++C) {
    if (NeedComma)
      OS << ", ";
    NeedComma = true;

    switch (C->getCaptureKind()) {
    case LCK_This:
      OS << "this";
      break;

    case LCK_ByRef:
      if (Node->getCaptureDefault() != LCD_ByRef || C->isInitCapture())
        OS << '&';
      OS << C->getCapturedVar()->getName();
      break;

    case LCK_ByCopy:
      OS << C->getCapturedVar()->getName();
      break;
    case LCK_VLAType:
      llvm_unreachable("VLA type in explicit captures.");
    }

    if (C->isInitCapture())
      PrintExpr(C->getCapturedVar()->getInit());
  }
  OS << ']';

  if (Node->hasExplicitParameters()) {
    OS << " (";
    CXXMethodDecl *Method = Node->getCallOperator();
    NeedComma = false;
    for (auto P : Method->params()) {
      if (NeedComma) {
        OS << ", ";
      } else {
        NeedComma = true;
      }
      std::string ParamStr = P->getNameAsString();
      P->getOriginalType().print(OS, Policy, ParamStr);
    }
    if (Method->isVariadic()) {
      if (NeedComma)
        OS << ", ";
      OS << "...";
    }
    OS << ')';

    if (Node->isMutable())
      OS << " mutable";

    const FunctionProtoType *Proto
      = Method->getType()->getAs<FunctionProtoType>();
    Proto->printExceptionSpecification(OS, Policy);

    // FIXME: Attributes

    // Print the trailing return type if it was specified in the source.
    if (Node->hasExplicitResultType()) {
      OS << " -> ";
      Proto->getReturnType().print(OS, Policy);
    }
  }

  // Print the body.
  CompoundStmt *Body = Node->getBody();
  OS << ' ';
  PrintStmt(Body);
}

void AsStmtPrinter::VisitCXXScalarValueInitExpr(CXXScalarValueInitExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (TypeSourceInfo *TSInfo = Node->getTypeSourceInfo())
    TSInfo->getType().print(OS, Policy);
  else
    Node->getType().print(OS, Policy);
  OS << "()";
}

void AsStmtPrinter::VisitCXXNewExpr(CXXNewExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  if (E->isGlobalNew())
    OS << "::";
  OS << "new ";
  unsigned NumPlace = E->getNumPlacementArgs();
  if (NumPlace > 0 && !isa<CXXDefaultArgExpr>(E->getPlacementArg(0))) {
    OS << "(";
    PrintExpr(E->getPlacementArg(0));
    for (unsigned i = 1; i < NumPlace; ++i) {
      if (isa<CXXDefaultArgExpr>(E->getPlacementArg(i)))
        break;
      OS << ", ";
      PrintExpr(E->getPlacementArg(i));
    }
    OS << ") ";
  }
  if (E->isParenTypeId())
    OS << "(";
  std::string TypeS;
  if (Expr *Size = E->getArraySize()) {
    llvm::raw_string_ostream s(TypeS);
    s << '[';
    Size->printPretty(s, nullptr, Policy);
    s << ']';
  }
  E->getAllocatedType().print(OS, Policy, TypeS);
  if (E->isParenTypeId())
    OS << ")";

  CXXNewExpr::InitializationStyle InitStyle = E->getInitializationStyle();
  if (InitStyle) {
    if (InitStyle == CXXNewExpr::CallInit)
      OS << "(";
    PrintExpr(E->getInitializer());
    if (InitStyle == CXXNewExpr::CallInit)
      OS << ")";
  }
}

void AsStmtPrinter::VisitCXXDeleteExpr(CXXDeleteExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  if (E->isGlobalDelete())
    OS << "::";
  OS << "delete ";
  if (E->isArrayForm())
    OS << "[] ";
  PrintExpr(E->getArgument());
}

void AsStmtPrinter::VisitCXXPseudoDestructorExpr(CXXPseudoDestructorExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  PrintExpr(E->getBase());
  if (E->isArrow())
    OS << "->";
  else
    OS << '.';
  if (E->getQualifier())
    E->getQualifier()->print(OS, Policy);
  OS << "~";

  if (IdentifierInfo *II = E->getDestroyedTypeIdentifier())
    OS << II->getName();
  else
    E->getDestroyedType().print(OS, Policy);
}

void AsStmtPrinter::VisitCXXConstructExpr(CXXConstructExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  if (E->isListInitialization())
    OS << "{ ";

  for (unsigned i = 0, e = E->getNumArgs(); i != e; ++i) {
    if (isa<CXXDefaultArgExpr>(E->getArg(i))) {
      // Don't print any defaulted arguments
      break;
    }

    if (i) OS << ", ";
    PrintExpr(E->getArg(i));
  }

  if (E->isListInitialization())
    OS << " }";
}

void AsStmtPrinter::VisitCXXStdInitializerListExpr(CXXStdInitializerListExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  PrintExpr(E->getSubExpr());
}

void AsStmtPrinter::VisitExprWithCleanups(ExprWithCleanups *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  // Just forward to the subexpression.
  PrintExpr(E->getSubExpr());
}

void
AsStmtPrinter::VisitCXXUnresolvedConstructExpr(
  CXXUnresolvedConstructExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Node->getTypeAsWritten().print(OS, Policy);
  OS << "(";
  for (CXXUnresolvedConstructExpr::arg_iterator Arg = Node->arg_begin(),
         ArgEnd = Node->arg_end();
       Arg != ArgEnd; ++Arg) {
    if (Arg != Node->arg_begin())
      OS << ", ";
    PrintExpr(*Arg);
  }
  OS << ")";
}

void AsStmtPrinter::VisitCXXDependentScopeMemberExpr(
  CXXDependentScopeMemberExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (!Node->isImplicitAccess()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
      OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

void AsStmtPrinter::VisitUnresolvedMemberExpr(UnresolvedMemberExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  if (!Node->isImplicitAccess()) {
    PrintExpr(Node->getBase());
    OS << (Node->isArrow() ? "->" : ".");
  }
  if (NestedNameSpecifier *Qualifier = Node->getQualifier())
    Qualifier->print(OS, Policy);
  if (Node->hasTemplateKeyword())
    OS << "template ";
  OS << Node->getMemberNameInfo();
  if (Node->hasExplicitTemplateArgs())
    TemplateSpecializationType::PrintTemplateArgumentList(
      OS, Node->getTemplateArgs(), Node->getNumTemplateArgs(), Policy);
}

static const char *getTypeTraitName(TypeTrait TT) {
  switch (TT) {
#define TYPE_TRAIT_1(Spelling, Name, Key)       \
    case clang::UTT_##Name: return #Spelling;
#define TYPE_TRAIT_2(Spelling, Name, Key)       \
    case clang::BTT_##Name: return #Spelling;
#define TYPE_TRAIT_N(Spelling, Name, Key)       \
    case clang::TT_##Name: return #Spelling;
#include "clang/Basic/TokenKinds.def"
  }
  llvm_unreachable("Type trait not covered by switch");
}

static const char *getTypeTraitName(ArrayTypeTrait ATT) {
  switch (ATT) {
  case ATT_ArrayRank:        return "__array_rank";
  case ATT_ArrayExtent:      return "__array_extent";
  }
  llvm_unreachable("Array type trait not covered by switch");
}

static const char *getExpressionTraitName(ExpressionTrait ET) {
  switch (ET) {
  case ET_IsLValueExpr:      return "__is_lvalue_expr";
  case ET_IsRValueExpr:      return "__is_rvalue_expr";
  }
  llvm_unreachable("Expression type trait not covered by switch");
}

void AsStmtPrinter::VisitTypeTraitExpr(TypeTraitExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << getTypeTraitName(E->getTrait()) << "(";
  for (unsigned I = 0, N = E->getNumArgs(); I != N; ++I) {
    if (I > 0)
      OS << ", ";
    E->getArg(I)->getType().print(OS, Policy);
  }
  OS << ")";
}

void AsStmtPrinter::VisitArrayTypeTraitExpr(ArrayTypeTraitExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << getTypeTraitName(E->getTrait()) << '(';
  E->getQueriedType().print(OS, Policy);
  OS << ')';
}

void AsStmtPrinter::VisitExpressionTraitExpr(ExpressionTraitExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << getExpressionTraitName(E->getTrait()) << '(';
  PrintExpr(E->getQueriedExpression());
  OS << ')';
}

void AsStmtPrinter::VisitCXXNoexceptExpr(CXXNoexceptExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << "noexcept(";
  PrintExpr(E->getOperand());
  OS << ")";
}

void AsStmtPrinter::VisitPackExpansionExpr(PackExpansionExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  PrintExpr(E->getPattern());
  OS << "...";
}

void AsStmtPrinter::VisitSizeOfPackExpr(SizeOfPackExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << "sizeof...(" << *E->getPack() << ")";
}

void AsStmtPrinter::VisitSubstNonTypeTemplateParmPackExpr(
  SubstNonTypeTemplateParmPackExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << *Node->getParameterPack();
}

void AsStmtPrinter::VisitSubstNonTypeTemplateParmExpr(
  SubstNonTypeTemplateParmExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  Visit(Node->getReplacement());
}

void AsStmtPrinter::VisitFunctionParmPackExpr(FunctionParmPackExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << *E->getParameterPack();
}

void AsStmtPrinter::VisitMaterializeTemporaryExpr(MaterializeTemporaryExpr *Node){
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->GetTemporaryExpr());
}

void AsStmtPrinter::VisitCXXFoldExpr(CXXFoldExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << "(";
  if (E->getLHS()) {
    PrintExpr(E->getLHS());
    OS << " " << BinaryOperator::getOpcodeStr(E->getOperator()) << " ";
  }
  OS << "...";
  if (E->getRHS()) {
    OS << " " << BinaryOperator::getOpcodeStr(E->getOperator()) << " ";
    PrintExpr(E->getRHS());
  }
  OS << ")";
}

// Obj-C

void AsStmtPrinter::VisitObjCStringLiteral(ObjCStringLiteral *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "@";
  VisitStringLiteral(Node->getString());
}

void AsStmtPrinter::VisitObjCBoxedExpr(ObjCBoxedExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << "@";
  Visit(E->getSubExpr());
}

void AsStmtPrinter::VisitObjCArrayLiteral(ObjCArrayLiteral *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << "@[ ";
  StmtRange ch = E->children();
  if (ch.first != ch.second) {
    while (1) {
      Visit(*ch.first);
      ++ch.first;
      if (ch.first == ch.second) break;
      OS << ", ";
    }
  }
  OS << " ]";
}

void AsStmtPrinter::VisitObjCDictionaryLiteral(ObjCDictionaryLiteral *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << "@{ ";
  for (unsigned I = 0, N = E->getNumElements(); I != N; ++I) {
    if (I > 0)
      OS << ", ";

    ObjCDictionaryElement Element = E->getKeyValueElement(I);
    Visit(Element.Key);
    OS << " : ";
    Visit(Element.Value);
    if (Element.isPackExpansion())
      OS << "...";
  }
  OS << " }";
}

void AsStmtPrinter::VisitObjCEncodeExpr(ObjCEncodeExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "@encode(";
  Node->getEncodedType().print(OS, Policy);
  OS << ')';
}

void AsStmtPrinter::VisitObjCSelectorExpr(ObjCSelectorExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "@selector(";
  Node->getSelector().print(OS);
  OS << ')';
}

void AsStmtPrinter::VisitObjCProtocolExpr(ObjCProtocolExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "@protocol(" << *Node->getProtocol() << ')';
}

void AsStmtPrinter::VisitObjCMessageExpr(ObjCMessageExpr *Mess) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Mess);

  OS << "[";
  switch (Mess->getReceiverKind()) {
  case ObjCMessageExpr::Instance:
    PrintExpr(Mess->getInstanceReceiver());
    break;

  case ObjCMessageExpr::Class:
    Mess->getClassReceiver().print(OS, Policy);
    break;

  case ObjCMessageExpr::SuperInstance:
  case ObjCMessageExpr::SuperClass:
    OS << "Super";
    break;
  }

  OS << ' ';
  Selector selector = Mess->getSelector();
  if (selector.isUnarySelector()) {
    OS << selector.getNameForSlot(0);
  } else {
    for (unsigned i = 0, e = Mess->getNumArgs(); i != e; ++i) {
      if (i < selector.getNumArgs()) {
        if (i > 0) OS << ' ';
        if (selector.getIdentifierInfoForSlot(i))
          OS << selector.getIdentifierInfoForSlot(i)->getName() << ':';
        else
          OS << ":";
      }
      else OS << ", "; // Handle variadic methods.

      PrintExpr(Mess->getArg(i));
    }
  }
  OS << "]";
}

void AsStmtPrinter::VisitObjCBoolLiteralExpr(ObjCBoolLiteralExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << (Node->getValue() ? "__objc_yes" : "__objc_no");
}

void
AsStmtPrinter::VisitObjCIndirectCopyRestoreExpr(ObjCIndirectCopyRestoreExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  PrintExpr(E->getSubExpr());
}

void
AsStmtPrinter::VisitObjCBridgedCastExpr(ObjCBridgedCastExpr *E) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(E);

  OS << '(' << E->getBridgeKindName();
  E->getType().print(OS, Policy);
  OS << ')';
  PrintExpr(E->getSubExpr());
}

void AsStmtPrinter::VisitBlockExpr(BlockExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  BlockDecl *BD = Node->getBlockDecl();
  OS << "^";

  const FunctionType *AFT = Node->getFunctionType();

  if (isa<FunctionNoProtoType>(AFT)) {
    OS << "()";
  } else if (!BD->param_empty() || cast<FunctionProtoType>(AFT)->isVariadic()) {
    OS << '(';
    for (BlockDecl::param_iterator AI = BD->param_begin(),
           E = BD->param_end(); AI != E; ++AI) {
      if (AI != BD->param_begin()) OS << ", ";
      std::string ParamStr = (*AI)->getNameAsString();
      (*AI)->getType().print(OS, Policy, ParamStr);
    }

    const FunctionProtoType *FT = cast<FunctionProtoType>(AFT);
    if (FT->isVariadic()) {
      if (!BD->param_empty()) OS << ", ";
      OS << "...";
    }
    OS << ')';
  }
  OS << "{ }";
}

void AsStmtPrinter::VisitOpaqueValueExpr(OpaqueValueExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  PrintExpr(Node->getSourceExpr());
}

void AsStmtPrinter::VisitTypoExpr(TypoExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  // TODO: Print something reasonable for a TypoExpr, if necessary.
  assert(false && "Cannot print TypoExpr nodes");
}

void AsStmtPrinter::VisitAsTypeExpr(AsTypeExpr *Node) {
  TRY_TO_EVALUATE_SYMEXPR_OR_SVAL(Node);

  OS << "__builtin_astype(";
  PrintExpr(Node->getSrcExpr());
  OS << ", ";
  Node->getType().print(OS, Policy);
  OS << ")";
}
