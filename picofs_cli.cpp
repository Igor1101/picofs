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
    cin >> cmd_line;
    cout << endl << dir_name << "=> " ;
}

void picofs_cli::parse()
{
    istringstream iss (cmd_line);
    vector <string> results(istream_iterator<string>{iss}, istream_iterator <string>());
    // now have split to vectors
    // get command
    string cmd = results.operator[](0);
    // get arguments:
    vector <string> args = results;
    args.erase(args.begin());
}

void picofs_cli::run_cmd(std::string cmd, std::vector <std::string>args)
{
    if(cmd == "help") {
        printf(" mount umount filestat ls create open close read write link unlink truncate");
    } else if(cmd == "mount") {
        fs->mount();
    } else if(cmd == "umount") {
        fs->umount();
    } else if(cmd == "format") {
        fs->format();
    }
}
