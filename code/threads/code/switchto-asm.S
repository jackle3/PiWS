#include "rpi-asm.h"

@ all of these are called from <switchto.h>: look there for more
@ help in understanding if need to.

@ drop in your code from last lab to get sp and lr.
@
@ void priv_get_sp_lr_asm(uint32_t mode, uint32_t *sp, uint32_t *lr);
@   - assumes caller is at privileged mode [we could check this]
@   - mode in r0
@   - sp pointer in r1
@   - lr pointer in r2
@
MK_FN(priv_get_sp_lr_asm)
    mrs r3, cpsr            @ save current mode

    push {r4}             @ save r4 before using it
    bic r4, r3, #0b11111  @ mask out just the mode bits
    orr r0, r4, r0        @ preserve flags from original cpsr, set mode bits
    msr cpsr_c, r0        @ switch to requested mode with preserved flags
    prefetch_flush(r12)   @ flush unused caller-saved reg
    str sp, [r1]          @ store sp in memory at r1
    str lr, [r2]          @ store lr in memory at r2
    msr cpsr_c, r3        @ restore original mode
    prefetch_flush(r12)   @ flush
    pop {r4}              @ restore r4 before returning
    bx lr
    asm_not_reached();


@ drop in your code from lab 13 [in 1-rfe-asm.S]
@
@ void switchto_user_asm(regs_t *r)  __attribute__((noreturn));
@  - <r> is a 17-entry register entry in r0.
@
@ switch to user mode loading r0-r15, and cpsr from <r>
MK_FN(switchto_user_asm)
        mov sp, r0           
    @ Load r0-14 using ^ modifier
    ldm r0, {r0-r12, sp, lr}^

    @ sp and lr are shadow registers, so we can still use them
    
    add sp, #60   @ point to pc,cpsr (15 * 4 = 60)
    rfe sp          
    asm_not_reached();

@ drop in your code from lab 13
@ 
@ void switchto_priv_asm(regs_t *r)  __attribute__((noreturn));
MK_FN(switchto_priv_asm)
    mov r1, r0          
    ldr r0, [r0, #64]    @ r0 = mode
    @ and r0, r0, #0b11111 @ mask out just the mode bits

    msr cpsr, r0         @ switch to requested mode
    prefetch_flush(r12)    @ flush unused caller-saved reg

    ldm r1, {r0-r14, pc}
    asm_not_reached();

@ you'll have to write this, but it should mirror your other save
@ routines.    
@
@ void cswitchto_priv_asm(regs_t *old, regs_t *next);
@ 
@ voluntary context switch: save the current registers in <old>
@ and call the right switchto on <next>
MK_FN(cswitchto_priv_asm)
    @ initially just forward to ours.
    @ b staff_cswitchto_priv_asm

    str r4, [r0, #(4*4)]
    str r5, [r0, #(5*4)]
    str r6, [r0, #(6*4)]
    str r7, [r0, #(7*4)]
    str r8, [r0, #(8*4)]
    str r9, [r0, #(9*4)]
    str r10, [r0, #(10*4)]
    str r11, [r0, #(11*4)]
    str r12, [r0, #(12*4)]
    str r13, [r0, #(13*4)]
    str r14, [r0, #(14*4)]
    str lr, [r0, #(15*4)]

    mrs r12, cpsr
    str r12, [r0, #(16*4)]  @ cpsr at offset 16



    @ 2. just call the right switch routine: note, we should
    @    *not* reach this location when switching back!
    @ move r1 to r0
    mov r0, r1
    bl switchto_priv_asm
    asm_not_reached();

@ you'll have to write this, but it should mirror your other save
@ routines.    
@
@ void cswitchto_user_asm(regs_t *old, regs_t *next);
@ 
@ voluntary context switch: save the current registers in <old>
@ and call the right switchto on <next>
MK_FN(cswitchto_user_asm)

    @ b staff_cswitchto_user_asm
    str r4, [r0, #(4*4)]
    str r5, [r0, #(5*4)]
    str r6, [r0, #(6*4)]
    str r7, [r0, #(7*4)]
    str r8, [r0, #(8*4)]
    str r9, [r0, #(9*4)]
    str r10, [r0, #(10*4)]
    str r11, [r0, #(11*4)]
    str r12, [r0, #(12*4)]
    str r13, [r0, #(13*4)]
    str r14, [r0, #(14*4)]
    str lr, [r0, #(15*4)]

    mrs r12, cpsr
    str r12, [r0, #(16*4)]  @ cpsr at offset 16

    mov r0, r1

    @ asm_todo("save the registers: identical to <switchto_priv_asm>")
    bl switchto_user_asm
    asm_not_reached();
