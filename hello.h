#ifndef include_hello_h
#define include_hello_h

#include <efi.h>
#include <protocol/efi-gop.h>
#include <protocol/efi-mpsp.h>
#include <stdarg.h>
#include <stddef.h>

typedef struct _HELLO *HELLO;

typedef void (*EFIABI HELLO_QUERY_GRAPHIC_OUTPUT_MODE_CALLBACK)(HELLO hello, UINTN mode, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info, void *arg);

EFI_STATUS EFIABI clear_console(HELLO hello);
EFI_STATUS EFIABI close_event(HELLO hello, EFI_EVENT event);
EFI_STATUS EFIABI create_event(HELLO hello, UINT32 type, EFI_TPL tpl, EFI_EVENT_NOTIFY notify, VOID *arg, EFI_EVENT *event);
void EFIABI dump_graphic_output_mode(HELLO hello);
void EFIABI dump_memory_map(HELLO hello);
EFI_GRAPHICS_OUTPUT_BLT_PIXEL *EFIABI get_framebuffer(HELLO hello, UINTN *horz, UINTN *vert);
const wchar_t *EFIABI get_memory_map_type_name(UINT32 type);
const wchar_t *EFIABI get_pixel_format_name(EFI_GRAPHICS_PIXEL_FORMAT format);
void EFIABI halt(void) __attribute__((noreturn));
HELLO EFIABI init_hello(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systbl);
void EFIABI lock_hello(HELLO hello);
EFI_STATUS EFIABI printf(HELLO hello, const wchar_t *fmt, ...);
EFI_STATUS EFIABI puts(HELLO hello, const wchar_t *str);
EFI_STATUS EFIABI query_graphic_output_modes(HELLO hello, HELLO_QUERY_GRAPHIC_OUTPUT_MODE_CALLBACK callback, void *arg);
void EFIABI reverse(wchar_t *buf, size_t len);
EFI_STATUS EFIABI startup_all_aps(HELLO hello, EFI_AP_PROCEDURE proc, UINT8 single, EFI_EVENT event, UINTN timeout, VOID *arg, UINTN **failed);
EFI_STATUS EFIABI switch_graphic_output_mode(HELLO hello, UINTN mode);
void EFIABI unlock_hello(HELLO hello);
EFI_STATUS EFIABI vprintf(HELLO hello, const wchar_t *fmt, va_list ap);
EFI_STATUS EFIABI wait_for_event(HELLO hello, UINTN num, EFI_HANDLE *events, UINTN *index);
EFI_STATUS EFIABI wait_for_key_event(HELLO hello, UINT16 *scan, UINT16 *uni);
EFI_STATUS EFIABI whoami(HELLO hello, UINTN *index);

#endif /* include_hello_h */
