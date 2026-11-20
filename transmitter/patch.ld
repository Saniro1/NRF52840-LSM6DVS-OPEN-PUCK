ENTRY(puck_transmit)

SECTIONS
{
    .text 0x3b000 : {
        KEEP(*(.text.puck_transmit))
        KEEP(*(.text.puck_publish))
        *(.text .text.*)
        *(.rodata .rodata.*)
    }
    .data : { *(.data .data.*) }
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.ARM.exidx* .ARM.extab*) }
}

ASSERT(SIZEOF(.text) <= 0x1000, "Radio patch exceeds the reserved flash page")
ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "Radio patch allocates uninitialized RAM")
