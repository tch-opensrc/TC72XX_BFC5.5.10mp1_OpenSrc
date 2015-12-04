#ifndef __LINUX_SPINLOCK_H__
#define __LINUX_SPINLOCK_H__


typedef struct { } spinlock_t;
#define SPIN_LOCK_UNLOCKED (spinlock_t) { }
#define spin_lock_init(lock) do{} while (0)
#define spin_lock(lock) do{} while (0)
#define spin_unlock(lock) do{} while (0)
#define spin_lock_bh(lock) do{} while (0)
#define spin_unlock_bh(lock) do{} while (0)

#endif /* __LINUX_SPINLOCK_H__ */
