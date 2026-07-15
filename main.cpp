#include "header.h"
#include <string>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <algorithm>
#include <vector>
#include <atomic>
#include <functional>
#include <climits>
#include <sstream>
#include <array>


bool ReadPlainInt(std::ifstream &input, int &num) {
    if (input.eof()) {
        return false;
    }
    std::string line;
    std::getline(input, line);
    bool has_digit = false;
    for (char c : line) {
        if (c >= '0' && c <= '9') {
            has_digit = true;
            break;
        }
    }
    if (!has_digit) {
        return false;
    }
    std::stringstream ss(line);
    ss >> num;
    return true;
}

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <psapi.h>
    unsigned long long getCurrentMemory() {
        HANDLE handle = GetCurrentProcess();
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(handle, &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
        return 0;
    }
#else
    #include <sys/resource.h>
    unsigned long long getCurrentMemory() {
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
#ifdef __APPLE__
            return (unsigned long long)usage.ru_maxrss;
#else
            return (unsigned long long)usage.ru_maxrss * 1024;
#endif
        }
        return 0;
    }
#endif

const int N = 10000; // 10000000
const std::array<int, 2> TEST_KS = {4, 64};
const unsigned int BENCHMARK_SEED = 20260706u;

unsigned int HashStudentID(const std::string &student_id) {
    return static_cast<unsigned int>(std::hash<std::string>{}(student_id));
}

void GenerateNumbers(const std::string &input_file_name, unsigned int student_seed) {
    std::ofstream unsorted(input_file_name);
    std::mt19937 engine(student_seed);
    std::uniform_int_distribution<> distrib(0, 1 << 30);
    for (int i = 0; i < N; i++) {
        unsorted << distrib(engine) << '\n';
    }
    unsorted.close();
}


struct BenchmarkResult {
    bool phase1_ok = false;
    bool phase2_ok = false;
    unsigned long long peak_memory_bytes = 0;
};

bool VerifyInitialRuns(const std::vector<std::string> &run_files, int expected_total) {
    if (run_files.empty()) {
        std::cerr << "[Phase1 Error]: GenerateInitialRuns returned empty list.\n";
        return false;
    }

    int total = 0;
    for (size_t i = 0; i < run_files.size(); i++) {
        std::ifstream in(run_files[i]);
        if (!in.is_open()) {
            std::cerr << "[Phase1 Error]: Cannot open run file: " << run_files[i] << "\n";
            return false;
        }

        int prev, cur;
        bool first = true;
        bool sorted = true;
        int count_in_run = 0;
        while (ReadPlainInt(in, cur)) {
            if (!first && cur < prev) {
                sorted = false;
            }
            prev = cur;
            first = false;
            count_in_run++;
        }
        in.close();

        if (count_in_run == 0) {
            std::cerr << "  " << run_files[i] << "  EMPTY  [ERROR]\n";
            return false;
        }

        std::cerr << "  " << run_files[i] << " : "
                  << count_in_run << " numbers, "
                  << (sorted ? "[sorted]" : "[NOT SORTED]") << "\n";

        if (!sorted) return false;
        total += count_in_run;
    }
    std::cerr << "  Total Check: " << total << " numbers across " << run_files.size() << " runs\n";
    return total == expected_total;
}

bool VerifyFinalSorted(const std::string &output_file_name) {
    std::ifstream sorted(output_file_name);
    if (!sorted.is_open()) return false;
    int cur;
    bool first = true;
    bool sorted_flag = true;
    int prev = 0;
    while (ReadPlainInt(sorted, cur)) {
        if (!first && cur < prev) {
            sorted_flag = false;
            break;
        }
        prev = cur;
        first = false;
    }
    sorted.close();
    return sorted_flag;
}

bool CheckFinalOutput(const std::string &output_file_name, unsigned int student_seed) {
    std::ifstream sorted(output_file_name);
    if (!sorted.is_open()) return false;
    int b;
    std::vector<int> arra, arrb;
    std::mt19937 engine(student_seed);
    std::uniform_int_distribution<> distrib(0, 1 << 30);
    for (int i = 0; i < N; i++) arra.emplace_back(distrib(engine));
    while (ReadPlainInt(sorted, b)) arrb.emplace_back(b);
    sorted.close();
    if (arra.size() != arrb.size()) return false;
    std::sort(arra.begin(), arra.end());
    for (size_t i = 0; i < arra.size(); i++) {
        if (arra[i] != arrb[i]) return false;
    }
    return true;
}

