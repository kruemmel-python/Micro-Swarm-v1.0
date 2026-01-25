#pragma once

#include <string>
#include <vector>

struct SystemMetrics {
    int step = 0;
    int dna_pool_size = 0;
    float avg_agent_energy = 0.0f;
    float avg_cognitive_load = 0.0f;
    int dna_global_size = 0;
    int dna_species_sizes[4] = {0, 0, 0, 0};
    float avg_energy_by_species[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct ReportOptions {
    std::string dump_dir;
    std::string dump_prefix;
    std::string report_html_path;
    int downsample = 32;
    bool paper_mode = false;
    bool global_normalization = false;
    int hist_bins = 64;
    bool include_sparklines = true;
    std::string scenario_summary;
    std::vector<SystemMetrics> system_metrics;
};

bool generate_dump_report_html(const ReportOptions &opts, std::string &error);
