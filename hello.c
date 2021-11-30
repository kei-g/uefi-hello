#include "hello.h"

#define tsprintf(T, dst, radix, ap)                 \
  {                                                 \
    const T value = va_arg(ap, T);                  \
    T c = value < 0 ? -value : value;               \
    for (int i = 0;;) {                             \
      if (c == 0) {                                 \
        if (value < 0)                              \
          dst[i++] = L'-';                          \
        else if (value == 0)                        \
          dst[i++] = L'0';                          \
        reverse(dst, i);                            \
        dst[i++] = L'\0';                           \
        break;                                      \
      }                                             \
      const T d = c % radix;                        \
      dst[i++] = d < 10 ? L'0' + d : L'a' + d - 10; \
      c /= radix;                                   \
    }                                               \
  }

struct _HELLO {
  UINTN dscsiz;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  EFI_HANDLE handle;
  uintptr_t locked;
  UINTN mapkey;
  UINTN mapsiz;
  EFI_MEMORY_DESCRIPTOR *memmap;
  EFI_MP_SERVICES_PROTOCOL *mpsp;
  UINTN num_enabled_proc;
  UINTN num_proc;
  EFI_SYSTEM_TABLE *systbl;
};

EFI_STATUS clear_console(HELLO hello) {
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *stdout = hello->systbl->ConOut;
  EFI_STATUS status = (*stdout->ClearScreen)(stdout);
  if (status & EFI_ERR)
    printf(hello, L"failed to clear screen, 0x%lx\r\n", status);
  return status;
}

EFI_STATUS create_event(HELLO hello, UINT32 type, EFI_TPL tpl, EFI_EVENT_NOTIFY notify, VOID *ctx, EFI_EVENT *event) {
  return (*hello->systbl->BootServices->CreateEvent)(type, tpl, notify, ctx, event);
}

void dump_graphic_output_mode(HELLO hello) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = hello->gop->Mode;
  printf(hello, L"gop.framebuffer: %p - %p\r\n", mode->FrameBufferBase, mode->FrameBufferBase + mode->FrameBufferSize);
  printf(hello, L"gop.mode: %u/%u\r\n", mode->Mode, mode->MaxMode);
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = mode->Info;
  printf(hello, L"gop.pixelformat: %s\r\n", get_pixel_format_name(info->PixelFormat));
  printf(hello, L"gop.resolution: %u x %u\r\n", info->HorizontalResolution, info->VerticalResolution);
  printf(hello, L"gop.scanline: %u pixels\r\n", info->PixelsPerScanLine);
}

void dump_memory_map(HELLO hello) {
  uintptr_t cur = (uintptr_t)hello->memmap;
  uintptr_t end = cur + hello->mapsiz;
  for (UINTN i = 0; cur < end; cur += hello->dscsiz, i++) {
    const EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)cur;
    printf(hello, L"phy=%p, virt=%p, %lu pages, %s\r\n", d->PhysicalStart, d->VirtualStart, d->NumberOfPages, get_memory_map_type_name(d->Type));
  }
}

EFI_GRAPHICS_OUTPUT_BLT_PIXEL *get_framebuffer(HELLO hello, UINTN *horz, UINTN *vert) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = hello->gop->Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = mode->Info;
  if (horz)
    *horz = info->HorizontalResolution;
  if (vert)
    *vert = info->VerticalResolution;
  return (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)mode->FrameBufferBase;
}

const wchar_t *get_memory_map_type_name(UINT32 type) {
  switch (type) {
    case EfiReservedMemoryType:
      return L"ReservedMemoryType";
    case EfiLoaderCode:
      return L"LoaderCode";
    case EfiLoaderData:
      return L"LoaderData";
    case EfiBootServicesCode:
      return L"BootServicesCode";
    case EfiBootServicesData:
      return L"BootServicesData";
    case EfiRuntimeServicesCode:
      return L"RuntimeServicesCode";
    case EfiRuntimeServicesData:
      return L"RuntimeServicesData";
    case EfiConventionalMemory:
      return L"ConventionalMemory";
    case EfiUnusableMemory:
      return L"UnusableMemory";
    case EfiACPIReclaimMemory:
      return L"ACPIReclaimMemory";
    case EfiACPIMemoryNVS:
      return L"ACPIMemoryNVS";
    case EfiMemoryMappedIO:
      return L"MemoryMappedIO";
    case EfiMemoryMappedIOPortSpace:
      return L"MemoryMappedIOPortSpace";
    case EfiPalCode:
      return L"PalCode";
    case EfiPersistentMemory:
      return L"PersistentMemory";
    default:
      return L"InvalidMemoryType";
  }
}

const wchar_t *get_pixel_format_name(EFI_GRAPHICS_PIXEL_FORMAT format) {
  switch (format) {
    case PixelRedGreenBlueReserved8BitPerColor:
      return L"RGB";
    case PixelBlueGreenRedReserved8BitPerColor:
      return L"BGR";
    case PixelBitMask:
      return L"BitMask";
    case PixelBltOnly:
      return L"BltOnly";
    default:
      return L"InvalidPixelFormat";
  }
}

