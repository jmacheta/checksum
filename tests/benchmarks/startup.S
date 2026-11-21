// Start-up of the bare-metal benchmarks, in QEMU system mode or loaded by a debugger: sets the stack, enables the
// floating-point unit and enters newlib's _start (rdimon.specs, semihosting). Any fault prints a message and exits with failure.

  .syntax unified

#if __ARM_ARCH_PROFILE == 'M'
  .section .isr_vector, "aR" // R: kept by --gc-sections
vector_table:
  .word __stack
  .word reset
  .rept 14
  .word fault
  .endr

  .text
  .thumb
  .thumb_func
  .global reset
reset:
  // A debugger may start here without a reset: mask interrupts, take over the stack and, where there is one, the
  // vector table.
  cpsid i
  ldr r0, =__stack
  mov sp, r0
#if __ARM_ARCH_ISA_THUMB == 2
  ldr r0, =0xE000ED08 // VTOR
  ldr r1, =vector_table
  str r1, [r0]
  dsb
  isb
#endif
#if defined(__ARM_FP)
  ldr r0, =0xE000ED88 // CPACR: full access to CP10 and CP11
  ldr r1, [r0]
  ldr r2, =0x00F00000
  orrs r1, r2
  str r1, [r0]
  dsb
  isb
#endif
  ldr r0, =_start
  bx r0

  .thumb_func
fault:
  movs r0, #0x04 // SYS_WRITE0
  ldr r1, =message
  bkpt 0xAB
  movs r0, #0x18 // SYS_EXIT
  ldr r1, =0x20023 // ADP_Stopped_InternalError
  bkpt 0xAB
  b fault
#else
  .text
  .arm
  .global reset
reset:
  cpsid if
  ldr sp, =__stack
  // _start enters main in user mode: let it use the performance monitors.
  mov r0, #1
  mcr p15, 0, r0, c9, c14, 0 // PMUSERENR.EN
#if __ARM_ARCH_PROFILE == 'A'
  ldr r0, =vectors
  mcr p15, 0, r0, c12, c0, 0 // VBAR
  // With the MMU off every access is strongly ordered and faults if unaligned: map the 4 GiB to itself as normal memory.
  ldr r0, =translation_table
  mov r1, #0
  ldr r2, =0x1C02 // section, full access, normal non-cacheable memory
1:
  orr r3, r2, r1, lsl #20
  str r3, [r0, r1, lsl #2]
  add r1, r1, #1
  cmp r1, #4096
  bne 1b
  mcr p15, 0, r0, c2, c0, 0 // TTBR0
  mvn r1, #0
  mcr p15, 0, r1, c3, c0, 0 // DACR: manager of every domain
  mrc p15, 0, r1, c1, c0, 0
  orr r1, r1, #1 // SCTLR.M
  mcr p15, 0, r1, c1, c0, 0
  isb
#endif
#if defined(__ARM_FP)
  mrc p15, 0, r0, c1, c0, 2 // CPACR: full access to CP10 and CP11
  orr r0, r0, #0x00F00000
  mcr p15, 0, r0, c1, c0, 2
  isb
  mov r0, #0x40000000 // FPEXC.EN
  vmsr fpexc, r0
#endif
  ldr r0, =_start
  bx r0

  .balign 32
vectors:
  .rept 8
  b fault
  .endr

fault:
  mov r0, #0x04 // SYS_WRITE0
  ldr r1, =message
  svc 0x123456
  mov r0, #0x18 // SYS_EXIT
  ldr r1, =0x20023 // ADP_Stopped_InternalError
  svc 0x123456
  b fault
#endif

  .section .rodata
message:
  .asciz "fault: an exception was taken\n"

#if __ARM_ARCH_PROFILE == 'A'
  // In .data: _start clears .bss after reset has filled the table.
  .data
  .balign 16384
translation_table:
  .space 16384
#endif

  // Heap and stack in .noinit, which _start does not clear: when the debugger reports no stack (OpenOCD), _start runs on
  // __stack while it clears .bss. newlib's sbrk() grows the heap from end up to the stack pointer. Creating a 64-bit
  // braided CRC dispatch engine (33 KB) keeps two copies of its table on the stack, hence 128 KiB of stack.
  .section .noinit, "aw", %nobits
  .balign 8
  .global end
end:
  .space 65536
  .space 131072
  .global __stack
__stack:
