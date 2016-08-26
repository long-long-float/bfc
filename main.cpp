#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <vector>
#include <string>
#include <cstdlib>

int main() {
  auto &context = llvm::getGlobalContext();
  auto *module = new llvm::Module("top", context);
  llvm::IRBuilder<> builder(context);

  // make main function
  // int main()
  auto *funcType = llvm::FunctionType::get(builder.getInt32Ty(), false);
  auto *mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "main", module);

  auto *entry = llvm::BasicBlock::Create(context, "entrypoint", mainFunc);
  builder.SetInsertPoint(entry);

  // int putchar(int)
  std::vector<llvm::Type*> putcharArgs;
  putcharArgs.push_back(builder.getInt32Ty());
  llvm::ArrayRef<llvm::Type*> argsRef(putcharArgs);
  auto *putcharType = llvm::FunctionType::get(builder.getInt32Ty(), argsRef, false);
  auto *putcharFunc = module->getOrInsertFunction("putchar", putcharType);

  const int MEMORY_SIZE = 3000;
  auto *memoryType = builder.getInt8Ty();//llvm::ArrayType::get(builder.getInt8Ty(), MEMORY_SIZE);
  llvm::Value *memory = builder.CreateAlloca(memoryType, builder.getInt32(MEMORY_SIZE), "memory");

  auto *pointerType = builder.getInt32Ty();
  llvm::Value *pointer_ptr = builder.CreateAlloca(pointerType, nullptr, "pointer_ptr");
  builder.CreateStore(builder.getInt32(0), pointer_ptr);

  // +
  {
    for (int i = 0; i < 65; i++) {
      auto *pointer = builder.CreateLoad(pointer_ptr);
      auto *inst = llvm::GetElementPtrInst::Create(memoryType, memory, pointer);
      auto *ptr = builder.Insert(inst);
      auto *c = builder.CreateLoad(ptr);
      auto *n = builder.CreateAdd(c, builder.getInt8(1));
      builder.CreateStore(n, ptr);
    }
  }

  // .
  {
    auto *pointer = builder.CreateLoad(pointer_ptr);
    auto *inst = llvm::GetElementPtrInst::Create(memoryType, memory, pointer);
    auto *ptr = builder.Insert(inst);
    auto *c = builder.CreateLoad(ptr);
    builder.CreateCall(putcharFunc, builder.CreateSExt(c, builder.getInt32Ty()));
  }

  builder.CreateRet(builder.getInt32(0));

  module->dump();

  // generate bitcode
  std::error_code error_info;
  llvm::raw_fd_ostream os("a.bc", error_info, llvm::sys::fs::OpenFlags::F_None);
  llvm::WriteBitcodeToFile(module, os);

  return 0;
}
