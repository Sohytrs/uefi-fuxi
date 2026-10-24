/** @file
  Load and start an optional UFP (USB device mode) application from a volume.

  The platform ships ufpdevicefw.efi, which starts the USB device controller through the
  UsbInit protocol. Nothing in this firmware launches it, so the port detects the cable and
  switches to the device role but never enumerates. Copy the application to the payload
  volume as \ufp.efi and this driver runs it, which is the cheapest way to find out whether
  the controller can enumerate at all.

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

#include <Guid/FileInfo.h>

STATIC CONST CHAR16  *mCandidates[] = {
  L"ufp.efi",
  L"EFI\\UniXNU\\ufp.efi",
  L"EFI\\BOOT\\ufp.efi"
};

STATIC
EFI_STATUS
LaunchFromRoot (
  IN EFI_FILE_PROTOCOL  *Root
  )
{
  EFI_STATUS         Status;
  EFI_FILE_PROTOCOL  *File;
  EFI_FILE_INFO      *Info;
  UINTN              InfoSize;
  UINTN              Size;
  VOID               *Image;
  EFI_HANDLE         Handle;
  UINTN              Index;

  for (Index = 0; Index < ARRAY_SIZE (mCandidates); Index++) {
    Status = Root->Open (Root, &File, (CHAR16 *)mCandidates[Index], EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR (Status)) {
      continue;
    }

    InfoSize = 0;
    Status   = File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      File->Close (File);
      continue;
    }
    Info = AllocatePool (InfoSize);
    if (Info == NULL) {
      File->Close (File);
      return EFI_OUT_OF_RESOURCES;
    }
    Status = File->GetInfo (File, &gEfiFileInfoGuid, &InfoSize, Info);
    if (EFI_ERROR (Status)) {
      FreePool (Info);
      File->Close (File);
      continue;
    }

    Size  = (UINTN)Info->FileSize;
    Image = AllocatePool (Size);
    if (Image == NULL) {
      FreePool (Info);
      File->Close (File);
      return EFI_OUT_OF_RESOURCES;
    }

    Status = File->Read (File, &Size, Image);
    File->Close (File);
    FreePool (Info);
    if (EFI_ERROR (Status)) {
      FreePool (Image);
      continue;
    }

    Status = gBS->LoadImage (FALSE, gImageHandle, NULL, Image, Size, &Handle);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "FuxiUfpLaunch: LoadImage %s failed: %r\n", mCandidates[Index], Status));
      FreePool (Image);
      continue;
    }

    DEBUG ((DEBUG_ERROR, "FuxiUfpLaunch: starting %s\n", mCandidates[Index]));
    Status = gBS->StartImage (Handle, NULL, NULL);
    DEBUG ((DEBUG_ERROR, "FuxiUfpLaunch: %s returned %r\n", mCandidates[Index], Status));
    /* The image owns its memory once started; keep it mapped. */
    return Status;
  }

  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
LaunchFromVolumes (
  VOID
  )
{
  EFI_STATUS                      Status;
  UINTN                           HandleCount;
  EFI_HANDLE                      *Handles;
  UINTN                           Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL               *Root;

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
    Status = LaunchFromRoot (Root);
    Root->Close (Root);
    if (!EFI_ERROR (Status)) {
      FreePool (Handles);
      return EFI_SUCCESS;
    }
  }

  FreePool (Handles);
  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
FuxiUfpLaunchDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  /*
   * Run once, immediately: the point is to hand the controller to the device stack before
   * anything else touches it. A missing file is not an error, the driver is opt-in.
   */
  LaunchFromVolumes ();
  return EFI_SUCCESS;
}
