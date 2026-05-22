// SPDX-License-Identifier: GPL-2.0
#include <user_mem_layout.h>

OUTPUT_ARCH(aarch64)
ENTRY(_proc_start)

SECTIONS
{
    . = USER_IMAGE_BASE;

    __proc_image_start = .;

    .text : ALIGN(PAGE_SIZE)
    {
        __proc_text_start = .;
        KEEP(*(.text.entry))
        *(.text .text.*)
        __proc_text_end = .;
    }

    .rodata : ALIGN(PAGE_SIZE)
    {
        __proc_rodata_start = .;
        *(.rodata .rodata.*)
        __proc_rodata_end = .;
    }

    .data : ALIGN(PAGE_SIZE)
    {
        __proc_data_start = .;
        *(.data .data.*)
        __proc_data_end = .;
    }

    . = ALIGN(PAGE_SIZE);
    __proc_file_end = .;

    .bss (NOLOAD) : ALIGN(PAGE_SIZE)
    {
        __proc_bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __proc_bss_end = .;
    }

    . = ALIGN(PAGE_SIZE);
    __proc_image_end = .;

    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.comment)
        *(.note .note.*)
    }
}