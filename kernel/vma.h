// Stores VMA properties
struct vma {
  int inuse;                    // whether VMA slot is in use or not
  uint64 addr;                  // VMA address
  int length;                   // size in bytes
  char readable;
  char writeable;
  enum { SHARED, PRIVATE} map;  // save (or not) changes to file
  struct file *file;            // file to be mapped
};
