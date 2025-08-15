#include "include/term.hpp"

int main(int argc, char **argv) {
    Term term;
    term.initEditor();
    if(argc >= 2) term.editorOpen(argv[1]);
    
    while (true) {
        term.editorRefreshScreen();
        if (term.editorProccessKeypress() == false) break;
    }
    
    return 0;
}