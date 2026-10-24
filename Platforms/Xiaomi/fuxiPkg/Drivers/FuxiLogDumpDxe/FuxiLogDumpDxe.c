/** @file
  Capture the DEBUG log and write it to a file on the payload volume.

  The firmware draws DEBUG output into the frame buffer, but every module links its own
  FrameBufferSerialPortLib, so nothing can collect the whole log in a module-local buffer.
  The platform already reserves a fixed 32 KiB region named "Log Buffer", so the library
  appends every byte there and this driver copies that region into a file the host can
  read after the boot stops.

  Copyright (c) 2026, UniXNU contributors. SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>

#include <Protocol/SimpleFileSystem.h>

#include <Guid/EventGroup.h>
#include <Guid/FileInfo.h>

#include "FuxiLogDumpDxe.h"

STATIC EFI_EVENT  mTimerEvent;
STATIC EFI_EVENT  mExitEvent;
STATIC BOOLEAN    mDone;

/**
  Copy the ring into a linear buffer and write it to \fuxi-log.txt on the volume that
  carries \EFI\UniXNU\unixnu.toml.
**/
STATIC
EFI_STATUS
DumpToVolume (
  IN EFI_FILE_PROTOCOL  *Root
  )
{
  EFI_STATUS         Status;
  EFI_FILE_PROTOCOL  *File;
  UINT8              *Linear;
  UINT8              *Ring;
  UINT32             Head;
  UINT32             Capacity;
  CHAR8              Header[96];
  UINTN              HeaderLength;

  Ring     = (UINT8 *)(UINTN)FUXI_LOG_BUFFER_ADDRESS;
  Head     = ((FUXI_LOG_HEADER *)Ring)->Head;
  Capacity = ((FUXI_LOG_HEADER *)Ring)->Capacity;

  if (((FUXI_LOG_HEADER *)Ring)->Magic != FUXI_LOG_MAGIC || Capacity == 0) {
    return EFI_NOT_FOUND;
  }

  if (Head > Capacity) {
    Head = Capacity;
  }

  Linear = AllocatePool (Capacity + 1);
  if (Linear == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Ring = (UINT8 *)(UINTN)(FUXI_LOG_BUFFER_ADDRESS + sizeof (FUXI_LOG_HEADER));

  /* The oldest byte sits at Head % Capacity; rotate it to the front. */
  CopyMem (Linear, Ring + (Head % Capacity), Capacity - (Head % Capacity));
  CopyMem (Linear + (Capacity - (Head % Capacity)), Ring, Head % Capacity);
  Linear[Capacity] = 0;

  Status = Root->Open (Root, &File, L"fuxi-log.txt",
                       EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    FreePool (Linear);
    return Status;
  }

  HeaderLength = AsciiSPrint (Header, sizeof (Header),
                              "FUXI UEFI DEBUG LOG\r\ntotal=%u capacity=%u dropped=%u\r\n--\r\n",
                              Head, Capacity, ((FUXI_LOG_HEADER *)(UINTN)FUXI_LOG_BUFFER_ADDRESS)->Dropped);
  Status = File->Write (File, &HeaderLength, Header);
  if (!EFI_ERROR (Status)) {
    UINTN ToWrite = Capacity;
    Status = File->Write (File, &ToWrite, Linear);
  }
  if (!EFI_ERROR (Status)) {
    File->Flush (File);
  }
  File->Close (File);
  FreePool (Linear);
  return Status;
}

STATIC
EFI_STATUS
DumpToPayloadVolume (
  VOID
  )
{
  EFI_STATUS                     Status;
  UINTN                          HandleCount;
  EFI_HANDLE                     *Handles;
  UINTN                          Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL              *Root;
  EFI_FILE_PROTOCOL              *Probe;
  EFI_STATUS                     Result;

  Result = EFI_NOT_FOUND;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL,
                                    &HandleCount, &Handles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }
    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) {
      continue;
    }
    /* Only the payload volume carries the loader configuration. */
    Status = Root->Open (Root, &Probe, L"EFI\\UniXNU\\unixnu.toml", EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR (Status)) {
      Probe->Close (Probe);
      Status = DumpToVolume (Root);
      if (!EFI_ERROR (Status)) {
        Result = EFI_SUCCESS;
      }
    }
    Root->Close (Root);
  }

  FreePool (Handles);
  return Result;
}

STATIC
VOID
EFIAPI
DumpHandler (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if (mDone) {
    return;
  }
  if (EFI_ERROR (DumpToPayloadVolume ())) {
    return;
  }
  /* Keep updating on the timer path; a single successful exit dump is enough. */
  if (Event == mExitEvent) {
    mDone = TRUE;
  }
}

EFI_STATUS
EFIAPI
FuxiLogDumpDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, DumpHandler, NULL,
                               &gEfiEventExitBootServicesGuid, &mExitEvent);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->CreateEventEx (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, DumpHandler, NULL,
                               NULL, &mTimerEvent);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = gBS->SetTimer (mTimerEvent, TimerPeriodic, 20 * 1000 * 1000);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
