// Simple file verifier: loads and executes proofs from a file
#include "../src/runtime/runtime.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.fol> [<file2.fol> ...]\n";
        return 1;
    }
    logic::Runtime rt;
    for (int i = 1; i < argc; ++i) {
        std::ifstream f(argv[i]);
        if (!f.is_open()) {
            std::cerr << "Cannot open: " << argv[i] << "\n";
            return 1;
        }
        std::stringstream buf;
        buf << f.rdbuf();
        auto pr = rt.load_with_proofs(buf.str());
        if (!pr.ok()) {
            std::cerr << argv[i] << ": parse error: " << pr.error().to_string() << "\n";
            return 1;
        }
        auto er = rt.execute_all_proofs(pr.value());
        if (!er.ok()) {
            std::cerr << argv[i] << ": verify error: " << er.error().to_string() << "\n";
            return 1;
        }
        std::cerr << argv[i] << ": OK\n";
    }
    std::cerr << "All files verified.\n";
    return 0;
}
