#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
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

static std::vector<std::string> split(const std::string& s, char delimiter);

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
    // here mount temporarily to reconfigure root dir
    if(!mount()) {
        p("cant mount");
        return false;
    }
    // init root
    init_dir(0);
    // and umount
    umount();
    return true;
}


bool picofs::create(std::string fname, ftype_t type)
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
    return create(fd, fname, true, type);
}

bool picofs::create(int fd, std::string fname, bool newfd, ftype_t type)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
    // now create fd
    if(newfd) {
        int blk0 = get_empty_blk();
        if(blk0 < 0) {
            p("no free blk");
            return false;
        }
        descs.inst[fd].blks[0] = blk0;
        descs.inst[fd].links_amount = 0;
        descs.inst[fd].sz = 0;
        descs.inst[fd].type = type;
    }
    // its name in current directory
    assert(fd_current_dir >= 0);
    // append to dir
    dir_add_file(fd_current_dir, fname, fd);
    // any additinal things:
    // if dir init dir
    if(!init_dir(fd, fd_current_dir)) {
        p("could not init dir");
    }
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
        for(int blki=0; (blki<MX_BLOCKS_PER_FILE) && (blki*BLK_SIZE<=descs.inst[des].sz); blki++) {
            int blk = descs.inst[des].blks[blki];
            if(blk>blk_amount) {
                p("in file descriptor invalid blk file=%d, blk=%u", des, blk);
            }
            if(blk < blks_for_dsks)
                continue;
            pd("blk%d busy", blk);
            blk_busy[blk] = true;
        }
    }
}

bool picofs::ls()
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
    f_link_t flink;
    // open current dir
    descr_t* dfd = &descs.inst[fd_current_dir];
    for(int i=0; i<dfd->sz; i+=sizeof(f_link_t)) {
        read(fd_current_dir, &flink, sizeof flink, i);
        p("%d %s", flink.desc_num, flink.name);
    }
    return true;
}

int picofs::fd_get(int dir, std::string fname)
{
    f_link_t flink;
    descr_t* dfd = &descs.inst[dir];
    for(int i=0; i<dfd->sz; i+=sizeof(f_link_t)) {
        read(dir, &flink, sizeof flink, i);
        if(flink.desc_num >= 0 && descs.inst[flink.desc_num].links_amount > 0 && !strncmp(flink.name, fname.c_str(), NAME_SZ)) {
            p("found file<%s>", fname.c_str());
            return flink.desc_num;
        }
    }
    p("not found <%s>", fname.c_str());
    return -1;
}

