#include "report.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "io.h"

namespace {
struct FieldStats {
    float min = 0.0f;
    float max = 0.0f;
    float mean = 0.0f;
    float stddev = 0.0f;
    float nonzero_ratio = 0.0f;
    float p95 = 0.0f;
    float entropy = 0.0f;
    float norm_entropy = 0.0f;
};

bool parse_dump_filename(const std::string &filename, const std::string &prefix, int &step, std::string &field) {
    const std::string tag = prefix + "_step";
    if (filename.rfind(tag, 0) != 0) {
        return false;
    }

    size_t pos = tag.size();
    if (pos + 6 >= filename.size()) {
        return false;
    }
    int step_val = 0;
    for (size_t i = 0; i < 6; ++i) {
        char c = filename[pos + i];
        if (c < '0' || c > '9') {
            return false;
        }
        step_val = step_val * 10 + (c - '0');
    }
    pos += 6;
    if (filename[pos] != '_') {
        return false;
    }
    pos += 1;
    if (filename.size() <= pos + 4) {
        return false;
    }
    if (filename.substr(filename.size() - 4) != ".csv") {
        return false;
    }
    std::string field_name = filename.substr(pos, filename.size() - pos - 4);
    if (field_name != "resources" && field_name != "phero_food" && field_name != "phero_danger" &&
        field_name != "molecules" && field_name != "mycel") {
        return false;
    }
    step = step_val;
    field = field_name;
    return true;
}

FieldStats compute_stats(const std::vector<float> &values, int bins) {
    FieldStats stats;
    if (values.empty()) {
        return stats;
    }
    stats.min = values.front();
    stats.max = values.front();
    double sum = 0.0;
    int nonzero = 0;
    for (float v : values) {
        if (v < stats.min) stats.min = v;
        if (v > stats.max) stats.max = v;
        sum += v;
        if (v > 1e-6f) nonzero++;
    }
    stats.mean = static_cast<float>(sum / static_cast<double>(values.size()));
    double var_sum = 0.0;
    for (float v : values) {
        double d = static_cast<double>(v) - stats.mean;
        var_sum += d * d;
    }
    stats.stddev = static_cast<float>(std::sqrt(var_sum / static_cast<double>(values.size())));
    stats.nonzero_ratio = static_cast<float>(nonzero) / static_cast<float>(values.size());
    std::vector<float> sorted(values.begin(), values.end());
    size_t idx = static_cast<size_t>(std::floor(0.95 * (sorted.size() - 1)));
    std::nth_element(sorted.begin(), sorted.begin() + idx, sorted.end());
    stats.p95 = sorted[idx];

    if (bins <= 1 || stats.max <= stats.min) {
        stats.entropy = 0.0f;
        stats.norm_entropy = 0.0f;
        return stats;
    }
    std::vector<int> hist(static_cast<size_t>(bins), 0);
    double range = static_cast<double>(stats.max - stats.min);
    for (float v : values) {
        int bin = static_cast<int>(std::floor((v - stats.min) / range * bins));
        if (bin < 0) bin = 0;
        if (bin >= bins) bin = bins - 1;
        hist[static_cast<size_t>(bin)]++;
    }
    double ent = 0.0;
    double denom = static_cast<double>(values.size());
    for (int c : hist) {
        if (c <= 0) continue;
        double p = static_cast<double>(c) / denom;
        ent -= p * std::log(p);
    }
    stats.entropy = static_cast<float>(ent);
    stats.norm_entropy = static_cast<float>(ent / std::log(static_cast<double>(bins)));
    return stats;
}

std::vector<float> downsample_grid(int width, int height, const std::vector<float> &values, int target) {
    if (target <= 0 || width <= 0 || height <= 0) {
        return {};
    }
    std::vector<float> out(static_cast<size_t>(target) * static_cast<size_t>(target), 0.0f);
    for (int ty = 0; ty < target; ++ty) {
        int y0 = static_cast<int>(std::floor(static_cast<double>(ty) * height / target));
        int y1 = static_cast<int>(std::floor(static_cast<double>(ty + 1) * height / target));
        if (y1 <= y0) y1 = std::min(height, y0 + 1);
        for (int tx = 0; tx < target; ++tx) {
            int x0 = static_cast<int>(std::floor(static_cast<double>(tx) * width / target));
            int x1 = static_cast<int>(std::floor(static_cast<double>(tx + 1) * width / target));
            if (x1 <= x0) x1 = std::min(width, x0 + 1);
            double sum = 0.0;
            int count = 0;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    sum += values[static_cast<size_t>(y) * width + x];
                    count++;
                }
            }
            float avg = (count > 0) ? static_cast<float>(sum / count) : 0.0f;
            out[static_cast<size_t>(ty) * target + tx] = avg;
        }
    }
    return out;
}

