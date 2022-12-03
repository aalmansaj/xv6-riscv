//
// Support functions for system calls that involve Virtual Memory Areas (VMAs).
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "proc.h"
#include "vma.h"

struct {
  struct spinlock lock;
  struct vma vma[NVMA];
} vtable;

void
vmainit(void)
{
  initlock(&vtable.lock, "vtable");
}

// Allocate a VMA structure.
struct vma*
vmaalloc(void)
{
  struct vma *v;

  acquire(&vtable.lock);
  for(v = vtable.vma; v < vtable.vma + NVMA; v++){
    if(v->inuse == 0){
      v->inuse = 1;
      release(&vtable.lock);
      return v;
    }
  }
  release(&vtable.lock);
  return 0;
}

// Fetch VMA given a specific page address.
struct vma*
vmafetch(uint64 page)
{
  struct proc *p = myproc();
  struct vma *v;

  for (int vd = 0; vd < NOVMA; vd++){
    v = p->ovma[vd];
    if (v->addr <= page && page < v->addr + v->length)
      return v;
  }

  return 0;
}

// Load VMA page from the corresponding file.
int
vmaloadpage(uint64 page)
{
  struct proc *p = myproc();
  struct vma *v;
  struct file *f;
  char *mem;
  int perm;
  int r;

  // search for VMA where page-fault took place
  if ((v = vmafetch(page)) == 0)
    return -1; // no VMA found

  mem = kalloc();
  if (mem == 0)
    return -1; // out of physical memory
  memset(mem, 0, PGSIZE);

  // set permissions
  perm = PTE_U;
  if (v->readable)
    perm |= PTE_R;
  if (v->writeable)
    perm |= PTE_W;

  // "mmappages(): va might not be page-aligned"
  if(mappages(p->pagetable, page, PGSIZE, (uint64)mem, perm) != 0){
    kfree(mem);
    return -1; // couldn't allocate a page-table page
  }

  // read page from file
  f = v->file;
  ilock(f->ip);
  if((r = readi(f->ip, 1, page, f->off, PGSIZE)) > 0)
    f->off += r;
  iunlock(f->ip);

  return 0;
}
