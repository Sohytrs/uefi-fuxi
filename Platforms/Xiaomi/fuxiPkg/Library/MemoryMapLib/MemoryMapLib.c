// SPDX-License-Identifier: BSD-3-Clause
//
// fuxi (Xiaomi 13, SM8550) memory map.
//
// Reservations: every fixed address of /reserved-memory in the fuxi device tree
// dumped from the phone (/sys/firmware/fdt, 801,360 B, FDT v17, 4,719 nodes,
// sha256 64ec3f8394f75e111b019efc7a3d7a21abcc746a034573b762ec237c208f9911,
// embedded as FdtBlob/sm8550-xiaomi-fuxi.dtb). Node names are kept as the tree
// spells them so device/scripts/check-fuxi.py can match them one to one.
//
// Conventional memory: the device tree's /memory entries minus every fixed
// carveout and minus the UEFI window below, computed mechanically, not guessed.
//
// UEFI window: the SM8550 layout shared with nuwaPkg/shengPkg (FD at 0xA7100000
// with the 3 MiB size from Resources/Configs/fuxi.toml, the stack at 0xA760D000
// from fuxi.dsc, the 256 MiB kernel window and the splash at 0xB8000000 from the
// device tree). fuxi's stock uefiplat.cfg is not available on this host, so the
// exact UEFI allocation sizes are UNVERIFIED(fuxi) and inherited from the
// sibling platform; the FD window itself is verified (stock 4 MiB at 0xA7000000,
// first 1 MiB reserved).
//
// UNVERIFIED(fuxi):
//   - the live tree is the recovery kernel's merge, not a stock-boot dump;
//   - it places ramoops at 0xA7000000, i.e. inside the UEFI FD window, because
//     the recovery kernel reuses that space after UEFI exits. That row is
//     deliberately not reserved here: the FD lives there when UEFI runs;
//   - /reserved-memory nodes without a fixed reg (dynamic carveouts such as
//     qseecom_region, linux,cma, cnss_wlan_region, demura_heap_region) get their
//     addresses from the vendor stack at boot and cannot be described statically.
//     They are absent until a stock-boot dump or fuxi's uefiplat.cfg is available;
//   - the UEFI allocation sizes other than the FD and the stack.
#include <Library/MemoryMapLib.h>

