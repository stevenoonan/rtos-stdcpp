/* SPDX-License-Identifier: MIT */
.syntax unified
.thumb
.section .vectors,"a",%progbits
.global vectors
vectors:
.word _estack, Reset_Handler, fault_handler, fault_handler
.word fault_handler, fault_handler, fault_handler, 0, 0, 0, 0
#if !defined(RTOS_STDCPP_BACKEND_THREADX)
.word SVC_Handler, fault_handler, 0, PendSV_Handler, SysTick_Handler
#elif __ARM_ARCH == 8
.word fault_handler, fault_handler, 0, PendSV_Handler, SysTick_Handler
#else
.word fault_handler, fault_handler, 0, __tx_PendSVHandler, SysTick_Handler
#endif
.rept 96
.word fault_handler
.endr
.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.thumb_func
Reset_Handler:
    cpsid i
    bl platform_start
    b .
#ifdef RTOS_STDCPP_BACKEND_THREADX
.global SysTick_Handler
.thumb_func
SysTick_Handler:
    push {r0, lr}
    bl rtos_test_threadx_tick
    bl _tx_timer_interrupt
    pop {r0, lr}
    bx lr
#endif
.global fault_handler
.thumb_func
fault_handler:
    tst lr, #4
    ite eq
    mrseq r0, msp
    mrsne r0, psp
    b platform_fault
