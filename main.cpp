#include <iostream>
#include <cstdio>
#include <picofs.h>
#include <picofs_cli.h>
using namespace std;

static void do_finish_staff(void);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
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
