// SPDX-License-Identifier: GPL-2.0
#include <pg_table.h>
#include <page.h>
#include <memory.h>

OUTPUT_ARCH(aarch64)
ENTRY(_stext)

SECTIONS
{
    /DISCARD/ : {
		*(.eh_frame .eh_frame.*)
	}

    . = IMAGE_VADDR;

    .text : ALIGN(SEGMENT_ALIGN) {	/* Real text segment		*/
        __relocate_copy_start = .;
		_stext = .;		/* Text and read-only data	*/
        KEEP(*(.text.boot))
        *(.text .text.*)
    }
    . = ALIGN(SEGMENT_ALIGN);
	_etext = .;			/* End of text section */

    .rodata : ALIGN(SEGMENT_ALIGN) {
        __start_rodata = .;
        *(.rodata .rodata.*)
        . = ALIGN(PAGE_SIZE);
        __end_rodata = .;
    }

    .got : { *(.got) }
	/*
	 * Make sure that the .got.plt is either completely empty or it
	 * contains only the lazy dispatch entries.
	 */
	.got.plt : { *(.got.plt) }
	ASSERT(SIZEOF(.got.plt) == 0 || SIZEOF(.got.plt) == 0x18,
	       "Unexpected GOT/PLT entries detected!")

    .rodata.text : {
        . = ALIGN(SZ_4K);
        __idmap_text_start = .;
        *(.idmap.text)
        __idmap_text_end = .;
        . = ALIGN(PAGE_SIZE);
    }

    . = ALIGN(PAGE_SIZE);

    reserved_pg_dir = .;
	. += PAGE_SIZE;

	swapper_pg_dir = .;
	. += PAGE_SIZE;

    . = ALIGN(SEGMENT_ALIGN);

    __init_begin = .;
    __inittext_begin = .;
    . = ALIGN(8);
	.init.text : AT(ADDR(.init.text)) {
		_sinittext = .;
		*(.init.text .init.text.*)
	    *(.text.startup)
		_einittext = .;
	}
    __exittext_begin = .;
    __exittext_end = .;
    . = ALIGN(SEGMENT_ALIGN);
	__inittext_end = .;

    __initdata_begin = .;

    __pi_init_idmap_pg_dir = .;
	. += (INIT_IDMAP_DIR_SIZE);
	__pi_init_idmap_pg_end = .;

    .rela.dyn : ALIGN(8) {
		__pi_rela_start = .;
		*(.rela .rela*)
		__pi_rela_end = .;
	}

    . = ALIGN(SEGMENT_ALIGN);
	__initdata_end = .;
    __init_end = .;

    _data = .;
	_sdata = .;

    .data :
    {
        . = ALIGN(THREAD_ALIGN);
        __start_init_stack = .;
        init_thread_union = .;
        init_stack = .;
        KEEP(*(.data..init_thread_info))
        . = __start_init_stack + THREAD_SIZE;
        __end_init_stack = .;

        *(.data .data.*)
    }
    . = ALIGN(PAGE_SIZE);
     _edata = .;

    /*
     * Embedded user/process image.
     *
     * This is not kernel text/data.
     * It is an opaque payload carried by the kernel image.
     */
    .process_blob : ALIGN(PAGE_SIZE)
    {
        __process_blob_start = .;
        KEEP(*(.process_blob))
        __process_blob_end = .;
    }

    __relocate_copy_end = .;

    . = ALIGN(PAGE_SIZE);

    __bss_start = .;
    .bss  : { *(.bss .bss.*) *(COMMON) }
    .sbss : { *(.sbss .sbss.*) }
    . = ALIGN(PAGE_SIZE);
    __bss_end = .;

	__pi_init_pg_dir = .;
	. += INIT_DIR_SIZE;
	__pi_init_pg_end = .;

    . += SZ_4K;		/* stack for the early C runtime */
    early_init_stack = .;

    . = ALIGN(SEGMENT_ALIGN);

    kimage_end = .;
    _end = .;
    kimage_limit = ALIGN(ABSOLUTE(_end + SZ_64K), SZ_2M);
}
