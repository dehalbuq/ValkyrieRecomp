/*
 * Lenneth R36S early Android AArch64 Bionic jmp_buf bridge, MIT.
 * Original independent implementation of the Android 5-era ABI.
 * 256-byte guest buffer, 32-bit magic 0x53657200 / 0x53657201.
 * This is a PRIVATE ABI; do not use for Android M+ cookie/checksum variant.
 */
#ifndef __aarch64__
#error "Phase23 jump bridge must be built as AArch64"
#endif
.text
.p2align 2
.globl lenneth_legacy_setjmp_arm64
.type lenneth_legacy_setjmp_arm64,%function
lenneth_legacy_setjmp_arm64:
    /* Preserve original guest return address and buffer across libc call. */
    stp x0, x30, [sp, #-16]!
    bl lenneth_phase23_get_mask
    mov w9, w0
    ldp x0, x30, [sp], #16
    mov w10, #0x7201
    movk w10, #0x5365, lsl #16
    str w10, [x0, #0]
    str w9, [x0, #4]
    mov x10, sp
    stp x30, x10, [x0, #8]
    stp x28, x29, [x0, #24]
    stp x26, x27, [x0, #40]
    stp x24, x25, [x0, #56]
    stp x22, x23, [x0, #72]
    stp x20, x21, [x0, #88]
    str x19, [x0, #104]
    stp d14, d15, [x0, #112]
    stp d12, d13, [x0, #128]
    stp d10, d11, [x0, #144]
    stp d8, d9, [x0, #160]
    mov w0, wzr
    ret
.size lenneth_legacy_setjmp_arm64,.-lenneth_legacy_setjmp_arm64

.p2align 2
.globl lenneth_legacy_longjmp_arm64
.type lenneth_legacy_longjmp_arm64,%function
lenneth_legacy_longjmp_arm64:
    /* Validate before adopting a saved stack or program counter. */
    ldr w9, [x0, #0]
    mov w10, #0x7200
    movk w10, #0x5365, lsl #16
    cmp w9, w10
    b.eq 1f
    add w10, w10, #1
    cmp w9, w10
    b.ne .Linvalid_jmp23
    /* Preserve live arguments across host-only signal mask restoration. */
    stp x0, x1, [sp, #-16]!
    stp x30, xzr, [sp, #-16]!
    ldr w0, [x0, #4]
    bl lenneth_phase23_restore_mask
    ldp x30, xzr, [sp], #16
    ldp x0, x1, [sp], #16
1:
    ldp x11, x12, [x0, #8]
    tst x11, #3
    b.ne .Linvalid_jmp23
    tst x12, #15
    b.ne .Linvalid_jmp23
    ldp x28, x29, [x0, #24]
    ldp x26, x27, [x0, #40]
    ldp x24, x25, [x0, #56]
    ldp x22, x23, [x0, #72]
    ldp x20, x21, [x0, #88]
    ldr x19, [x0, #104]
    ldp d14, d15, [x0, #112]
    ldp d12, d13, [x0, #128]
    ldp d10, d11, [x0, #144]
    ldp d8, d9, [x0, #160]
    mov x30, x11
    mov sp, x12
    cmp w1, wzr
    csinc w0, w1, wzr, ne
    ret
.Linvalid_jmp23:
    bl lenneth_phase23_bad_jump
    brk #0
.size lenneth_legacy_longjmp_arm64,.-lenneth_legacy_longjmp_arm64
.section .note.GNU-stack,"",%progbits
