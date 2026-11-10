/* SPDX-License-Identifier: MIT */
.syntax unified
.arm
.fpu vfpv3
.text
.global rtos_test_fpu_probe
.type rtos_test_fpu_probe, %function
rtos_test_fpu_probe:
    push {r4-r6, lr}
    vpush {d8-d15}
    mov r4, r0
    mov r5, r1
    mov r6, r2
    vldmia r4!, {d0-d15}
    vldmia r4, {d16-d31}
    ldr r1, [r6]
1:
    ldr r0, [r6]
    sub r0, r0, r1
    cmp r0, #4
    blo 1b
    vstmia r5!, {d0-d15}
    vstmia r5, {d16-d31}
    vpop {d8-d15}
    pop {r4-r6, pc}
.size rtos_test_fpu_probe, .-rtos_test_fpu_probe