std::string picofs::name_get(int dir, int fd)
{
    f_link_t flink;
    descr_t* dfd = &descs.inst[dir];
    for(int i=0; i<dfd->sz; i+=sizeof(f_link_t)) {
        read(dir, &flink, sizeof flink, i);
        if(flink.desc_num >= 0 && descs.inst[flink.desc_num].links_amount > 0 && (fd == flink.desc_num)) {
            return string(flink.name);
        }
    }
    p("not found <%d>", fd);
    return string();
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

bool picofs::dir_rem_file(int dir, std::string fname, int fd)
{
    f_link_t flink;
    descr_t* dfd = &descs.inst[dir];
    int offset = 0;
    bool found = false;
    for(int i=0; i<dfd->sz; i+=sizeof(f_link_t)) {
        read(dir, &flink, sizeof flink, i);
        if(!strncmp(flink.name, fname.c_str(), NAME_SZ)) {
            // here it is
            found = true;
            offset = i;
        }
    }
    size_t sz_move = dfd->sz - offset - sizeof(f_link_t);
    if(found) {
        // here read sz_move part of dir from offset to end
        uint8_t* hpart = new uint8_t[sz_move+1] ();
        read(dir, hpart, sz_move, offset+sizeof(f_link_t));
        write(dir, hpart, offset, sz_move);
        // truncate dir file
        dfd->sz -= sizeof(f_link_t);
        // now remove 1 link
        descs.inst[fd].links_amount--;
        return true;
    }
    return false;
}

bool picofs::append(int fd, void *data, size_t sz)
{
    // split data by BLK_SIZE blocks
    for(size_t i=0; i<sz; i += BLK_SIZE) {
        size_t left = sz - i;
        size_t sz_write = (left > BLK_SIZE)?BLK_SIZE:left;
        if(!append_blk(fd, (uint8_t*)data+i, sz_write)) {
            return false;
        }
    }
    return true;
}
/* this is restricted append, can only append data
 *  with size <= BLK_SIZE
*/
bool picofs::append_blk(int fd, void*data, size_t sz)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
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

size_t picofs::write(int fd, void*data, size_t offset, size_t sz)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return 0;
    }
    if(fd < 0)
        return 0;
    descr_t* dfd = &descs.inst[fd];
    // file current size
    size_t fsz = dfd->sz;
    // cur data
    size_t cur_data = offset;
    for(; cur_data < fsz && (cur_data - offset) < sz; ) {
        size_t blk_num_cur = cur_data / BLK_SIZE;
        size_t blk_p_cur = dfd->blks[blk_num_cur];
        size_t blk_offset = cur_data - blk_num_cur * BLK_SIZE;
        // till the end of requested size:
        size_t sz_till_end = sz - (cur_data - offset);
        // till the end of file:
        size_t sz_till_fsz = fsz - cur_data;
        size_t fill_blk_sz;
        if(sz_till_end > BLK_SIZE) {
            fill_blk_sz = BLK_SIZE;
        } else {
            fill_blk_sz = (sz_till_end > sz_till_fsz)?sz_till_fsz:sz_till_end;
        }
        pd("fill_blk_sz=%zu", fill_blk_sz);
        void*blk = readblk(blk_p_cur);
        if(blk == NULL) {
            p("cant write because cant read blk%zu",blk_p_cur);
            return 0;
        }
        memcpy((uint8_t*)blk+blk_offset, (uint8_t*)data+(cur_data - offset), fill_blk_sz);
        writeblk(blk_p_cur, blk);
        cur_data += fill_blk_sz;
    }
    // written part that is not appended but substituted, + appended up to end of block
    // now append
    if(cur_data - offset >= sz) {
        return sz;
    }
    size_t app_sz = sz - (cur_data-offset);
    if(append(fd, (uint8_t*)data+cur_data, app_sz))
        return sz;
    return app_sz;
}
bool picofs::append(int fd, std::string str)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
    void* data = (void*)str.c_str();
    size_t sz = str.size();
    return append(fd, data, sz);
}

ssize_t picofs::read(int fd, void *buf, size_t count, size_t offset)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
    if(buf == NULL)
        return 0;
    descr_t* dfd = &descs.inst[fd];
    size_t buf_offset = 0;
    for(size_t i=offset; i < count+offset && i < dfd->sz; ) {
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

int picofs::open(std::string fname)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return -1;
    }
    // find in fs
    int fd = fd_get(fname);
    if(fd < 0) {
        // cant find such file
        return -1;
    }
    return fd;
}

int picofs::fd_get(std::string fname)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return -1;
    }
    // split by name
    string name = get_fname(fname);
    string path = get_path(fname);
    // split str by delimiters
    vector<string> tokens = split(path, delimiter.at(0));
    // now find
    int cur_dir = fd_current_dir;
    for(;!tokens.empty();) {
        p("pseudo cd <%s>", tokens.back().c_str());
        if(!pseudo_cd(&cur_dir, tokens.back())) {
            p("error pseudo cd at <%s>", tokens.back().c_str());
            return -1;
        }
        tokens.pop_back();
    }
    return fd_get(cur_dir, name);
}

std::string picofs::get_fname(std::string fname)
{
    // find last occurence of delim
    size_t occr = fname.find_last_of(delimiter);
    occr++;
    return fname.substr(occr);
}

std::string picofs::get_path(std::string fname)
{
    // find last occurence of delim
    size_t occr = fname.find_last_of(delimiter);
    if(occr == string::npos)
        return "";
    if(occr == 0)
        return delimiter;
    return fname.substr(0, occr);
}

descr_t picofs::descr_fget(int fd)
{
    return descs.inst[fd];
}

bool picofs::unlink(std::string fname)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
    // find this file in cur dir
    int fd = fd_get(fname);
    if(fd < 0) {
        p("file <%s> not found", fname.c_str());
        return false;
    }
    return dir_rem_file(fd_current_dir, fname, fd);
}

