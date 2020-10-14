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

void writeblk(size_t num, void* blk)
{
    int res = fseek(file_fs, num*BLK_SIZE, SEEK_SET);
    if(res != 0) {
        pd("can`t fseek to blk=%zu", num);
        return;
    }
    fwrite(blk, BLK_SIZE, 1, file_fs);
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
        p("disk does not exist or something wrong with it");
        return false;
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
    size_t i;
    for(i=0; i<sizeof descs; i+=BLK_SIZE) {
        pd("getting descs blk%zu", i);
        void*blki = readblk(i);
        if(blki == NULL) {
            p("cant get blk%zu for descriptors", i);
            return false;
        }
        memcpy(reinterpret_cast<uint8_t*>(&descs) + i, blki, sizeof descs);
    }
    blks_for_descs = i / sizeof descs;
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
    mounted = false;
    return true;
}

bool picofs::format()
{
    if(is_mounted()) {
        p("is mounted");
        return false;
    }
    // configure descriptors
    strncpy(descs.magic_num, magic.c_str(), NAME_SZ);
    for(int i=0; i<DESCS_NUMBER; i++) {
        descs.inst[i].links_amount = 0;
    }
    // write to the 1st blks
    size_t i;
    for(i=0; i<sizeof descs; i+=BLK_SIZE) {
        pd("writing descs blk%zu", i);
        writeblk(i,reinterpret_cast<uint8_t*>(&descs) + i);
    }
    return true;
}
