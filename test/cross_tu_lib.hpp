#define WOID_SYMBOL_VISIBILITY

#include "woid.hpp"

template <woid::Copy kCopy, woid::FunPtr kFunPtr>
using Any8 = woid::Any<8,
                       kCopy,
                       woid::ExceptionGuarantee::NONE,
                       alignof(void*),
                       kFunPtr,
                       woid::SafeAnyCast::ENABLED>;

using AnyMoveOnlyCombined = Any8<woid::Copy::DISABLED, woid::FunPtr::COMBINED>;
using AnyMoveOnlyDedicated = Any8<woid::Copy::DISABLED, woid::FunPtr::DEDICATED>;
using AnyCopyableCombined = Any8<woid::Copy::ENABLED, woid::FunPtr::COMBINED>;
using AnyCopyableDedicated = Any8<woid::Copy::ENABLED, woid::FunPtr::DEDICATED>;

template <typename Storage, typename IntType>
__attribute__((visibility("default"))) Storage mkAny(int i);
