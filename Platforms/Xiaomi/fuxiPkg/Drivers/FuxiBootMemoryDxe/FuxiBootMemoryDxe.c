/** @file
  Memory and RAM disk from the device tree the previous stage handed over.

  The previous stage (a Linux-style boot loader) passes the physical address
  of a flattened device tree in x0, which SEC keeps in a HOB. Its /memory
  nodes list the real DRAM and its /reserved-memory nodes list what the
  secure side owns. Without the vendor memory manager the firmware only knows
  the few regions of its own memory map, so:

  * DRAM that no firmware region claims and no reserved-memory node covers is
    added to the GCD as system memory, up to a fixed amount;
  * the initrd the loader placed in memory (/chosen linux,initrd-start/-end)
    is published as a read-only block device, so a FAT image in the boot
    image's ramdisk shows up as a normal volume.

  Nothing here depends on a hardware address; every range comes from the DT.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/DiskIo.h>

// The stub experiment keeps the helpers for the next iteration.
#define FUXI_UNUSED  __attribute__((unused))

#define MAX_RANGES        64
// UNVERIFIED(fuxi): sheng board value; needs the fuxi PMIC/DT confirmation.
#define MIN_PIECE_SIZE    SIZE_16MB
// UNVERIFIED(fuxi): sheng board value; needs the fuxi PMIC/DT confirmation.
#define ADD_LIMIT         SIZE_1GB
// UNVERIFIED(fuxi): sheng board value; needs the fuxi PMIC/DT confirmation.
#define ADD_BELOW         SIZE_4GB     // stay inside what the MMU setup maps

typedef struct {
  UINT64  Base;
  UINT64  End;       // exclusive
} RANGE;

STATIC
UINT64
ReadCells (
  IN CONST VOID  *Data,
  IN UINT32      Cells
  )
{
  if (Cells == 2) {
    return Fdt64ToCpu (ReadUnaligned64 (Data));
  }

  return Fdt32ToCpu (ReadUnaligned32 (Data));
}

STATIC
UINT32
GetCells (
  IN CONST VOID  *Fdt,
  IN INT32       Node,
  IN CONST CHAR8 *Name,
  IN UINT32      Default
  )
{
  CONST VOID  *Prop;
  INT32       Length;

  Prop = FdtGetProp (Fdt, Node, Name, &Length);
  if ((Prop == NULL) || (Length != sizeof (UINT32))) {
    return Default;
  }

  return Fdt32ToCpu (ReadUnaligned32 (Prop));
}

STATIC
UINTN
CollectReg (
  IN     CONST VOID  *Fdt,
  IN     INT32       Node,
  IN     UINT32      AddressCells,
  IN     UINT32      SizeCells,
  IN OUT RANGE       *Ranges,
  IN     UINTN       Count
  )
{
  CONST UINT8  *Reg;
  INT32        Length;
  UINT32       Entry;
  INT32        Offset;
  UINT64       Base;
  UINT64       Size;

  Reg = FdtGetProp (Fdt, Node, "reg", &Length);
  if (Reg == NULL) {
    return Count;
  }

  Entry = (AddressCells + SizeCells) * sizeof (UINT32);
  for (Offset = 0; (Offset + (INT32)Entry <= Length) && (Count < MAX_RANGES); Offset += Entry) {
    Base = ReadCells (Reg + Offset, AddressCells);
    Size = ReadCells (Reg + Offset + AddressCells * sizeof (UINT32), SizeCells);
    if ((Size != 0) && (Base + Size > Base)) {
      Ranges[Count].Base = Base;
      Ranges[Count].End  = Base + Size;
      Count++;
    }
  }

  return Count;
}

/**
  Remove every part of Pieces[] that lies inside Cut.
**/
STATIC
UINTN
Subtract (
  IN OUT RANGE  *Pieces,
  IN     UINTN  Count,
  IN     RANGE  Cut
  )
{
  UINTN  Index;
  UINTN  Result;
  RANGE  Out[MAX_RANGES];

  Result = 0;
  for (Index = 0; Index < Count; Index++) {
    if ((Cut.End <= Pieces[Index].Base) || (Cut.Base >= Pieces[Index].End)) {
      if (Result < MAX_RANGES) {
        Out[Result++] = Pieces[Index];
      }

      continue;
    }

    if ((Cut.Base > Pieces[Index].Base) && (Result < MAX_RANGES)) {
      Out[Result].Base  = Pieces[Index].Base;
      Out[Result].End   = Cut.Base;
      Result++;
    }

    if ((Cut.End < Pieces[Index].End) && (Result < MAX_RANGES)) {
      Out[Result].Base  = Cut.End;
      Out[Result].End   = Pieces[Index].End;
      Result++;
    }
  }

  CopyMem (Pieces, Out, Result * sizeof (RANGE));
  return Result;
}