std::string render_svg_heatmap(const std::vector<float> &values, int size, float min, float max) {
    if (values.empty() || size <= 0) {
        return "";
    }
    const int cell = 4;
    const int w = size * cell;
    const int h = size * cell;
    std::ostringstream ss;
    ss << "<svg width=\"" << w << "\" height=\"" << h << "\" viewBox=\"0 0 " << w << " " << h
       << "\" xmlns=\"http://www.w3.org/2000/svg\" shape-rendering=\"crispEdges\">";
    float range = max - min;
    if (range <= 0.0f) {
        range = 1.0f;
    }
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float v = values[static_cast<size_t>(y) * size + x];
            float norm = (v - min) / range;
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;
            int shade = static_cast<int>(std::round(norm * 255.0f));
            ss << "<rect x=\"" << x * cell << "\" y=\"" << y * cell << "\" width=\"" << cell
               << "\" height=\"" << cell << "\" fill=\"rgb(" << shade << "," << shade << "," << shade
               << ")\"/>";
        }
    }
    ss << "</svg>";
    return ss.str();
}

std::string sparkline(const std::vector<float> &values, float &out_min, float &out_max) {
    static const char *blocks[] = {
        "&#9601;", "&#9602;", "&#9603;", "&#9604;",
        "&#9605;", "&#9606;", "&#9607;", "&#9608;"
    };
    if (values.empty()) {
        out_min = 0.0f;
        out_max = 0.0f;
        return "";
    }
    out_min = values.front();
    out_max = values.front();
    for (float v : values) {
        if (v < out_min) out_min = v;
        if (v > out_max) out_max = v;
    }
    float range = out_max - out_min;
    std::string out;
    for (float v : values) {
        float t = (range > 0.0f) ? (v - out_min) / range : 0.0f;
        int idx = static_cast<int>(std::round(t * 7.0f));
        if (idx < 0) idx = 0;
        if (idx > 7) idx = 7;
        out += blocks[idx];
    }
    return out;
}

std::string make_relative_link(const std::filesystem::path &from_dir, const std::filesystem::path &to_file) {
    std::error_code ec;
    std::filesystem::path rel = std::filesystem::relative(to_file, from_dir, ec);
    if (ec) {
        return to_file.filename().generic_string();
    }
    return rel.generic_string();
}
} // namespace

