#pragma once
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdio>
#include <string>
#include <cstdlib>
#include <cstring>

struct EditorConfig {
    int tab_stop = 8;
    int quit_times = 3;
    int hl_comment = 90;
    int hl_mlcomment = 90;
    int hl_keyword1 = 93;
    int hl_keyword2 = 92;
    int hl_number = 94;
    int hl_match = 33;
    int hl_string = 95;
};

class Config {
public:
    EditorConfig config;
    int loadConfig(const std::string &path);
};

int Config::loadConfig(const std::string &path) {
    FILE *f = fopen(path.c_str(), "r");
    if (!f) return 1;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "tab_stop=", 9) == 0) config.tab_stop = atoi(line + 9);
        else if (strncmp(line, "quit_times=", 11) == 0) config.quit_times = atoi(line + 11);
        else if (strncmp(line, "hl_comment=", 11) == 0) config.hl_comment = atoi(line + 11);
        else if (strncmp(line, "hl_mlcomment=", 13) == 0) config.hl_mlcomment = atoi(line + 13);
        else if (strncmp(line, "hl_keyword1=", 12) == 0) config.hl_keyword1 = atoi(line + 12);
        else if (strncmp(line, "hl_keyword2=", 12) == 0) config.hl_keyword2 = atoi(line + 12);
        else if (strncmp(line, "hl_number=", 10) == 0) config.hl_number = atoi(line + 10);
        else if (strncmp(line, "hl_match=", 9) == 0) config.hl_match = atoi(line + 9);
        else if (strncmp(line, "hl_string=", 10) == 0) config.hl_string = atoi(line + 10);
    }
    fclose(f);
    return 0;
}

#endif