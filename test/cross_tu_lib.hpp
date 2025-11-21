#define WOID_SYMBOL_VISIBILITY

#include "woid.hpp"

using Storage = woid::Any<8,
                          woid::Copy::DISABLED,
                          woid::ExceptionGuarantee::NONE,
                          alignof(void*),
                          woid::FunPtr::COMBINED,
                          woid::SafeAnyCast::ENABLED>;

__attribute__((visibility("default"))) Storage mkAny();

__attribute__((visibility("default"))) Storage mkBigAny();
