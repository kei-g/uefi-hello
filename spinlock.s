.section .text
  .global spinlock_lock
  .global spinlock_unlock

spinlock_lock:
  movq $1,%rcx
  movq (%rdi),%rax
  test %rax,%rax
  jz try_lock
loop1:
  pause
  movq (%rdi),%rax
  test %rax,%rax
  jnz loop1
try_lock:
  lock
  cmpxchgq %rcx,(%rdi)
  jnz loop1
  ret

spinlock_unlock:
  movq $0,(%rdi)
  ret