STATIC
FUXI_UNUSED
VOID
AddMemory (
  IN CONST VOID  *Fdt
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *Map;
  UINTN                            MapCount;
  INT32                            Root;
  INT32                            Node;
  INT32                            Reserved;
  INT32                            Length;
  CONST CHAR8                      *Type;
  UINT32                           AddressCells;
  UINT32                           SizeCells;
  RANGE                            Banks[MAX_RANGES];
  RANGE                            Holes[MAX_RANGES];
  RANGE                            Pieces[MAX_RANGES];
  UINTN                            BankCount;
  UINTN                            HoleCount;
  UINTN                            PieceCount;
  UINTN                            ReservedCount;
  RANGE                            ReservedRanges[MAX_RANGES];
  UINTN                            Index;
  UINTN                            Other;
  UINT64                           Added;
  UINT64                           Base;
  UINT64                           End;

  Root         = FdtPathOffset (Fdt, "/");
  AddressCells = GetCells (Fdt, Root, "#address-cells", 2);
  SizeCells    = GetCells (Fdt, Root, "#size-cells", 2);

  BankCount = 0;
  for (Node = FdtFirstSubnode (Fdt, Root); Node >= 0; Node = FdtNextSubnode (Fdt, Node)) {
    Type = FdtGetProp (Fdt, Node, "device_type", &Length);
    if ((Type != NULL) && (AsciiStrCmp (Type, "memory") == 0)) {
      BankCount = CollectReg (Fdt, Node, AddressCells, SizeCells, Banks, BankCount);
    }
  }

  ReservedCount = 0;
  Reserved      = FdtPathOffset (Fdt, "/reserved-memory");
  if (Reserved >= 0) {
    UINT32  ResAddress = GetCells (Fdt, Reserved, "#address-cells", AddressCells);
    UINT32  ResSize    = GetCells (Fdt, Reserved, "#size-cells", SizeCells);

    for (Node = FdtFirstSubnode (Fdt, Reserved); Node >= 0; Node = FdtNextSubnode (Fdt, Node)) {
      ReservedCount = CollectReg (Fdt, Node, ResAddress, ResSize, ReservedRanges, ReservedCount);
    }
  }

  DEBUG ((DEBUG_ERROR, "FUXIMEM: DT has %u memory ranges and %u reserved ranges\n", (UINT32)BankCount, (UINT32)ReservedCount));
  if (BankCount == 0) {
    return;
  }

  Status = gDS->GetMemorySpaceMap (&MapCount, &Map);
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Holes: DRAM the GCD does not know yet.
  //
  HoleCount = 0;
  for (Index = 0; Index < BankCount; Index++) {
    for (Other = 0; Other < MapCount; Other++) {
      if (Map[Other].GcdMemoryType != EfiGcdMemoryTypeNonExistent) {
        continue;
      }

      Base = MAX (Banks[Index].Base, Map[Other].BaseAddress);
      End  = MIN (Banks[Index].End, Map[Other].BaseAddress + Map[Other].Length);
      if ((Base < End) && (HoleCount < MAX_RANGES)) {
        Holes[HoleCount].Base = Base;
        Holes[HoleCount].End  = End;
        HoleCount++;
      }
    }
  }

  FreePool (Map);

  //
  // Minus everything the DT reserves; what is left is added right away.
  //
  Added = 0;
  for (Index = 0; (Index < HoleCount) && (Added < ADD_LIMIT); Index++) {
    Pieces[0]  = Holes[Index];
    PieceCount = 1;
    for (Other = 0; Other < ReservedCount; Other++) {
      PieceCount = Subtract (Pieces, PieceCount, ReservedRanges[Other]);
    }

    for (Other = 0; (Other < PieceCount) && (Added < ADD_LIMIT); Other++) {
      Base = ALIGN_VALUE (Pieces[Other].Base, SIZE_2MB);
      End  = Pieces[Other].End & ~((UINT64)SIZE_2MB - 1);
      if ((End <= Base) || (End - Base < MIN_PIECE_SIZE) || (End > ADD_BELOW)) {
        continue;
      }

      End = MIN (End, Base + (ADD_LIMIT - Added));

      //
      // Add and map before anything can allocate from it.
      //
      Status = gDS->AddMemorySpace (
                      EfiGcdMemoryTypeSystemMemory,
                      Base,
                      End - Base,
                      EFI_MEMORY_UC | EFI_MEMORY_WC | EFI_MEMORY_WT | EFI_MEMORY_WB
                      );
      if (!EFI_ERROR (Status)) {
        Status = gDS->SetMemorySpaceAttributes (Base, End - Base, EFI_MEMORY_WB);
        if (EFI_ERROR (Status)) {
          gDS->RemoveMemorySpace (Base, End - Base);
        } else {
          Added += End - Base;
        }
      }

      DEBUG ((DEBUG_ERROR, "FUXIMEM: system memory 0x%lx-0x%lx %r\n", Base, End - 1, Status));
    }
  }

  DEBUG ((DEBUG_ERROR, "FUXIMEM: added 0x%lx bytes of system memory\n", Added));
}

