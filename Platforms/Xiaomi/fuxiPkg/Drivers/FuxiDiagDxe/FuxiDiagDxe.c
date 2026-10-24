/** @file
  Fuxi storage bring-up diagnostic.

  The first version printed once during DXE dispatch and the boot manager cleared
  the screen afterwards, so the text was lost. Print on a timer instead: the rows
  land after the menu has drawn and stay readable on top of it.
**/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>

STATIC EFI_EVENT  mTimer;
STATIC UINTN      mRounds;

STATIC
VOID
Report (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       Count;
  UINTN       Index;
  UINTN       Blocks;
  UINTN       Filesystems;

  Blocks      = 0;
  Filesystems = 0;
  Handles     = NULL;

  Print (L"\n=== FUXI DIAG round %u ===\n", (UINT32)mRounds);

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    Blocks = Count;
    Print (L"FUXI DIAG: BlockIo=%u\n", (UINT32)Count);
    for (Index = 0; Index < Count; Index++) {
      EFI_BLOCK_IO_PROTOCOL  *Block;
      if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&Block))) {
        continue;
      }
      Print (
        L"FUXI DIAG:  blk%u last=%lu sz=%u ro=%u\n",
        (UINT32)Index,
        Block->Media->LastBlock,
        (UINT32)Block->Media->BlockSize,
        (UINT32)Block->Media->ReadOnly
        );
    }
    FreePool (Handles);
  } else {
    Print (L"FUXI DIAG: BlockIo fail %r\n", Status);
  }

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    Filesystems = Count;
    Print (L"FUXI DIAG: SimpleFS=%u\n", (UINT32)Count);
    FreePool (Handles);
  } else {
    Print (L"FUXI DIAG: SimpleFS fail %r\n", Status);
  }

  Print (L"FUXI DIAG: SUM blk=%u fs=%u\n", (UINT32)Blocks, (UINT32)Filesystems);
}

STATIC
VOID
EFIAPI
OnTimer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  (VOID)Event;
  (VOID)Context;
  mRounds++;
  Report ();
  if (mRounds >= 3) {
    gBS->SetTimer (mTimer, TimerCancel, 0);
  }
}

EFI_STATUS
EFIAPI
FuxiDiagEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  (VOID)ImageHandle;
  (VOID)SystemTable;
  mTimer  = NULL;
  mRounds = 0;

  if (EFI_ERROR (gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnTimer, NULL, &mTimer))) {
    return EFI_SUCCESS;
  }

  // Fire at 3 s, then once a second until three rounds have been printed.
  gBS->SetTimer (mTimer, TimerPeriodic, EFI_TIMER_PERIOD_SECONDS (1));
  return EFI_SUCCESS;
}
