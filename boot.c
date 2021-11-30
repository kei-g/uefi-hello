#include "hello.h"

static void ApMain(void *arg) {
  HELLO hello = arg;
  UINTN index;
  EFI_STATUS status = whoami(hello, &index);
  if (status & EFI_ERR) {
    lock_hello(hello);
    printf(hello, L"failed to whoami, 0x%lx\r\n", status);
    unlock_hello(hello);
    return;
  }
  for (int i = 0; i < 8; i++) {
    lock_hello(hello);
    if (index == 0)
      printf(hello, L"BSP: %d\r\n", i);
    else
      printf(hello, L"AP%lu: %d\r\n", index, i);
    unlock_hello(hello);
    __asm__("outb %%al,$0x80"
            :
            :
            : "cc", "memory");
  }
}

static void Notify(EFI_EVENT event, void *arg) {
}

static void callback(HELLO hello, UINTN mode, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info, void *arg) {
  printf(hello, L"%lu: %s, %u x %u\r\n", mode, get_pixel_format_name(info->PixelFormat), info->HorizontalResolution, info->VerticalResolution);
}

static void cleanup_preceding_event(HELLO hello, EFI_EVENT *evt) {
  if (*evt == NULL)
    unlock_hello(hello);
  else {
    printf(hello, L"waiting for preceding executions...\r\n");
    unlock_hello(hello);

    UINTN index;
    EFI_STATUS status = wait_for_event(hello, 1, evt, &index);
    if (status & EFI_ERR) {
      lock_hello(hello);
      printf(hello, L"failed to wait for an event, 0x%lx\r\n", status);
      unlock_hello(hello);
    }

    status = close_event(hello, *evt);
    *evt = NULL;
    if (status & EFI_ERR) {
      lock_hello(hello);
      printf(hello, L"failed to close an event, 0x%lx\r\n", status);
      unlock_hello(hello);
    }
  }

  EFI_STATUS status = create_event(hello, EVT_NOTIFY_WAIT, TPL_NOTIFY, Notify, hello, evt);
  if (status & EFI_ERR) {
    lock_hello(hello);
    printf(hello, L"failed to create an event, 0x%lx\r\n", status);
    unlock_hello(hello);
    halt();
  }
}

static void test_all_APs(HELLO hello, EFI_EVENT *evt) {
  cleanup_preceding_event(hello, evt);
  EFI_STATUS status = startup_all_aps(hello, ApMain, EFI_FALSE, *evt, 0, hello, NULL);
  if (status & EFI_ERR) {
    lock_hello(hello);
    printf(hello, L"failed to startup all APs, 0x%lx\r\n", status);
    unlock_hello(hello);
    status = close_event(hello, *evt);
    if (EFI_ERR) {
      lock_hello(hello);
      printf(hello, L"failed to close an event, 0x%lx\r\n", status);
      unlock_hello(hello);
    }
    *evt = NULL;
  }
  lock_hello(hello);
}

EFI_STATUS EfiMain(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systbl) {
  HELLO hello = init_hello(handle, systbl);
  for (EFI_EVENT evt = NULL;;) {
    lock_hello(hello);
    printf(hello, L"waiting for a key event...\r\n");
    unlock_hello(hello);
    UINT16 uni;
    EFI_STATUS status = wait_for_key_event(hello, NULL, &uni);
    lock_hello(hello);
    if (status & EFI_ERR)
      printf(hello, L"failed to wait for a key event, 0x%lx\r\n", status), unlock_hello(hello), halt();
    switch (uni) {
      case L'a':
        test_all_APs(hello, &evt);
        break;
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
