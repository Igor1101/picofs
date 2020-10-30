#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>
#include <algorithm>
#include <picofs.h>
#include "picofs_cli.h"

using namespace std;
void picofs_cli::newline()
{
    cmd_line.erase();
    cout << endl << fs->current_path() << "=> " ;
    getline(cin, cmd_line);
}

void picofs_cli::parse()
{
    istringstream iss (cmd_line);
    vector <string> results(istream_iterator<string>{iss}, istream_iterator <string>());
    if(results.empty()) {
        return;
    }
    // now have split to vectors
    // get command
    string cmd = results.operator[](0);
    // get arguments:
    vector <string> args = results;
    args.erase(args.begin());
    run_cmd(cmd, args);
}

void picofs_cli::run_cmd(std::string cmd, std::vector <std::string>args)
{
    if(cmd == "help") {
        p(" mount umount filestat ls create open close read write link unlink truncate");
    } else if(cmd == "mount") {
        fs->mount();
    } else if(cmd == "umount") {
        fs->umount();
    } else if(cmd == "format") {
        fs->format();
    } else if(cmd == "create") {
        fs->create(args.operator[](0), ftype_reg);
    } else if(cmd == "exit") {
        if(!fs->is_mounted())
            fs->umount();
        exit(0);
    } else if(cmd == "ls") {
        fs->ls();
    } else if(cmd == "read") {
        // read fd offs size
        int fd = strtol(args.operator[](0).c_str(), NULL, 10);
        size_t offs = strtol(args.operator[](1).c_str(), NULL, 10);
        size_t sz = strtol(args.operator[](2).c_str(), NULL, 10);
        char*buf = new  char[sz+2]();
        fs->read(fd, buf, sz, offs);
        p("%s", buf);
        delete []buf;
    } else if(cmd == "write") {
        // write fd offs size [string]
        int fd = strtol(args.operator[](0).c_str(), NULL, 10);
        size_t offs = strtol(args.operator[](1).c_str(), NULL, 10);
        size_t sz = strtol(args.operator[](2).c_str(), NULL, 10);
        string data = args.operator[](3);
        size_t res = fs->write(fd, (void*)data.c_str(), offs,  sz);
        p("written %zu", res);
    } else if(cmd == "filestat") {
        int fd = strtol(args.operator[](0).c_str(), NULL, 10);
        descr_t dfd = fs->descr_fget(fd);
        p("fd %d links %d sz %zu type=%d", fd, dfd.links_amount, dfd.sz, dfd.type);
        p("blks:");
        for(size_t i=0; i<=dfd.sz; i+=BLK_SIZE) {
            p("blk%d=%d", i/BLK_SIZE, dfd.blks[i/BLK_SIZE]);
        }
    } else if(cmd == "unlink") {
        if(!fs->unlink(args.operator[](0))) {
            p("unlink unsuccessfull");
        } else {
            p("unlink success");
        }
    } else if(cmd == "link") {
        string filename_old = args.operator[](0);
        string filename_new = args.operator[](1);
        if(!fs->link(filename_old, filename_new)) {
            p("unsuccessfull link");
        } else {
            p("linked successfully");
        }
    } else if(cmd == "truncate") {
        string fname = args.operator[](0);
        size_t sz = strtol(args.operator[](1).c_str(), NULL, 10);
        if(fs->truncate(fname, sz)) {
            p("successfully truncated");
        } else {
            p("truncate unsuccessfull");
        }
    } else if(cmd == "mkdir") {
        string fname = args.operator[](0);
        if(fs->create(fname, ftype_dir)){
            p("created");
        } else {
            p("not created");
        }
    } else if(cmd == "cd") {
        string fname = args.operator[](0);
        if(!fs->cd(fname)) {
            p("cant cd");
        }
    } else if(cmd == "symlink") {
        string fname = args.operator[](0);
        string fname_sym = args.operator[](1);
        if(!fs->symlink(fname, fname_sym)) {
            p("not successfull linking");
        } {
            p("successfully created link");
        }
    } else if(cmd == "rmdir") {
        string fname = args.operator[](0);
        if(fs->rmdir(fname)) {
            p("dir successfully removed");
        } else {
            p("cant remove dir");
        }
    }
    else {
        p("not found cmd");
    }
}
