#ifndef include_hello_h
#define include_hello_h

#include <efi.h>
#include <protocol/efi-gop.h>
#include <protocol/efi-mpsp.h>
#include <stdarg.h>
#include <stddef.h>

typedef struct _HELLO *HELLO;

EFI_STATUS clear_console(HELLO hello);
EFI_STATUS create_event(HELLO hello, UINT32 type, EFI_TPL tpl, EFI_EVENT_NOTIFY notify, VOID *arg, EFI_EVENT *event);
void dump_graphic_output_mode(HELLO hello);
void dump_memory_map(HELLO hello);
EFI_GRAPHICS_OUTPUT_BLT_PIXEL *get_framebuffer(HELLO hello, UINTN *horz, UINTN *vert);
const wchar_t *get_memory_map_type_name(UINT32 type);
const wchar_t *get_pixel_format_name(EFI_GRAPHICS_PIXEL_FORMAT format);
void halt(void) __attribute__((noreturn));
HELLO init_hello(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systbl);
void lock_hello(HELLO hello);
EFI_STATUS printf(HELLO hello, const wchar_t *fmt, ...);
EFI_STATUS puts(HELLO hello, const wchar_t *str);
void reverse(wchar_t *buf, size_t len);
EFI_STATUS startup_all_aps(HELLO hello, EFI_AP_PROCEDURE proc, UINT8 single, EFI_EVENT event, UINTN timeout, VOID *arg, UINTN **failed);
void unlock_hello(HELLO hello);
EFI_STATUS vprintf(HELLO hello, const wchar_t *fmt, va_list ap);
EFI_STATUS whoami(HELLO hello, UINTN *index);

#endif /* include_hello_h */
