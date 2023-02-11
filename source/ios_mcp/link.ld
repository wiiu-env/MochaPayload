OUTPUT_ARCH(arm)

INCLUDE "imports.ld"

PROVIDE(snprintf = 0x05059010);

SECTIONS
{
	.text 0x05116000 : {
		_text_start = .;
		*(.text*)
		*(.rodata*)
	}
    _text_end = .;

	.bss 0x050BD000 : {
		_bss_start = .;
		*(.bss*);
	}
	_bss_end = .;

	/DISCARD/ : {
		*(*);
	}
}

ASSERT((SIZEOF(.text)) < 0x9F00, "text section is too big");
ASSERT((SIZEOF(.bss)) < 0x3000, "bss section is too big");

