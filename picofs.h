#ifndef FS_H
#define FS_H

#include <cstdio>
#include <string>
#define p(...)    do { printf(__VA_ARGS__); puts("");  }while(0)
#define pd(...)    do { printf(__VA_ARGS__); puts("");  }while(0)
#define xstr(s) str(s)
#define str(s) #s
#define BLK_SIZE 512
#define MX_BLOCKS_PER_FILE 16
#define NAME_SZ 16
#define DESCS_NUMBER 128
// platform dependent
void close_access(void);
void open_access(void);
void* readblk(int num);

// platform independent
enum ftype_t {
    ftype_dir,
    ftype_hlink,
    ftype_slink
};

struct descr_t {
    ftype_t type;
    uint8_t links_amount;
    uint16_t sz;
    // blocks
    uint16_t blks[MX_BLOCKS_PER_FILE];
};
struct f_link_t {
    uint16_t desc_num;
    char name[NAME_SZ];
};

struct all_descs_t {
    char magic_num[NAME_SZ];
   // number of descriptors
   size_t num;
    // descs
   descr_t inst[DESCS_NUMBER];
};

class picofs
{
private:
    const std::string magic = "PICOFS" str(BLK_SIZE);
    all_descs_t* descs = NULL;
    // return amount of blks
    int get_blk_amount();
    // return 0 if fs 0 blk is OK,
    // return -1 if something wrong with it
    int verify_magic();
    bool exists = false;
    bool mounted = false;
public:
    bool is_mounted ()
    {
        return mounted;
    }
    bool is_exists()
    {
        return exists;
    }
    bool fs_start_correct();
    int blk_amount;
    picofs();
    ~picofs();
};

#endif // FS_H
