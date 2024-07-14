OUTPUT_ARCH(arm)

INCLUDE "imports.ld"

SECTIONS {
    . = 0x12431900; /* real .text section is ending at 0x12431843, Hokaku is starting at 0x12432000 */

    .text : {
        _text_start = .;
        *(.text*);
        *(.rodata*);
        *(.data*);
    }
    _text_end = .;

/*  We don't need a .bss section for now. Keep Hokaku in mind when adding .bss section.
    .bss : {
        _bss_start = .;
		*(.bss*);
	}
	_bss_end = .;*/

    /DISCARD/ : {
        *(*);
    }
}

ASSERT((0x12431900 + SIZEOF(.text)) < 0x12432000, "ios_net text is too big");
