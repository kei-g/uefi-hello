#ifndef include_spinlock_h
#define include_spinlock_h

void spinlock_lock(uintptr_t *lock);
void spinlock_unlock(uintptr_t *lock);

#endif /* include_spinlock_h */