void halt(void) {
  for (;;)
    __asm__("hlt");
}

HELLO init_hello(EFI_HANDLE handle, EFI_SYSTEM_TABLE *systbl) {
  UINTN bufsiz = 0, mapkey, dscsiz;
  UINT32 dscver;
  EFI_STATUS status = (*systbl->BootServices->GetMemoryMap)(&bufsiz, NULL, &mapkey, &dscsiz, &dscver);
  if (status != EFI_BUFFER_TOO_SMALL)
    (*systbl->ConOut->OutputString)(systbl->ConOut, L"failed to get memory map\r\n"), halt();
  UINTN poolsiz = ((sizeof(struct _HELLO) + 4095) & ~4095) + bufsiz + ((sizeof(EFI_MEMORY_DESCRIPTOR) + 7) & ~7) * dscsiz;
  void *pool;
  status = (*systbl->BootServices->AllocatePool)(EfiLoaderData, poolsiz, &pool);
  if (status & EFI_ERR)
    (*systbl->ConOut->OutputString)(systbl->ConOut, L"failed to allocate pool\r\n"), halt();
  HELLO hello = pool;
  hello->handle = handle;
  hello->memmap = (EFI_MEMORY_DESCRIPTOR *)((((uintptr_t)hello) + 4095) & ~4095);
  hello->systbl = systbl;
  printf(hello, L"firmware.vendor=%s\r\n", systbl->FirmwareVendor);
  printf(hello, L"firmware.revision=0x%x\r\n", systbl->FirmwareRevision);
  printf(hello, L"memory pool has been allocated %lu bytes at %p\r\n", poolsiz, pool);
  bufsiz = poolsiz - sizeof(struct _HELLO);
  status = (*systbl->BootServices->GetMemoryMap)(&bufsiz, hello->memmap, &hello->mapkey, &hello->dscsiz, &dscver);
  if (status & EFI_ERR)
    printf(hello, L"failed to get memory map, 0x%lx\r\n", status), halt();
  hello->mapsiz = bufsiz;
  printf(hello, L"memmap.address: %p\r\n", hello->memmap);
  printf(hello, L"memmap.bufsiz: %lu bytes\r\n", bufsiz);
  printf(hello, L"memmap.dscsiz: %lu bytes\r\n", hello->dscsiz);
  printf(hello, L"memmap.dscver: 0x%x\r\n", dscver);
  printf(hello, L"memmap.mapkey: 0x%lx\r\n", hello->mapkey);
  EFI_GUID guids[] = {
      EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
      EFI_MP_SERVICES_PROTOCOL_GUID,
  };
  VOID *gop;
  status = (*systbl->BootServices->LocateProtocol)(&guids[0], NULL, &gop);
  if (status & EFI_ERR)
    printf(hello, L"failed to locate graphic output protocol, 0x%lx\r\n", status), halt();
  hello->gop = gop;
  hello->locked = 0;
  VOID *mpsp = NULL;
  status = (*systbl->BootServices->LocateProtocol)(&guids[1], NULL, &mpsp);
  if (status & EFI_ERR)
    printf(hello, L"failed to locate mpsp protocol, 0x%lx\r\n", status);
  hello->mpsp = mpsp;
  if (hello->mpsp) {
    status = (*hello->mpsp->GetNumberOfProcessors)(hello->mpsp, &hello->num_proc, &hello->num_enabled_proc);
    if (status & EFI_ERR)
      printf(hello, L"failed to get number of processors, 0x%lx\r\n", status);
  }
  return hello;
}

void lock_hello(HELLO hello) {
  __asm__(
      "mov (%%rdx),%%rax\n"
      "test %%rax,%%rax\n"
      "jz try_lock%=\n"
      "head%=:\n"
      "pause\n"
      "mov (%%rdx),%%rax\n"
      "test %%rax,%%rax\n"
      "jnz head%=\n"
      "try_lock%=:\n"
      "lock; cmpxchg %%rcx,(%%rdx)\n"
      "jnz head%=\n"
      :
      : "c"(1), "d"(&hello->locked)
      : "cc", "memory");
}

EFI_STATUS printf(HELLO hello, const wchar_t *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  EFI_STATUS status = vprintf(hello, fmt, ap);
  va_end(ap);
  return status;
}

EFI_STATUS puts(HELLO hello, const wchar_t *str) {
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *stdout = hello->systbl->ConOut;
  EFI_STATUS status = (*stdout->OutputString)(stdout, (CHAR16 *)str);
  return status;
}

EFI_STATUS query_graphic_output_modes(HELLO hello, HELLO_QUERY_GRAPHIC_OUTPUT_MODE_CALLBACK callback, void *arg) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = hello->gop;
  for (UINTN mode = 0; mode < gop->Mode->MaxMode; mode++) {
    UINTN size;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    EFI_STATUS status = (*gop->QueryMode)(gop, mode, &size, &info);
    if (status & EFI_ERR)
      return status;
    (*callback)(hello, mode, info, arg);
  }
  return EFI_SUCCESS;
}

