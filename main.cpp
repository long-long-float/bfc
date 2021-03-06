#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <stack>
#include <fstream>
#include <iterator>

class BrainFuckCompiler {
public:
  BrainFuckCompiler(): context(llvm::getGlobalContext()), module(new llvm::Module("top", context)), builder(llvm::IRBuilder<>(context)){
    // int main()
    auto *funcType = llvm::FunctionType::get(builder.getInt32Ty(), false);
    mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module);

    // int putchar(int)
    std::vector<llvm::Type*> putcharArgs;
    putcharArgs.push_back(builder.getInt32Ty());
    llvm::ArrayRef<llvm::Type*> putcharArgsRef(putcharArgs);
    auto *putcharType = llvm::FunctionType::get(builder.getInt32Ty(), putcharArgsRef, false);
    putcharFunc = module->getOrInsertFunction("putchar", putcharType);

    // int getchar()
    std::vector<llvm::Type*> getcharArgs;
    llvm::ArrayRef<llvm::Type*> getcharArgsRef(getcharArgs);
    auto *getcharType = llvm::FunctionType::get(builder.getInt32Ty(), getcharArgsRef, false);
    getcharFunc = module->getOrInsertFunction("getchar", getcharType);
  }

  ~BrainFuckCompiler() {
    delete module;
  }

  void compile(const std::string &code) {
    auto *entry = llvm::BasicBlock::Create(context, "entrypoint", mainFunc);
    builder.SetInsertPoint(entry);

    memory = builder.CreateAlloca(builder.getInt8Ty(), builder.getInt32(MEMORY_SIZE), "memory");
    // TODO: initialize

    current_index_ptr = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "pointer_ptr");
    builder.CreateStore(builder.getInt32(0), current_index_ptr);

    // for [, ]
    // whileBB, mergeBB
    using BBPair = std::pair<llvm::BasicBlock*, llvm::BasicBlock*>;
    std::stack<BBPair> loop_stack;

    for (size_t i = 0; i < code.size(); i++) {
      auto op = code[i];
      switch (op) {
        case '>':
          createIncIndex();
          break;
        case '<':
          createDecIndex();
          break;
        case '+': {
          size_t pcount = 0;
          while (i + pcount < code.size() && code[i + pcount] == '+') {
            pcount++;
          }
          createIncValue(pcount);
          i += pcount - 1;
          break;
        }
        case '-': {
          size_t mcount = 0;
          while (i + mcount < code.size() && code[i + mcount] == '-') {
            mcount++;
          }
          createDecValue(mcount);
          i += mcount - 1;
          break;
        }
        case '.':
          createOutput();
          break;
        case ',':
          createInput();
          break;
        case '[': {
          auto *whileBB = llvm::BasicBlock::Create(context, "while", mainFunc);

          builder.CreateBr(whileBB);

          builder.SetInsertPoint(whileBB);

          auto valptr = createGetCurrent();
          auto *cond = builder.CreateICmpNE(std::get<0>(valptr), builder.getInt8(0));
          auto *thenBB = llvm::BasicBlock::Create(context, "then", mainFunc);
          auto *mergeBB = llvm::BasicBlock::Create(context, "ifcont");

          builder.CreateCondBr(cond, thenBB, mergeBB);

          builder.SetInsertPoint(thenBB);

          loop_stack.push(BBPair(whileBB, mergeBB));

          break;
        }
        case ']': {
          if (loop_stack.size() == 0) {
            throw "no '[' corresponding ']'";
          }

          auto pair = loop_stack.top();
          loop_stack.pop();
          builder.CreateBr(std::get<0>(pair));

          auto mergeBB = std::get<1>(pair);
          mainFunc->getBasicBlockList().push_back(mergeBB);
          builder.SetInsertPoint(mergeBB);

          break;
        }
      }
    }

    if (loop_stack.size() > 0) {
      throw "no ']' corresponding '['";
    }

    builder.CreateRet(builder.getInt32(0));
  }

  llvm::Module *getModule() { return module; }

private:
  const int MEMORY_SIZE = 3000;

  llvm::LLVMContext &context;
  llvm::Module *module;
  llvm::IRBuilder<> builder;

  llvm::Function *mainFunc;
  llvm::Constant *putcharFunc;
  llvm::Constant *getcharFunc;

  llvm::Value *current_index_ptr;
  llvm::Value *memory;

  using value_pointer = std::tuple<llvm::Value*, llvm::Value*>;

  value_pointer createGetCurrent() {
    auto *pointer = builder.CreateLoad(current_index_ptr);
    auto *inst = llvm::GetElementPtrInst::Create(builder.getInt8Ty(), memory, pointer);
    auto *ptr = builder.Insert(inst);
    return value_pointer(builder.CreateLoad(ptr), ptr);
  }

  // >
  void createIncIndex() {
    auto *pointer = builder.CreateLoad(current_index_ptr);
    auto *n = builder.CreateAdd(pointer, builder.getInt32(1));
    auto *n2 = builder.CreateSRem(n, builder.getInt32(MEMORY_SIZE));
    builder.CreateStore(n2, current_index_ptr);
  }

  // <
  void createDecIndex() {
    auto *pointer = builder.CreateLoad(current_index_ptr);
    auto *n = builder.CreateSub(pointer, builder.getInt32(1));
    builder.CreateStore(n, current_index_ptr);
  }

  // +
  void createIncValue(size_t count) {
    auto valptr = createGetCurrent();
    auto *c = std::get<0>(valptr);
    auto *n = builder.CreateAdd(c, builder.getInt8(count));
    builder.CreateStore(n, std::get<1>(valptr));
  }

  // -
  void createDecValue(size_t count) {
    auto valptr = createGetCurrent();
    auto *c = std::get<0>(valptr);
    auto *n = builder.CreateSub(c, builder.getInt8(count));
    builder.CreateStore(n, std::get<1>(valptr));
  }

  // .
  void createOutput() {
    auto *c = std::get<0>(createGetCurrent());
    builder.CreateCall(putcharFunc, builder.CreateSExt(c, builder.getInt32Ty()));
  }

  // ,
  void createInput() {
    auto *val = builder.CreateCall(getcharFunc, llvm::ArrayRef<llvm::Value*>());

    auto valptr = createGetCurrent();
    builder.CreateStore(builder.CreateTrunc(val, builder.getInt8Ty()), std::get<1>(valptr));
  }
};

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage " << argv[0] << " FILE" << std::endl;
    return 1;
  }

  std::fstream ifs(argv[1]);
  if (ifs.fail()) {
    std::cerr << "cannot read " << argv[1] << std::endl;
    return 1;
  }

  std::string code = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

  BrainFuckCompiler bfc;
  bfc.compile(code);

  bfc.getModule()->dump();

  // generate bitcode
  std::error_code error_info;
  llvm::raw_fd_ostream os("a.bc", error_info, llvm::sys::fs::OpenFlags::F_None);
  llvm::WriteBitcodeToFile(bfc.getModule(), os);

  return 0;
}
