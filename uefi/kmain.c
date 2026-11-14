#include "uefi.h"
#include "../drivers/framebuffer.h"

static const efi_guid graphics_output_guid = {
    0x9042a9de, 0x23dc, 0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}
};

static void efi_print(efi_system_table *system_table, const efi_char16 *text)
{
    typedef efi_status (EFIAPI *output_string_fn)(void *, efi_char16 *);
    void *console = system_table->con_out;
    output_string_fn output_string =
        *(output_string_fn *)((u8 *)console + sizeof(void *));
    (void)output_string(console, (efi_char16 *)text);
}

efi_status EFIAPI efi_main(void *image_handle,
                           efi_system_table *system_table)
{
    (void)image_handle;
    efi_print(system_table, (const efi_char16 *)u"Starting x86-64 UEFI kernel...\r\n");

    efi_graphics_output_protocol *gop = 0;
    efi_status status = system_table->boot_services->locate_protocol(
        &graphics_output_guid, 0, (void **)&gop
    );
    if (EFI_ERROR(status) || !gop || !gop->mode || !gop->mode->info) {
        efi_print(system_table, (const efi_char16 *)u"Unable to locate UEFI GOP.\r\n");
        return status;
    }

    typedef efi_status (EFIAPI *gop_set_mode_fn)(
        efi_graphics_output_protocol *, u32
    );
    gop_set_mode_fn set_mode = (gop_set_mode_fn)gop->set_mode;
    status = set_mode(gop, gop->mode->mode);
    if (EFI_ERROR(status)) {
        efi_print(system_table, (const efi_char16 *)u"Unable to reapply UEFI GOP mode.\r\n");
        return status;
    }

    efi_graphics_output_mode_information *info = gop->mode->info;
    u8 red_position = 16;
    u8 green_position = 8;
    u8 blue_position = 0;
    if (info->pixel_format == 0) {
        red_position = 0;
        green_position = 8;
        blue_position = 16;
    } else if (info->pixel_format != 1) {
        efi_print(system_table, (const efi_char16 *)u"Unsupported UEFI pixel format.\r\n");
        return 1;
    }

    if (!fb64_init(gop->mode->framebuffer_base,
                   info->horizontal_resolution,
                   info->vertical_resolution,
                   info->pixels_per_scan_line * 4,
                   red_position, green_position, blue_position)) {
        efi_print(system_table, (const efi_char16 *)u"UEFI framebuffer is too small.\r\n");
        return 1;
    }

    fb_clear(FB_WHITE, FB_BLACK);
    fb_write_string("Hello from sivajiOS!",
                    0, 0, FB_GREEN, FB_BLACK);
    fb_write_string("Framebuffer driver is working!",
                    2, 0, FB_LIGHT_GREY, FB_BLACK);
    fb_write_string("Talking directly to hardware.",
                    4, 0, FB_CYAN, FB_BLACK);
    fb_write_string("Running in x86-64 UEFI long mode.",
                    6, 0, FB_GREEN, FB_BLACK);

    for (;;)
        __asm__ volatile ("hlt");
}
