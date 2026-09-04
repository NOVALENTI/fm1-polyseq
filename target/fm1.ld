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
 * RAM map VERIFIED (same source, with-custom-firmware link.ld):
 *   stock .bss ends 0x01C211FC; boot hwinfo struct at 0x01C7FD50.
 *   Free gap 0x01C30000 + 192K sits clear between them — this draft uses
 *   a conservative 128K slice of it, so a future blob can coexist with
 *   stock (trampoline-hook strategy) instead of replacing app.bin.
 * Package structure (same source, docs/io/11-ota-protocol.md):
 *   .fwsc = 20x0x30 header blocks (47 data + 1 marker each, strip to get
 *   the logical image) + raw rest; UFW header decrypts with HDRKEY
 *   0xFFFF to 8 entries (flash.bin, info.log, USR, isd_config.ini,
 *   ota.bin, script.ver, blimit.bin, tail.bin). ota.bin is the nested
 *   bootable that executes updates (loadaddr 0x1C0A800). cfg gate:
 *   verifier refuses identical cfg — one padding byte flip in
 *   eq_cfg_hw.bin's name field passes it (verified on hardware).
 *   Custom-image packaging = replicate this container (open work).
 *   (Previous draft used 0x1c01000, which collides with stock .bss.)
 *
 * SAFER-FIRST-FLASH PATTERN (same source): append blob in app.bin's
 * font/bitmap region (XIP-mapped), run from RAM gap, hook stock via
 * 6-byte trampolines over verified function entries (e.g. usr_app_task
 * entry for install, midi_msg_dispatch for MIDI parse). Stock updater
 * and bootloader stay intact, preserving the OTA return path.
 *
 * NOT a bootable image by itself: real boot needs the SDK crt0/uboot,
 * initcall sections, LTO plugin flags (-pi32v2-csync-before-rti) and the
 * platform liba archives. This script exists so `make image-dryrun` can
 * prove our section placement (text->rom, data/bss->ram) long before the
 * SDK link. ENTRY(main) is dry-run only; the SDK owns reset/boot. */
MEMORY
{
    rom (rx)  : ORIGIN = 0x02000000, LENGTH = 1M
    ram (rwx) : ORIGIN = 0x01C30000, LENGTH = 128K
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
