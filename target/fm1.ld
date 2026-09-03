/* target/fm1.ld — DRAFT link script for the fm1-polyseq firmware image.
 *
 * Origins VERIFIED in fw-AC79_AIoT_SDK, cpu/wl82/sdk_ld_sfc.c (NO_SDRAM):
 *   rom(rwx)  : ORIGIN = 0x2000120  (XIP NOR flash, code executes in place)
 *   ram0(rwx) : ORIGIN = 0x1c00000 + TLB_SIZE, internal SRAM
 * OPEN QUESTION (do not flash until resolved): community research
 * (aroum/fm1-custom-fw) shows the FM-1 app partition (app.bin) starts at
 * FILE offset 0x4000 or 0x10000 per isd_config.ini, with uboot at
 * 0x00000000 and VM/settings at flash end (4/8 MB SPI flash, XIP via
 * hardware cache confirmed). Reconcile this file offset with the SDK XIP
 * remap address above (via the FM-1 isd_config + M-UPGRADE-FM1 packaging:
 * UFW container @JMUA/JLUFW, 7-bit SysEx chunks) before any flash.
 * This draft reserves a conservative ram slice (low ram0 holds the TLB and
 * boot info; exact sizes live in the SDK app_config) and a 1 MB rom window
 * (narrow to the FM-1 flash chip size once identified).
 *
 * NOT a bootable image by itself: real boot needs the SDK crt0/uboot,
 * initcall sections, LTO plugin flags (-pi32v2-csync-before-rti) and the
 * platform liba archives. This script exists so `make image-dryrun` can
 * prove our section placement (text->rom, data/bss->ram) long before the
 * SDK link. ENTRY(main) is dry-run only; the SDK owns reset/boot. */
MEMORY
{
    rom (rx)  : ORIGIN = 0x2000120, LENGTH = 1M
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
