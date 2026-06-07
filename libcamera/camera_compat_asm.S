    .syntax unified
    .text
    .align  2
    .thumb
    .thumb_func
    .global y210_fixed_mapMemory
    .type   y210_fixed_mapMemory, %function
y210_fixed_mapMemory:
    push    {r4, lr}
    mov     r4, r0
    bl      y210_mapMemory_impl
    pop     {r4, pc}
    .size   y210_fixed_mapMemory, . - y210_fixed_mapMemory