bool generate_dump_report_html(const ReportOptions &opts, std::string &error) {
    if (opts.dump_dir.empty()) {
        error = "Dump-Verzeichnis ist leer";
        return false;
    }
    if (opts.dump_prefix.empty()) {
        error = "Dump-Prefix ist leer";
        return false;
    }
    if (opts.hist_bins <= 0) {
        error = "Histogramm-Bins muessen > 0 sein";
        return false;
    }

    std::filesystem::path dump_dir = opts.dump_dir;
    if (!std::filesystem::exists(dump_dir)) {
        error = "Dump-Verzeichnis existiert nicht: " + opts.dump_dir;
        return false;
    }

    std::map<int, std::map<std::string, std::filesystem::path>> mapping;
    for (const auto &entry : std::filesystem::directory_iterator(dump_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        int step = 0;
        std::string field;
        if (!parse_dump_filename(name, opts.dump_prefix, step, field)) {
            continue;
        }
        mapping[step][field] = entry.path();
    }

    if (mapping.empty()) {
        error = "Keine Dump-Dateien gefunden";
        return false;
    }

    const std::vector<std::string> fields = {"resources", "phero_food", "phero_danger", "molecules", "mycel"};
    std::map<int, SystemMetrics> system_by_step;
    for (const auto &m : opts.system_metrics) {
        system_by_step[m.step] = m;
    }

    struct StepData {
        int step = 0;
        int width = 0;
        int height = 0;
        std::map<std::string, GridData> grids;
        std::map<std::string, FieldStats> stats;
        std::map<std::string, std::string> previews;
        std::map<std::string, std::filesystem::path> paths;
        FieldStats alpha_beta_stats;
        std::string alpha_beta_preview;
    };

    std::vector<StepData> steps;
    steps.reserve(mapping.size());

    for (const auto &pair : mapping) {
        StepData data;
        data.step = pair.first;
        for (const auto &field : fields) {
            auto it = pair.second.find(field);
            if (it == pair.second.end()) {
                error = "Fehlendes Feld " + field + " fuer Step " + std::to_string(pair.first);
                return false;
            }
            data.paths[field] = it->second;
        }
        steps.push_back(std::move(data));
    }

    for (auto &step : steps) {
        int step_width = 0;
        int step_height = 0;
        for (const auto &field : fields) {
            GridData grid;
            std::string load_error;
            if (!load_grid_csv(step.paths[field].string(), grid, load_error)) {
                error = "CSV-Fehler: " + load_error;
                return false;
            }
            if (grid.values.empty()) {
                error = "Leere CSV: " + step.paths[field].string();
                return false;
            }
            if (step_width == 0 && step_height == 0) {
                step_width = grid.width;
                step_height = grid.height;
            } else if (grid.width != step_width || grid.height != step_height) {
                error = "Inkonsistente Rastergroesse in Step " + std::to_string(step.step);
                return false;
            }
            step.grids[field] = std::move(grid);
        }
        step.width = step_width;
        step.height = step_height;

        for (const auto &field : fields) {
            const auto &grid = step.grids[field];
            step.stats[field] = compute_stats(grid.values, opts.hist_bins);
            if (opts.downsample > 0) {
                std::vector<float> down = downsample_grid(grid.width, grid.height, grid.values, opts.downsample);
                step.previews[field] = render_svg_heatmap(down, opts.downsample, step.stats[field].min, step.stats[field].max);
            }
        }

        auto alpha_it = step.grids.find("phero_food");
        auto beta_it = step.grids.find("phero_danger");
        if (alpha_it != step.grids.end() && beta_it != step.grids.end()) {
            const auto &alpha = alpha_it->second.values;
            const auto &beta = beta_it->second.values;
            std::vector<float> diff(alpha.size(), 0.0f);
            for (size_t i = 0; i < diff.size(); ++i) {
                diff[i] = alpha[i] - beta[i];
            }
            step.alpha_beta_stats = compute_stats(diff, opts.hist_bins);
            if (opts.downsample > 0) {
                std::vector<float> down = downsample_grid(step.width, step.height, diff, opts.downsample);
                step.alpha_beta_preview = render_svg_heatmap(down, opts.downsample,
                                                            step.alpha_beta_stats.min,
                                                            step.alpha_beta_stats.max);
            }
        }
    }

    std::map<std::string, std::pair<float, float>> global_minmax;
    for (const auto &field : fields) {
        bool init = false;
        float gmin = 0.0f;
        float gmax = 0.0f;
        for (const auto &step : steps) {
            const auto &stats = step.stats.at(field);
            if (!init) {
                gmin = stats.min;
                gmax = stats.max;
                init = true;
            } else {
                gmin = std::min(gmin, stats.min);
                gmax = std::max(gmax, stats.max);
            }
        }
        global_minmax[field] = {gmin, gmax};
    }
    float alpha_beta_global_min = 0.0f;
    float alpha_beta_global_max = 0.0f;
    {
        bool init = false;
        for (const auto &step : steps) {
            if (!init) {
                alpha_beta_global_min = step.alpha_beta_stats.min;
                alpha_beta_global_max = step.alpha_beta_stats.max;
                init = true;
            } else {
                alpha_beta_global_min = std::min(alpha_beta_global_min, step.alpha_beta_stats.min);
                alpha_beta_global_max = std::max(alpha_beta_global_max, step.alpha_beta_stats.max);
            }
        }
    }

    if (opts.global_normalization && opts.downsample > 0) {
        for (auto &step : steps) {
            for (const auto &field : fields) {
                const auto &grid = step.grids[field];
                std::vector<float> down = downsample_grid(grid.width, grid.height, grid.values, opts.downsample);
                auto minmax = global_minmax[field];
                step.previews[field] = render_svg_heatmap(down, opts.downsample, minmax.first, minmax.second);
            }
            if (!step.alpha_beta_preview.empty()) {
                const auto &alpha = step.grids.at("phero_food").values;
                const auto &beta = step.grids.at("phero_danger").values;
                std::vector<float> diff(alpha.size(), 0.0f);
                for (size_t i = 0; i < diff.size(); ++i) {
                    diff[i] = alpha[i] - beta[i];
                }
                std::vector<float> down = downsample_grid(step.width, step.height, diff, opts.downsample);
                step.alpha_beta_preview = render_svg_heatmap(down, opts.downsample,
                                                             alpha_beta_global_min, alpha_beta_global_max);
            }
        }
    }

    if (opts.paper_mode) {
        std::filesystem::path metrics_path = dump_dir / (opts.dump_prefix + "_metrics.csv");
        std::ofstream metrics(metrics_path);
        if (!metrics.is_open()) {
            error = "Metrics CSV konnte nicht geschrieben werden: " + metrics_path.string();
            return false;
        }
        metrics << "step,field,min,max,mean,stddev,nonzero_ratio,p95,entropy,norm_entropy,dna_pool_size,dna_global_size,dna_s0,dna_s1,dna_s2,dna_s3,avg_agent_energy,energy_s0,energy_s1,energy_s2,energy_s3\n";
        for (const auto &step : steps) {
            for (const auto &field : fields) {
                const auto &stats = step.stats.at(field);
                metrics << step.step << "," << field << ","
                        << std::fixed << std::setprecision(6)
                        << stats.min << "," << stats.max << "," << stats.mean << "," << stats.stddev << ","
                        << stats.nonzero_ratio << "," << stats.p95 << "," << stats.entropy << "," << stats.norm_entropy
                        << ",,,,,,,,,,,\n";
            }
            auto it = system_by_step.find(step.step);
            if (it != system_by_step.end()) {
                const auto &m = it->second;
                metrics << step.step << ",__system__,"
                        << "0,0,0,0,0,0,0,0,"
                        << m.dna_pool_size << ","
                        << m.dna_global_size << ","
                        << m.dna_species_sizes[0] << "," << m.dna_species_sizes[1] << "," << m.dna_species_sizes[2] << "," << m.dna_species_sizes[3] << ","
                        << std::fixed << std::setprecision(6) << m.avg_agent_energy << ","
                        << m.avg_energy_by_species[0] << "," << m.avg_energy_by_species[1] << "," << m.avg_energy_by_species[2] << "," << m.avg_energy_by_species[3]
                        << "\n";
            }
        }
    }

    std::filesystem::path report_path;
    if (opts.report_html_path.empty()) {
        report_path = dump_dir / (opts.dump_prefix + "_report.html");
    } else {
        report_path = opts.report_html_path;
    }

    std::ofstream out(report_path);
    if (!out.is_open()) {
        error = "Report konnte nicht geschrieben werden: " + report_path.string();
        return false;
    }

    std::filesystem::path report_dir = report_path.parent_path();
    out << "<!doctype html>\n";
    out << "<html><head><meta charset=\"utf-8\">";
    out << "<title>Micro-Swarm Dump Report</title>";
    out << "<style>";
    out << "body{font-family:Arial,Helvetica,sans-serif;margin:20px;color:#222;}";
    out << "table{border-collapse:collapse;width:100%;margin:10px 0;}";
    out << "th,td{border:1px solid #ccc;padding:6px 8px;vertical-align:top;}";
    out << "th{background:#f2f2f2;text-align:left;}";
    out << ".meta{margin-bottom:16px;}";
    out << ".preview{margin-top:4px;}";
    out << "</style></head><body>";
    out << "<h1>Micro-Swarm Dump Report</h1>";
    out << "<div class=\"meta\">";
    out << "<div>dump_dir: " << opts.dump_dir << "</div>";
    out << "<div>prefix: " << opts.dump_prefix << "</div>";
    out << "<div>steps: " << steps.size() << "</div>";
    out << "<div>normalization: " << (opts.global_normalization ? "global" : "local") << "</div>";
    out << "</div>";

    if (!opts.scenario_summary.empty()) {
        out << "<h2>Scenario</h2>";
        out << "<div>" << opts.scenario_summary << "</div>";
    }

    if (opts.include_sparklines) {
        out << "<h2>Summary over time</h2>";
        out << "<table>";
        out << "<tr><th>Field</th><th>mean</th><th>nonzero_ratio</th><th>norm_entropy</th></tr>";
        for (const auto &field : fields) {
            std::vector<float> mean_series;
            std::vector<float> nonzero_series;
            std::vector<float> entropy_series;
            mean_series.reserve(steps.size());
            nonzero_series.reserve(steps.size());
            entropy_series.reserve(steps.size());
            for (const auto &step : steps) {
                const auto &stats = step.stats.at(field);
                mean_series.push_back(stats.mean);
                nonzero_series.push_back(stats.nonzero_ratio);
                entropy_series.push_back(stats.norm_entropy);
            }
            float minv = 0.0f;
            float maxv = 0.0f;
            std::string mean_spark = sparkline(mean_series, minv, maxv);
            float nz_min = 0.0f;
            float nz_max = 0.0f;
            std::string nz_spark = sparkline(nonzero_series, nz_min, nz_max);
            float ent_min = 0.0f;
            float ent_max = 0.0f;
            std::string ent_spark = sparkline(entropy_series, ent_min, ent_max);

            out << "<tr>";
            out << "<td>" << field << "</td>";
            out << "<td>" << mean_spark << " <span>(" << std::fixed << std::setprecision(4) << minv << " .. " << maxv << ")</span></td>";
            out << "<td>" << nz_spark << " <span>(" << std::fixed << std::setprecision(4) << nz_min << " .. " << nz_max << ")</span></td>";
            out << "<td>" << ent_spark << " <span>(" << std::fixed << std::setprecision(4) << ent_min << " .. " << ent_max << ")</span></td>";
            out << "</tr>";
        }
        out << "</table>";

        out << "<h2>Semantics (Alpha/Beta)</h2>";
        out << "<table>";
        out << "<tr><th>Metric</th><th>Sparkline</th></tr>";
        auto series_for = [&](auto getter) {
            std::vector<float> series;
            series.reserve(steps.size());
            for (const auto &step : steps) {
                series.push_back(static_cast<float>(getter(step)));
            }
            return series;
        };
        auto add_sem_row = [&](const std::string &label, const std::vector<float> &series) {
            float minv = 0.0f;
            float maxv = 0.0f;
            std::string s = sparkline(series, minv, maxv);
            out << "<tr><td>" << label << "</td><td>" << s
                << " <span>(" << std::fixed << std::setprecision(4) << minv << " .. " << maxv << ")</span></td></tr>";
        };
        add_sem_row("alpha_mean", series_for([](const StepData &s) { return s.stats.at("phero_food").mean; }));
        add_sem_row("beta_mean", series_for([](const StepData &s) { return s.stats.at("phero_danger").mean; }));
        add_sem_row("alpha_minus_beta_mean", series_for([](const StepData &s) { return s.alpha_beta_stats.mean; }));
        out << "</table>";

        if (!system_by_step.empty()) {
            out << "<h2>System over time</h2>";
            out << "<table>";
            out << "<tr><th>Metric</th><th>Sparkline</th></tr>";

            auto collect_series = [&](auto getter) {
                std::vector<float> series;
                series.reserve(steps.size());
                for (const auto &step : steps) {
                    auto it = system_by_step.find(step.step);
                    if (it != system_by_step.end()) {
                        series.push_back(static_cast<float>(getter(it->second)));
                    } else {
                        series.push_back(0.0f);
                    }
                }
                return series;
            };

            auto add_row = [&](const std::string &label, const std::vector<float> &series) {
                float minv = 0.0f;
                float maxv = 0.0f;
                std::string s = sparkline(series, minv, maxv);
                out << "<tr><td>" << label << "</td><td>" << s
                    << " <span>(" << std::fixed << std::setprecision(4) << minv << " .. " << maxv << ")</span></td></tr>";
            };

            add_row("dna_global_size", collect_series([](const SystemMetrics &m) { return m.dna_global_size; }));
            add_row("dna_s0", collect_series([](const SystemMetrics &m) { return m.dna_species_sizes[0]; }));
            add_row("dna_s1", collect_series([](const SystemMetrics &m) { return m.dna_species_sizes[1]; }));
            add_row("dna_s2", collect_series([](const SystemMetrics &m) { return m.dna_species_sizes[2]; }));
            add_row("dna_s3", collect_series([](const SystemMetrics &m) { return m.dna_species_sizes[3]; }));
            add_row("energy_s0", collect_series([](const SystemMetrics &m) { return m.avg_energy_by_species[0]; }));
            add_row("energy_s1", collect_series([](const SystemMetrics &m) { return m.avg_energy_by_species[1]; }));
            add_row("energy_s2", collect_series([](const SystemMetrics &m) { return m.avg_energy_by_species[2]; }));
            add_row("energy_s3", collect_series([](const SystemMetrics &m) { return m.avg_energy_by_species[3]; }));
            add_row("cognitive_load", collect_series([](const SystemMetrics &m) { return m.avg_cognitive_load; }));

            out << "</table>";
        }
    }

    for (const auto &step : steps) {
        out << "<h2>Step " << step.step << "</h2>";
        out << "<table>";
        out << "<tr><th>Field</th><th>CSV</th><th>Stats</th><th>Preview</th></tr>";
        for (const auto &field : fields) {
            const auto &stats = step.stats.at(field);
            std::string link = make_relative_link(report_dir, step.paths.at(field));
            out << "<tr>";
            out << "<td>" << field << "</td>";
            out << "<td><a href=\"" << link << "\">" << link << "</a></td>";
            out << "<td>";
            out << "min=" << std::fixed << std::setprecision(4) << stats.min << "<br>";
            out << "max=" << std::fixed << std::setprecision(4) << stats.max << "<br>";
            out << "mean=" << std::fixed << std::setprecision(4) << stats.mean << "<br>";
            out << "stddev=" << std::fixed << std::setprecision(4) << stats.stddev << "<br>";
            out << "nonzero_ratio=" << std::fixed << std::setprecision(4) << stats.nonzero_ratio << "<br>";
            out << "p95=" << std::fixed << std::setprecision(4) << stats.p95 << "<br>";
            out << "entropy=" << std::fixed << std::setprecision(4) << stats.entropy << "<br>";
            out << "norm_entropy=" << std::fixed << std::setprecision(4) << stats.norm_entropy;
            out << "</td>";
            out << "<td>";
            if (opts.downsample > 0) {
                out << "<div class=\"preview\">" << step.previews.at(field) << "</div>";
            } else {
                out << "-";
            }
            out << "</td>";
            out << "</tr>";
        }
        out << "<tr>";
        out << "<td>alpha_minus_beta</td>";
        out << "<td>-</td>";
        out << "<td>";
        out << "min=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.min << "<br>";
        out << "max=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.max << "<br>";
        out << "mean=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.mean << "<br>";
        out << "stddev=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.stddev << "<br>";
        out << "nonzero_ratio=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.nonzero_ratio << "<br>";
        out << "p95=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.p95 << "<br>";
        out << "entropy=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.entropy << "<br>";
        out << "norm_entropy=" << std::fixed << std::setprecision(4) << step.alpha_beta_stats.norm_entropy;
        out << "</td>";
        out << "<td>";
        if (opts.downsample > 0 && !step.alpha_beta_preview.empty()) {
            out << "<div class=\"preview\">" << step.alpha_beta_preview << "</div>";
        } else {
            out << "-";
        }
        out << "</td>";
        out << "</tr>";
        out << "</table>";
    }

    out << "</body></html>";
    return true;
}
