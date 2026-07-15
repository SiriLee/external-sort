#include "header.h"
#include <sstream>
#include <fstream>
#include <string>


IOStats &GetIOStats() {
    static IOStats stats;
    return stats;
}


void ResetIOStats() {
    GetIOStats() = IOStats{};
}


bool IntWriter(std::ofstream &output, int num) {
    output << num << '\n';
    if (!output.good()) {
        return false;
    }
    GetIOStats().write_ops++;
    return true;
}

bool IntReader(std::ifstream &input, int &num) {
    if (input.eof()) {
        return false;
    }
    std::string s;
    std::getline(input, s);
    bool tag = true;
    for (auto c : s) {
        if (c >= '0' && c <= '9') {
            tag = false;
            break;
        }
    }
    if (tag) {
        return false;
    }
    std::stringstream ss(s);
    ss >> num;
    GetIOStats().read_ops++;
    return true;
}