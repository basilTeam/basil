#include "rc.h"

RC::RC() : _count(1) {}

RC::~RC() {}

void RC::inc() {
  _count ++;
}

void RC::dec() {
  _count --;
  if (!_count) delete this;
}