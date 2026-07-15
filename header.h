#ifndef EXTERNAL_SORT_HEADER_H
#define EXTERNAL_SORT_HEADER_H

#include <cstddef>
#include <string>
#include <fstream>
#include <vector>


struct IOStats {
    std::size_t read_ops = 0;
    std::size_t write_ops = 0;
};


IOStats &GetIOStats();


void ResetIOStats();


bool IntWriter(std::ofstream &output, int num);


std::vector<std::string> GenerateInitialRuns(
    std::ifstream &input, int k);


std::string KWayMerge(
    const std::vector<std::string> &run_files, int k);


bool IntReader(std::ifstream &input, int &num);

#endif