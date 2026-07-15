#include "header.h"
#include <vector>
#include <string>
#include <algorithm>
#include <queue>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>

static const std::string _SAVE_DIR = "save/";

static const std::string _OUTPUT_FILE = _SAVE_DIR + "sorted_output.txt";

static std::unordered_map<std::string, std::size_t> _data_count_cache; // File name to data count cache

inline std::string _GetRunFileName(int run_index) {
    return _SAVE_DIR + "run_" + std::to_string(run_index) + ".txt";
}

std::vector<std::string> GenerateInitialRuns(std::ifstream &input, int k) {
    _data_count_cache.clear(); // Clear cache
    // Check if save directory exists, if not create it
    if (!std::filesystem::exists(_SAVE_DIR)) {
        std::filesystem::create_directory(_SAVE_DIR);
    }

    std::vector<std::string> run_files; // Return vector of run file names

    int M = k; // buffer size

    using Entry = std::pair<std::size_t, int>; // Pair of (index, value)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq; // Min-heap

    for (int i = 0; i < M; ++i) {
        int num;
        if (IntReader(input, num)) pq.push({0, num});
        else break; // No more numbers to read
    }

    if (pq.empty()) return {}; // No runs generated
    
    std::size_t current_run_index = 0; // Index of the current run
    std::string current_run_file = _GetRunFileName(current_run_index); // Current run file name
    std::ofstream current_run_output; // Output stream for the current run file
    std::size_t current_run_data_count = 0; // Count of data items in the current run

    while (!pq.empty()) {
        auto [index, value] = pq.top(); pq.pop();

        if (index > current_run_index) {
            // Close the current run file and update the index
            _data_count_cache[current_run_file] = current_run_data_count; // Cache the data count
            if (current_run_output.is_open()) {
                current_run_output.close();
            }
            current_run_index = index;
        }

        // Open a new run file if not already open
        if (!current_run_output.is_open()) {
            current_run_file = _GetRunFileName(current_run_index);
            current_run_output.open(current_run_file);
            if (!current_run_output.is_open()) return {};
            run_files.push_back(current_run_file);
            current_run_data_count = 0; // Reset data count for the new run
        }

        // Write the value to the current run file
        IntWriter(current_run_output, value);
        ++current_run_data_count;

        // Read the next number
        int next_num;
        if (IntReader(input, next_num)) {
            std::size_t new_index = (next_num >= value) ? index : index + 1;
            pq.push({new_index, next_num});
        }
    }
    
    // Close the last run file if it's still open
    if (current_run_output.is_open()) {
        current_run_output.close();
        _data_count_cache[current_run_file] = current_run_data_count; // Cache the data count
    }

    return run_files;
}

std::string _Merge(const std::vector<std::size_t>& to_merge, const std::vector<std::string>& run_files) {
    std::size_t k = to_merge.size();

    using Entry = std::pair<int, std::size_t>; // Pair of (value, file_index)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;

    // Open input streams for the runs to merge
    std::vector<std::ifstream> inputs(k);
    for (std::size_t i = 0; i < k; ++i) {
        if (inputs[i].is_open()) inputs[i].close();
        inputs[i].open(run_files[to_merge[i]]);
    }

    for (std::size_t i = 0; i < k; ++i) {
        int num;
        if (IntReader(inputs[i], num)) pq.push({num, i});
    }

    std::string merged_file = _GetRunFileName(run_files.size()); // Name for the merged file
    std::ofstream output(merged_file); // Output stream for the merged file
    std::size_t merged_data_count = 0; // Count of data items in the merged file

    while(!pq.empty()) {
        auto [val, idx] = pq.top(); pq.pop();
        if (IntWriter(output, val)) { // Write the smallest value
            ++merged_data_count;
        }

        int next_num;
        if (IntReader(inputs[idx], next_num)) {
            pq.push({next_num, idx}); // Push the next number from the same run
        }
    }

    // Close all the streams
    for (auto& in : inputs) if (in.is_open()) in.close();
    if (output.is_open()) output.close();

    _data_count_cache[merged_file] = merged_data_count; // Cache the data count
    return merged_file; // Return the name of the merged file
}


std::string KWayMerge(const std::vector<std::string> &run_files, int k) {
    if (run_files.empty() || k <= 1) return "";

    std::size_t n = run_files.size(); // Total number of runs

    std::vector<std::string> current_run_files = run_files; // Current set of run files

    std::size_t d = (k - 1 - (n - 1) % (k - 1)) % (k - 1); // Number of dummy runs needed
    for (std::size_t i = 0; i < d; ++i) {
        std::string dummy_run_file = _GetRunFileName(n + i); // Create file name
        std::ofstream dummy_output(dummy_run_file); // Create dummy run file
        dummy_output.close();
        
        current_run_files.push_back(dummy_run_file); // Add to current run files
        _data_count_cache[dummy_run_file] = 0; // Cache the data count
    }

    using Entry = std::pair<std::size_t, std::size_t>; // Pair of (data_count, file_index)
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq; // Min-heap

    for (std::size_t i = 0; i < n + d; ++i) {
        std::size_t data_count = _data_count_cache.at(_GetRunFileName(i));
        pq.push({data_count, i});
    }
    
    while (pq.size() > 1) {
        // Select k runs with the smallest data counts
        std::vector<std::size_t> to_merge;
        for (int i = 0; i < k && !pq.empty(); ++i) {
            auto [cnt, idx] = pq.top(); pq.pop();
            if (_data_count_cache.at(current_run_files[idx]) > 0) {
                to_merge.push_back(idx);
            }
        }
        // Merge the selected runs
        std::string merged_file = _Merge(to_merge, current_run_files);
        
        current_run_files.push_back(merged_file); // Add the merged file to the list
        std::size_t merged_data_count = _data_count_cache.at(merged_file);
        pq.push({merged_data_count, current_run_files.size() - 1}); // Push back into the priority queue
    }

    // Rename the final merged file to the output file
    std::string final_merged_file = current_run_files.back();
    std::filesystem::rename(final_merged_file, _OUTPUT_FILE);

    // Clear the cache and temp files
    _data_count_cache.clear();
    for (const auto& file : current_run_files) {
        std::filesystem::remove(file);
    }
    return _OUTPUT_FILE;
}