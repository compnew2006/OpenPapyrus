// Copyright 2017 The Abseil Authors.
// @license Apache License 2.0
//
// base::AddressIsReadable() probes an address to see whether it is readable, without faulting.
//
#include "absl/absl-internal.h"
#pragma hdrstop
#include "absl/debugging/internal/address_is_readable.h"

#if !defined(__linux__) || defined(__ANDROID__)

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {
// On platforms other than Linux, just return true.
bool AddressIsReadable(const void* /* addr */) {
	return true;
}
}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#else  // __linux__ && !__ANDROID__

#include <stdint.h>
#include <syscall.h>
#include <unistd.h>
#include "absl/base/internal/errno_saver.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {
// NOTE: be extra careful about adding any interposable function calls here
// (such as open(), read(), etc.). These symbols may be interposed and will get
// invoked in contexts they don't expect.
//
// NOTE: any new system calls here may also require sandbox reconfiguration.
//
bool AddressIsReadable(const void * addr) {
	// Align address on 8-byte boundary. On aarch64, checking last
	// byte before inaccessible page returned unexpected EFAULT.
	const uintptr_t u_addr = reinterpret_cast<uintptr_t>(addr) & ~7;
	addr = reinterpret_cast<const void *>(u_addr);

	// rt_sigprocmask below will succeed for this input.
	if(addr == nullptr) return false;

	absl::base_internal::ErrnoSaver errno_saver;

	// Here we probe with some syscall which
	// - accepts an 8-byte region of user memory as input
	// - tests for EFAULT before other validation
	// - has no problematic side-effects
	//
	// rt_sigprocmask(2) works for this.  It copies sizeof(kernel_sigset_t)==8
	// bytes from the address into the kernel memory before any validation.
	//
	// The call can never succeed, since the `how` parameter is not one of
	// SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK.
	//
	// This strategy depends on Linux implementation details,
	// so we rely on the test to alert us if it stops working.
	//
	// Some discarded past approaches:
	// - msync() doesn't reject PROT_NONE regions
	// - write() on /dev/null doesn't return EFAULT
	// - write() on a pipe requires creating it and draining the writes
	// - connect() works but is problematic for sandboxes and needs a valid
	//   file descriptor
	//
	// This can never succeed (invalid first argument to sigprocmask).
	ABSL_RAW_CHECK(syscall(SYS_rt_sigprocmask, ~0, addr, nullptr,
	    /*sizeof(kernel_sigset_t)*/ 8) == -1,
	    "unexpected success");
	ABSL_RAW_CHECK(errno == EFAULT || errno == EINVAL, "unexpected errno");
	return errno != EFAULT;
}
}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // __linux__ && !__ANDROID__
