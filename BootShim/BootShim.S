_Head:
  adr   x1, _Payload    /* Set _Entry address */
  b     _Start          /* Jump to the real code */

#if REQUIRES_KERNEL_HEADER == 1
_StackBase:
#ifndef TEXT_OFFSET
#define TEXT_OFFSET FD_BASE
#endif
  .quad TEXT_OFFSET     /* Text Offset: the kernel load address the bootloader is told to use */

_StackSize:
  .quad FD_SIZE + (_Payload - _Head) /* Complete shim and payload */
  .quad 0               /* Flags */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .quad 0               /* Reserved */
  .ascii "ARM\x64"      /* ARM64 Magic */
  .long 0               /* Reserved */
#endif

_Start:
  mov   x4, x1
  ldr   x5, =FD_BASE
  cmp   x4, x5
  beq   _Entry
  ldr   x6,  =FD_SIZE

_CopyLoop:
  ldp   x2, x3, [x4], #16
  stp   x2, x3, [x5], #16
  subs  x6, x6, #16
  b.ne  _CopyLoop
  ldr   x5, =FD_BASE

_Entry:
  /* Publish the copied instructions before executing them. */
  dsb   sy
  mrs   x9, ctr_el0
  ubfx  x9, x9, #16, #4
  mov   x10, #4
  lsl   x9, x10, x9
  sub   x10, x9, #1
  bic   x8, x5, x10
  ldr   x7, =FD_SIZE
  add   x7, x5, x7

_CleanLoop:
  dc    cvac, x8
  add   x8, x8, x9
  cmp   x8, x7
  b.lo  _CleanLoop
  dsb   sy
  ic    iallu
  dsb   sy
  isb

  /* The payload volume, when the image carries one, starts right after the
     firmware volume: hand its address over in x1 so the firmware can publish
     it as a block device (the platform's own ramdisk channel is not available
     on every bootloader). x1 is free here; the copy loop is done. */
  adr   x1, _Payload
  ldr   x2, =FD_SIZE
  add   x1, x1, x2

  br    x5

_Dead:
  b     _Dead

.text
.ltorg
.align 4

/* The payload volume follows the firmware volume, and the block device needs a
   512-byte aligned base, so the shim pads itself to one sector first. */
.balign 512
_Payload:
