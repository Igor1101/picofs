#include <iostream>
#include <cstdio>
#include <picofs.h>
#include <picofs_cli.h>
using namespace std;

static void do_finish_staff(void);

int main(int argc, char *argv[])
{
    // args: fs path
    if(argc <= 1) {
        cout << "invalid args: 1t: fs path" << endl;
    }
    cout << "fs " << endl;
    picofs fs;
    if(!fs.is_exists()) {
        p("cant open");
        return -1;
    }
    picofs_cli cli(&fs);
    do_finish_staff();
    return 0;
}

static void do_finish_staff(void)
{
}
