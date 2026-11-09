#ifndef UEFI_H
#define UEFI_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long usize;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef u64 efi_status;
typedef void *efi_handle;
typedef u16 efi_char16;

#define EFIAPI __attribute__((ms_abi))
#define EFI_ERROR_BIT (1ULL << 63)
#define EFI_ERROR(status) (((status) & EFI_ERROR_BIT) != 0)
#define EFI_SUCCESS 0

typedef struct {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc32;
    u32 reserved;
} efi_table_header;

typedef struct efi_boot_services efi_boot_services;

typedef struct {
    u32 version;
    u32 horizontal_resolution;
    u32 vertical_resolution;
    u32 pixel_format;
    u32 pixel_information[4];
    u32 pixels_per_scan_line;
} efi_graphics_output_mode_information;

typedef struct {
    u32 max_mode;
    u32 mode;
    efi_graphics_output_mode_information *info;
    unsigned long size_of_info;
    u64 framebuffer_base;
    unsigned long framebuffer_size;
} efi_graphics_output_protocol_mode;

typedef struct {
    void *query_mode;
    void *set_mode;
    void *blt;
    efi_graphics_output_protocol_mode *mode;
} efi_graphics_output_protocol;

typedef struct {
    u32 data1;
    u16 data2;
    u16 data3;
    u8 data4[8];
} efi_guid;

typedef u64 (EFIAPI *efi_locate_protocol_fn)(
    const efi_guid *protocol, void *registration, void **interface
);

struct efi_boot_services {
    efi_table_header header;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    void *get_memory_map;
    void *allocate_pool;
    void *free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    void *handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    void *exit_boot_services;
    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;
    efi_locate_protocol_fn locate_protocol;
};

typedef struct {
    efi_table_header header;
    efi_char16 *firmware_vendor;
    u32 firmware_revision;
    u32 padding;
    void *console_in_handle;
    void *con_in;
    void *console_out_handle;
    void *con_out;
    void *standard_error_handle;
    void *std_err;
    void *runtime_services;
    efi_boot_services *boot_services;
    unsigned long number_of_table_entries;
    void *configuration_table;
} efi_system_table;

int fb64_init(u64 address, u32 width, u32 height, u32 pitch_bytes,
              u8 red_position, u8 green_position, u8 blue_position);

#endif
