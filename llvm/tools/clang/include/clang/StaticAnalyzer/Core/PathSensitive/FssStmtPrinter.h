//== FssStmtPrinter.h - Registration mechanism for checkers -------------*- C++ -*--=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Statement printer for FSS
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_STATICANALYZER_CORE_FSS_STMT_PRINTER_H
#define LLVM_CLANG_STATICANALYZER_CORE_FSS_STMT_PRINTER_H
#include "clang/StaticAnalyzer/Core/PathSensitive/SymbolManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramState.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Basic/CharInfo.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Format.h"

namespace clang {
namespace ento {

class FssStmtPrinter : public StmtVisitor<FssStmtPrinter> {
  raw_ostream &OS;
  const LocationContext *LCtx;
  const ProgramStateRef &PS;
  const ASTContext &Ctx;
  unsigned IndentLevel;
  PrintingPolicy Policy;
  int Level;
  bool IsLValue;

  bool tryToEvalSymExprOrSVal(const Stmt *S);

public:
  FssStmtPrinter(raw_ostream &os,
                 const LocationContext *lctx, const ProgramStateRef &ps, 
                 int level, bool islvalue)
    : OS(os), LCtx(lctx), PS(ps), Ctx(PS->getStateManager().getContext()), 
      IndentLevel(0), Policy(Ctx.getPrintingPolicy()), 
      Level(level), IsLValue(islvalue) {}

  void PrintStmt(Stmt *S) {
    PrintStmt(S, Policy.Indentation);
  }

  void PrintStmt(Stmt *S, int SubIndent) {
    IndentLevel += SubIndent;
    if (S && isa<Expr>(S)) {
      // If this is an expr used in a stmt context, indent and newline it.
      Indent();
      Visit(S);
      OS << ";\n";
    } else if (S) {
      Visit(S);
    } else {
      Indent() << "<<<NULL STATEMENT>>>\n";
    }
    IndentLevel -= SubIndent;
  }

  void PrintRawCompoundStmt(CompoundStmt *S);
  void PrintRawDecl(Decl *D);
  void PrintRawDeclStmt(const DeclStmt *S);
  void PrintRawIfStmt(IfStmt *If);
  void PrintRawCXXCatchStmt(CXXCatchStmt *Catch);
  void PrintCallArgs(CallExpr *E);
  void PrintRawSEHExceptHandler(SEHExceptStmt *S);
  void PrintRawSEHFinallyStmt(SEHFinallyStmt *S);
  void PrintOMPExecutableDirective(OMPExecutableDirective *S);

  void PrintExpr(Expr *E) {
    if (E)
      Visit(E);
    else
      OS << "<null expr>";
  }

  raw_ostream &Indent(int Delta = 0) {
    for (int i = 0, e = IndentLevel+Delta; i < e; ++i)
      OS << "  ";
    return OS;
  }

  void VisitStmt(Stmt *Node) LLVM_ATTRIBUTE_UNUSED {
    Indent() << "<<unknown stmt type>>\n";
  }
  void VisitExpr(Expr *Node) LLVM_ATTRIBUTE_UNUSED {
    OS << "<<unknown expr type>>";
  }
  void VisitCXXNamedCastExpr(CXXNamedCastExpr *Node);

#define ABSTRACT_STMT(CLASS)
#define STMT(CLASS, PARENT)                     \
  void Visit##CLASS(CLASS *Node);
#include "clang/AST/StmtNodes.inc"
};


} // end ento namespace

} // end clang namespace

#endif
