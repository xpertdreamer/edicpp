#include "include/term.hpp"

int main(int argc, char **argv) {
    Term term;
    term.initEditor();
    if(argc >= 2) term.editorOpen(argv[1]);
    
    term.editorSetStatusMessage("HELP: CTRL-S = save | CTRL-Q = quit | CTRL-F = find");

    while (true) {
        term.editorRefreshScreen();
        if (term.editorProccessKeypress() == false) break;
    }
    
    return 0;
}