STATIC EFI_MEMORY_REGION_DESCRIPTOR gMemoryDescriptor[] = {
  // Name, Address, Length, HobOption, ResourceType, ResourceAttribute, MemoryType, ArmAttribute
  {"gunyah_hyp_region", 0x80000000, 0xA00000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"cpusys_vm_region", 0x80A00000, 0x400000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"xbl_dt_log_merged", 0x81A00000, 0x260000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"aop_cmd_db_region", 0x81C60000, 0x20000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"aop_config_merged_region", 0x81C80000, 0x74000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"smem_region", 0x81D00000, 0x200000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"adsp_mhi_region", 0x81F00000, 0x20000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"global_sync_region", 0x82600000, 0x100000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"tz_stat_region", 0x82700000, 0x100000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"mpss_region", 0x8A800000, 0x10800000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"q6_mpss_dtb_region", 0x9B000000, 0x80000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"ipa_fw_region", 0x9B080000, 0x10000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"ipa_gsi_region", 0x9B090000, 0xA000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"gpu_micro_code_region", 0x9B09A000, 0x2000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"spss_region_region", 0x9B100000, 0x180000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"spu_tz_shared_mem", 0x9B280000, 0x60000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"spu_modem_shared_mem", 0x9B2E0000, 0x20000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"camera_region", 0x9B300000, 0x800000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"video_region", 0x9BB00000, 0x700000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"cvp_region", 0x9C200000, 0x700000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"cdsp_region", 0x9C900000, 0x2000000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"q6_cdsp_dtb_region", 0x9E900000, 0x80000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"q6_adsp_dtb_region", 0x9E980000, 0x80000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"adspslpi_region", 0x9EA00000, 0x6080000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  // The DT declares 0x800 at 0xA4A80000; UEFI descriptors are page granular, so the row is rounded
  // up to one page. UNVERIFIED(fuxi): no stock uefiplat.cfg to confirm the vendor window.
  {"dump_display_region", 0xA4A80000, 0x1000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map, rounded to a page
  {"mpss_dsm_region", 0xD4D00000, 0x3300000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"tz_reserved_region", 0xD8000000, 0x100000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"xbl_sc_region", 0xD8100000, 0x40000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"cpucp_fw_region", 0xD8140000, 0x1C0000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"qtee_region", 0xD8300000, 0x500000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"hwfence-shmem", 0xE6440000, 0x279000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"oem_vm_vblk0_ring", 0xF7AFC000, 0x4000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"oem_vm_swiotlb", 0xF7B00000, 0x100000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"oem_vm_region", 0xF7C00000, 0x4800000, AddMem, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT reserved
  {"llcc_lpi_region", 0xFF800000, 0x600000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  // The live tree also carries ramoops@0xa7000000 (0xA7000000 + 0x400000), i.e. exactly the UEFI FD
  // window, because the recovery kernel reuses that space after UEFI has exited. It is deliberately
  // NOT reserved here: the FD Reserved / UEFI FD rows own that window while UEFI runs. That is the one
  // device-tree reservation this table does not reproduce, and check-fuxi.py knows the exception.
  {"xbl_ramdump_region", 0x81200000, 0x280000, HobOnlyNoCacheSetting, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},   // DT no-map
  {"FD Reserved", 0xA7000000, 0x100000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},   // UEFI layout
  {"UEFI FD", 0xA7100000, 0x300000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},   // UEFI layout
  {"UEFI FD Reserved", 0xA7400000, 0x200000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},   // UEFI layout
  {"CPU Vectors", 0xA7600000, 0x1000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"Info Blk", 0xA7601000, 0x1000, AddMem, SYS_MEM, SYS_MEM_CAP, RtData, WRITE_BACK_XN},   // UEFI layout
  {"MMU PageTables", 0xA7602000, 0x3000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"Log Buffer", 0xA7605000, 0x8000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"UEFI Stack", 0xA760D000, 0x40000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"SEC Heap", 0xA764D000, 0x8C000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"Sched Heap", 0xA76D9000, 0x400000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"FV Region", 0xA7AD9000, 0x400000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"UEFI RESV", 0xA7ED9000, 0x127000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},   // UEFI layout
  {"Kernel", 0xA8000000, 0x10000000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},   // UEFI layout
  // The DT calls this /reserved-memory/splash_region (label cont_splash_region); the row keeps the name
  // FuxiGopLogDxe looks for. Same range as the device tree, 0xB8000000 + 0x2B00000.
  {"Display Reserved", 0xB8000000, 0x2B00000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},   // DT splash_region
  {"DRAM 0x811d0000", 0x811D0000, 0x30000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x81480000", 0x81480000, 0x580000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x81cf4000", 0x81CF4000, 0xC000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x81f20000", 0x81F20000, 0x6E0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x82800000", 0x82800000, 0x8000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x9b09c000", 0x9B09C000, 0x64000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0xa4a81000", 0xA4A81000, 0x257F000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0xbab00000", 0xBAB00000, 0x1A200000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0xe7cb0000", 0xE7CB0000, 0xFE4C000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0xfc400000", 0xFC400000, 0x3400000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0xffe00000", 0xFFE00000, 0x200000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x880000000", 0x880000000, 0x3B300000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x8bb500000", 0x8BB500000, 0x1E00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"DRAM 0x8c0000000", 0x8C0000000, 0x140000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv, WRITE_BACK_XN},   // DT /memory minus carveouts
  {"IMEM Base", 0x14680000, 0x2A000, NoHob, MMAP_IO, INITIALIZED, Conv, NS_DEVICE},   // SM8550 SoC
  {"IMEM Cookie Base", 0x146AA000, 0x16000, AddDev, MMAP_IO, INITIALIZED, Conv, NS_DEVICE},   // SM8550 SoC
  {"IPC_ROUTER_TOP", 0x400000, 0x100000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"SECURITY CONTROL", 0x780000, 0x7000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"QUP", 0x800000, 0x300000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"PRNG_CFG_PRNG", 0x10C0000, 0xC000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"CRYPTO0 CRYPTO", 0x1DC0000, 0x40000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"CORE_TOP_CSR", 0x1F00000, 0x100000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"UFS MEM", 0x1D84000, 0x3000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"UFS ICE", 0x1D88000, 0x8000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"UFS ICE HWKM", 0x1D90000, 0x9000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"PERIPH_SS", 0x8800000, 0x100000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"USB", 0xA600000, 0x200000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"AOSS", 0xB000000, 0x4000000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"TLMM", 0xF000000, 0x1000000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"SMMU", 0x15000000, 0x200000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
  {"APSS_HM", 0x17000000, 0x2000000, AddDev, MMAP_IO, UNCACHEABLE, MmIO, NS_DEVICE},   // SM8550 SoC
};

VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount
  )
{
  // Pass Data
  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = ARRAY_SIZE (gMemoryDescriptor);
}
