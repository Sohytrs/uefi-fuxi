/** @file
  Fuxi diagnostic: capture the panel and the storage report into files.

  Photos of the panel are slow and lossy, and the console text scrolls away before
  anyone can read it. This driver runs on a timer, grabs the GOP framebuffer and
  writes it as a 32-bit BMP into the FAT volume that holds \EFI\UniXNU, plus a
  small text report. After a reboot the files can be pulled over adb.

  The FAT volume is found by scanning every SimpleFileSystem handle for the
  config path, so no fixed block device is assumed.
**/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Guid/FileInfo.h>

#define SCREEN_PATH  L"\\fuxi-screen.bmp"
#define REPORT_PATH  L"\\fuxi-diag.txt"
#define CONFIG_PATH  L"\\EFI\\UniXNU\\unixnu.toml"

STATIC EFI_EVENT  mTimer;
STATIC UINTN      mTicks;

STATIC
EFI_FILE_PROTOCOL *
OpenConfigVolume (
  VOID
  )
{
  EFI_STATUS                     Status;
  EFI_HANDLE                     *Handles;
  UINTN                          Count;
  UINTN                          Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL              *Root;
  EFI_FILE_PROTOCOL              *File;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  for (Index = 0; Index < Count; Index++) {
    Status = gBS->HandleProtocol (Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }
    if (EFI_ERROR (Fs->OpenVolume (Fs, &Root))) {
      continue;
    }
    if (!EFI_ERROR (Root->Open (Root, &File, (CHAR16 *)CONFIG_PATH, EFI_FILE_MODE_READ, 0))) {
      File->Close (File);
      FreePool (Handles);
      return Root;
    }
    Root->Close (Root);
  }

  FreePool (Handles);
  return NULL;
}

STATIC
BOOLEAN
SaveFile (
  IN EFI_FILE_PROTOCOL  *Root,
  IN CHAR16             *Path,
  IN VOID               *Data,
  IN UINTN               Size
  )
{
  EFI_STATUS  Status;
  EFI_FILE_PROTOCOL  *File;

  Status = Root->Open (Root, &File, Path, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  Status = File->Write (File, &Size, Data);
  File->Close (File);
  return !EFI_ERROR (Status);
}

STATIC
VOID
CaptureScreen (
  IN EFI_FILE_PROTOCOL  *Root
  )
{
  EFI_STATUS                     Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *Gop;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  UINT8                          Header[54];
  UINT8                          *Buffer;
  UINTN                          Width;
  UINTN                          Height;
  UINTN                          Stride;
  UINTN                          ImageSize;
  UINTN                          Row;
  UINT8                          *RowBuf;
  UINT32                         PixelSize;

  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
  if (EFI_ERROR (Status) || (Gop->Mode == NULL) || (Gop->Mode->Info == NULL)) {
    return;
  }
  Info = Gop->Mode->Info;
  Width  = Info->HorizontalResolution;
  Height = Info->VerticalResolution;
  Stride = Gop->Mode->Info->PixelsPerScanLine;

  if ((Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) ||
      (Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor))
  {
    PixelSize = 4;
  } else if (Info->PixelFormat == PixelBitMask) {
    PixelSize = 4;
  } else {
    return;
  }

  ImageSize = Width * Height * PixelSize;
  Buffer = AllocatePool (54 + ImageSize);
  if (Buffer == NULL) {
    return;
  }

  ZeroMem (Header, sizeof (Header));
  Header[0] = 'B';
  Header[1] = 'M';
  *(UINT32 *)&Header[2]  = (UINT32)(54 + ImageSize);
  *(UINT32 *)&Header[10] = 54;
  *(UINT32 *)&Header[14] = 40;
  *(UINT32 *)&Header[18] = (UINT32)Width;
  *(UINT32 *)&Header[22] = (UINT32)Height;
  *(UINT16 *)&Header[26] = 1;
  *(UINT16 *)&Header[28] = 32;
  *(UINT32 *)&Header[34] = (UINT32)ImageSize;
  CopyMem (Buffer, Header, 54);

  /* GOP rows run top to bottom, BMP rows bottom to top. */
  RowBuf = Buffer + 54;
  for (Row = 0; Row < Height; Row++) {
    CopyMem (
      RowBuf + (Height - 1 - Row) * Width * PixelSize,
      (UINT8 *)(UINTN)Gop->Mode->FrameBufferBase + Row * Stride * PixelSize,
      Width * PixelSize
      );
  }

  SaveFile (Root, SCREEN_PATH, Buffer, 54 + ImageSize);
  FreePool (Buffer);
}

STATIC
VOID
WriteReport (
  IN EFI_FILE_PROTOCOL  *Root,
  IN UINTN               Blocks,
  IN UINTN               Filesystems
  )
{
  CHAR8   Text[512];
  UINTN   Len;

  Len = 0;
  AsciiStrCpyS (Text, sizeof (Text), "FUXI DIAG\r\n");
  Len = AsciiStrLen (Text);
  AsciiStrCatS (Text, sizeof (Text), "BlockIo=");
  Len += 8;
  if (Blocks >= 10) { Text[Len++] = (CHAR8)('0' + Blocks / 10); }
  Text[Len++] = (CHAR8)('0' + Blocks % 10);
  AsciiStrCatS (Text, sizeof (Text), "\r\nSimpleFS=");
  Len = AsciiStrLen (Text);
  if (Filesystems >= 10) { Text[Len++] = (CHAR8)('0' + Filesystems / 10); }
  Text[Len++] = (CHAR8)('0' + Filesystems % 10);
  Text[Len++] = '\r';
  Text[Len++] = '\n';
  Text[Len]   = '\0';

  SaveFile (Root, REPORT_PATH, Text, AsciiStrLen (Text));
}

STATIC
VOID
EFIAPI
OnTimer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS      Status;
  EFI_HANDLE      *Handles;
  UINTN           Count;
  UINTN           Blocks;
  UINTN           Filesystems;
  EFI_FILE_PROTOCOL *Root;

  (VOID)Event;
  (VOID)Context;
  mTicks++;

  /* Give the boot manager time to finish drawing before the capture. */
  if ((mTicks != 20) && (mTicks != 40)) {
    if (mTicks >= 60) {
      gBS->SetTimer (mTimer, TimerCancel, 0);
    }
    return;
  }

  Blocks      = 0;
  Filesystems = 0;
  Handles     = NULL;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    Blocks = Count;
    FreePool (Handles);
  }
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  if (!EFI_ERROR (Status)) {
    Filesystems = Count;
    FreePool (Handles);
  }

  Root = OpenConfigVolume ();
  if (Root == NULL) {
    return;
  }

  WriteReport (Root, Blocks, Filesystems);
  CaptureScreen (Root);
  Root->Close (Root);
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
  mTicks  = 0;

  if (EFI_ERROR (gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, OnTimer, NULL, &mTimer))) {
    return EFI_SUCCESS;
  }
  gBS->SetTimer (mTimer, TimerPeriodic, EFI_TIMER_PERIOD_SECONDS (1));
  return EFI_SUCCESS;
}