#define RAM_BLOCK_SIZE  512

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  EFI_DEVICE_PATH_PROTOCOL    End;
} RAM_BLOCK_DEVICE_PATH;
#pragma pack()

STATIC RAM_BLOCK_DEVICE_PATH  mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { (UINT8)sizeof (VENDOR_DEVICE_PATH), (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8) } },
    { 0xA9C6827C, 0x1BD8, 0x410C, { 0x88, 0x5A, 0xC4, 0x65, 0x00, 0xF7, 0xD8, 0x36 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC UINT8                    *mRamBase;
STATIC EFI_BLOCK_IO_MEDIA       mMedia;
STATIC EFI_HANDLE               mVolumeHandle;
STATIC UINTN                    mConnectTries;

//
// Connect the volume once DXE has finished dispatching. The drivers that turn a
// BlockIo into a file system (Partition, DiskIo, Fat) are dispatched after this
// one, and a driver with a DEPEX is not connected to controllers on its own, so
// connecting here and now finds nothing to bind. A short periodic timer runs in
// the event loop after dispatch, before BDS starts the menu, and binds the
// chain for real.
//
STATIC
VOID
FUXI_UNUSED
EFIAPI
ConnectVolume (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_HANDLE  *Handles;
  UINTN       Count;

  if (mVolumeHandle != NULL) {
    gBS->ConnectController (mVolumeHandle, NULL, NULL, TRUE);
  }

  Handles = NULL;
  Count   = 0;
  if (!EFI_ERROR (gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &Count, &Handles))) {
    for (UINTN Index = 0; Index < Count; Index++) {
      gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
    }

    FreePool (Handles);
  }

  {
    EFI_HANDLE  *Volumes;
    UINTN       Count;

    Volumes = NULL;
    Count   = 0;
    if (!EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Volumes))) {
      FreePool (Volumes);
    }

    if (Count > 0) {
      DEBUG ((DEBUG_ERROR, "FUXIMEM: %u file system(s) up after %u connect rounds\n", Count, mConnectTries + 1));
      gBS->CloseEvent (Event);
      return;
    }
  }

  if (++mConnectTries >= 40) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: no file system after %u connect rounds\n", mConnectTries));
    gBS->CloseEvent (Event);
  }
}

