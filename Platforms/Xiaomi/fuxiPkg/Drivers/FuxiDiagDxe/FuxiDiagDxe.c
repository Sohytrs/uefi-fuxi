/** @file
  Fuxi storage bring-up diagnostic.

  Prints, after the GOP console exists, how many BlockIo and SimpleFileSystem
  handles the firmware actually produced. This separates "the UFS driver never
  published a block device" from "the volume exists but the filesystem or the
  config lookup failed" when UniXNU reports "no boot entry found".
**/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>

EFI_STATUS
EFIAPI
FuxiDiagEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
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
        L"FUXI DIAG:   blk%u last=%lu size=%u ro=%u\n",
        (UINT32)Index,
        Block->Media->LastBlock,
        (UINT32)Block->Media->BlockSize,
        (UINT32)Block->Media->ReadOnly
        );
    }
    FreePool (Handles);
    Handles = NULL;
  } else {
    Print (L"FUXI DIAG: BlockIo locate failed %r\n", Status);
  }

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    Filesystems = Count;
    Print (L"FUXI DIAG: SimpleFS=%u\n", (UINT32)Count);
    FreePool (Handles);
  } else {
    Print (L"FUXI DIAG: SimpleFS locate failed %r\n", Status);
  }

  Print (L"FUXI DIAG: summary BlockIo=%u SimpleFS=%u\n", (UINT32)Blocks, (UINT32)Filesystems);
  return EFI_SUCCESS;
}