void reverse(wchar_t *buf, size_t len) {
  for (size_t i = 0, j = len - 1; i < len / 2; i++, j--) {
    const wchar_t c = buf[i];
    buf[i] = buf[j];
    buf[j] = c;
  }
}

EFI_STATUS startup_all_aps(HELLO hello, EFI_AP_PROCEDURE proc, UINT8 single, EFI_EVENT event, UINTN timeout, VOID *arg, UINTN **failed) {
  EFI_MP_SERVICES_PROTOCOL *mpsp = hello->mpsp;
  EFI_BOOT_SERVICES *bs = hello->systbl->BootServices;
  return mpsp ? (*mpsp->StartupAllAPs)(mpsp, proc, single, event, timeout, arg, failed) : ((*proc)(arg), (*bs->SignalEvent)(event), 1);
}

EFI_STATUS switch_graphic_output_mode(HELLO hello, UINTN mode) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = hello->gop;
  return (*gop->SetMode)(gop, mode);
}

void unlock_hello(HELLO hello) {
  __asm__("mov %%rax,(%%rdx)"
          :
          : "a"(0), "d"(&hello->locked)
          : "cc", "memory");
}

EFI_STATUS vprintf(HELLO hello, const wchar_t *fmt, va_list ap) {
  for (const wchar_t *ptr = fmt; *ptr != L'\0'; ptr++) {
    wchar_t buf[32];
    EFI_STATUS status;
    if (*ptr == L'%') {
      ptr++;
      switch (*ptr) {
        case L'c':
          buf[0] = (wchar_t)va_arg(ap, int);
          buf[1] = L'\0';
          status = puts(hello, buf);
          if (status & EFI_ERR)
            return status;
          break;
        case L'd':
          tsprintf(int32_t, buf, 10, ap);
          status = puts(hello, buf);
          if (status & EFI_ERR)
            return status;
          break;
        case L'l':
          ptr++;
          switch (*ptr) {
            case L'd':
              tsprintf(int64_t, buf, 10, ap);
              status = puts(hello, buf);
              if (status & EFI_ERR)
                return status;
              break;
            case L'u':
              tsprintf(uint64_t, buf, 10, ap);
              status = puts(hello, buf);
              if (status & EFI_ERR)
                return status;
              break;
            case L'x':
              tsprintf(uint64_t, buf, 16, ap);
              status = puts(hello, buf);
              if (status & EFI_ERR)
                return status;
              break;
          }
          break;
        case L'p':
          status = puts(hello, L"0x");
          if (status & EFI_ERR)
            return status;
          tsprintf(uintptr_t, buf, 16, ap);
          status = puts(hello, buf);
          if (status & EFI_ERR)
            return status;
          break;
        case L's':
          status = puts(hello, va_arg(ap, const wchar_t *));
          if (status & EFI_ERR)
            return status;
          break;
        case L'u':
          tsprintf(uint32_t, buf, 10, ap);
          status = puts(hello, buf);
          if (status & EFI_ERR)
            return status;
          break;
        case L'x':
          tsprintf(uint32_t, buf, 16, ap);
          status = puts(hello, buf);
          if (status & EFI_ERR)
            return status;
          break;
        default:
          status = puts(hello, L"%%");
          if (status & EFI_ERR)
            return status;
          break;
      }
    } else {
      buf[0] = *ptr;
      buf[1] = L'\0';
      status = puts(hello, buf);
      if (status & EFI_ERR)
        return status;
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS wait_for_event(HELLO hello, UINTN num, EFI_HANDLE *events, UINTN *index) {
  EFI_SYSTEM_TABLE *systbl = hello->systbl;
  return (*systbl->BootServices->WaitForEvent)(num, events, index);
}

EFI_STATUS wait_for_key_event(HELLO hello, UINT16 *scan, UINT16 *uni) {
  EFI_SYSTEM_TABLE *systbl = hello->systbl;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *stdin = systbl->ConIn;
  EFI_HANDLE events[] = {stdin->WaitForKey};
  UINTN index;
  EFI_STATUS status = (*systbl->BootServices->WaitForEvent)(1, events, &index);
  if (status & EFI_ERR)
    return status;
  EFI_INPUT_KEY ik;
  status = (*stdin->ReadKeyStroke)(stdin, &ik);
  if (status & EFI_ERR)
    return status;
  if (scan)
    *scan = ik.ScanCode;
  if (uni)
    *uni = ik.UnicodeChar;
  return status;
}

EFI_STATUS whoami(HELLO hello, UINTN *index) {
  EFI_MP_SERVICES_PROTOCOL *mpsp = hello->mpsp;
  return mpsp ? (*mpsp->WhoAmI)(mpsp, index) : (*index = 0, EFI_SUCCESS);
}
