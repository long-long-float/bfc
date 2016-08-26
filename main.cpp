#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <vector>
#include <string>
#include <cstdlib>

class BrainFuckCompiler {
public:
  BrainFuckCompiler(): context(llvm::getGlobalContext()), module(new llvm::Module("top", context)), builder(llvm::IRBuilder<>(context)){
    // int main()
    auto *funcType = llvm::FunctionType::get(builder.getInt32Ty(), false);
    mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module);

    // int putchar(int)
    std::vector<llvm::Type*> putcharArgs;
    putcharArgs.push_back(builder.getInt32Ty());
    llvm::ArrayRef<llvm::Type*> argsRef(putcharArgs);
    auto *putcharType = llvm::FunctionType::get(builder.getInt32Ty(), argsRef, false);
    putcharFunc = module->getOrInsertFunction("putchar", putcharType);
  }

  ~BrainFuckCompiler() {
    delete module;
  }

  void compile(std::string code) {
    auto *entry = llvm::BasicBlock::Create(context, "entrypoint", mainFunc);
    builder.SetInsertPoint(entry);

    memory = builder.CreateAlloca(builder.getInt8Ty(), builder.getInt32(MEMORY_SIZE), "memory");
    // TODO: initialize

    current_index_ptr = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "pointer_ptr");
    builder.CreateStore(builder.getInt32(0), current_index_ptr);

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
    builder.CreateAdd(pointer, builder.getInt32(1));
    builder.CreateStore(pointer, current_index_ptr);
  }

  // <
  void createDecIndex() {
    auto *pointer = builder.CreateLoad(current_index_ptr);
    builder.CreateSub(pointer, builder.getInt32(1));
    builder.CreateStore(pointer, current_index_ptr);
  }

  // +
  void createIncValue() {
    auto valptr = createGetCurrent();
    auto *c = std::get<0>(valptr);
    auto *n = builder.CreateAdd(c, builder.getInt8(1));
    builder.CreateStore(n, std::get<1>(valptr));
  }

  // -
  void createDecValue() {
    auto valptr = createGetCurrent();
    auto *c = std::get<0>(valptr);
    auto *n = builder.CreateSub(c, builder.getInt8(1));
    builder.CreateStore(n, std::get<1>(valptr));
  }

  // .
  void createOutput() {
    auto *c = std::get<0>(createGetCurrent());
    builder.CreateCall(putcharFunc, builder.CreateSExt(c, builder.getInt32Ty()));
  }
};

int main() {
  BrainFuckCompiler bfc;
  bfc.compile("+++++++++[>++++++++>+++++++++++>+++++<<<-]>.>++.+++++++..+++.>-.------------.<++++++++.--------.+++.------.--------.>+.");

  bfc.getModule()->dump();

  // generate bitcode
  std::error_code error_info;
  llvm::raw_fd_ostream os("a.bc", error_info, llvm::sys::fs::OpenFlags::F_None);
  llvm::WriteBitcodeToFile(bfc.getModule(), os);

  return 0;
}
