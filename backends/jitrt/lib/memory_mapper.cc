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

//===- memory_mapper.cc - -------------------------------------------------===//
// Support for managing memory from a SectionMemoryManager via a memfd.
// This allows profilers to correctly label JIT-executed code.
//===----------------------------------------------------------------------===//

#include "tfrt/jitrt/memory_mapper.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "llvm/ExecutionEngine/SectionMemoryManager.h"

// Support for memfd_create(2) was added in glibc v2.27.
#if defined(__linux__) && defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 27)
#define ENABLE_JITRT_MEMORY_MAPPER
#endif  // __GLIBC_PREREQ(2, 27)
#endif  // __linux__ and __GLIBC__ and __GLIBC_PREREQ

#ifndef ENABLE_JITRT_MEMORY_MAPPER
namespace tfrt {
namespace jitrt {

std::unique_ptr<JitRtMemoryMapper> JitRtMemoryMapper::Create(
    llvm::StringRef name) {
  return nullptr;
}

llvm::sys::MemoryBlock JitRtMemoryMapper::allocateMappedMemory(
    llvm::SectionMemoryManager::AllocationPurpose purpose, size_t len,
    const llvm::sys::MemoryBlock* const near_block, unsigned prot_flags,
    std::error_code& error_code) {
  llvm_unreachable("JitRtMemoryMapper is not implemented");
}

std::error_code JitRtMemoryMapper::protectMappedMemory(
    const llvm::sys::MemoryBlock& block, unsigned prot_flags) {
  llvm_unreachable("JitRtMemoryMapper is not implemented");
}

std::error_code JitRtMemoryMapper::releaseMappedMemory(
    llvm::sys::MemoryBlock& block) {
  llvm_unreachable("JitRtMemoryMapper is not implemented");
}

}  // namespace jitrt
}  // namespace tfrt

#else  // ENABLE_JITRT_MEMORY_MAPPER

namespace {
using MemoryMapper = llvm::SectionMemoryManager::MemoryMapper;
using AllocationPurpose = llvm::SectionMemoryManager::AllocationPurpose;

// Some syscalls can be interrupted by a signal handler; retry if that happens.
template <typename FunctionType>
static auto RetryOnEINTR(FunctionType func, decltype(func()) failure_value)
    -> decltype(func()) {
  using ReturnType = decltype(func());

  ReturnType ret;
  do {
    ret = func();
  } while (ret == failure_value && errno == EINTR);
  return ret;
}

int retrying_close(int fd) {
  return RetryOnEINTR([&]() { return close(fd); }, -1);
}

int retrying_ftruncate(int fd, off_t length) {
  return RetryOnEINTR([&]() { return ftruncate(fd, length); }, -1);
}

int retrying_memfd_create(const char* name, unsigned int flags) {
  return RetryOnEINTR([&]() { return memfd_create(name, flags); }, -1);
}

void* retrying_mmap(void* addr, size_t length, int prot, int flags, int fd,
                    off_t offset) {
  return RetryOnEINTR(
      [&]() { return mmap(addr, length, prot, flags, fd, offset); },
      MAP_FAILED);
}

int retrying_mprotect(void* addr, size_t len, int prot) {
  return RetryOnEINTR([&]() { return mprotect(addr, len, prot); }, -1);
}

int retrying_munmap(void* addr, size_t length) {
  return RetryOnEINTR([&]() { return munmap(addr, length); }, -1);
}

int64_t retrying_sysconf(int name) {
  return RetryOnEINTR([&]() { return sysconf(name); }, -1);
}

static int ToPosixProtectionFlags(unsigned flags) {
  int ret = 0;
  if (flags & llvm::sys::Memory::MF_READ) {
    ret |= PROT_READ;
  }
  if (flags & llvm::sys::Memory::MF_WRITE) {
    ret |= PROT_WRITE;
  }
  if (flags & llvm::sys::Memory::MF_EXEC) {
    ret |= PROT_EXEC;
  }
  return ret;
}

}  // namespace

namespace tfrt {
namespace jitrt {

std::unique_ptr<JitRtMemoryMapper> JitRtMemoryMapper::Create(
    llvm::StringRef name) {
  std::unique_ptr<JitRtMemoryMapper> ret(new JitRtMemoryMapper(name));
  return ret;
}

llvm::sys::MemoryBlock JitRtMemoryMapper::allocateMappedMemory(
    AllocationPurpose purpose, size_t len,
    const llvm::sys::MemoryBlock* const near_block, unsigned prot_flags,
    std::error_code& error_code) {
  auto round_up = [](size_t size, size_t align) {
    return (size + align - 1) & ~(align - 1);
  };
  int64_t page_size = retrying_sysconf(_SC_PAGESIZE);
  len = round_up(len, page_size);

  int fd = -1;
  int mmap_flags = MAP_PRIVATE;
  if (purpose == llvm::SectionMemoryManager::AllocationPurpose::Code) {
    // Try to get a truncated memfd. If that fails, use an anonymous mapping.
    fd = retrying_memfd_create(name_.c_str(), 0);
    if (fd != -1 && retrying_ftruncate(fd, len) == -1) {
      retrying_close(fd);
      fd = -1;
    }
  }
  if (fd == -1) {
    mmap_flags |= MAP_ANONYMOUS;
  }
  prot_flags = ToPosixProtectionFlags(prot_flags);
  void* map = retrying_mmap(nullptr, len, prot_flags, mmap_flags, fd, 0);
  // Regardless of the outcome of the mmap, we can close the fd now.
  if (fd != -1) retrying_close(fd);

  if (map == MAP_FAILED) {
    error_code = std::error_code(errno, std::generic_category());
    return llvm::sys::MemoryBlock();
  }
  return llvm::sys::MemoryBlock(map, len);
}

std::error_code JitRtMemoryMapper::protectMappedMemory(
    const llvm::sys::MemoryBlock& block, unsigned prot_flags) {
  int64_t page_size = retrying_sysconf(_SC_PAGESIZE);
  uintptr_t base = reinterpret_cast<uintptr_t>(block.base());
  uintptr_t rounded_down_base = base & ~(page_size - 1);
  size_t size = block.allocatedSize();
  size += base - rounded_down_base;

  prot_flags = ToPosixProtectionFlags(prot_flags);
  void* addr = reinterpret_cast<void*>(rounded_down_base);
  if (retrying_mprotect(addr, size, prot_flags) == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

std::error_code JitRtMemoryMapper::releaseMappedMemory(
    llvm::sys::MemoryBlock& block) {
  if (retrying_munmap(block.base(), block.allocatedSize()) == -1) {
    return std::error_code(errno, std::generic_category());
  }
  return std::error_code();
}

}  // namespace jitrt
}  // namespace tfrt

#endif  // ENABLE_JITRT_MEMORY_MAPPER
