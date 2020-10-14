#include <cstdio>
#include <cstdint>
#include <cstring>
#include "picofs.h"

using namespace std;

// platform dependent staff

FILE* file_fs;
void open_access(void)
{
    file_fs = fopen("bin.txt", "r+");
    if(file_fs == NULL) {
        fprintf(stderr, "%s (opening bin.txt)", strerror(errno));
        exit(-1);
    }
}

void close_access(void)
{
    fclose(file_fs);
}

void* readblk(size_t num)
{
    static uint8_t* blk = new uint8_t[BLK_SIZE];
    int res = fseek(file_fs, num*BLK_SIZE, SEEK_SET);
    if(res != 0) {
        pd("can`t fseek to blk=%zu", num);
        return NULL;
    }
    size_t sz = fread(blk, BLK_SIZE, 1, file_fs);
    if(sz != 1) {
        perror(strerror(errno));
        pd("can`t fread to blk=%zu sz returned =%zd", num, sz);
        return NULL;
    }
    return blk;
}

void* readblks(size_t num, size_t amount)
{
    static uint8_t* blk = new uint8_t[BLK_SIZE*amount];
    int res = fseek(file_fs, num*BLK_SIZE, SEEK_SET);
    if(res != 0) {
        pd("can`t fseek to blk=%zu", num);
        return NULL;
    }
    size_t sz = fread(blk, BLK_SIZE, amount, file_fs);
    if(sz != amount) {
        perror(strerror(errno));
        pd("can`t fread to blk=%zu sz returned =%zd amount=%d", num, sz, amount);
        return NULL;
    }
    return blk;
}

void writeblk(size_t num, void* blk)
{
    int res = fseek(file_fs, num*BLK_SIZE, SEEK_SET);
    if(res != 0) {
        pd("can`t fseek to blk=%zu", num);
        return;
    }
    fwrite(blk, BLK_SIZE, 1, file_fs);
}

void writeblks(size_t num, void*data, size_t datasz)
{
  //  void* old_data = readblks(num, datasz / BLK_SIZE + 1);
 //   memcpy(old_data, data, datasz);
    int res = fseek(file_fs, num*BLK_SIZE, SEEK_SET);
    if(res != 0) {
        pd("can`t fseek to blk=%zu", num);
        return;
    }
    fwrite(data, datasz,1, file_fs);
}
// platform not dependent

picofs::picofs()
{
    pd("opening drive");
    open_access();
    pd("gettting amount of blks");
    blk_amount = get_blk_amount();
    pd("amount of blks = %d", blk_amount);
    if(blk_amount < 8/* this is too little or fatal error occured*/) {
        exists = false;
        close_access();
        exit(-1);
    } else {
        exists = true;
    }
}

bool picofs::start_is_correct()
{
    // blk 0 should have: dir called with magic number std:string magic
    void* blk0 = readblk(0);
    if(blk0 == NULL) {
        p("cant open blk 0");
        return false;
    }
    all_descs_t* fs = reinterpret_cast<all_descs_t*>(blk0);
    string curfs_magic = fs->magic_num;
    if(curfs_magic == magic) {
        return true;
    }
    return false;
}

picofs::~picofs()
{
    pd("closing drive");
    close_access();
}

int picofs::get_blk_amount()
{
    for(int i=0; ;i++) {
        if(readblk(i) == NULL) {
            return i;
        }
    }
}



bool picofs::mount()
{
    if(is_mounted()) {
        p("fs is already mounted");
        return true;
    }
    if(!is_exists()) {
        p("open disc");
        open_access();
        exists = true;
    }
    bool start_correct = start_is_correct();
    if(!start_correct) {
        p("blk 0 start of fs is not correct");
        return false;
    }
    void* blk0 = readblk(0);
    if(blk0 == NULL) {
        p("cant open blk 0");
        return false;
    }
    unsigned int i;
    int blks_for_dsks = sizeof descs / BLK_SIZE + 1;
    // create array of blks
    void *blks = readblks(0, blks_for_dsks);
    memcpy(&descs, blks, sizeof descs);
    // here goto root directory (0 descriptor)
    current_dir = &descs.inst[0];
    current_dir_name = magic;
    mounted = true;
    return true;
}

bool picofs::umount()
{
    if(!is_mounted()) {
        p("already not mounted");
        return false;
    }
    // save all changes if needed
    writeblks(0, &descs, sizeof descs);

    mounted = false;
    exists = false;
    current_dir_name = "";
    current_dir = NULL;
    close_access();
    return true;
}

bool picofs::format()
{
    if(is_mounted()) {
        p("is mounted");
        return false;
    }
    if(!is_exists()) {
        p("opening disc");
        open_access();
        exists = true;
    }
    // configure descriptors
    strncpy(descs.magic_num, magic.c_str(), NAME_SZ);
    for(int i=0; i<DESCS_NUMBER; i++) {
        descs.inst[i].links_amount = 0;
    }
    // write from the 1st blks
    writeblks(0, &descs, sizeof descs);
    return true;
}


bool picofs::create(std::string fname)
{
    // find empty space for descriptor
    for(int i=0; i<DESCS_NUMBER; i++) {
        if(descs.inst[i].links_amount == 0) {
            // found such space
            // find space in fs
        }
    }
    return false;
}
