//===- COFFObjcopy.cpp ----------------------------------------------------===//
//
//                      The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "COFFObjcopy.h"
#include "Buffer.h"
#include "CopyConfig.h"
#include "Object.h"
#include "Reader.h"
#include "Writer.h"
#include "llvm-objcopy.h"

#include "llvm/Object/Binary.h"
#include "llvm/Object/COFF.h"
#include "llvm/Support/Errc.h"
#include <cassert>

namespace llvm {
namespace objcopy {
namespace coff {

using namespace object;
using namespace COFF;

static Error handleArgs(const CopyConfig &Config, Object &Obj) {
  // If we need to do per-symbol removals, initialize the Referenced field.
  if (!Config.SymbolsToRemove.empty())
    if (Error E = Obj.markSymbols())
      return E;

  // Actually do removals of symbols.
  Obj.removeSymbols([&](const Symbol &Sym) {
    if (is_contained(Config.SymbolsToRemove, Sym.Name)) {
      // Explicitly removing a referenced symbol is an error.
      if (Sym.Referenced)
        reportError(Config.OutputFilename,
                    make_error<StringError>(
                        "not stripping symbol '" + Sym.Name +
                            "' because it is named in a relocation.",
                        llvm::errc::invalid_argument));
      return true;
    }

    return false;
  });
  return Error::success();
}

void executeObjcopyOnBinary(const CopyConfig &Config,
                            object::COFFObjectFile &In, Buffer &Out) {
  COFFReader Reader(In);
  Expected<std::unique_ptr<Object>> ObjOrErr = Reader.create();
  if (!ObjOrErr)
    reportError(Config.InputFilename, ObjOrErr.takeError());
  Object *Obj = ObjOrErr->get();
  assert(Obj && "Unable to deserialize COFF object");
  if (Error E = handleArgs(Config, *Obj))
    reportError(Config.InputFilename, std::move(E));
  COFFWriter Writer(*Obj, Out);
  if (Error E = Writer.write())
    reportError(Config.OutputFilename, std::move(E));
}

} // end namespace coff
} // end namespace objcopy
} // end namespace llvm
