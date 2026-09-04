/* target/fm1.ld — DRAFT link script for the fm1-polyseq firmware image.
 *
 * Flash map VERIFIED against AL-255/FM-1-RE (V13 disassembly + reassembly):
 *   app VMA   : 0x02000000 … 0x0208E59C (XIP NOR, .text + .rodata + .data)
 *   mapping   : XIP = flash_offset - 0x4120 + 0x02000000 (1 MB linear window)
 *   file 0x00000000       : UBOOT (16-64 KB)
 *   file 0x000038D0       : isd_config.ini (chip key 0x980F)
 *   file 0x00004000       : app area (JLFS, chipkey-encrypted via
 *                            jl_sfc_cipher base 0x4000): directory +
 *                            app.bin at 0x4120 (0x8E59C measured) +
 *                            cfg_tool.bin + cfg; VM/settings at flash end.
 *   ram0      : 0x1c00000 + TLB_SIZE (SDK sdk_ld_sfc.c, NO_SDRAM config)
 * This draft reserves a conservative ram slice (low ram0 holds the TLB and
 * boot info; exact sizes live in the SDK app_config).
 *
 * NOT a bootable image by itself: real boot needs the SDK crt0/uboot,
 * initcall sections, LTO plugin flags (-pi32v2-csync-before-rti) and the
 * platform liba archives. This script exists so `make image-dryrun` can
 * prove our section placement (text->rom, data/bss->ram) long before the
 * SDK link. ENTRY(main) is dry-run only; the SDK owns reset/boot. */
MEMORY
{
    rom (rx)  : ORIGIN = 0x02000000, LENGTH = 1M
    ram (rwx) : ORIGIN = 0x1c01000, LENGTH = 128K
}

ENTRY(main)

SECTIONS
{
    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
    } >rom

    .data :
    {
        *(.data .data.*)
    } >ram AT>rom

    .bss (NOLOAD) :
    {
        *(.bss .bss.*)
        *(COMMON)
    } >ram
}