STATIC
EFI_STATUS
EFIAPI
RamBlockReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockRead (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSize,
  OUT VOID                   *Buffer
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (MediaId != mMedia.MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if ((BufferSize % RAM_BLOCK_SIZE) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((Lba > mMedia.LastBlock) || ((BufferSize / RAM_BLOCK_SIZE) > (mMedia.LastBlock + 1 - Lba))) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (Buffer, mRamBase + Lba * RAM_BLOCK_SIZE, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockWrite (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockFlush (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC EFI_BLOCK_IO_PROTOCOL  mBlockIo = {
  EFI_BLOCK_IO_PROTOCOL_REVISION,
  &mMedia,
  RamBlockReset,
  RamBlockRead,
  RamBlockWrite,
  RamBlockFlush
};

STATIC
VOID
PublishVolume (
  IN UINT64  Start,
  IN UINT64  Size
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  mRamBase                = (UINT8 *)(UINTN)Start;
  mMedia.MediaId          = 1;
  mMedia.RemovableMedia   = FALSE;
  mMedia.MediaPresent     = TRUE;
  mMedia.LogicalPartition = FALSE;
  mMedia.ReadOnly         = TRUE;
  mMedia.WriteCaching     = FALSE;
  mMedia.BlockSize        = RAM_BLOCK_SIZE;
  mMedia.IoAlign          = 1;
  mMedia.LastBlock        = Size / RAM_BLOCK_SIZE - 1;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiDevicePathProtocolGuid,
                  &mDevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &mBlockIo,
                  NULL
                  );
  DEBUG ((DEBUG_ERROR, "FUXIMEM: volume block device 0x%lx size 0x%lx %r\n", Start, Size, Status));
  if (EFI_ERROR (Status)) {
    return;
  }

  mVolumeHandle = Handle;

  //
  // Bind the drivers that turn a BlockIo into a file system. This handle is
  // installed while DXE is still dispatching, so PartitionDxe, DiskIoDxe and
  // FatPkg have not registered their notifications yet and nothing would ever
  // connect it: the menu would scan zero volumes and report "no boot entry
  // found". Connect every handle a few times so the chain is up before the
  // application runs.
  //
  Status = gBS->ConnectController (Handle, NULL, NULL, TRUE);
  DEBUG ((DEBUG_ERROR, "FUXIMEM: connect %r\n", Status));
  for (UINTN Pass = 0; Pass < 2; Pass++) {
    EFI_HANDLE  *Handles;
    UINTN       Count;

    Handles = NULL;
    Count   = 0;
    if (!EFI_ERROR (gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &Count, &Handles))) {
      for (UINTN Index = 0; Index < Count; Index++) {
        gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
      }

      FreePool (Handles);
    }

    gBS->Stall (50 * 1000);
  }

  {
    EFI_HANDLE  *Handles;
    UINTN       Blocks;
    UINTN       Disks;
    UINTN       Volumes;

    Handles = NULL;
    Blocks  = 0;
    if (!EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Blocks, &Handles))) {
      FreePool (Handles);
    }

    Handles = NULL;
    Disks   = 0;
    if (!EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiDiskIoProtocolGuid, NULL, &Disks, &Handles))) {
      FreePool (Handles);
    }

    Handles = NULL;
    Volumes = 0;
    if (!EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Volumes, &Handles))) {
      FreePool (Handles);
    }

    DEBUG ((DEBUG_ERROR, "FUXIMEM: after connect: %u block, %u disk, %u file system\n", Blocks, Disks, Volumes));
  }
}

//
// Publish the payload volume the boot shim appended to the image, addressed by
// the FAT boot sector at its start. Used when the bootloader hands over no
// initrd (fuxi's ABL keeps the ramdisk in vendor_boot, so a boot image ramdisk
// never reaches the firmware).
//
STATIC
VOID
PublishPayload (
  IN UINT64  Base
  )
{
  CONST UINT8  *Bpb;
  UINT32       SectorBytes;
  UINT32       TotalSectors;

  if ((Base == 0) || ((Base & 0x1FF) != 0)) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: no payload volume (x1 0x%lx)\n", Base));
    return;
  }

  Bpb = (CONST UINT8 *)(UINTN)Base;
  DEBUG ((DEBUG_ERROR, "FUXIMEM: payload head %02x %02x %02x %02x sig %02x%02x\n",
          Bpb[0], Bpb[1], Bpb[2], Bpb[3], Bpb[0x1FF], Bpb[0x1FE]));
  if (*(UINT16 *)(Bpb + 0x1FE) != 0xAA55) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: x1 0x%lx is not a FAT boot sector or MBR\n", Base));
    return;
  }

  //
  // Decide what the image is by parsing, not by looking for a partition-type
  // byte: a bare FAT boot sector also has bytes in the partition-table area
  // (they are boot code), so a type-byte test mistakes it for an MBR.
  //
  SectorBytes  = *(UINT16 *)(Bpb + 0x0B);
  TotalSectors = *(UINT16 *)(Bpb + 0x13);
  if (TotalSectors == 0) {
    TotalSectors = *(UINT32 *)(Bpb + 0x20);
  }

  if ((SectorBytes != RAM_BLOCK_SIZE) || (TotalSectors == 0)) {
    //
    // Not a FAT boot sector: try an MBR with one partition.
    //
    UINT32  PartStart = *(UINT32 *)(Bpb + 0x1BE + 8);
    UINT32  PartCount = *(UINT32 *)(Bpb + 0x1BE + 12);

    if ((PartStart != 0) && (PartCount != 0)) {
      TotalSectors = PartStart + PartCount;
      SectorBytes  = RAM_BLOCK_SIZE;
      DEBUG ((DEBUG_ERROR, "FUXIMEM: payload MBR partition at %u, %u sectors\n", PartStart, PartCount));
    } else {
      TotalSectors = 0;
      SectorBytes  = 0;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: payload is a bare FAT, %u sectors\n", TotalSectors));
  }

  if ((SectorBytes != RAM_BLOCK_SIZE) || (TotalSectors == 0)) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: payload sector %u total %u is unusable\n", SectorBytes, TotalSectors));
    return;
  }

  DEBUG ((DEBUG_ERROR, "FUXIMEM: payload volume at 0x%lx, %u sectors\n", Base, TotalSectors));
  PublishVolume (Base, (UINT64)TotalSectors * SectorBytes);
}

