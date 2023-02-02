#pragma once

#include <ATen/Tensor.h>
#include <ATen/core/Scalar.h>
#include <ATen/TensorUtils.h>
#include <ATen/native/ReductionType.h>
#include <ATen/native/cpu/SpmmReduceKernel.h>

namespace at {
namespace native {
namespace sparse {
namespace impl {

// Returns true if all entries of self are zero
// TODO: This has potential to be a generic helper
inline bool _is_sparse_and_zero(const Tensor& self) {
  if (self.layout() == kSparse || self.layout() == kSparseCsr ||
      self.layout() == kSparseCsc || self.layout() == kSparseBsr ||
      self.layout() == kSparseBsc) {
    if (self._nnz() == 0) {
      return true;
    }
  }
  return false;
}

inline void _check_is_cpu(const Tensor& self, c10::string_view name) {
  TORCH_CHECK(
      self.is_cpu(),
      "Expected all tensors to be on the same device. addmm expected '",
      name,
      "' to be CPU tensor, but got ",
      self.device(),
      " tensor");
}

inline void _check_is_cuda(const Tensor& self, c10::string_view name) {
  TORCH_CHECK(
      self.is_cuda(),
      "Expected all tensors to be on the same device. addmm expected '",
      name,
      "' to be CUDA tensor, but got ",
      self.device(),
      " tensor");
}

inline void _check_dim(const Tensor& self, int64_t target_dim, c10::string_view name) {
  if (target_dim == 2) {
    TORCH_CHECK(
        self.dim() == target_dim,
        name, " must be a matrix, ",
        "got ", self.dim(), "-D tensor");
  }
  TORCH_CHECK(
      self.dim() == target_dim,
      "Expected ",
      name,
      " to be of dimension ",
      target_dim,
      " but got ",
      self.dim(),
      " instead.");
}

template <bool train>
inline void check_sparse_mm_reduce_impl_inputs(
    const Tensor& self,
    const Tensor& grad_out,
    const Tensor& other,
    const Tensor& row_indices,
    const Tensor& ccol_indices,
    const Tensor& csr2csc) {
  TORCH_CHECK(
      self.is_sparse_csr(),
     "Expected self to be sparse CSR tensor.");
  TORCH_CHECK(
      self.dense_dim() == 0,
      "Expected non-hybrid self tensor.");
  TORCH_CHECK(
      self.dim() == 2,
      "Expected self to be a 2-D tensor, got ",
      self.dim(),
      "-D tensor.");

  const auto input_scalar_type = self.values().scalar_type();
  const auto index_scalar_type = self.col_indices().scalar_type();
  int64_t nnz = self._nnz();

  CheckedFrom c = train ? "sparse_mm_reduce_backward" : "sparse_mm_reduce";
  if (train) {
    checkLayout(c, grad_out, kStrided);
    checkScalarType(c, {grad_out, "grad_out", 1}, input_scalar_type);
    check_dim_size(grad_out, 2, 0, self.size(0));
    check_dim_size(grad_out, 2, 1, other.size(1));
  }

  int pos = train ? 2 : 1;
  checkLayout(c, other, kStrided);
  checkScalarType(c, {other, "other", pos}, input_scalar_type);
  check_dim_size(other, 2, 0, self.size(1));

  if (row_indices.defined()) {
    checkLayout(c, row_indices, kStrided);
    checkScalarType(c, {row_indices, "row_indices", pos++}, index_scalar_type);
    check_dim_size(row_indices, 1, 0, nnz);
  }
  if (ccol_indices.defined()) {
    checkLayout(c, ccol_indices, kStrided);
    checkScalarType(c, {ccol_indices, "ccol_indices", pos++}, index_scalar_type);
    check_dim_size(ccol_indices, 1, 0, self.size(1) + 1);
  }
  if (csr2csc.defined()) {
    checkLayout(c, csr2csc, kStrided);
    checkScalarType(c, {csr2csc, "csr2csc", pos++}, index_scalar_type);
    check_dim_size(csr2csc, 1, 0, nnz);
  }
}

}
}
}
}
