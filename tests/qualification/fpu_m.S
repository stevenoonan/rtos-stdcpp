/* SPDX-License-Identifier: MIT */
.syntax unified
.thumb
.text
.global rtos_test_fpu_probe
.type rtos_test_fpu_probe, %function
.thumb_func
/* Keep ALL FP registers live across several tick interrupts. No calls or
 * compiler spills can hide a broken automatic/ThreadX FP context save. */
rtos_test_fpu_probe:
    push {r4-r6, lr}
    vpush {s16-s31}
    mov r4, r0
    mov r5, r1
    mov r6, r2
    vldmia r4, {s0-s31}
    ldr r1, [r6]
1:
    ldr r0, [r6]
    subs r0, r0, r1
    cmp r0, #4
    blo 1b
    vstmia r5, {s0-s31}
    vpop {s16-s31}
    pop {r4-r6, pc}
.size rtos_test_fpu_probe, .-rtos_test_fpu_probe
