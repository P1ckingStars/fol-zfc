#pragma once

// Profiling framework — enabled by PROFILING_ENABLED define.
// When disabled, all macros expand to nothing (zero overhead).
//
// Usage:
//   { PROFILE_SCOPE("parsing"); ... }   // time a scope
//   PROFILE_REPORT();                    // print summary to stderr
//
// Enable via bazel: bazel build --define profiling=1 //src/tools:proof_checker

#ifdef PROFILING_ENABLED

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdio>

class ProfilerImpl {
public:
    struct Entry {
        uint64_t total_ns = 0;
        uint64_t count = 0;
    };

    static std::unordered_map<std::string, Entry>& data() {
        static std::unordered_map<std::string, Entry> instance;
        return instance;
    }

    static void record(const std::string& category, uint64_t ns) {
        auto& e = data()[category];
        e.total_ns += ns;
        e.count++;
    }

    static void report() {
        auto& d = data();
        if (d.empty()) return;

        std::vector<std::pair<std::string, Entry>> sorted(d.begin(), d.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second.total_ns > b.second.total_ns; });

        fprintf(stderr, "\n=== PROFILER REPORT ===\n");
        for (const auto& [name, e] : sorted) {
            double ms = e.total_ns / 1e6;
            fprintf(stderr, "  %-40s %10.1f ms  (%lu calls)\n",
                name.c_str(), ms, (unsigned long)e.count);
        }
        fprintf(stderr, "=======================\n");
    }

    class ScopeTimer {
    public:
        explicit ScopeTimer(const char* category)
            : category_(category),
              start_(std::chrono::high_resolution_clock::now()) {}

        ~ScopeTimer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
            ProfilerImpl::record(category_, ns);
        }

        ScopeTimer(const ScopeTimer&) = delete;
        ScopeTimer& operator=(const ScopeTimer&) = delete;

    private:
        const char* category_;
        std::chrono::high_resolution_clock::time_point start_;
    };
};

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE(name) ProfilerImpl::ScopeTimer PROFILE_CONCAT(_prof_, __LINE__)(name)
#define PROFILE_REPORT() ProfilerImpl::report()

#else  // PROFILING_ENABLED

#define PROFILE_SCOPE(name) ((void)0)
#define PROFILE_REPORT() ((void)0)

#endif  // PROFILING_ENABLED