bool picofs::link(std::string fname_old, std::string fname_new)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
    // find file in cur dir
    int fd = fd_get(fname_old);
    // findout type of file
    ftype_t type = descs.inst[fd].type;
    if(fd < 0) {
        p("file <%s> not found", fname_old.c_str());
        return false;
    }
    if(create(fd, fname_new, false, type)) {
        descs.inst[fd].links_amount++;
        return true;
    }
    return false;
}

bool picofs::truncate(std::string fname, size_t sz)
{
    if(!is_mounted()) {
        p("fs is not mounted");
        return false;
    }
    // find this file
    int fd = fd_get(fname);
    if(fd < 0) {
        p("file <%s> not found", fname.c_str());
        return false;
    }
    // get size
    size_t old_sz = descs.inst[fd].sz;
    if(old_sz > sz) {
        // just reconfigure size
        descs.inst[fd].sz = sz;
        // update info about file
        blk_busy_refresh();
        return true;
    }
    // new sz is bigger
    size_t diff = sz - old_sz;
    uint8_t *zero= new uint8_t[diff] ();
    return append(fd, zero, diff);
}

bool picofs::init_dir(int fd)
{
    if(!is_mounted()) {
        p("not mounted");
        return false;
    }
    f_link_t cur_link;
    cur_link.desc_num = fd;
    strcpy(cur_link.name, ".");
    if(!append(fd, &cur_link, sizeof cur_link)) {
        p("cant append init_dir");
        return false;
    }
    return true;
}

bool picofs::init_dir(int fd, int parent)
{
    if(!is_mounted()) {
        p("not mounted");
        return false;
    }
    f_link_t parent_dir;
    parent_dir.desc_num = parent;
    strcpy(parent_dir.name, "..");
    if(!append(fd, &parent_dir, sizeof parent_dir)) {
        p("cant append init_dir parent");
        return false;
    }
    if(!init_dir(fd)) {
        p("init_dir fd failed");
        return false;
    }
    return true;
}

bool picofs::pseudo_cd(int*cur_dir, int fd)
{
    assert(cur_dir!=NULL);
    if(!is_mounted()) {
        p("not mounted");
        return false;
    }
    if(descs.inst[fd].type == ftype_dir && descs.inst[fd].links_amount > 0) {
        *cur_dir = fd;
        return true;
    }
    p("dir not found %d", fd);
    return false;

}
bool picofs::cd(int fd)
{
    if(!is_mounted()) {
        p("not mounted");
        return false;
    }
    if(descs.inst[fd].type == ftype_dir && descs.inst[fd].links_amount > 0) {
        fd_current_dir = fd;
        return true;
    }
    p("dir not found %d", fd);
    return false;
}

bool picofs::pseudo_cd(int*cur_dir, std::string dname)
{
    assert(cur_dir!=NULL);
    if(!is_mounted()) {
        p("not mounted");
        return false;
    }
    // if we just need to goto the root
    if(dname == delimiter) {
        pd("pseudo cd goto root");
        *cur_dir = 0;
        return true;
    }
    int fd = fd_get(*cur_dir, dname);
    if(fd < 0) {
        return false;
        p("pseudo_cd dir not found");
    }
    if(pseudo_cd(cur_dir, fd)) {
        return true;
    }
    p("could not pseudo cd");
    return false;
}

static std::vector<std::string> split(const std::string& s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   if(s.size() == 0)
       return tokens;
   if(s.at(0) == delimiter) {
       tokens.insert(tokens.begin(), string(1,delimiter));
   }
   while (std::getline(tokenStream, token, delimiter))
   {
       tokens.insert(tokens.begin(), token);
   }
   return tokens;
}

bool picofs::cd(std::string path)
{
    int fd_dir = fd_get(path);
    if(fd_dir < 0) {
        return false;
    }
    return cd(fd_dir);
}



std::string picofs::current_path()
{
    if(!is_mounted()) {
        p("not mounted");
        return "<not mounted>";
    }
    string path = string();
    int dir = fd_current_dir;
    for(;;) {
        // get prev dir if not found then finish
        int fd = fd_get(dir, "..");
        if(fd < 0) {
            return magic+ delimiter + path;
        }
        // find there current dir name
        string curname = name_get(fd, dir);
        path = curname + delimiter + path;
        dir = fd;
    }
    return path;
}
