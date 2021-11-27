#include "hello.h"

static void ApMain(void *arg) {
  HELLO hello = arg;
  UINTN index;
  EFI_STATUS status = whoami(hello, &index);
  lock_hello(hello);
  if (status & EFI_ERR)
    printf(hello, L"failed to whoami, 0x%lx\r\n", status);
  else
    printf(hello, L"AP%lu\r\n", index);
  unlock_hello(hello);
  halt();
}

static void Notify(EFI_EVENT event, void *arg) {
}

EFI_STATUS EfiMain(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systbl) {
  HELLO hello = init_hello(handle, systbl);
  EFI_EVENT evt;
  EFI_STATUS status = create_event(hello, EVT_NOTIFY_WAIT, TPL_NOTIFY, Notify, hello, &evt);
  if (status & EFI_ERR)
    printf(hello, L"failed to create event, 0x%lx\r\n", status);
  status = startup_all_aps(hello, ApMain, EFI_FALSE, evt, 0, hello, NULL);
  if (status & EFI_ERR)
    printf(hello, L"failed to startup all APs, 0x%lx\r\n", status);
  else
    ApMain(hello);
  return EFI_SUCCESS;
}
