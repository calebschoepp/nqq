#ifndef nqq_compiler_h
#define nqq_compiler_h

#include "compiler.h"
#include "vm.h"

ObjFunction* compile(const char* source);
void markCompilerRoots();

#endif