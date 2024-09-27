OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    ram : ORIGIN = 0x00000000, LENGTH = 0x003F0000
}

SECTIONS
{
    .crt0 0x00000000 : { *(.crt0*) } > ram
    .text : { *(.text*) } > ram
    .bss : { *(.bss*) } > ram
}

