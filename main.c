#include "hello.h"

static void ApMain(void *arg) {
  HELLO hello = arg;
  UINTN index;
  EFI_STATUS status = whoami(hello, &index);
  lock_hello(hello);
  if (status & EFI_ERR)
    printf(hello, L"failed to whoami, 0x%lx\r\n", status);
  else if (index == 0)
    printf(hello, L"BSP\r\n");
  else
    printf(hello, L"AP%lu\r\n", index);
  unlock_hello(hello);
}

static void Notify(EFI_EVENT event, void *arg) {
}

static void callback(HELLO hello, UINTN mode, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info, void *arg) {
  printf(hello, L"%lu: %s, %u x %u\r\n", mode, get_pixel_format_name(info->PixelFormat), info->HorizontalResolution, info->VerticalResolution);
}

EFI_STATUS EfiMain(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systbl) {
  HELLO hello = init_hello(handle, systbl);

  EFI_EVENT evt;
  EFI_STATUS status = create_event(hello, EVT_NOTIFY_WAIT, TPL_NOTIFY, Notify, hello, &evt);
  if (status & EFI_ERR)
    printf(hello, L"failed to create event, 0x%lx\r\n", status), halt();
  status = startup_all_aps(hello, ApMain, EFI_FALSE, evt, 0, hello, NULL);
  if (status & EFI_ERR)
    printf(hello, L"failed to startup all APs, 0x%lx\r\n", status), halt();

  for (int i = 0; i < 1024 * 1024; i++)
    __asm__("pause");

  for (;;) {
    lock_hello(hello);
    printf(hello, L"waiting for a key event...\r\n");
    unlock_hello(hello);
    UINT16 uni;
    EFI_STATUS status = wait_for_key_event(hello, NULL, &uni);
    lock_hello(hello);
    if (status & EFI_ERR)
      printf(hello, L"failed to wait for a key event, 0x%lx\r\n", status), unlock_hello(hello), halt();
    switch (uni) {
      case L'g':
        dump_graphic_output_mode(hello);
        break;
      case L'm':
        dump_memory_map(hello);
        break;
      case L'q':
        query_graphic_output_modes(hello, callback, NULL);
        break;
    }
    printf(hello, L"----\r\n");
    unlock_hello(hello);
  }

  return EFI_SUCCESS;
}
