OUTPUT_ARCH(arm)

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

