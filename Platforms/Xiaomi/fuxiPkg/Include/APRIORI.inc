# Provenance: there is no fuxi vendor firmware dump on this host (no Binaries/fuxi,
# no porting-inputs, no uefi.elf). The vendor module set below is therefore the
# nuwa (Xiaomi 13 Pro, SM8550) one that ships in Binaries/nuwa, and every vendor
# entry is gated behind FUXI_VENDOR_HW until a fuxi dump is imported. At
# FUXI_VENDOR_HW = 0 only the Mu and fuxiPkg drivers are built into the volume.

APRIORI DXE {
  INF MdeModulePkg/Core/Dxe/DxeMain.inf
  INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/EnvDxe/EnvDxe.inf
!endif

  INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  INF MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf
  INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
  INF EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/SmemDxe/SmemDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/DALSYSDxe/DALSYSDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/HWIODxe/HWIODxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ChipInfoDxe/ChipInfoDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/PlatformInfoDxe/PlatformInfoDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ProjectInfoDxe/ProjectInfoDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/HALIOMMUDxe/HALIOMMUDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ULogDxe/ULogDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/IPCCDxe/IPCCDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/GLinkDxe/GLinkDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/CmdDbDxe/CmdDbDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/PwrUtilsDxe/PwrUtilsDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/NpaDxe/NpaDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/RpmhDxe/RpmhDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/VcsDxe/VcsDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ClockDxe/ClockDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ICBDxe/ICBDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ShmBridgeDxe/ShmBridgeDxeLA.inf
  INF Binaries/nuwa/QcomPkg/Drivers/TzDxe/ScmDxeCompat.inf
  INF Binaries/nuwa/QcomPkg/Drivers/TLMMDxe/TLMMDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/I2CDxe/I2CDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/SPMIDxe/SPMIDxe.inf
!endif

  INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/SdccDxe/SdccDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/UFSDxe/UFSDxe.inf
!endif

  INF FatPkg/EnhancedFatDxe/Fat.inf
  # The memory driver publishes the payload volume as a BlockIo. It must be
  # dispatched after DiskIo/Partition/Fat: APRIORI is what sets that order, not
  # the position of the INF in DXE.inc.
  INF fuxiPkg/Drivers/FuxiBootMemoryDxe/FuxiBootMemoryDxe.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/PmicDxe/PmicDxeLa.inf
  INF Binaries/nuwa/QcomPkg/Drivers/TzDxe/TzDxeLA.inf
!endif

  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/DDRInfoDxe/DDRInfoDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/FeatureEnablerDxe/FeatureEnablerDxe.inf
!endif

!if $(USE_CUSTOM_DISPLAY_DRIVER) == 1
  INF fuxiPkg/Drivers/FuxiGopLogDxe/FuxiGopLogDxe.inf
  INF fuxiPkg/Drivers/FuxiDisplayDepexDxe/FuxiDisplayDepexDxe.inf
!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/DisplayDxe/DisplayDxe.inf
!endif
!else
  INF SiliciumPkg/Drivers/SimpleFbDxe/SimpleFbDxe.inf
!endif

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/PILProxyDxe/PILProxyDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/PILDxe/PILDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/SPSSDxe/SPSSDxe.inf
!endif

  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
  INF MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  INF EmbeddedPkg/EmbeddedMonotonicCounter/EmbeddedMonotonicCounter.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  INF MdeModulePkg/Universal/PrintDxe/PrintDxe.inf
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/SPIDxe/SPIDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/PmicGlinkDxe/PmicGlinkDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/UsbPwrCtrlDxe/UsbPwrCtrlDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/QcomChargerDxe/QcomChargerDxeLA.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ChargerExDxe/ChargerExDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/UsbfnDwc3Dxe/UsbfnDwc3Dxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/XhciPciEmulationDxe/XhciPciEmulationDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/XhciDxe/XhciDxe.inf
!endif

  INF MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/UsbMsdDxe/UsbMsdDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/UsbDeviceDxe/UsbDeviceDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/UsbConfigDxe/UsbConfigDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/ButtonsDxe/ButtonsDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/TsensDxe/TsensDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/LimitsDxe/LimitsDxe.inf
!endif
!if $(USE_CUSTOM_DISPLAY_DRIVER) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/CPRDxe/CPRDxe.inf
!endif
!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/GpiDxe/GpiDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/UCDxe/UCDxe.inf
  INF Binaries/nuwa/QcomPkg/Drivers/RNGDxe/RngDxe.inf
!endif

  INF EmbeddedPkg/SimpleTextInOutSerial/SimpleTextInOutSerial.inf
  INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf

!if $(FUXI_VENDOR_HW) == 1
  INF Binaries/nuwa/QcomPkg/Drivers/RscDxe/RscDxe.inf
!endif
}
