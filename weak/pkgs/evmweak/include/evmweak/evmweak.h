// evmweak: Fast Ethereum Virtual Machine implementation
// Copyright 2018-2019 The evmweak Authors.
// SPDX-License-Identifier: Apache-2.0

#ifndef EVMWEAK_H
#define EVMWEAK_H

#include <evmc/evmc.h>
#include <evmc/utils.h>

#if __cplusplus
extern "C" {
#endif

EVMC_EXPORT struct evmc_vm* evmc_create_evmweak(void) EVMC_NOEXCEPT;

#if __cplusplus
}
#endif

#endif  // EVMWEAK_H
