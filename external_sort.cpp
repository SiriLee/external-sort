#include "header.h"
#include <vector>
#include <string>
#include <algorithm>
#include <queue>
#include <iostream>
#include <fstream>
#include <filesystem>

static const std::string SAVE_DIR = "save/";

static const std::string OUTPUT_FILE = SAVE_DIR + "sorted_output.txt";

inline std::string GetRunFileName(int run_index) {
    return SAVE_DIR + "run_" + std::to_string(run_index) + ".txt";
}

std::vector<std::string> GenerateInitialRuns(std::ifstream &input, int k) {
    std::vector<std::string> run_files; // Return vector of run file names

    int M = k; // buffer size

    using Entry = std::pair<int, int>; // Pair of (index, value)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq; // Min-heap

    for (int i = 0; i < M; ++i) {
        int num;
        if (IntReader(input, num)) pq.push({0, num});
        else break; // No more numbers to read
    }

    if (pq.empty()) return {}; // No runs generated
    
    int current_run_index = 0; // Index of the current run
    std::ofstream current_run_output; // Output stream for the current run file

    while (!pq.empty()) {
        auto [index, value] = pq.top(); pq.pop();

        if (index > current_run_index) {
            // Close the current run file and start a new one
            if (current_run_output.is_open()) {
                current_run_output.close();
            }
            current_run_index = index;
        }

        // Open a new run file if not already open
        if (!current_run_output.is_open()) {
            std::string current_run_file(GetRunFileName(current_run_index));
            current_run_output.open(current_run_file);
            run_files.push_back(move(current_run_file));
        }

        // Write the value to the current run file
        IntWriter(current_run_output, value);

        // Read the next number
        int next_num;
        if (IntReader(input, next_num)) {
            int new_index = (next_num >= value) ? index : index + 1;
            pq.push({new_index, next_num});
        }
    }
    
    // Close the last run file if it's still open
    if (current_run_output.is_open()) {
        current_run_output.close();
    }

    return run_files;
}




std::string KWayMerge(const std::vector<std::string> &run_files, int k) {
    if (run_files.empty() || k <= 0) return "";
}