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


  // make puts function
  // int puts(char*)
  std::vector<llvm::Type*> putsArgs;
  putsArgs.push_back(builder.getInt8Ty()->getPointerTo());
  llvm::ArrayRef<llvm::Type*> argsRef(putsArgs);
  auto *putsType = llvm::FunctionType::get(builder.getInt32Ty(), argsRef, false);
  auto *putsFunc = module->getOrInsertFunction("puts", putsType);

  auto *helloWorld = builder.CreateGlobalStringPtr("Hello LLVM!\n");

  builder.CreateCall(putsFunc, helloWorld);
  builder.CreateRet(builder.getInt32(0));

  module->dump();

  std::error_code error_info;
  llvm::raw_fd_ostream os("a.bc", error_info, llvm::sys::fs::OpenFlags::F_None);
  llvm::WriteBitcodeToFile(module, os);

  return 0;
}
