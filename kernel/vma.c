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

// Deallocate a VMA structure.
void
vmadealloc(struct vma* v)
{
  v->inuse = 0;
}

// Duplicate a VMA structure.
struct vma*
vmadup(struct vma *v)
{
  struct vma *nv;

  if((nv = vmaalloc()) == 0)
    return 0;
  nv->addr = v->addr;
  nv->length = v->length;
  nv->readable = v->readable;
  nv->writeable = v->writeable;
  nv->map = v->map;
  nv->file = filedup(v->file);
 
  return nv;
}

// Fetch VMA given a specific page address.
struct vma*
vmafetch(uint64 page)
{
  struct proc *p = myproc();
  struct vma *v;

  for (int vd = 0; vd < NOVMA; vd++){
    v = p->ovma[vd];
    if (v && v->inuse && v->addr <= page 
        && page < v->addr + v->length)
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
  int perm, off;

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
  off = page - v->addr;
  ilock(f->ip);
  readi(f->ip, 1, page, off, PGSIZE);
  iunlock(f->ip);

  return 0;
}

// Unmap VMA pages in the given address range.
int
vmaunmap(struct vma *v, uint64 addr, int n)
{
  struct proc *p = myproc();

  // Write changes to file if required
  if(v->map == SHARED)
    vmawrite(v, addr, n);

  // Unmap selected pages
  uvmunmap(p->pagetable, PGROUNDDOWN(addr), n/PGSIZE, 1);

  // Unmap *whole* memory area: Decrease file ref. count and free VMA
  if(v->addr == addr && v->length == n){
    fileclose(v->file);
    vmadealloc(v);
  }
  // Unmap *start* of memory area: Update VMA scope
  else if(v->addr == addr && v->length > n){
    v->addr = addr + n;
    v->length -= n;
  }
  // Unmap *end* of memory area: Update VMA scope
  else if(v->addr < addr && v->length == n){
    v->length -= n;
  }
  // Error: Can't create a gap within a memory area
  else
    return -1;

  return 0;
}

// Write mapped memory area content to file.
int
vmawrite(struct vma *v, uint64 addr, int n)
{
  int r = 0;
  struct file *f = v->file;
  int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
  int off = addr - v->addr;

  int i = 0;
  while(i < n){
    int n1 = n - i;
    if(n1 > max)
      n1 = max;

    begin_op();
    ilock(f->ip);
    if ((r = writei(f->ip, 1, addr + i, off, n1)) > 0)
      off += r;
    iunlock(f->ip);
    end_op();

    if(r != n1){
      // error from writei
      break;
    }
    i += r;
  }
  return (i == n ? n : -1);
}
