/*
 * Copyright 2022 The TensorFlow Runtime Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TFRT_BACKENDS_JITRT_INCLUDE_TFRT_JITRT_SYMBOLIC_SHAPE_H_
#define TFRT_BACKENDS_JITRT_INCLUDE_TFRT_JITRT_SYMBOLIC_SHAPE_H_

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "tfrt/jitrt/constraints.h"
#include "tfrt/jitrt/types.h"
#include "tfrt/support/forward_decls.h"

namespace tfrt {
namespace jitrt {

// Symbolic shapes resolver computes the symbolic shapes of the operands based
// on the function signature, and concrete shapes of the operands at runtime.
//
// Example: dimensions that have the same symbolic shape at runtime.
//
//   signature: func @compute(%arg0: tensor<?xf32>, %arg1: tensor<?xf32)
//                            ^                     ^
//   operands:                memref<123xf32>       memref<123xf32>
//                            ^                     ^
//   symbolic shapes:         [-2xf32]              [-2xf32]
//
// Each unknown dimension in the function signature will be assigned a symbolic
// dimension. If multiple operands have unknown dimensions that are the same
// at runtime, they will be assigned the same symbolic dimensions value
// (e.g. `-2` in the example above).
//
// If an unknown dimension at runtime is equal to some statically known
// dimension in the function signature (of any operand), it will be resolved to
// that statically known constant value:
//
// Example: in this example unknown dimension of `arg0` replaced with a `32`.
//
//  signature:  func @compute(%arg0: tensor<?xf32>, %arg1: tensor<32xf32>)
//                            ^                     ^
//  operands:                 memref<32xf32>        memref<32xf32>
//                            ^                     ^
//  symbolic shapes:          [32xf32]              [32xf32]
//
// Unknown dimensions that are `1` at runtime are always materialized as a
// statically known `1` in the symbolic shape.
class SymbolicShapesResolver {
 public:
  using SymbolicShape = llvm::SmallVector<int64_t>;

  explicit SymbolicShapesResolver(const FunctionType& signature,
                                  ArrayRef<OperandConstraint> constraints);

  // Resolves symbolic shapes from the runtime operands. Returns failure if
  // runtime dimensions do not match the statically known dimensions.
  mlir::FailureOr<llvm::SmallVector<SymbolicShape>> Resolve(
      ArrayRef<MemrefDesc> operands);

  // Replaces all symbolic dimensions with dynamic dimension.
  static llvm::SmallVector<int64_t> Normalize(const SymbolicShape& shape);

 private:
  // Constraints on the function operands.
  llvm::SmallVector<OperandConstraint> constraints_;

  // Statically known sizes of operands from the function signature.
  llvm::SmallVector<Optional<llvm::SmallVector<Index>>> operands_sizes_;

  // Values of statically known dimensions sizes in the function signature.
  llvm::DenseSet<int64_t> seen_static_sizes_;

  // The iteration order for the operands when resolving symbolic shapes.
  llvm::SmallVector<size_t> iteration_order_;
};

}  // namespace jitrt
}  // namespace tfrt

#endif  // TFRT_BACKENDS_JITRT_INCLUDE_TFRT_JITRT_SYMBOLIC_SHAPE_H_