STATIC
FUXI_UNUSED
BOOLEAN
PublishInitrd (
  IN CONST VOID  *Fdt
  )
{
  INT32       Chosen;
  CONST VOID  *StartProp;
  CONST VOID  *EndProp;
  INT32       StartLength;
  INT32       EndLength;
  UINT64      Start;
  UINT64      End;

  Chosen = FdtPathOffset (Fdt, "/chosen");
  if (Chosen < 0) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: no /chosen\n"));
    return FALSE;
  }

  StartProp = FdtGetProp (Fdt, Chosen, "linux,initrd-start", &StartLength);
  EndProp   = FdtGetProp (Fdt, Chosen, "linux,initrd-end", &EndLength);
  if ((StartProp == NULL) || (EndProp == NULL) ||
      ((StartLength != 4) && (StartLength != 8)) || ((EndLength != 4) && (EndLength != 8)))
  {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: no initrd in /chosen\n"));
    return FALSE;
  }

  Start = ReadCells (StartProp, StartLength / 4);
  End   = ReadCells (EndProp, EndLength / 4);
  if ((End <= Start) || (((End - Start) % RAM_BLOCK_SIZE) != 0)) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: initrd 0x%lx-0x%lx is not a whole number of sectors\n", Start, End));
    return FALSE;
  }

  PublishVolume (Start, End - Start);
  return TRUE;
}

EFI_STATUS
EFIAPI
FuxiBootMemoryDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_HOB_GUID_TYPE  *Hob;
  UINT64             Dtb;
  UINT64             Payload;

  Hob = GetFirstGuidHob (&gSiliciumEntryArgHobGuid);
  if (Hob == NULL) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: no entry argument HOB\n"));
    return EFI_SUCCESS;
  }

  Dtb     = ((UINT64 *)GET_GUID_HOB_DATA (Hob))[0];
  Payload = ((UINT64 *)GET_GUID_HOB_DATA (Hob))[1];

  DEBUG ((DEBUG_ERROR, "FUXIMEM: x0 0x%lx x1 0x%lx\n", Dtb, Payload));

  //
  // The payload volume does not depend on the device tree, so publish it first:
  // a tree the previous stage did not hand over must not cost the menu its only
  // volume.
  //
  PublishPayload (Payload);

  if ((Dtb == 0) || ((Dtb & 3) != 0) || (FdtCheckHeader ((VOID *)(UINTN)Dtb) != 0)) {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: x0 0x%lx is not a device tree\n", Dtb));
  } else {
    DEBUG ((DEBUG_ERROR, "FUXIMEM: device tree at 0x%lx, %u bytes\n", Dtb, (UINT32)FdtTotalSize ((VOID *)(UINTN)Dtb)));
    AddMemory ((VOID *)(UINTN)Dtb);
    if (!PublishInitrd ((VOID *)(UINTN)Dtb)) {
      DEBUG ((DEBUG_ERROR, "FUXIMEM: no initrd volume\n"));
    }
  }

  return EFI_SUCCESS;
}


