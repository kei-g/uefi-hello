#include "hello.h"

#include "spinlock.h"

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
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  EFI_HANDLE handle;
  uintptr_t locked;
  struct {
    UINTN buffer_size;
    union {
      UINT8 buffer[0x6000];
      EFI_MEMORY_DESCRIPTOR map[1];
    };
    UINTN map_key;
    UINTN desc_size;
    UINT32 version;
  } memmap;
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

void dump_memory_map(HELLO hello) {
  uintptr_t cur = (uintptr_t)hello->memmap.buffer;
  uintptr_t end = cur + hello->memmap.buffer_size;
  for (UINTN i = 0; cur < end; cur += hello->memmap.desc_size, i++) {
    const EFI_MEMORY_DESCRIPTOR *d = &hello->memmap.map[i];
    printf(hello, L"memmap.%lu: %s attr=0x%lx, addr=%p, %lu pages\r\n", i, get_memory_map_type_name(d->Type), d->Attribute, d->PhysicalStart, d->NumberOfPages);
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
  void *pool;
  EFI_STATUS status = (*systbl->BootServices->AllocatePool)(EfiLoaderData, sizeof(struct _HELLO), &pool);
  if (status & EFI_ERR)
    (*systbl->ConOut->OutputString)(systbl->ConOut, L"failed to allocate pool\r\n"), halt();
  HELLO hello = pool;
  hello->handle = handle;
  hello->systbl = systbl;
  printf(hello, L"firmware.vendor=%s\r\n", systbl->FirmwareVendor);
  printf(hello, L"firmware.revision=0x%x\r\n", systbl->FirmwareRevision);
  printf(hello, L"memory pool has been allocated %lu at %p\r\n", sizeof(*hello), pool);
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
  hello->memmap.buffer_size = sizeof(hello->memmap.buffer);
  status = (*systbl->BootServices->GetMemoryMap)(&hello->memmap.buffer_size, hello->memmap.map, &hello->memmap.map_key, &hello->memmap.desc_size, &hello->memmap.version);
  if (status & EFI_ERR)
    printf(hello, L"failed to get memory map, 0x%lx\r\n", status), halt();
  printf(hello, L"memory map has been gotten %lu bytes at %p\r\n", hello->memmap.buffer_size, hello->memmap.buffer);
  VOID *mpsp = NULL;
  status = (*systbl->BootServices->LocateProtocol)(&guids[1], NULL, &mpsp);
  if (status & EFI_ERR)
    printf(hello, L"failed to locate mpsp protocol, 0x%lx\r\n", status);
  hello->mpsp = mpsp;
  return hello;
}

void lock_hello(HELLO hello) {
  spinlock_lock(&hello->locked);
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

void reverse(wchar_t *buf, size_t len) {
  for (size_t i = 0, j = len - 1; i < len / 2; i++, j--) {
    const wchar_t c = buf[i];
    buf[i] = buf[j];
    buf[j] = c;
  }
}

EFI_STATUS startup_all_aps(HELLO hello, EFI_AP_PROCEDURE proc, UINT8 single, EFI_EVENT event, UINTN timeout, VOID *arg, UINTN **failed) {
  EFI_MP_SERVICES_PROTOCOL *mpsp = hello->mpsp;
  return mpsp ? (*mpsp->StartupAllAPs)(mpsp, proc, single, event, timeout, arg, failed) : ((*proc)(arg), EFI_SUCCESS);
}

void unlock_hello(HELLO hello) {
  spinlock_unlock(&hello->locked);
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
        case 'x':
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

EFI_STATUS whoami(HELLO hello, UINTN *index) {
  EFI_MP_SERVICES_PROTOCOL *mpsp = hello->mpsp;
  return mpsp ? (*mpsp->WhoAmI)(mpsp, index) : (*index = 0, EFI_SUCCESS);
}