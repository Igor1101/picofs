#include <iostream>
#include <cstdio>
#include <picofs.h>
using namespace std;

static void do_finish_staff(void);

int main(int argc, char *argv[])
{
    // args: fs path
    if(argc <= 1) {
        cout << "invalid args: 1t: fs path" << endl;
    }
    cout << "fs " << endl;
    picofs();
    do_finish_staff();
}

static void do_finish_staff(void)
{
}