void OutputScore(int score, const std::string &student_id) {
    std::ofstream result_file(student_id + "_result.txt");
    result_file << score;
    result_file.close();
}


int ScoreFromMemory(unsigned long long peak_memory_bytes) {
    double peak_memory_mb = peak_memory_bytes / (1024.0 * 1024.0);
    if (peak_memory_mb <= 0.0) {
        return 100;
    }
    int score = static_cast<int>(200.0 / peak_memory_mb);
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return score;
}


BenchmarkResult RunBenchmarkCase(const std::string &input_file_name, int k, unsigned int student_seed) {
    BenchmarkResult result;
    std::atomic<bool> sort_end(false);
    unsigned long long memory_cost = 0;
    std::ifstream input(input_file_name);

    std::thread monitor([&memory_cost, &sort_end]() {
        while (!sort_end.load()) {
            unsigned long long cur = getCurrentMemory();
            if (cur > memory_cost) memory_cost = cur;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::cerr << "--- Benchmark: k = " << k << " ---\n";
    std::vector<std::string> run_files = GenerateInitialRuns(input, k);
    input.close();

    result.phase1_ok = VerifyInitialRuns(run_files, N);
    if (!result.phase1_ok) {
        sort_end.store(true);
        monitor.join();
        result.peak_memory_bytes = memory_cost;
        return result;
    }

    std::string output_file_name = KWayMerge(run_files, k);
    sort_end.store(true);
    monitor.join();

    result.phase2_ok = !output_file_name.empty() && VerifyFinalSorted(output_file_name) && CheckFinalOutput(output_file_name, student_seed);
    result.peak_memory_bytes = memory_cost;
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <student_id> [k]\n";
        return 1;
    }
    std::string student_id = argv[1];
    bool has_cli_k = argc >= 3;
    int cli_k = has_cli_k ? std::atoi(argv[2]) : TEST_KS[0];

    std::cerr << "Student ID: " << student_id << "\n";

    unsigned int student_seed = BENCHMARK_SEED;
    std::string input_file_name = "benchmark_big.input";
    GenerateNumbers(input_file_name, student_seed);
    ResetIOStats();

    unsigned long long peak_memory_bytes = 0;
    bool all_passed = true;
    std::vector<int> benchmark_ks;
    if (has_cli_k) {
        benchmark_ks.push_back(cli_k);
    } else {
        benchmark_ks.assign(TEST_KS.begin(), TEST_KS.end());
    }

    for (int k : benchmark_ks) {
        std::cerr << "\n=== Running benchmark with k = " << k << " ===\n";
        BenchmarkResult result = RunBenchmarkCase(input_file_name, k, student_seed);
        peak_memory_bytes = std::max(peak_memory_bytes, result.peak_memory_bytes);
        std::cerr << "Phase 1: " << (result.phase1_ok ? "PASS" : "FAILED") << "\n";
        std::cerr << "Phase 2: " << (result.phase2_ok ? "PASS" : "FAILED") << "\n";
        all_passed = all_passed && result.phase1_ok && result.phase2_ok;
    }

    int total_score = all_passed ? ScoreFromMemory(peak_memory_bytes) : 0;
    double peak_memory_mb = peak_memory_bytes / (1024.0 * 1024.0);

    std::cerr << "\n====== Final Step Score Breakdown ======\n";
    std::cerr << "  k values tested          : ";
    if (has_cli_k) {
        std::cerr << cli_k << "\n";
    } else {
        std::cerr << "4, 64\n";
    }
    std::cerr << "  Peak Memory              : " << peak_memory_mb << " MB\n";
    std::cerr << "  File Read Ops            : " << GetIOStats().read_ops << "\n";
    std::cerr << "  File Write Ops           : " << GetIOStats().write_ops << "\n";
    std::cerr << "========================================\n";
    OutputScore(total_score, student_id);
    return 0;
}