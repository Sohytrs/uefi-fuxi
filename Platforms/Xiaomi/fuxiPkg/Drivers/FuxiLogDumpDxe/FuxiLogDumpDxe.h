/** @file
  Shared layout of the platform log ring and the driver entry point.

  Copyright (c) 2026, UniXNU contributors. SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef FUXI_LOG_DUMP_DXE_H_
#define FUXI_LOG_DUMP_DXE_H_

/* Keep in sync with PcdDebugLogBufferAddress in fuxi.dsc. */
#define FUXI_LOG_BUFFER_ADDRESS  0xA7605000ULL
#define FUXI_LOG_MAGIC           0x474C5846U  /* 'FXGL' */

typedef struct {
  UINT32    Magic;
  UINT32    Head;
  UINT32    Capacity;
  UINT32    Dropped;
} FUXI_LOG_HEADER;

#endif

/* Android reserves this window for ramoops; the kernel console is mirrored there. */
#define FUXI_RAMOOPS_ADDRESS  0xA7000000ULL
#define FUXI_RAMOOPS_SIZE     (2ULL * 1024 * 1024)
