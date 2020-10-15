#ifndef FS_H
#define FS_H

#include <cstdio>
#include <string>
#define p(...)    do { printf(__VA_ARGS__); puts("");  }while(0)
#define pd(...)    do { printf(__VA_ARGS__); puts("");  }while(0)
#define STR(s) XSTR(s)
#define XSTR(s) #s
#define BLK_SIZE 512
#define MX_BLOCKS_PER_FILE 16
#define NAME_SZ 16
#define DESCS_NUMBER 128
// platform dependent
void close_access(void);
void open_access(void);
void* readblk(size_t num);
void writeblk(size_t num, void* blk);
void* readblks(size_t num, size_t amount);
void writeblks(size_t num, void*data, size_t datasz);

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
    const std::string magic = "PICOFS" STR(BLK_SIZE);
    all_descs_t descs ;
    // return amount of blks
    int get_blk_amount();
    // return 0 if fs 0 blk is OK,
    // return -1 if something wrong with it
    int verify_magic();
    // return desc if success
    // return NULL if not found empty
    descr_t* get_empty_desc();
    bool exists = false;
    bool mounted = false;
    descr_t* current_dir;
    std::string current_dir_name;
    bool *blk_busy = NULL;
    int blk_amount;
    void blk_busy_refresh();
    int get_empty_blk();
public:
    std::string get_current_dir()
    {
        return current_dir_name;
    }
    bool is_mounted ()
    {
        return mounted;
    }
    bool is_exists()
    {
        return exists;
    }
    bool mount();
    bool umount();
    bool format();
    bool create(std::string fname);
    bool start_is_correct();
    picofs();
    ~picofs();
};

#endif // FS_H
