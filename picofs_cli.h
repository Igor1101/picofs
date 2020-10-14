#ifndef PICOFS_CLI_H
#define PICOFS_CLI_H
#include <vector>
#include <string>
#include "picofs.h"
class picofs_cli
{
private:
    picofs*fs = NULL;
    std::string cmd_line;
    void run_cmd(std::string cmd, std::vector <std::string>args);
public:
    void newline();
    void parse();
    picofs_cli(picofs* fs)
    {
        this->fs = fs;
        for(;;) {
            newline();
            parse();
        }
    }
};

#endif // PICOFS_CLI_H
