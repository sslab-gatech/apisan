#include "llvm/IR/Constants.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

using namespace llvm;

#define DL_NAME "fs-semantic"
#define DEBUG_TYPE DL_NAME

static cl::opt<bool> ClVerbose(
  "fs-semantic-verbose", cl::desc("Verbose outputs for fs-semantic"), 
  cl::Hidden,cl::init(false));

#define FSS_DEBUG(stmt)                         \
  do {                                          \
    if (ClVerbose)                              \
      llvm::errs() << stmt;                     \
  } while(0)

#define FSS_PRINT(stmt)                         \
  do {                                          \
    if (OutFile)                                \
      *OutFile << stmt;                         \
    FSS_DEBUG(stmt);                            \
  } while(0)

#define FSS_PRINT_TYPE(type)                    \
  do {                                          \
    if (OutFile)                                \
      (type)->print(*OutFile);                  \
    if (ClVerbose)                              \
      (type)->print(llvm::errs());              \
  } while(0)

namespace {

class FsSemantic : public FunctionPass, public InstVisitor<FsSemantic> {
  friend class InstVisitor<FsSemantic>;

  FsSemantic(const FsSemantic &); // do not implement
protected:
  Function *F;

  // Allocator
  BumpPtrAllocator Alloc;

  /// List of target functions
  StringMap<StringRef> Targets;

  /// Set to prevent us from cycling
  SmallPtrSet<Function *, 8> Visited;

  /// Analysis result
  raw_ostream *OutFile; 
  StringRef OutDir;


  bool loadConfFile(StringRef ConfFileName);
  void visitCallSite(CallSite CS);
  Function *resolveCallee(Value *V);
  void createOutputFile(StringRef OpName, StringRef FuncName);
  void closeOutputFile() {
    delete(OutFile); 
    OutFile = nullptr;
  }

public:
  static char ID; // Pass identification, replacement for typeid

  FsSemantic(StringRef ConfFileName = "", StringRef OutDirName = "") 
    : FunctionPass(ID), OutFile(nullptr) {
    if (!loadConfFile(ConfFileName)) {
      errs() << "Error loading '" << ConfFileName << "\n";
      exit(1);
    }

    OutDir = OutDirName.copy<BumpPtrAllocator>(Alloc);
    initializeFsSemanticPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F_) override;
  // void getAnalysisUsage(AnalysisUsage &AU) const override;
  // void print(raw_ostream &O, const Module *M = nullptr) const override;
};

} // end anonymous namespace

static std::pair<StringRef, StringRef> tokenize(StringRef Str) {
  Str = Str.ltrim(); 

  size_t I, E;
  for (I = 0, E = Str.size(); I != E; ++I) {
    if (strchr(" \t\n\r\f\v", Str[I]))
      break;
  }  
  return std::make_pair( Str.slice(0, I), Str.slice(std::min(I+1, E), E) );
}

bool FsSemantic::loadConfFile(StringRef ConfFile) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> MemBufOrErr =
    MemoryBuffer::getFile(ConfFile);
  if (!MemBufOrErr)
    return false;

  MemoryBuffer &MemBuf = *MemBufOrErr.get();
  StringRef Str(MemBuf.getBufferStart(), MemBuf.getBufferSize());

  std::pair<StringRef, StringRef> Split = Str.split('\n');
  for(; !Split.first.empty() || !Split.second.empty(); Split = Split.second.split('\n')) {
    StringRef &Line = Split.first;
    // Coment or empty line
    if (Line.empty() || Line[0] == '#')
      continue;

    // toyfs.inode_operations.setattr        toyfs_file_setattr
    std::pair<StringRef, StringRef> Token = tokenize(Line);
    StringRef OpName = Token.first; 
    if (OpName.empty()) continue;

    Token = tokenize(Token.second); 
    StringRef FuncName = Token.first;
    if (FuncName.empty()) continue;

    Targets.insert(std::make_pair(FuncName.copy<BumpPtrAllocator>(Alloc), 
                                  OpName.copy<BumpPtrAllocator>(Alloc)));
  }

  return true;
}

void FsSemantic::visitCallSite(CallSite CS) {
  // Callee of call or invoke instruction
  Value *V = CS.getCalledValue();
  Function *F;

  // direct call
  if ( (F = dyn_cast<Function>(V)) || (F = resolveCallee(V)) )
    FSS_PRINT(F->getName() << "\n");
  // indirect call
  else {
    FSS_PRINT("# indirect call: ");
    FSS_PRINT_TYPE(V->getType());
    FSS_PRINT("\n");
  }

  // go deeper
  if (F && Visited.insert(F).second)
    visit(F); 
}

Function *FsSemantic::resolveCallee(Value *V) {
  // Resolve callee of indirect call

  // XXX need to implement 
  return NULL;
}

void FsSemantic::createOutputFile(StringRef OpName, StringRef FuncName) {
  std::error_code EC;
  std::string OutFileName = OutDir.str() + "/" + 
    OpName.str() + "." + FuncName.str() + ".fss";
  OutFile = new raw_fd_ostream(OutFileName, EC, sys::fs::F_Text);
  if (EC) {
    errs() << "Error opening '" << OutFileName << "': " << EC.message()
           << '\n';
    exit(1);
  }
}

bool FsSemantic::runOnFunction(Function &F) {
  StringMap<StringRef>::const_iterator I = Targets.find(F.getName());
  if (I != Targets.end()) {
    createOutputFile(I->second.str(), F.getName());
    FSS_PRINT(F.getName() << "\n");
    Visited.clear();
    Visited.insert(&F);
    visit(&F);
    closeOutputFile();
  }
  return false;
}

char FsSemantic::ID = 0;
static const char fssemantic_name[] = "FsSemantic";
INITIALIZE_PASS_BEGIN(FsSemantic, DL_NAME, fssemantic_name, true,
                      true)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_END(FsSemantic, DL_NAME, fssemantic_name, true, true)

FunctionPass *llvm::createFsSemanticFunctionPass(StringRef ConfFile, StringRef OutDir) { 
  return new FsSemantic(ConfFile, OutDir); 
}
