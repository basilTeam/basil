#ifndef BASIL_NATIVE_H
#define BASIL_NATIVE_H

#include "util/defs.h"
#include "ir.h"
#include "jasmine/x64.h"

namespace basil {
	void display_native_list(const Type* t, void* list);
	void add_native_functions(jasmine::Object& object);
  const u8* _read_line();
}

#endif