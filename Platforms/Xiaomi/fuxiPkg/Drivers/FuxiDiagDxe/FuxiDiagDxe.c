/** @file
  Fuxi storage diagnostic + persistent log.

  Two jobs:
    1. Report BlockIo and SimpleFileSystem counts on the panel (timer driven, so
       the rows survive the boot manager's screen clear).
    2. Mirror the same text into the ramoops window at 0xA7000000 so it survives a
       reboot and shows up in Android's /sys/fs/pstore/.

  Why 0xA7000000: the kernel reports "ramoops: using 0x400000@0xa7000000", and the
  fuxi UEFI FD starts at 0xA7100000, so the first 1 MiB of that window belongs to
  ramoops and is free while UEFI runs. Zone layout from the device tree is dmesg
  (record-size 0x20000) first, then console (console-size 0x80000).

  Record headers follow include/linux/pstore_ram.h:
      struct ramoops_dmesg   { u32 sig='RAMO'; u32 pos; u32 size; u32 flags; }
      struct ramoops_console { u32 sig='CONS'; u32 pos; u32 size; u32 flags; }
  Text follows the 16 byte header.
**/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>

#define FUXI_RAMOOPS_BASE   0x00000000A7000000ULL
#define FUXI_DMESG_OFFSET   0x0ULL
#define FUXI_DMESG_SIZE     0x20000U
#define FUXI_CONSOLE_OFFSET 0x20000ULL
#define FUXI_CONSOLE_SIZE   0x80000U

#define RAMOOPS_SIG         0x52414D4FU   /* "RAMO" */
#define RAMOOPS_CONSOLE_SIG 0x434F4E53U   /* "CONS" */

typedef struct {
  UINT32  Sig;
  UINT32  Pos;
  UINT32  Size;
  UINT32  Flags;
} FUXI_RAMOOPS_HEADER;

STATIC EFI_EVENT  mTimer;
STATIC UINTN      mRounds;
STATIC CHAR8      mLog[4096];
STATIC UINTN      mLogLen;

STATIC
VOID
LogAppend (
  IN CONST CHAR8  *Text
  )
{
  UINTN  Len;

  Len = AsciiStrLen (Text);
  if (mLogLen + Len + 1 >= sizeof (mLog)) {
    return;
  }
  CopyMem (mLog + mLogLen, Text, Len);
  mLogLen += Len;
  mLog[mLogLen] = '\0';
}

STATIC
VOID
LogAppendUint (
  IN CONST CHAR8  *Label,
  IN UINTN         Value
  )
{
  CHAR8  Buffer[64];
  UINTN  Index;

  AsciiStrCpyS (Buffer, sizeof (Buffer), Label);
  Index = AsciiStrLen (Buffer);
  if (Value == 0) {
    Buffer[Index++] = '0';
  } else {
    CHAR8  Digits[24];
    UINTN  Count;

    Count = 0;
    while ((Value != 0) && (Count < sizeof (Digits))) {
      Digits[Count++] = (CHAR8)('0' + (Value % 10));
      Value         /= 10;
    }
    while (Count > 0) {
      Buffer[Index++] = Digits[--Count];
    }
  }
  Buffer[Index++] = '\n';
  Buffer[Index]   = '\0';
  LogAppend (Buffer);
}

STATIC
VOID
WriteRamoops (
  IN UINT64  Offset,
  IN UINT32  ZoneSize,
  IN UINT32  Sig
  )
{
  volatile FUXI_RAMOOPS_HEADER  *Header;
  volatile UINT8                *Data;
  UINTN                         Index;

  Header = (volatile FUXI_RAMOOPS_HEADER *)(UINTN)(FUXI_RAMOOPS_BASE + Offset);
  Data   = (volatile UINT8 *)(UINTN)(FUXI_RAMOOPS_BASE + Offset + sizeof (FUXI_RAMOOPS_HEADER));

  for (Index = 0; Index < mLogLen; Index++) {
    Data[Index] = (UINT8)mLog[Index];
  }

  Header->Pos   = (UINT32)mLogLen;
  Header->Size  = ZoneSize;
  Header->Flags = 0;
  Header->Sig   = Sig;

  /* Make the header land after the text, in case a reader looks at Sig first. */
  MemoryFence ();
}

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
  mLogLen     = 0;

  LogAppend ("FUXI DIAG: storage report\n");

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    Blocks = Count;
    LogAppendUint ("FUXI DIAG: BlockIo=", Count);
    for (Index = 0; Index < Count; Index++) {
      EFI_BLOCK_IO_PROTOCOL  *Block;
      if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiBlockIoProtocolGuid, (VOID **)&Block))) {
        continue;
      }
      LogAppendUint ("FUXI DIAG:  blk last=", (UINTN)Block->Media->LastBlock);
      LogAppendUint ("FUXI DIAG:  blk size=", (UINTN)Block->Media->BlockSize);
    }
    FreePool (Handles);
  } else {
    LogAppend ("FUXI DIAG: BlockIo locate failed\n");
  }

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    Filesystems = Count;
    LogAppendUint ("FUXI DIAG: SimpleFS=", Count);
    FreePool (Handles);
  } else {
    LogAppend ("FUXI DIAG: SimpleFS locate failed\n");
  }

  LogAppendUint ("FUXI DIAG: SUM blk=", Blocks);
  LogAppendUint ("FUXI DIAG: SUM fs=", Filesystems);

  /* Panel copy: survives until the next redraw. */
  Print (L"\n=== FUXI DIAG round %u ===\n", (UINT32)mRounds);
  Print (L"FUXI DIAG: BlockIo=%u SimpleFS=%u\n", (UINT32)Blocks, (UINT32)Filesystems);

  /* Persistent copy: survives a reboot, readable from /sys/fs/pstore. */
  WriteRamoops (FUXI_DMESG_OFFSET, FUXI_DMESG_SIZE, RAMOOPS_SIG);
  WriteRamoops (FUXI_CONSOLE_OFFSET, FUXI_CONSOLE_SIZE, RAMOOPS_CONSOLE_SIG);
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
  mLogLen = 0;

  if (EFI_ERROR (gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnTimer, NULL, &mTimer))) {
    return EFI_SUCCESS;
  }

  gBS->SetTimer (mTimer, TimerPeriodic, EFI_TIMER_PERIOD_SECONDS (1));
  return EFI_SUCCESS;
}
