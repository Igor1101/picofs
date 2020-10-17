#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
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
    int blks_for_dsks = sizeof descs / BLK_SIZE + 1;
    // create array of blks
    void *blks = readblks(0, blks_for_dsks);
    memcpy(&descs, blks, sizeof descs);
    // here create map with free and busy blks
    blk_amount = get_blk_amount();
    blk_busy = new bool[blk_amount] ;
    blk_busy_refresh();
    // here goto root directory (0 descriptor)
    fd_current_dir = 0;
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
    fd_current_dir = -1;
    delete [] blk_busy;
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
    int blks_for_dsks = sizeof descs / BLK_SIZE + 1;
    // create root directory, its 0 blk
    descs.inst[0].blks[0] = blks_for_dsks;
    descs.inst[0].links_amount = 1;
    descs.inst[0].sz = 0;
    descs.inst[0].type = ftype_dir;
    // write from the 1st blks
    writeblks(0, &descs, sizeof descs);
    return true;
}


bool picofs::create(std::string fname)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }

    // find empty space for descriptor
    int fd = get_empty_desc();
    if(fd < 0) {
        p("no empty descs found");
        return false;
    }
    int blk0 = get_empty_blk();
    if(blk0 < 0) {
        p("no free blk");
        return false;
    }
    // now create fd
    descs.inst[fd].blks[0] = blk0;
    descs.inst[fd].links_amount = 0;
    descs.inst[fd].sz = 0;
    descs.inst[fd].type = ftype_hlink;
    // its name in current directory
    assert(fd_current_dir >= 0);
    // append to dir
    dir_add_file(fd_current_dir, fname, fd);
    return true;
}

int picofs::get_empty_desc()
{
     for(int i=0; i<DESCS_NUMBER; i++) {
        if(descs.inst[i].links_amount == 0) {
            return i;
        }
    }
     return -1;
}

int picofs::get_empty_blk()
{
    for(int i=0; i<blk_amount; i++) {
        if(!blk_busy[i]) {
            // blk not busy
            blk_busy[i] = true;
            return i;
        }
    }
    return -1;
}

// this is usually only needed at mount
void picofs::blk_busy_refresh()
{
    if(blk_busy == NULL) {
        p("blk_busy == NULL");
        exit(-1);
    }
    memset(blk_busy, 0, blk_amount / sizeof blk_busy[0]);
    // fill blks used by descriptors
    int blks_for_dsks = sizeof descs / BLK_SIZE + 1;
    for(int i=0; i<blks_for_dsks; i++) {
        blk_busy[i] = true;
    }
    // fill blks according to descriptors
    for(int des=0; des<DESCS_NUMBER; des++) {
        if(descs.inst[des].links_amount == 0) {
            continue;
        }
        for(int blki=0; (blki<MX_BLOCKS_PER_FILE) && (blki*BLK_SIZE<descs.inst[des].sz); blki++) {
            int blk = descs.inst[des].blks[blki];
            if(blk>blk_amount) {
                p("in file descriptor invalid blk file=%d, blk=%u", des, blk);
            }
            if(blki < blks_for_dsks)
                continue;
            blk_busy[blk] = true;
        }
    }
}

void picofs::ls()
{
    f_link_t flink;
    // open current dir
    descr_t* dfd = &descs.inst[fd_current_dir];
    for(int i=0; i<dfd->sz; i+=sizeof(f_link_t)) {
        read(fd_current_dir, &flink, sizeof flink, i);
        p("%d %s", flink.desc_num, flink.name);
    }
}

bool picofs::dir_add_file(int dir, std::string fname, int fd)
{
    f_link_t link;
    link.desc_num = fd;
    strncpy(link.name, fname.c_str(), NAME_SZ);
    // here append flink to directory
    descr_t *ddir = &descs.inst[dir];
    assert (ddir->type == ftype_dir );
    bool res = append(dir, &link, sizeof link);
    if(res) {
        descs.inst[fd].links_amount++;
        return true;
    }
    return false;
}

bool picofs::append(int fd, void*data, size_t sz)
{
    if(sz > BLK_SIZE) {
        p("cant write more than  1 blk");
        return false;
    }
    if(fd < 0)
        return false;
    descr_t* dfd = &descs.inst[fd];
    // file current size
    size_t fsz = dfd->sz;
    // amount of blks used for this file
    size_t blk_amount_used = fsz / BLK_SIZE+1;
    // last part size
    size_t last_blk_sz = fsz - (blk_amount_used-1) * BLK_SIZE;
    // verify if we need to add a new block for this
    bool use2blks = false;
    if(sz + last_blk_sz >= BLK_SIZE) {
        use2blks = true;
        if(blk_amount_used > MX_BLOCKS_PER_FILE) {
            p("size of file has reached max possible");
            return false;
        }
        // add new blk
        int blknew = get_empty_blk();
        if(blknew < 0) {
            p("cant append, cant get new blk");
            return false;
        }
        dfd->blks[blk_amount_used] = blknew;
    }
    // now write
    dfd->sz += sz;
    // append prev block part
    int blk0_num = blk_amount_used - 1;
    int blk1_num = blk_amount_used;
    if(!use2blks) {
        void*blk0 = readblk(dfd->blks[blk0_num]);
        memcpy((uint8_t*)blk0+last_blk_sz,  data, sz);
        writeblk(dfd->blks[blk0_num], blk0);
    } else {
        void*blk0 = readblk(dfd->blks[blk0_num]);
        void*blk1 = readblk(dfd->blks[blk1_num]);
        memcpy((uint8_t*)blk0+last_blk_sz,  data, BLK_SIZE - last_blk_sz);
        memcpy((uint8_t*)blk1, (uint8_t*)data + BLK_SIZE - last_blk_sz, sz - (BLK_SIZE - last_blk_sz));
        writeblk(dfd->blks[blk0_num], blk0);
        writeblk(dfd->blks[blk1_num], blk1);
    }
    return true;
}

bool picofs::append(int fd, std::string str)
{
    void* data = (void*)str.c_str();
    size_t sz = str.size();
    return append(fd, data, sz);
}

ssize_t picofs::read(int fd, void *buf, size_t count, size_t offset)
{
    if(buf == NULL)
        return 0;
    descr_t* dfd = &descs.inst[fd];
    size_t buf_offset = 0;
    for(size_t i=offset; i < count && i < dfd->sz; ) {
        // block num
        int blk_num = i / BLK_SIZE;
        // read block
        void* blk = readblk(dfd->blks[blk_num]);
        if(blk == NULL) {
            p("error reading file blk");
            return buf_offset;
        }
        // offset inside blk
        size_t blk_offset = i - blk_num * BLK_SIZE;
        size_t read_amount = (count < BLK_SIZE - blk_offset)?count:(BLK_SIZE - blk_offset);
        memcpy((uint8_t*)buf+buf_offset, (uint8_t*)blk+blk_offset, read_amount);
        buf_offset += read_amount;
        i += read_amount;
    }
    return buf_offset;
}
