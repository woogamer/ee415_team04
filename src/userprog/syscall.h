#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock sys_lock;

void syscall_init (void);

void isUseraddr(void * esp, int argnum, int pointer_index);
struct file * fd2file(int fd);
int sys_file_size(int fd);
void munmap_clear(struct MMTE *m);
#endif /* userprog/syscall.h */
