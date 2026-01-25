#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>
#include <cmath>
#include <array>
#include <unordered_map>
#include <chrono>
#include <ctime>

#include "compute/opencl_loader.h"
#include "compute/opencl_runtime.h"
#include "sim/agent.h"
#include "sim/db_engine.h"
#include "sim/db_sql.h"
#include "sim/dna_memory.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/io.h"
#include "sim/mycel.h"
#include "sim/params.h"
#include "sim/report.h"
#include "sim/rng.h"

namespace {
struct CliOptions {
    bool width_set = false;
    bool height_set = false;
    SimParams params;
    uint32_t seed = 42;
    std::string resources_path;
    std::string pheromone_path;
    std::string molecules_path;
    int dump_every = 0;
    std::string dump_dir = "dumps";
    std::string dump_prefix = "swarm";
    std::string dump_subdir;
    std::string report_html_path;
    int report_downsample = 32;
    bool paper_mode = false;
    bool report_global_norm = false;
    int report_hist_bins = 64;
    bool report_include_sparklines = true;
    int log_verbosity = 1;
    bool logic_inputs_set = false;
    bool logic_output_set = false;
    std::string dna_export_path;

    bool ocl_enable = false;
    int ocl_device = 0;
    int ocl_platform = 0;
    bool ocl_print_devices = false;
    bool ocl_no_copyback = false;

    bool stress_enable = false;
    int stress_at_step = 120;
    bool stress_block_rect_set = false;
    int stress_block_x = 0;
    int stress_block_y = 0;
    int stress_block_w = 0;
    int stress_block_h = 0;
    bool stress_shift_set = false;
    int stress_shift_dx = 0;
    int stress_shift_dy = 0;
    float stress_pheromone_noise = 0.0f;
    uint32_t stress_seed = 0;
    bool stress_seed_set = false;

    bool evo_enable = false;
    float evo_elite_frac = 0.20f;
    float evo_min_energy_to_store = 1.6f;
    float evo_mutation_sigma = 0.05f;
    float evo_exploration_delta = 0.05f;
    int evo_fitness_window = 50;
    float evo_age_decay = 0.995f;

    std::array<SpeciesProfile, 4> species_profiles;
    std::array<float, 4> species_fracs{0.40f, 0.25f, 0.20f, 0.15f};
    float global_spawn_frac = 0.15f;

    std::string mode = "sim";
    std::string db_input;
    std::string db_output;
    std::string db_dump_path;
    int db_dump_scale = 4;
    std::string ingest_rules_path;
    std::string db_path;
    std::string db_query;
    int db_radius = 5;
    int db_merge_agents = 256;
    int db_merge_steps = 2000;
    uint32_t db_merge_seed = 42;
    int db_merge_threshold = 0;
    std::string sql_output_format = "table";
};

std::array<SpeciesProfile, 4> default_species_profiles() {
    std::array<SpeciesProfile, 4> profiles;

    SpeciesProfile explorator;
    explorator.exploration_mul = 1.4f;
    explorator.food_attraction_mul = 0.6f;
    explorator.danger_aversion_mul = 0.8f;
    explorator.deposit_food_mul = 0.6f;
    explorator.deposit_danger_mul = 0.5f;
    explorator.resource_weight_mul = 1.4f;
    explorator.molecule_weight_mul = 1.4f;
    explorator.mycel_attraction_mul = 0.6f;
    explorator.novelty_weight = 0.6f;
    explorator.mutation_sigma_mul = 1.0f;
    explorator.exploration_delta_mul = 1.0f;
    explorator.dna_binding = 0.9f;

    SpeciesProfile integrator;
    integrator.exploration_mul = 0.7f;
    integrator.food_attraction_mul = 1.4f;
    integrator.danger_aversion_mul = 1.0f;
    integrator.deposit_food_mul = 1.5f;
    integrator.deposit_danger_mul = 0.8f;
    integrator.resource_weight_mul = 0.9f;
    integrator.molecule_weight_mul = 0.8f;
    integrator.mycel_attraction_mul = 1.5f;
    integrator.novelty_weight = 0.0f;
    integrator.mutation_sigma_mul = 1.0f;
    integrator.exploration_delta_mul = 1.0f;
    integrator.dna_binding = 1.0f;

    SpeciesProfile regulator;
    regulator.exploration_mul = 0.9f;
    regulator.food_attraction_mul = 0.8f;
    regulator.danger_aversion_mul = 1.8f;
    regulator.deposit_food_mul = 0.8f;
    regulator.deposit_danger_mul = 1.4f;
    regulator.resource_weight_mul = 0.9f;
    regulator.molecule_weight_mul = 0.8f;
    regulator.mycel_attraction_mul = 0.8f;
    regulator.novelty_weight = 0.0f;
    regulator.mutation_sigma_mul = 1.0f;
    regulator.exploration_delta_mul = 1.0f;
    regulator.dna_binding = 1.0f;
    regulator.over_density_threshold = 0.6f;
    regulator.counter_deposit_mul = 0.5f;

    SpeciesProfile innovator;
    innovator.exploration_mul = 1.3f;
    innovator.food_attraction_mul = 0.7f;
    innovator.danger_aversion_mul = 0.9f;
    innovator.deposit_food_mul = 0.7f;
    innovator.deposit_danger_mul = 0.7f;
    innovator.resource_weight_mul = 1.1f;
    innovator.molecule_weight_mul = 1.2f;
    innovator.mycel_attraction_mul = 0.6f;
    innovator.novelty_weight = 0.8f;
    innovator.mutation_sigma_mul = 1.6f;
    innovator.exploration_delta_mul = 1.6f;
    innovator.dna_binding = 0.6f;

    profiles[0] = explorator;
    profiles[1] = integrator;
    profiles[2] = regulator;
    profiles[3] = innovator;
    return profiles;
}

int pick_species(Rng &rng, const std::array<float, 4> &fracs) {
    float r = rng.uniform(0.0f, 1.0f);
    float accum = 0.0f;
    for (int i = 0; i < 4; ++i) {
        accum += fracs[i];
        if (r <= accum) {
            return i;
        }
    }
    return 3;
}

void print_help() {
    std::cout << "micro_swarm Optionen:\n"
              << "  --mode NAME     sim | db_ingest | db_query\n"
              << "  --input PATH    SQL-Input fuer db_ingest\n"
              << "  --output PATH   MYCO-Output fuer db_ingest\n"
              << "  --db-dump PATH  Cluster-PPM-Output fuer db_ingest\n"
              << "  --db-dump-scale N  Skalierung fuer PPM-Output (Default 4)\n"
              << "  --ingest-rules PATH  JSON-Regeln fuer Trait-Cluster beim Ingest\n"
              << "  --db PATH       MYCO-Input fuer db_query\n"
              << "  --query TEXT    Query fuer db_query (SQL-Light)\n"
              << "  --db-radius N   Radius fuer db_query (Default 5)\n"
              << "  --db-merge-agents N   Agentenanzahl fuer Merge (Default 256)\n"
              << "  --db-merge-steps N    Schritte fuer Merge (Default 2000)\n"
              << "  --db-merge-seed N     Seed fuer Merge (Default 42)\n"
              << "  --db-merge-threshold N  Auto-Merge ab Delta-Size N (0=aus)\n"
              << "  --sql-format F  Output-Format fuer SQL (table|csv|json)\n"
              << "  --width N        Rasterbreite\n"
              << "  --height N       Rasterhoehe\n"
              << "  --size N         Setzt Breite und Hoehe gleich\n"
              << "  --agents N       Anzahl Agenten\n"
              << "  --steps N        Simulationsschritte\n"
              << "  --seed N         RNG-Seed\n"
              << "  --info-cost F    Kosten pro Informations-Last\n"
              << "  --resources CSV  Startwerte Ressourcenfeld\n"
              << "  --pheromone CSV  Startwerte Pheromonfeld\n"
              << "  --molecules CSV  Startwerte Molekuelfeld\n"
              << "  --resource-regen F  Ressourcen-Regeneration\n"
              << "  --mycel-growth F     Mycel-Wachstumsrate\n"
              << "  --mycel-decay F      Mycel-Decay\n"
              << "  --mycel-transport F  Mycel-Transport\n"
              << "  --mycel-threshold F  Mycel-Drive-Schwelle\n"
              << "  --mycel-drive-p F    Mycel-Drive-Gewicht Pheromon\n"
              << "  --mycel-drive-r F    Mycel-Drive-Gewicht Ressourcen\n"
              << "  --phero-food-deposit F     Pheromon Food Deposit\n"
              << "  --phero-danger-deposit F   Pheromon Danger Deposit\n"
              << "  --danger-delta-threshold F Danger Delta Schwelle\n"
              << "  --danger-bounce-deposit F  Danger Deposit bei Bounce\n"
              << "  --dump-every N   Dump-Intervall (0=aus)\n"
              << "  --dump-dir PATH  Dump-Verzeichnis\n"
              << "  --dump-prefix N  Dump-Dateiprefix\n"
              << "  [subdir]         Optionaler letzter Parameter: Unterordner in dump-dir\n"
              << "  --report-html PATH  Report-HTML-Pfad\n"
              << "  --report-downsample N  Report-Downsample (0=aus)\n"
              << "  --paper-mode           Paper-Modus aktivieren\n"
              << "  --report-global-norm   Globale Normalisierung fuer Previews\n"
              << "  --report-hist-bins N   Histogramm-Bins fuer Entropie\n"
              << "  --report-no-sparklines Sparklines deaktivieren\n"
              << "  --dna-export PATH   DNA-Pool als CSV exportieren\n"
              << "  --ocl-enable           OpenCL Diffusion aktivieren\n"
              << "  --ocl-device N         OpenCL Device Index\n"
              << "  --ocl-platform N       OpenCL Platform Index\n"
              << "  --ocl-print-devices    OpenCL Platforms/Devices auflisten\n"
              << "  --ocl-no-copyback      Host-Backcopy nur bei Dump/Ende\n"
              << "  --gpu N                Alias fuer OpenCL (0=aus, 1=an)\n"
              << "  --species-fracs f0 f1 f2 f3           Spezies-Anteile\n"
              << "  --species-profile S e f d df dd       Spezies-Profilwerte\n"
              << "  --global-spawn-frac F                 Anteil Spawn aus Global-Pool\n"
              << "  --dna-global-capacity N               Kapazitaet Global-Pool\n"
              << "  --stress-enable                  Stress-Test aktivieren\n"
              << "  --stress-at-step N               Stress-Zeitpunkt\n"
              << "  --stress-block-rect x y w h      Ressourcen-Blockade\n"
              << "  --stress-shift-hotspots dx dy    Hotspots verschieben\n"
              << "  --stress-pheromone-noise F       Pheromon-Noise\n"
              << "  --stress-seed N                  Seed fuer Stress-Noise\n"
              << "  --evo-enable                     Evolution-Tuning aktivieren\n"
              << "  --evo-elite-frac F               Elite-Anteil\n"
              << "  --evo-min-energy-to-store F      Mindestenergie fuer Speicherung\n"
              << "  --evo-mutation-sigma F           Mutationsstaerke\n"
              << "  --evo-exploration-delta F        Exploration-Mutation\n"
              << "  --evo-fitness-window N           Fitness-Fenster\n"
              << "  --evo-age-decay F                Age-Decay pro Tick\n"
              << "  --toxic-enable                   Toxische Codons aktivieren\n"
              << "  --toxic-disable                  Toxische Codons deaktivieren\n"
              << "  --toxic-max-frac F               Max-Anteil toxischer Codons pro Quadrant (0..1)\n"
              << "  --toxic-stride-min N             Min Stride fuer toxische Codons\n"
              << "  --toxic-stride-max N             Max Stride fuer toxische Codons\n"
              << "  --toxic-iters-min N              Min Iterationen fuer toxische Codons\n"
              << "  --toxic-iters-max N              Max Iterationen fuer toxische Codons\n"
              << "  --toxic-max-frac-quadrant Q F    Max-Anteil toxischer Codons pro Quadrant (Q=0..3)\n"
              << "  --toxic-max-frac-species S F     Max-Anteil toxischer Codons pro Spezies (S=0..3)\n"
              << "  --logic-mode NAME               Logic-Target (NONE|XOR|AND|OR)\n"
              << "  --logic-inputs x1 y1 x2 y2       Input-Koordinaten fuer A/B\n"
              << "  --logic-output x y               Output-Koordinate\n"
              << "  --logic-pulse-period N           Puls-Periode in Steps\n"
              << "  --logic-pulse-strength F         Pheromon-Pulsstaerke\n"
              << "  --log-verbosity N                Logging-Level (0=leise,1=normal,2=detail)\n"
              << "  --help           Hilfe anzeigen\n";
}

bool parse_int(const char *value, int &out) {
    try {
        out = std::stoi(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_seed(const char *value, uint32_t &out) {
    try {
        out = static_cast<uint32_t>(std::stoul(value));
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_float(const char *value, float &out) {
    try {
        out = std::stof(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_string(const char *value, std::string &out) {
    if (!value) {
        return false;
    }
    out = value;
    return !out.empty();
}

int parse_logic_mode(const std::string &value, bool &ok) {
    std::string v = value;
    for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (v == "none") { ok = true; return 0; }
    if (v == "xor") { ok = true; return 1; }
    if (v == "and") { ok = true; return 2; }
    if (v == "or") { ok = true; return 3; }
    ok = false;
    return 0;
}

float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float clamp_range(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float gaussian(Rng &rng, float sigma) {
    if (sigma <= 0.0f) return 0.0f;
    float u1 = std::max(1e-6f, rng.uniform(0.0f, 1.0f));
    float u2 = rng.uniform(0.0f, 1.0f);
    float mag = std::sqrt(-2.0f * std::log(u1));
    float z0 = mag * std::cos(6.283185307f * u2);
    return z0 * sigma;
}

void randomize_semantics(Rng &rng, Genome &g) {
    g.response_matrix[0] = 1.0f + rng.uniform(-0.3f, 0.3f);
    g.response_matrix[1] = -1.0f + rng.uniform(-0.3f, 0.3f);
    g.response_matrix[2] = 0.0f + rng.uniform(-0.3f, 0.3f);
    g.emission_matrix[0] = 1.0f + rng.uniform(-0.3f, 0.3f);
    g.emission_matrix[1] = 0.0f + rng.uniform(-0.3f, 0.3f);
    g.emission_matrix[2] = 0.0f + rng.uniform(-0.3f, 0.3f);
    g.emission_matrix[3] = 1.0f + rng.uniform(-0.3f, 0.3f);
}

void clamp_semantics(Genome &g) {
    g.response_matrix[0] = clamp_range(g.response_matrix[0], -2.0f, 2.0f);
    g.response_matrix[1] = clamp_range(g.response_matrix[1], -2.0f, 2.0f);
    g.response_matrix[2] = clamp_range(g.response_matrix[2], -2.0f, 2.0f);
    for (int i = 0; i < 4; ++i) {
        g.emission_matrix[i] = clamp_range(g.emission_matrix[i], -2.0f, 2.0f);
    }
}

void apply_semantic_defaults(Genome &g, const SpeciesProfile &profile) {
    g.response_matrix[0] = clamp_range(profile.food_attraction_mul, -1.5f, 1.5f);
    g.response_matrix[1] = clamp_range(-profile.danger_aversion_mul, -1.5f, 1.5f);
    g.response_matrix[2] = 0.0f;
    g.emission_matrix[0] = clamp_range(profile.deposit_food_mul, -1.5f, 1.5f);
    g.emission_matrix[1] = 0.0f;
    g.emission_matrix[2] = 0.0f;
    g.emission_matrix[3] = clamp_range(profile.deposit_danger_mul, -1.5f, 1.5f);
    clamp_semantics(g);
}

int logic_target_for_case(int mode, int case_idx) {
    int a = (case_idx >> 0) & 1;
    int b = (case_idx >> 1) & 1;
    switch (mode) {
        case 1: return a ^ b;
        case 2: return a & b;
        case 3: return (a | b);
        default: return 0;
    }
}

float distance_to_segment(float ax, float ay, float bx, float by, float px, float py) {
    float vx = bx - ax;
    float vy = by - ay;
    float wx = px - ax;
    float wy = py - ay;
    float c1 = vx * wx + vy * wy;
    if (c1 <= 0.0f) {
        float dx = px - ax;
        float dy = py - ay;
        return std::sqrt(dx * dx + dy * dy);
    }
    float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        float dx = px - bx;
        float dy = py - by;
        return std::sqrt(dx * dx + dy * dy);
    }
    float t = c1 / c2;
    float projx = ax + t * vx;
    float projy = ay + t * vy;
    float dx = px - projx;
    float dy = py - projy;
    return std::sqrt(dx * dx + dy * dy);
}

bool export_dna_csv(const std::string &path,
                    const std::array<DNAMemory, 4> &dna_species,
                    const DNAMemory &dna_global) {
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }
    out << "pool,species,fitness,sense_gain,pheromone_gain,exploration_bias,"
           "response0,response1,response2,emit0,emit1,emit2,emit3,"
           "codon0,codon1,codon2,codon3,lws_x,lws_y,toxic_stride,toxic_iters\n";
    for (int s = 0; s < 4; ++s) {
        for (const auto &e : dna_species[s].entries) {
            out << "species," << s << "," << e.fitness << ","
                << e.genome.sense_gain << "," << e.genome.pheromone_gain << "," << e.genome.exploration_bias << ","
                << e.genome.response_matrix[0] << "," << e.genome.response_matrix[1] << "," << e.genome.response_matrix[2] << ","
                << e.genome.emission_matrix[0] << "," << e.genome.emission_matrix[1] << ","
                << e.genome.emission_matrix[2] << "," << e.genome.emission_matrix[3] << ","
                << e.genome.kernel_codons[0] << "," << e.genome.kernel_codons[1] << ","
                << e.genome.kernel_codons[2] << "," << e.genome.kernel_codons[3] << ","
                << e.genome.lws_x << "," << e.genome.lws_y << ","
                << e.genome.toxic_stride << "," << e.genome.toxic_iters << "\n";
        }
    }
    for (const auto &e : dna_global.entries) {
        out << "global,-1," << e.fitness << ","
            << e.genome.sense_gain << "," << e.genome.pheromone_gain << "," << e.genome.exploration_bias << ","
            << e.genome.response_matrix[0] << "," << e.genome.response_matrix[1] << "," << e.genome.response_matrix[2] << ","
            << e.genome.emission_matrix[0] << "," << e.genome.emission_matrix[1] << ","
            << e.genome.emission_matrix[2] << "," << e.genome.emission_matrix[3] << ","
            << e.genome.kernel_codons[0] << "," << e.genome.kernel_codons[1] << ","
            << e.genome.kernel_codons[2] << "," << e.genome.kernel_codons[3] << ","
            << e.genome.lws_x << "," << e.genome.lws_y << ","
            << e.genome.toxic_stride << "," << e.genome.toxic_iters << "\n";
    }
    return true;
}

bool parse_cli(int argc, char **argv, CliOptions &opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return false;
        }
        if (arg == "--ocl-enable") {
            opts.ocl_enable = true;
            continue;
        }
        if (arg == "--ocl-print-devices") {
            opts.ocl_print_devices = true;
            continue;
        }
        if (arg == "--ocl-no-copyback") {
            opts.ocl_no_copyback = true;
            continue;
        }
        if (!arg.empty() && arg[0] != '-' && i == argc - 1) {
            if (!parse_string(arg.c_str(), opts.dump_subdir)) {
                std::cerr << "Ungueltiger Wert fuer dump-subdir\n";
                return false;
            }
            continue;
        }
        if (arg == "--paper-mode") {
            opts.paper_mode = true;
            continue;
        }
        if (arg == "--report-global-norm") {
            opts.report_global_norm = true;
            continue;
        }
        if (arg == "--report-no-sparklines") {
            opts.report_include_sparklines = false;
            continue;
        }
        if (arg == "--dna-export") {
            if (i + 1 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            if (!parse_string(argv[i + 1], opts.dna_export_path)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            i += 1;
            continue;
        }
        if (arg == "--stress-enable") {
            opts.stress_enable = true;
            continue;
        }
        if (arg == "--evo-enable") {
            opts.evo_enable = true;
            continue;
        }
        if (arg == "--toxic-enable") {
            opts.params.toxic_enable = 1;
            continue;
        }
        if (arg == "--toxic-disable") {
            opts.params.toxic_enable = 0;
            continue;
        }
        if (arg == "--toxic-max-frac-quadrant") {
            if (i + 2 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            int q = 0;
            float f = 0.0f;
            if (!parse_int(argv[i + 1], q) || q < 0 || q > 3 || !parse_float(argv[i + 2], f)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.params.toxic_max_fraction_by_quadrant[q] = f;
            i += 2;
            continue;
        }
        if (arg == "--toxic-max-frac-species") {
            if (i + 2 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            int s = 0;
            float f = 0.0f;
            if (!parse_int(argv[i + 1], s) || s < 0 || s > 3 || !parse_float(argv[i + 2], f)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.params.toxic_max_fraction_by_species[s] = f;
            i += 2;
            continue;
        }
        if (arg == "--logic-inputs") {
            if (i + 4 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            if (!parse_int(argv[i + 1], opts.params.logic_input_ax) ||
                !parse_int(argv[i + 2], opts.params.logic_input_ay) ||
                !parse_int(argv[i + 3], opts.params.logic_input_bx) ||
                !parse_int(argv[i + 4], opts.params.logic_input_by)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.logic_inputs_set = true;
            i += 4;
            continue;
        }
        if (arg == "--logic-output") {
            if (i + 2 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            if (!parse_int(argv[i + 1], opts.params.logic_output_x) ||
                !parse_int(argv[i + 2], opts.params.logic_output_y)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.logic_output_set = true;
            i += 2;
            continue;
        }
        if (arg == "--stress-block-rect") {
            if (i + 4 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            if (!parse_int(argv[i + 1], opts.stress_block_x) ||
                !parse_int(argv[i + 2], opts.stress_block_y) ||
                !parse_int(argv[i + 3], opts.stress_block_w) ||
                !parse_int(argv[i + 4], opts.stress_block_h)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.stress_block_rect_set = true;
            i += 4;
            continue;
        }
        if (arg == "--stress-shift-hotspots") {
            if (i + 2 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            if (!parse_int(argv[i + 1], opts.stress_shift_dx) ||
                !parse_int(argv[i + 2], opts.stress_shift_dy)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.stress_shift_set = true;
            i += 2;
            continue;
        }
        if (arg == "--species-fracs") {
            if (i + 4 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            for (int s = 0; s < 4; ++s) {
                float v = 0.0f;
                if (!parse_float(argv[i + 1 + s], v)) {
                    std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                    return false;
                }
                opts.species_fracs[s] = v;
            }
            i += 4;
            continue;
        }
        if (arg == "--species-profile") {
            if (i + 6 >= argc) {
                std::cerr << "Fehlender Wert fuer " << arg << "\n";
                return false;
            }
            int s = 0;
            if (!parse_int(argv[i + 1], s) || s < 0 || s > 3) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            float e = 0.0f, fa = 0.0f, da = 0.0f, df = 0.0f, dd = 0.0f;
            if (!parse_float(argv[i + 2], e) ||
                !parse_float(argv[i + 3], fa) ||
                !parse_float(argv[i + 4], da) ||
                !parse_float(argv[i + 5], df) ||
                !parse_float(argv[i + 6], dd)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.species_profiles[s].exploration_mul = e;
            opts.species_profiles[s].food_attraction_mul = fa;
            opts.species_profiles[s].danger_aversion_mul = da;
            opts.species_profiles[s].deposit_food_mul = df;
            opts.species_profiles[s].deposit_danger_mul = dd;
            i += 6;
            continue;
        }
        if (i + 1 >= argc) {
            std::cerr << "Fehlender Wert fuer " << arg << "\n";
            return false;
        }
        const char *value = argv[++i];
        if (arg == "--mode") {
            if (!parse_string(value, opts.mode)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--input") {
            if (!parse_string(value, opts.db_input)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--output") {
            if (!parse_string(value, opts.db_output)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--db-dump") {
            if (!parse_string(value, opts.db_dump_path)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--db-dump-scale") {
            if (!parse_int(value, opts.db_dump_scale)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--ingest-rules") {
            if (!parse_string(value, opts.ingest_rules_path)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--db") {
            if (!parse_string(value, opts.db_path)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--query") {
            if (!parse_string(value, opts.db_query)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--db-radius") {
            if (!parse_int(value, opts.db_radius)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--db-merge-agents") {
            if (!parse_int(value, opts.db_merge_agents)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--db-merge-steps") {
            if (!parse_int(value, opts.db_merge_steps)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--db-merge-seed") {
            uint32_t seed_val = 0;
            if (!parse_seed(value, seed_val)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.db_merge_seed = seed_val;
        } else if (arg == "--db-merge-threshold") {
            if (!parse_int(value, opts.db_merge_threshold)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--sql-format") {
            if (!parse_string(value, opts.sql_output_format)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--width" || arg == "--wight") {
            if (!parse_int(value, opts.params.width)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.width_set = true;
        } else if (arg == "--height" || arg == "--hight") {
            if (!parse_int(value, opts.params.height)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.height_set = true;
        } else if (arg == "--size") {
            int size = 0;
            if (!parse_int(value, size)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.params.width = size;
            opts.params.height = size;
            opts.width_set = true;
            opts.height_set = true;
        } else if (arg == "--agents") {
            if (!parse_int(value, opts.params.agent_count)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--steps") {
            if (!parse_int(value, opts.params.steps)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--seed") {
            if (!parse_seed(value, opts.seed)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--info-cost") {
            if (!parse_float(value, opts.params.info_metabolism_cost)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--resources") {
            opts.resources_path = value;
        } else if (arg == "--pheromone") {
            opts.pheromone_path = value;
        } else if (arg == "--molecules") {
            opts.molecules_path = value;
        } else if (arg == "--resource-regen") {
            if (!parse_float(value, opts.params.resource_regen)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--mycel-growth") {
            if (!parse_float(value, opts.params.mycel_growth)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--mycel-decay") {
            if (!parse_float(value, opts.params.mycel_decay)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--mycel-transport") {
            if (!parse_float(value, opts.params.mycel_transport)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--mycel-threshold") {
            if (!parse_float(value, opts.params.mycel_drive_threshold)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--mycel-drive-p") {
            if (!parse_float(value, opts.params.mycel_drive_p)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--mycel-drive-r") {
            if (!parse_float(value, opts.params.mycel_drive_r)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--phero-food-deposit") {
            if (!parse_float(value, opts.params.phero_food_deposit_scale)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--phero-danger-deposit") {
            if (!parse_float(value, opts.params.phero_danger_deposit_scale)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--danger-delta-threshold") {
            if (!parse_float(value, opts.params.danger_delta_threshold)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--danger-bounce-deposit") {
            if (!parse_float(value, opts.params.danger_bounce_deposit)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--dump-every") {
            if (!parse_int(value, opts.dump_every)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--dump-dir") {
            if (!parse_string(value, opts.dump_dir)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--dump-prefix") {
            if (!parse_string(value, opts.dump_prefix)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--report-html") {
            if (!parse_string(value, opts.report_html_path)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--report-downsample") {
            if (!parse_int(value, opts.report_downsample)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--report-hist-bins") {
            if (!parse_int(value, opts.report_hist_bins)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--global-spawn-frac") {
            if (!parse_float(value, opts.global_spawn_frac)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--dna-global-capacity") {
            if (!parse_int(value, opts.params.dna_global_capacity)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--gpu") {
            int gpu = 0;
            if (!parse_int(value, gpu) || (gpu != 0 && gpu != 1)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.ocl_enable = (gpu == 1);
        } else if (arg == "--ocl-device") {
            if (!parse_int(value, opts.ocl_device)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--ocl-platform") {
            if (!parse_int(value, opts.ocl_platform)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--stress-at-step") {
            if (!parse_int(value, opts.stress_at_step)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--stress-pheromone-noise") {
            if (!parse_float(value, opts.stress_pheromone_noise)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--stress-seed") {
            if (!parse_seed(value, opts.stress_seed)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.stress_seed_set = true;
        } else if (arg == "--evo-elite-frac") {
            if (!parse_float(value, opts.evo_elite_frac)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--evo-min-energy-to-store") {
            if (!parse_float(value, opts.evo_min_energy_to_store)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--evo-mutation-sigma") {
            if (!parse_float(value, opts.evo_mutation_sigma)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--evo-exploration-delta") {
            if (!parse_float(value, opts.evo_exploration_delta)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--evo-fitness-window") {
            if (!parse_int(value, opts.evo_fitness_window)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--evo-age-decay") {
            if (!parse_float(value, opts.evo_age_decay)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--toxic-max-frac") {
            if (!parse_float(value, opts.params.toxic_max_fraction)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--toxic-stride-min") {
            if (!parse_int(value, opts.params.toxic_stride_min)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--toxic-stride-max") {
            if (!parse_int(value, opts.params.toxic_stride_max)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--toxic-iters-min") {
            if (!parse_int(value, opts.params.toxic_iters_min)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--toxic-iters-max") {
            if (!parse_int(value, opts.params.toxic_iters_max)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--logic-mode") {
            bool ok = false;
            int mode = parse_logic_mode(value, ok);
            if (!ok) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
            opts.params.logic_mode = mode;
        } else if (arg == "--logic-pulse-period") {
            if (!parse_int(value, opts.params.logic_pulse_period)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--logic-pulse-strength") {
            if (!parse_float(value, opts.params.logic_pulse_strength)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else if (arg == "--log-verbosity") {
            if (!parse_int(value, opts.log_verbosity)) {
                std::cerr << "Ungueltiger Wert fuer " << arg << "\n";
                return false;
            }
        } else {
            std::cerr << "Unbekanntes Argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char **argv) {
    CliOptions opts;
    opts.species_profiles = default_species_profiles();
    if (!parse_cli(argc, argv, opts)) {
        return 1;
    }
    auto normalize_format = [](std::string s) {
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    opts.sql_output_format = normalize_format(opts.sql_output_format);
    if (opts.sql_output_format != "table" && opts.sql_output_format != "csv" && opts.sql_output_format != "json") {
        std::cerr << "Ungueltiger Wert fuer --sql-format\n";
        return 1;
    }
    if (opts.ocl_print_devices) {
        std::string output;
        std::string err;
        if (!OpenCLRuntime::print_devices(output, err)) {
            std::cerr << "[OpenCL] " << err << "\n";
            return 1;
        }
        std::cout << output;
        return 0;
    }
    if (opts.mode != "sim" && opts.mode != "db_ingest" && opts.mode != "db_query" && opts.mode != "db_shell") {
        std::cerr << "Unbekannter Modus: " << opts.mode << "\n";
        return 1;
    }
    if (opts.mode == "db_ingest") {
        if (opts.db_input.empty() || opts.db_output.empty()) {
            std::cerr << "db_ingest benoetigt --input und --output\n";
            return 1;
        }
        DbWorld world;
        world.width = opts.params.width;
        world.height = opts.params.height;
        std::string error;
        if (!db_load_sql(opts.db_input, world, error)) {
            std::cerr << "SQL-Fehler: " << error << "\n";
            return 1;
        }
        DbIngestConfig cfg;
        cfg.agent_count = opts.params.agent_count;
        cfg.steps = opts.params.steps;
        cfg.seed = opts.seed;
        cfg.rules_path = opts.ingest_rules_path;
        if (!db_run_ingest(world, cfg, error)) {
            std::cerr << "Ingest-Fehler: " << error << "\n";
            return 1;
        }
        if (!db_save_myco(opts.db_output, world, error)) {
            std::cerr << "MYCO-Fehler: " << error << "\n";
            return 1;
        }
        if (!opts.db_dump_path.empty()) {
            if (!db_save_cluster_ppm(opts.db_dump_path, world, opts.db_dump_scale, error)) {
                std::cerr << "Dump-Fehler: " << error << "\n";
                return 1;
            }
        }
        std::cout << "ingest_done payloads=" << world.payloads.size() << " tables=" << world.table_names.size() << "\n";
        return 0;
    }
    auto escape_csv = [](const std::string &s) {
        bool need = s.find_first_of(",\"\n\r") != std::string::npos;
        if (!need) return s;
        std::string out = "\"";
        for (char c : s) {
            if (c == '"') out += "\"\"";
            else out.push_back(c);
        }
        out += "\"";
        return out;
    };
    auto escape_json = [](const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    };
    auto print_sql_result = [&](const DbSqlResult &result, const std::string &format) {
        if (format == "csv") {
            if (!result.columns.empty()) {
                for (size_t i = 0; i < result.columns.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << escape_csv(result.columns[i]);
                }
                std::cout << "\n";
            }
            for (const auto &row : result.rows) {
                for (size_t i = 0; i < row.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << escape_csv(row[i]);
                }
                std::cout << "\n";
            }
            return;
        }
        if (format == "json") {
            std::cout << "[\n";
            for (size_t r = 0; r < result.rows.size(); ++r) {
                std::cout << "  {";
                for (size_t c = 0; c < result.columns.size(); ++c) {
                    if (c > 0) std::cout << ", ";
                    std::string key = (c < result.columns.size()) ? result.columns[c] : ("col" + std::to_string(c));
                    std::string val = (c < result.rows[r].size()) ? result.rows[r][c] : "";
                    std::cout << "\"" << escape_json(key) << "\": \"" << escape_json(val) << "\"";
                }
                std::cout << "}";
                if (r + 1 < result.rows.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "]\n";
            return;
        }
        std::cout << "cols=" << result.columns.size() << " rows=" << result.rows.size() << "\n";
        if (!result.columns.empty()) {
            for (size_t i = 0; i < result.columns.size(); ++i) {
                if (i > 0) std::cout << " | ";
                std::cout << result.columns[i];
            }
            std::cout << "\n";
        }
        for (const auto &row : result.rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) std::cout << " | ";
                std::cout << row[i];
            }
            std::cout << "\n";
        }
    };
    if (opts.mode == "db_query") {
        if (opts.db_path.empty() || opts.db_query.empty()) {
            std::cerr << "db_query benoetigt --db und --query\n";
            return 1;
        }
        DbWorld world;
        std::string error;
        if (!db_load_myco(opts.db_path, world, error)) {
            std::cerr << "MYCO-Fehler: " << error << "\n";
            return 1;
        }
        std::string qtrim = opts.db_query;
        auto lower_copy = [](std::string s) {
            for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        std::string qlower = lower_copy(qtrim);
        if (qlower.rfind("select", 0) == 0 || qlower.rfind("with", 0) == 0 ||
            qlower.rfind("insert", 0) == 0 || qlower.rfind("update", 0) == 0 ||
            qlower.rfind("delete", 0) == 0) {
            DbSqlResult result;
            if (!db_execute_sql(world, opts.db_query, false, 0, 0, opts.db_radius, result, error)) {
                std::cerr << "SQL-Fehler: " << error << "\n";
                return 1;
            }
            print_sql_result(result, opts.sql_output_format);
            return 0;
        }
        DbQuery query;
        if (!db_parse_query(opts.db_query, query)) {
            std::cerr << "Query-Format ungueltig. Erwartet: SELECT ... FROM Table WHERE Col=Value\n";
            return 1;
        }
        std::vector<int> hits = db_execute_query(world, query, opts.db_radius);
        std::cout << "hits=" << hits.size() << "\n";
        for (int idx : hits) {
            if (idx < 0 || idx >= static_cast<int>(world.payloads.size())) continue;
            const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
            std::cout << "id=" << p.id << " table=" << world.table_names[static_cast<size_t>(p.table_id)]
                      << " x=" << p.x << " y=" << p.y << " data=\"" << p.raw_data << "\"\n";
        }
        return 0;
    }
    if (opts.mode == "db_shell") {
        if (opts.db_path.empty()) {
            std::cerr << "db_shell benoetigt --db\n";
            return 1;
        }
        DbWorld world;
        std::string error;
        if (!db_load_myco(opts.db_path, world, error)) {
            std::cerr << "MYCO-Fehler: " << error << "\n";
            return 1;
        }
        DbIngestConfig merge_cfg;
        merge_cfg.agent_count = opts.db_merge_agents;
        merge_cfg.steps = opts.db_merge_steps;
        merge_cfg.seed = opts.db_merge_seed;
        merge_cfg.rules_path = opts.ingest_rules_path;
        bool focus_set = false;
        int focus_x = 0;
        int focus_y = 0;
        int radius = opts.db_radius;

        std::string shell_format = opts.sql_output_format;
        DbSqlResult last_sql_result;
        DbSqlResult last_sql_original;
        bool last_sql_valid = false;
        int auto_merge_threshold = opts.db_merge_threshold;
        std::vector<std::string> history;
        std::unordered_map<std::string, std::string> macros;
        std::vector<std::string> global_show;
        bool global_show_enabled = false;
        struct LastQueryInfo {
            std::string text;
            bool is_sql = false;
            bool local = false;
            bool fallback_global = false;
            int hits = 0;
        };
        LastQueryInfo last_query;
        auto trim_ws = [](std::string s) {
            size_t start = 0;
            while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
            size_t end = s.size();
            while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
            return s.substr(start, end - start);
        };
        auto lower_copy = [](std::string s) {
            for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        auto apply_limit = [&](DbSqlResult &result) {
            if (world.default_limit < 0) return;
            if (static_cast<int>(result.rows.size()) <= world.default_limit) return;
            std::vector<std::vector<std::string>> limited;
            limited.reserve(static_cast<size_t>(world.default_limit));
            for (int i = 0; i < world.default_limit; ++i) {
                limited.push_back(result.rows[static_cast<size_t>(i)]);
            }
            result.rows.swap(limited);
        };
        auto sql_has_limit_offset = [&](const std::string &sql) {
            const std::string lower = lower_copy(sql);
            if (lower.rfind("limit ", 0) == 0 || lower.rfind("offset ", 0) == 0) return true;
            if (lower.find(" limit ") != std::string::npos || lower.find(" offset ") != std::string::npos) return true;
            return false;
        };
        auto sql_selects_all_no_limit = [&](const std::string &sql) {
            const std::string lower = lower_copy(trim_ws(sql));
            if (sql_has_limit_offset(lower)) return false;
            size_t pos = lower.find("select");
            if (pos == std::string::npos) return false;
            pos += 6;
            while (pos < lower.size() && std::isspace(static_cast<unsigned char>(lower[pos]))) ++pos;
            return pos < lower.size() && lower[pos] == '*';
        };
        auto json_escape = [](const std::string &value) {
            std::string out;
            out.reserve(value.size() + 8);
            for (char c : value) {
                switch (c) {
                    case '\\': out += "\\\\"; break;
                    case '"': out += "\\\""; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out.push_back(c); break;
                }
            }
            return out;
        };
        auto json_read_string = [](const std::string &s, size_t &i, std::string &out) {
            if (i >= s.size() || s[i] != '"') return false;
            ++i;
            std::string result;
            while (i < s.size()) {
                char c = s[i++];
                if (c == '"') {
                    out = result;
                    return true;
                }
                if (c == '\\' && i < s.size()) {
                    char esc = s[i++];
                    switch (esc) {
                        case '\\': result.push_back('\\'); break;
                        case '"': result.push_back('"'); break;
                        case 'n': result.push_back('\n'); break;
                        case 'r': result.push_back('\r'); break;
                        case 't': result.push_back('\t'); break;
                        default: result.push_back(esc); break;
                    }
                } else {
                    result.push_back(c);
                }
            }
            return false;
        };
        auto save_macros = [&](const std::string &path) {
            std::ofstream out(path, std::ios::binary);
            if (!out) {
                std::cout << "Konnte Datei nicht schreiben: " << path << "\n";
                return;
            }
            out << "[\n";
            bool first = true;
            for (const auto &entry : macros) {
                if (!first) out << ",\n";
                first = false;
                out << "  {\"name\":\"" << json_escape(entry.first) << "\",\"command\":\""
                    << json_escape(entry.second) << "\"}";
            }
            out << "\n]\n";
            std::cout << "Makros gespeichert: " << path << "\n";
        };
        auto load_macros = [&](const std::string &path) {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                std::cout << "Konnte Datei nicht lesen: " << path << "\n";
                return;
            }
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            size_t i = 0;
            std::string name;
            std::string command;
            int loaded = 0;
            while (i < content.size()) {
                if (content[i] == '"') {
                    std::string key;
                    size_t key_pos = i;
                    if (!json_read_string(content, i, key)) {
                        i = key_pos + 1;
                        continue;
                    }
                    while (i < content.size() && std::isspace(static_cast<unsigned char>(content[i]))) ++i;
                    if (i < content.size() && content[i] == ':') ++i;
                    while (i < content.size() && std::isspace(static_cast<unsigned char>(content[i]))) ++i;
                    if (key == "name" || key == "command") {
                        std::string value;
                        if (json_read_string(content, i, value)) {
                            if (key == "name") name = value;
                            if (key == "command") command = value;
                            if (!name.empty() && !command.empty()) {
                                macros[name] = command;
                                name.clear();
                                command.clear();
                                ++loaded;
                            }
                        }
                    }
                } else {
                    ++i;
                }
            }
            std::cout << "Makros geladen: " << loaded << "\n";
        };
        auto print_duration = [&](long long ms) {
            if (ms < 1000) {
                std::cout << "Ausfuehrungszeit: " << ms << " ms\n";
            } else {
                double sec = static_cast<double>(ms) / 1000.0;
                std::cout << "Ausfuehrungszeit: " << std::fixed << std::setprecision(3) << sec << " s\n";
                std::cout.unsetf(std::ios::floatfield);
                std::cout << std::setprecision(6);
            }
        };
        auto serialize_sql_result = [&](const DbSqlResult &result, const std::string &format) -> std::string {
            std::ostringstream out;
            auto escape_csv = [&](const std::string &s) {
                std::string v = s;
                bool need_quotes = v.find(',') != std::string::npos || v.find('"') != std::string::npos ||
                                   v.find('\n') != std::string::npos || v.find('\r') != std::string::npos;
                if (v.find('"') != std::string::npos) {
                    std::string escaped;
                    for (char c : v) {
                        if (c == '"') escaped += "\"\"";
                        else escaped.push_back(c);
                    }
                    v = escaped;
                    need_quotes = true;
                }
                if (need_quotes) {
                    return "\"" + v + "\"";
                }
                return v;
            };
            auto escape_json = [&](const std::string &s) {
                std::string v;
                v.reserve(s.size());
                for (char c : s) {
                    switch (c) {
                        case '\\': v += "\\\\"; break;
                        case '"': v += "\\\""; break;
                        case '\n': v += "\\n"; break;
                        case '\r': v += "\\r"; break;
                        case '\t': v += "\\t"; break;
                        default: v.push_back(c); break;
                    }
                }
                return v;
            };
            if (format == "csv") {
                if (!result.columns.empty()) {
                    for (size_t i = 0; i < result.columns.size(); ++i) {
                        if (i > 0) out << ",";
                        out << escape_csv(result.columns[i]);
                    }
                    out << "\n";
                }
                for (const auto &row : result.rows) {
                    for (size_t i = 0; i < row.size(); ++i) {
                        if (i > 0) out << ",";
                        out << escape_csv(row[i]);
                    }
                    out << "\n";
                }
                return out.str();
            }
            if (format == "json") {
                out << "[\n";
                for (size_t r = 0; r < result.rows.size(); ++r) {
                    out << "  {";
                    for (size_t c = 0; c < result.columns.size(); ++c) {
                        if (c > 0) out << ", ";
                        std::string key = (c < result.columns.size()) ? result.columns[c] : ("col" + std::to_string(c));
                        std::string val = (c < result.rows[r].size()) ? result.rows[r][c] : "";
                        out << "\"" << escape_json(key) << "\": \"" << escape_json(val) << "\"";
                    }
                    out << "}";
                    if (r + 1 < result.rows.size()) out << ",";
                    out << "\n";
                }
                out << "]\n";
                return out.str();
            }
            return "";
        };
        std::cout << "myco shell bereit. 'help' fuer Befehle, 'exit' zum Beenden.\n";
        for (;;) {
            std::cout << "myco> ";
            std::string line;
            if (!std::getline(std::cin, line)) {
                break;
            }
            line = trim_ws(line);
            if (line.empty()) continue;
            if (line == "history") {
                for (size_t i = 0; i < history.size(); ++i) {
                    std::cout << (i + 1) << ": " << history[i] << "\n";
                }
                continue;
            }
            if (line == "cls" || line == "clear") {
#ifdef _WIN32
                std::system("cls");
#else
                std::system("clear");
#endif
                continue;
            }
            if (line == "last" || line == "redo") {
                if (history.empty()) {
                    std::cout << "Keine Historie.\n";
                    continue;
                }
                line = history.back();
                std::cout << line << "\n";
            } else if (!line.empty() && line[0] == '!') {
                std::string num = line.substr(1);
                int idx = 0;
                try { idx = std::stoi(num); } catch (...) { idx = 0; }
                if (idx <= 0 || static_cast<size_t>(idx) > history.size()) {
                    std::cout << "Ungueltige History-ID.\n";
                    continue;
                }
                line = history[static_cast<size_t>(idx - 1)];
                std::cout << line << "\n";
            }
            if (line.rfind("save ", 0) == 0) {
                std::string rest = trim_ws(line.substr(5));
                if (rest.empty()) {
                    std::cout << "save <name> [command]\n";
                    continue;
                }
                std::string name;
                std::string cmd;
                size_t sp = rest.find(' ');
                if (sp == std::string::npos) {
                    name = rest;
                    if (history.empty()) {
                        std::cout << "Keine Historie.\n";
                        continue;
                    }
                    cmd = history.back();
                } else {
                    name = trim_ws(rest.substr(0, sp));
                    cmd = trim_ws(rest.substr(sp + 1));
                }
                macros[name] = cmd;
                std::cout << "saved " << name << "\n";
                continue;
            }
            if (line.rfind("run ", 0) == 0) {
                std::string name = trim_ws(line.substr(4));
                auto it = macros.find(name);
                if (it == macros.end()) {
                    std::cout << "Makro nicht gefunden.\n";
                    continue;
                }
                line = it->second;
                std::cout << line << "\n";
            }
            if (line.rfind("macros save", 0) == 0) {
                std::string rest = trim_ws(line.substr(11));
                std::string path = rest;
                if (path.empty()) {
                    auto now = std::chrono::system_clock::now();
                    std::time_t tt = std::chrono::system_clock::to_time_t(now);
                    std::tm tm{};
#ifdef _WIN32
                    localtime_s(&tm, &tt);
#else
                    localtime_r(&tt, &tm);
#endif
                    std::ostringstream name;
                    name << std::put_time(&tm, "%Y-%m-%d") << "_macros.json";
                    path = (std::filesystem::current_path() / name.str()).string();
                }
                save_macros(path);
                continue;
            }
            if (line.rfind("macros load ", 0) == 0) {
                std::string path = trim_ws(line.substr(12));
                if (path.empty()) {
                    std::cout << "macros load <path>\n";
                    continue;
                }
                load_macros(path);
                continue;
            }
            history.push_back(line);
            if (line == "exit" || line == "quit") break;
            if (line == "help") {
                std::cout << "Formate:\n";
                std::cout << "  Album 1                -> Primary-Key Query\n";
                std::cout << "  Track AlbumId=1         -> Foreign-Key Query\n";
                std::cout << "  goto <payload_id>       -> Fokus setzen\n";
                std::cout << "  radius <n>              -> Suchradius setzen\n";
                std::cout << "  focus                   -> Aktuellen Fokus anzeigen\n";
                std::cout << "  limit <n|off>           -> Default-Limit fuer Shell/SQL\n";
                std::cout << "  show <cols|off>          -> Globale Show-Filter\n";
                std::cout << "  describe <table>         -> Schema + Beispiel\n";
                std::cout << "  tables                  -> Tabellenliste\n";
                std::cout << "  stats                   -> Payload-Counts pro Tabelle\n";
                std::cout << "  delta                   -> Delta-Status\n";
                std::cout << "  merge                   -> Delta in Cluster mergen\n";
                std::cout << "  merge auto <n>           -> Auto-Merge ab Delta-Size N\n";
                std::cout << "  delta show              -> Delta-Details\n";
                std::cout << "  undo                    -> Letztes Delta rueckgaengig\n";
                std::cout << "  schema <table>           -> Spaltenliste\n";
                std::cout << "  ingest <sql> [rules]     -> SQL-Dump ingestieren (ersetzen)\n";
                std::cout << "  history                 -> Historie anzeigen\n";
                std::cout << "  last | redo | !n         -> Query aus Historie\n";
                std::cout << "  save <name> [cmd]        -> Makro speichern\n";
                std::cout << "  run <name>               -> Makro ausfuehren\n";
                std::cout << "  macros save [path]        -> Makros als JSON speichern\n";
                std::cout << "  macros load <path>        -> Makros aus JSON laden\n";
                std::cout << "  cls | clear              -> Shell leeren\n";
                std::cout << "  <Table> ... show Cols    -> Ausgabe auf Spalten filtern\n";
                std::cout << "  Col=Value                -> Globale Spaltenabfrage\n";
                std::cout << "  sql <statement>          -> SQL (SELECT/INSERT/UPDATE/DELETE)\n";
                std::cout << "  sort <col|index> [asc|desc] [num][, <col|index> [asc|desc] [num] ...]\n";
                std::cout << "                           -> Letztes SQL-Result sortieren\n";
                std::cout << "  sort reset               -> Letztes SQL-Result zuruecksetzen\n";
                std::cout << "  export <csv|json> <path> -> Letztes Result exportieren\n";
                std::cout << "  explain                 -> Letzte Query erklaeren\n";
                std::cout << "  format <table|csv|json>  -> SQL-Output-Format\n";
                std::cout << "  exit                    -> Beenden\n";
                continue;
            }
            if (line == "focus") {
                if (focus_set) {
                    std::cout << "focus=" << focus_x << "," << focus_y << " radius=" << radius << "\n";
                } else {
                    std::cout << "focus=none radius=" << radius << "\n";
                }
                continue;
            }
            if (line == "limit") {
                if (world.default_limit < 0) {
                    std::cout << "limit=off\n";
                } else {
                    std::cout << "limit=" << world.default_limit << "\n";
                }
                continue;
            }
            if (line.rfind("limit ", 0) == 0) {
                std::string arg = trim_ws(line.substr(6));
                if (arg == "off") {
                    world.default_limit = -1;
                } else {
                    try {
                        world.default_limit = std::stoi(arg);
                    } catch (...) {
                        std::cout << "Ungueltiger Limit-Wert.\n";
                        continue;
                    }
                }
                std::cout << "limit=" << (world.default_limit < 0 ? std::string("off") : std::to_string(world.default_limit)) << "\n";
                continue;
            }
            if (line == "show") {
                if (!global_show_enabled || global_show.empty()) {
                    std::cout << "show=off\n";
                } else {
                    std::cout << "show=";
                    for (size_t i = 0; i < global_show.size(); ++i) {
                        if (i > 0) std::cout << ",";
                        std::cout << global_show[i];
                    }
                    std::cout << "\n";
                }
                continue;
            }
            if (line.rfind("show ", 0) == 0) {
                std::string cols = trim_ws(line.substr(5));
                if (cols == "off") {
                    global_show_enabled = false;
                    global_show.clear();
                    std::cout << "show=off\n";
                    continue;
                }
                global_show.clear();
                std::stringstream ss(cols);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    std::string v = trim_ws(item);
                    if (!v.empty() && v != "*") {
                        global_show.push_back(v);
                    }
                }
                global_show_enabled = !global_show.empty();
                std::cout << "show=" << (global_show_enabled ? "on" : "off") << "\n";
                continue;
            }
            if (line.rfind("describe ", 0) == 0) {
                std::string tname = trim_ws(line.substr(9));
                int table_id = db_find_table(world, tname);
                if (table_id < 0) {
                    std::cout << "Tabelle nicht gefunden.\n";
                    continue;
                }
                std::vector<std::string> cols;
                if (table_id >= 0 && table_id < static_cast<int>(world.table_columns.size())) {
                    cols = world.table_columns[static_cast<size_t>(table_id)];
                }
                if (cols.empty()) {
                    for (const auto &p : world.payloads) {
                        if (p.table_id == table_id) {
                            int64_t key = db_payload_key(p.table_id, p.id);
                            if (world.tombstones.find(key) != world.tombstones.end()) continue;
                            if (!p.is_delta && world.delta_index_by_key.find(key) != world.delta_index_by_key.end()) continue;
                            for (const auto &f : p.fields) {
                                cols.push_back(f.name);
                            }
                            break;
                        }
                    }
                }
                std::cout << "schema " << world.table_names[static_cast<size_t>(table_id)] << ":\n";
                for (const auto &c : cols) {
                    std::cout << "- " << c;
                    std::string col_lower = c;
                    for (char &ch : col_lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    std::string tbl_lower = world.table_names[static_cast<size_t>(table_id)];
                    for (char &ch : tbl_lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                    if (col_lower == "id" || col_lower == tbl_lower + "id" || col_lower == tbl_lower + "_id") {
                        std::cout << " [pk]";
                    } else if (col_lower.size() >= 2 && (col_lower.rfind("id") == col_lower.size() - 2 ||
                                                         (col_lower.size() >= 3 && col_lower.rfind("_id") == col_lower.size() - 3))) {
                        std::string fk_table = col_lower;
                        if (fk_table.rfind("_id") == fk_table.size() - 3) {
                            fk_table = fk_table.substr(0, fk_table.size() - 3);
                        } else if (fk_table.rfind("id") == fk_table.size() - 2) {
                            fk_table = fk_table.substr(0, fk_table.size() - 2);
                        }
                        if (!fk_table.empty()) {
                            std::cout << " [fk->" << fk_table << "]";
                        }
                    }
                    std::cout << "\n";
                }
                bool printed = false;
                for (const auto &p : world.payloads) {
                    if (p.table_id != table_id) continue;
                    int64_t key = db_payload_key(p.table_id, p.id);
                    if (world.tombstones.find(key) != world.tombstones.end()) continue;
                    if (!p.is_delta && world.delta_index_by_key.find(key) != world.delta_index_by_key.end()) continue;
                    if (p.fields.empty()) continue;
                    std::cout << "example: " << p.raw_data << "\n";
                    printed = true;
                    break;
                }
                if (!printed) {
                    std::cout << "example: (keine Daten)\n";
                }
                continue;
            }
            if (line.rfind("goto ", 0) == 0) {
                std::string id_str = trim_ws(line.substr(5));
                int payload_id = 0;
                try {
                    payload_id = std::stoi(id_str);
                } catch (...) {
                    std::cout << "Ungueltige ID.\n";
                    continue;
                }
                bool found = false;
                for (const auto &p : world.payloads) {
                    int64_t key = db_payload_key(p.table_id, p.id);
                    if (world.tombstones.find(key) != world.tombstones.end()) continue;
                    if (!p.is_delta && world.delta_index_by_key.find(key) != world.delta_index_by_key.end()) continue;
                    if (p.id == payload_id && p.placed) {
                        focus_x = p.x;
                        focus_y = p.y;
                        focus_set = true;
                        std::cout << "goto id=" << p.id << " table=" << world.table_names[static_cast<size_t>(p.table_id)]
                                  << " x=" << p.x << " y=" << p.y << "\n";
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::cout << "ID nicht gefunden oder nicht platziert.\n";
                }
                continue;
            }
            if (line.rfind("radius ", 0) == 0) {
                std::string r_str = trim_ws(line.substr(7));
                int r = 0;
                try {
                    r = std::stoi(r_str);
                } catch (...) {
                    std::cout << "Ungueltiger Radius.\n";
                    continue;
                }
                if (r <= 0) {
                    std::cout << "Radius muss > 0 sein.\n";
                    continue;
                }
                radius = r;
                std::cout << "radius=" << radius << "\n";
                continue;
            }
            if (line.rfind("sort", 0) == 0) {
                std::string args = trim_ws(line.substr(4));
                if (!last_sql_valid) {
                    std::cout << "Kein SQL-Result vorhanden.\n";
                    continue;
                }
                if (args == "reset") {
                    last_sql_result = last_sql_original;
                    print_sql_result(last_sql_result, shell_format);
                    continue;
                }
                if (args.empty()) {
                    std::cout << "Sort benoetigt eine Spalte oder einen Index.\n";
                    continue;
                }
                std::vector<std::string> tokens;
                {
                    std::stringstream ss(args);
                    std::string tok;
                    while (ss >> tok) {
                        tokens.push_back(tok);
                    }
                }
                if (tokens.empty()) {
                    std::cout << "Sort benoetigt eine Spalte oder einen Index.\n";
                    continue;
                }
                struct SortKey {
                    int col_index = -1;
                    bool asc = true;
                    bool numeric = false;
                };
                auto lower_copy = [](std::string s) {
                    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    return s;
                };
                std::vector<SortKey> keys;
                bool bad = false;
                std::stringstream ss(args);
                std::string segment;
                while (std::getline(ss, segment, ',')) {
                    std::string seg = trim_ws(segment);
                    if (seg.empty()) continue;
                    std::vector<std::string> parts;
                    {
                        std::stringstream ps(seg);
                        std::string tok;
                        while (ps >> tok) {
                            parts.push_back(tok);
                        }
                    }
                    if (parts.empty()) continue;
                    SortKey key;
                    std::string col_key = parts[0];
                    for (size_t i = 1; i < parts.size(); ++i) {
                        std::string opt = lower_copy(parts[i]);
                        if (opt == "asc") {
                            key.asc = true;
                        } else if (opt == "desc") {
                            key.asc = false;
                        } else if (opt == "num" || opt == "numeric") {
                            key.numeric = true;
                        } else {
                            std::cout << "Ungueltige Sort-Option: " << parts[i] << "\n";
                            bad = true;
                            break;
                        }
                    }
                    if (bad) break;
                    int col_index = -1;
                    bool is_index = !col_key.empty() && std::all_of(col_key.begin(), col_key.end(), [](unsigned char c) {
                        return c >= '0' && c <= '9';
                    });
                    if (is_index) {
                        try {
                            int idx = std::stoi(col_key);
                            col_index = idx - 1;
                        } catch (...) {
                            col_index = -1;
                        }
                    } else {
                        std::string want = lower_copy(col_key);
                        for (size_t i = 0; i < last_sql_result.columns.size(); ++i) {
                            if (lower_copy(last_sql_result.columns[i]) == want) {
                                col_index = static_cast<int>(i);
                                break;
                            }
                        }
                    }
                    if (col_index < 0 || col_index >= static_cast<int>(last_sql_result.columns.size())) {
                        std::cout << "Spalte nicht gefunden.\n";
                        bad = true;
                        break;
                    }
                    key.col_index = col_index;
                    keys.push_back(key);
                }
                if (bad || keys.empty()) {
                    continue;
                }
                auto parse_number = [](const std::string &s, double &out) -> bool {
                    try {
                        size_t idx = 0;
                        out = std::stod(s, &idx);
                        return idx > 0;
                    } catch (...) {
                        return false;
                    }
                };
                std::vector<size_t> order_idx(last_sql_result.rows.size());
                for (size_t i = 0; i < order_idx.size(); ++i) order_idx[i] = i;
                std::stable_sort(order_idx.begin(), order_idx.end(), [&](size_t ia, size_t ib) {
                    const auto &ra = last_sql_result.rows[ia];
                    const auto &rb = last_sql_result.rows[ib];
                    for (const auto &key : keys) {
                        int col_index = key.col_index;
                        std::string va = (static_cast<size_t>(col_index) < ra.size()) ? ra[static_cast<size_t>(col_index)] : "";
                        std::string vb = (static_cast<size_t>(col_index) < rb.size()) ? rb[static_cast<size_t>(col_index)] : "";
                        if (va == vb) continue;
                        double na = 0.0;
                        double nb = 0.0;
                        bool a_num = parse_number(va, na);
                        bool b_num = parse_number(vb, nb);
                        bool use_num = key.numeric || (a_num && b_num);
                        if (use_num && a_num && b_num) {
                            if (na == nb) continue;
                            return key.asc ? (na < nb) : (na > nb);
                        }
                        return key.asc ? (va < vb) : (va > vb);
                    }
                    return false;
                });
                std::vector<std::vector<std::string>> sorted_rows;
                sorted_rows.reserve(last_sql_result.rows.size());
                for (size_t idx : order_idx) {
                    sorted_rows.push_back(last_sql_result.rows[idx]);
                }
                last_sql_result.rows.swap(sorted_rows);
                print_sql_result(last_sql_result, shell_format);
                continue;
            }
            if (line == "tables") {
                for (size_t t = 0; t < world.table_names.size(); ++t) {
                    std::cout << t << ": " << world.table_names[t] << "\n";
                }
                continue;
            }
            if (line == "stats") {
                std::vector<int> counts(world.table_names.size(), 0);
                for (const auto &p : world.payloads) {
                    if (p.table_id >= 0 && p.table_id < static_cast<int>(counts.size())) {
                        int64_t key = db_payload_key(p.table_id, p.id);
                        if (world.tombstones.find(key) != world.tombstones.end()) continue;
                        if (!p.is_delta && world.delta_index_by_key.find(key) != world.delta_index_by_key.end()) continue;
                        counts[static_cast<size_t>(p.table_id)]++;
                    }
                }
                for (size_t t = 0; t < world.table_names.size(); ++t) {
                    std::cout << t << ": " << world.table_names[t] << " -> " << counts[t] << "\n";
                }
                continue;
            }
            if (line == "delta") {
                std::cout << "delta=" << db_delta_count(world)
                          << " tombstones=" << world.tombstones.size() << "\n";
                continue;
            }
            if (line == "delta show") {
                std::cout << "delta=" << db_delta_count(world)
                          << " tombstones=" << world.tombstones.size() << "\n";
                for (const auto &pair : world.delta_index_by_key) {
                    int idx = pair.second;
                    if (idx < 0 || idx >= static_cast<int>(world.payloads.size())) continue;
                    const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
                    if (p.table_id < 0 || p.table_id >= static_cast<int>(world.table_names.size())) continue;
                    std::cout << "UPSERT table=" << world.table_names[static_cast<size_t>(p.table_id)]
                              << " id=" << p.id << " data=\"" << p.raw_data << "\"\n";
                }
                for (const auto &key : world.tombstones) {
                    int table_id = static_cast<int>(key >> 32);
                    int id = static_cast<int>(key & 0xffffffff);
                    std::string tname = (table_id >= 0 && table_id < static_cast<int>(world.table_names.size()))
                                            ? world.table_names[static_cast<size_t>(table_id)]
                                            : "unknown";
                    std::cout << "DELETE table=" << tname << " id=" << id << "\n";
                }
                continue;
            }
            if (line == "merge") {
                std::string merge_error;
                if (!db_merge_delta(world, merge_cfg, merge_error)) {
                    std::cout << "merge_error: " << merge_error << "\n";
                } else {
                    std::cout << "merge_ok\n";
                }
                continue;
            }
            if (line.rfind("merge auto ", 0) == 0) {
                std::string v = trim_ws(line.substr(11));
                int n = 0;
                try { n = std::stoi(v); } catch (...) { n = -1; }
                if (n < 0) {
                    std::cout << "Ungueltiger Wert.\n";
                    continue;
                }
                auto_merge_threshold = n;
                std::cout << "merge_auto=" << auto_merge_threshold << "\n";
                continue;
            }
            if (line == "undo") {
                std::string undo_error;
                if (!db_undo_last_delta(world, undo_error)) {
                    std::cout << "undo_error: " << undo_error << "\n";
                } else {
                    std::cout << "undo_ok\n";
                }
                continue;
            }
            if (line == "explain") {
                if (last_query.text.empty()) {
                    std::cout << "Kein Query vorhanden.\n";
                    continue;
                }
                std::cout << "query=" << last_query.text << "\n";
                std::cout << "scope=" << (last_query.local ? "local" : "global") << "\n";
                std::cout << "hits=" << last_query.hits << "\n";
                std::cout << "radius=" << radius << "\n";
                if (last_query.fallback_global) {
                    std::cout << "fallback_global=1\n";
                }
                continue;
            }
            if (line.rfind("export ", 0) == 0) {
                std::string rest = trim_ws(line.substr(7));
                size_t sp = rest.find(' ');
                if (sp == std::string::npos) {
                    std::cout << "export <csv|json> <path>\n";
                    continue;
                }
                std::string fmt = trim_ws(rest.substr(0, sp));
                std::string path = trim_ws(rest.substr(sp + 1));
                if (fmt != "csv" && fmt != "json") {
                    std::cout << "Ungueltiges Format.\n";
                    continue;
                }
                if (!last_sql_valid) {
                    std::cout << "Kein Result vorhanden.\n";
                    continue;
                }
                std::string data = serialize_sql_result(last_sql_result, fmt);
                std::ofstream out(path, std::ios::binary);
                if (!out.is_open()) {
                    std::cout << "Export fehlgeschlagen.\n";
                    continue;
                }
                out << data;
                std::cout << "export_ok\n";
                continue;
            }
            if (line.rfind("ingest ", 0) == 0) {
                std::string rest = trim_ws(line.substr(7));
                if (rest.empty()) {
                    std::cout << "ingest <sql_path> [rules_path]\n";
                    continue;
                }
                std::vector<std::string> parts;
                std::stringstream ss(rest);
                std::string part;
                while (ss >> part) {
                    parts.push_back(part);
                }
                if (parts.empty()) {
                    std::cout << "ingest <sql_path> [rules_path]\n";
                    continue;
                }
                const std::string &sql_path = parts[0];
                std::string rules_path;
                if (parts.size() >= 2) {
                    rules_path = parts[1];
                } else {
                    rules_path = merge_cfg.rules_path;
                }
                DbWorld new_world;
                new_world.width = world.width > 0 ? world.width : 2048;
                new_world.height = world.height > 0 ? world.height : 2048;
                std::string ingest_error;
                if (!db_load_sql(sql_path, new_world, ingest_error)) {
                    std::cout << "Ingest-Fehler: " << ingest_error << "\n";
                    continue;
                }
                DbIngestConfig ingest_cfg = merge_cfg;
                ingest_cfg.rules_path = rules_path;
                if (!db_run_ingest(new_world, ingest_cfg, ingest_error)) {
                    std::cout << "Ingest-Fehler: " << ingest_error << "\n";
                    continue;
                }
                world = std::move(new_world);
                focus_set = false;
                focus_x = 0;
                focus_y = 0;
                last_sql_result = DbSqlResult{};
                last_sql_original = DbSqlResult{};
                last_sql_valid = false;
                last_query = LastQueryInfo{};
                std::cout << "ingest_ok payloads=" << world.payloads.size()
                          << " tables=" << world.table_names.size() << "\n";
                continue;
            }
            if (line.rfind("schema ", 0) == 0) {
                std::string tname = trim_ws(line.substr(7));
                int table_id = db_find_table(world, tname);
                if (table_id < 0) {
                    std::cout << "Tabelle nicht gefunden.\n";
                    continue;
                }
                std::vector<std::string> cols;
                if (table_id >= 0 && table_id < static_cast<int>(world.table_columns.size())) {
                    cols = world.table_columns[static_cast<size_t>(table_id)];
                }
                if (cols.empty()) {
                    for (const auto &p : world.payloads) {
                        if (p.table_id == table_id) {
                            int64_t key = db_payload_key(p.table_id, p.id);
                            if (world.tombstones.find(key) != world.tombstones.end()) continue;
                            if (!p.is_delta && world.delta_index_by_key.find(key) != world.delta_index_by_key.end()) continue;
                            for (const auto &f : p.fields) {
                                cols.push_back(f.name);
                            }
                            break;
                        }
                    }
                }
                if (cols.empty()) {
                    std::cout << "Keine Spalten bekannt.\n";
                    continue;
                }
                std::cout << "schema " << world.table_names[static_cast<size_t>(table_id)] << ":\n";
                for (const auto &c : cols) {
                    std::cout << "- " << c << "\n";
                }
                continue;
            }
            if (line.rfind("sql ", 0) == 0) {
                std::string sql = trim_ws(line.substr(4));
                std::vector<std::string> statements;
                {
                    std::string cur;
                    bool in_string = false;
                    char quote = 0;
                    for (size_t i = 0; i < sql.size(); ++i) {
                        char c = sql[i];
                        if ((c == '\'' || c == '"') && (!in_string || c == quote)) {
                            if (in_string && c == quote) {
                                in_string = false;
                            } else if (!in_string) {
                                in_string = true;
                                quote = c;
                            }
                        }
                        if (!in_string && c == ';') {
                            std::string stmt = trim_ws(cur);
                            if (!stmt.empty()) statements.push_back(stmt);
                            cur.clear();
                            continue;
                        }
                        cur.push_back(c);
                    }
                    std::string stmt = trim_ws(cur);
                    if (!stmt.empty()) statements.push_back(stmt);
                }
                for (size_t si = 0; si < statements.size(); ++si) {
                    DbSqlResult result;
                    std::string sql_error;
                    long long duration_ms = 0;
                    if (world.default_limit < 0 && sql_selects_all_no_limit(statements[si])) {
                        std::cout << "WARNUNG: SELECT * ohne LIMIT/OFFSET kann bei grossen Tabellen sehr langsam sein "
                                  << "oder das System instabil machen.\n"
                                  << "Empfehlung: nutze LIMIT/OFFSET oder Paging.\n"
                                  << "Trotzdem ausfuehren? (y/N) ";
                        std::string answer;
                        if (!std::getline(std::cin, answer)) {
                            break;
                        }
                        answer = lower_copy(trim_ws(answer));
                        if (answer != "y" && answer != "yes") {
                            std::cout << "abgebrochen.\n";
                            continue;
                        }
                    }
                    const auto start = std::chrono::steady_clock::now();
                    if (!db_execute_sql(world, statements[si], focus_set, focus_x, focus_y, radius, result, sql_error)) {
                        std::cout << "SQL-Fehler: " << sql_error << "\n";
                        break;
                    }
                    apply_limit(result);
                    const auto end = std::chrono::steady_clock::now();
                    duration_ms =
                      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    last_sql_result = result;
                    last_sql_original = result;
                    last_sql_valid = true;
                    print_sql_result(result, shell_format);
                    last_query.text = "sql " + statements[si];
                    last_query.is_sql = true;
                    last_query.local = focus_set;
                    last_query.fallback_global = false;
                    last_query.hits = static_cast<int>(result.rows.size());
                    if (auto_merge_threshold > 0) {
                        std::string lower_sql = statements[si];
                        for (char &c : lower_sql) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        if (lower_sql.rfind("insert", 0) == 0 || lower_sql.rfind("update", 0) == 0 ||
                            lower_sql.rfind("delete", 0) == 0) {
                            if (db_delta_count(world) >= static_cast<size_t>(auto_merge_threshold)) {
                                std::string merge_error;
                                if (!db_merge_delta(world, merge_cfg, merge_error)) {
                                    std::cout << "merge_error: " << merge_error << "\n";
                                } else {
                                    std::cout << "merge_ok\n";
                                }
                            }
                        }
                    }
                    print_duration(duration_ms);
                }
                continue;
            }
            if (line.rfind("format ", 0) == 0) {
                std::string fmt = trim_ws(line.substr(7));
                fmt = normalize_format(fmt);
                if (fmt == "table" || fmt == "csv" || fmt == "json") {
                    shell_format = fmt;
                    std::cout << "format=" << shell_format << "\n";
                } else {
                    std::cout << "Ungueltiges Format.\n";
                }
                continue;
            }

            auto is_digits = [](const std::string &s) {
                if (s.empty()) return false;
                for (char c : s) {
                    if (c < '0' || c > '9') return false;
                }
                return true;
            };

            std::string lower_line = line;
            for (char &c : lower_line) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            std::string show_cols;
            size_t show_pos = lower_line.find(" show ");
            if (show_pos != std::string::npos) {
                show_cols = trim_ws(line.substr(show_pos + 6));
                line = trim_ws(line.substr(0, show_pos));
            }

            std::string table;
            std::string cond;
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos) {
                table = trim_ws(line.substr(0, space_pos));
                cond = trim_ws(line.substr(space_pos + 1));
            }
            std::vector<int> hits;
            bool used_focus = false;
            bool fallback_global = false;
            long long duration_ms = -1;

            auto run_query = [&](const DbQuery &q) {
                if (focus_set) {
                    hits = db_execute_query_focus(world, q, focus_x, focus_y, radius);
                    used_focus = true;
                } else {
                    hits = db_execute_query(world, q, radius);
                }
                if (used_focus && hits.empty()) {
                    hits = db_execute_query(world, q, radius);
                    fallback_global = true;
                }
            };

            if (!table.empty()) {
                if (cond.empty()) {
                    std::cout << "Ungueltige Query.\n";
                    continue;
                }
                const auto start = std::chrono::steady_clock::now();
                DbQuery query;
                size_t eq = cond.find('=');
                if (eq == std::string::npos) {
                    if (!is_digits(cond)) {
                        std::cout << "Ungueltige ID.\n";
                        continue;
                    }
                    query.table = table;
                    query.column = table + "Id";
                    query.value = cond;
                } else {
                    query.table = table;
                    query.column = trim_ws(cond.substr(0, eq));
                    query.value = trim_ws(cond.substr(eq + 1));
                }
                run_query(query);
                const auto end = std::chrono::steady_clock::now();
                duration_ms =
                  std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                if (world.default_limit >= 0 && hits.size() > static_cast<size_t>(world.default_limit)) {
                    hits.resize(static_cast<size_t>(world.default_limit));
                }
            } else {
                size_t eq = line.find('=');
                if (eq == std::string::npos) {
                    std::cout << "Ungueltige Query.\n";
                    continue;
                }
                std::string col = trim_ws(line.substr(0, eq));
                std::string val = trim_ws(line.substr(eq + 1));
                hits.clear();
                const auto start = std::chrono::steady_clock::now();
                for (const auto &tname : world.table_names) {
                    DbQuery query;
                    query.table = tname;
                    query.column = col;
                    query.value = val;
                    std::vector<int> local;
                    if (focus_set) {
                        local = db_execute_query_focus(world, query, focus_x, focus_y, radius);
                        used_focus = true;
                    } else {
                        local = db_execute_query(world, query, radius);
                    }
                    hits.insert(hits.end(), local.begin(), local.end());
                }
                if (used_focus && hits.empty()) {
                    for (const auto &tname : world.table_names) {
                        DbQuery query;
                        query.table = tname;
                        query.column = col;
                        query.value = val;
                        std::vector<int> local = db_execute_query(world, query, radius);
                        hits.insert(hits.end(), local.begin(), local.end());
                    }
                    fallback_global = true;
                }
                const auto end = std::chrono::steady_clock::now();
                duration_ms =
                  std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                if (world.default_limit >= 0 && hits.size() > static_cast<size_t>(world.default_limit)) {
                    hits.resize(static_cast<size_t>(world.default_limit));
                }
            }
            std::cout << "hits=" << hits.size();
            if (fallback_global) {
                std::cout << " (fallback_global)";
            }
            std::cout << "\n";
            last_query.text = line;
            last_query.is_sql = false;
            last_query.local = used_focus;
            last_query.fallback_global = fallback_global;
            last_query.hits = static_cast<int>(hits.size());

            std::vector<std::string> show_list;
            if (!show_cols.empty()) {
                std::stringstream ss(show_cols);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    std::string v = trim_ws(item);
                    if (!v.empty() && v != "*") {
                        show_list.push_back(v);
                    }
                }
            }
            if (show_list.empty() && global_show_enabled) {
                show_list = global_show;
            }

            auto equals_ci = [](const std::string &a, const std::string &b) {
                return a.size() == b.size() &&
                       std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                           return std::tolower(static_cast<unsigned char>(x)) ==
                                  std::tolower(static_cast<unsigned char>(y));
                       });
            };
            DbSqlResult shortcut_result;
            std::vector<std::string> shortcut_columns;
            if (!show_list.empty()) {
                shortcut_columns = show_list;
            } else if (!table.empty()) {
                int table_id = db_find_table(world, table);
                if (table_id >= 0 && table_id < static_cast<int>(world.table_columns.size())) {
                    shortcut_columns = world.table_columns[static_cast<size_t>(table_id)];
                }
                if (shortcut_columns.empty()) {
                    for (const auto &p : world.payloads) {
                        if (p.table_id == table_id) {
                            for (const auto &f : p.fields) {
                                shortcut_columns.push_back(f.name);
                            }
                            break;
                        }
                    }
                }
            }
            if (shortcut_columns.empty()) {
                shortcut_columns = {"data"};
            }
            shortcut_result.columns = shortcut_columns;
            shortcut_result.rows.reserve(hits.size());

            for (int idx : hits) {
                if (idx < 0 || idx >= static_cast<int>(world.payloads.size())) continue;
                const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
                std::string out_data;
                if (show_list.empty()) {
                    out_data = p.raw_data;
                } else {
                    bool first = true;
                    for (const auto &sel : show_list) {
                        for (const auto &f : p.fields) {
                            if (f.name == sel || equals_ci(sel, f.name)) {
                                if (!first) out_data += ", ";
                                out_data += f.name + "=" + f.value;
                                first = false;
                                break;
                            }
                        }
                    }
                }
                std::cout << "id=" << p.id << " table=" << world.table_names[static_cast<size_t>(p.table_id)]
                          << " x=" << p.x << " y=" << p.y << " data=\"" << out_data << "\"\n";

                std::vector<std::string> row;
                if (shortcut_columns.size() == 1 && shortcut_columns[0] == "data") {
                    row.push_back(p.raw_data);
                } else {
                    row.reserve(shortcut_columns.size());
                    for (const auto &sel : shortcut_columns) {
                        std::string value;
                        for (const auto &f : p.fields) {
                            if (f.name == sel || equals_ci(sel, f.name)) {
                                value = f.value;
                                break;
                            }
                        }
                        row.push_back(value);
                    }
                }
                shortcut_result.rows.push_back(std::move(row));
            }

            apply_limit(shortcut_result);
            last_sql_result = shortcut_result;
            last_sql_original = shortcut_result;
            last_sql_valid = !shortcut_result.rows.empty();
            if (duration_ms >= 0) {
                print_duration(duration_ms);
            }
        }
        return 0;
    }
    SimParams params = opts.params;
    for (int i = 0; i < 4; ++i) {
        params.toxic_max_fraction_by_quadrant[i] = opts.params.toxic_max_fraction_by_quadrant[i];
        params.toxic_max_fraction_by_species[i] = opts.params.toxic_max_fraction_by_species[i];
    }
    Rng rng(opts.seed);
    if (!opts.stress_seed_set) {
        opts.stress_seed = opts.seed;
    }

    if (opts.evo_enable) {
        if (opts.evo_elite_frac <= 0.0f || opts.evo_elite_frac > 1.0f) {
            std::cerr << "Ungueltiger Wert fuer --evo-elite-frac\n";
            return 1;
        }
        if (opts.evo_fitness_window <= 0) {
            std::cerr << "Ungueltiger Wert fuer --evo-fitness-window\n";
            return 1;
        }
        if (opts.evo_mutation_sigma < 0.0f || opts.evo_exploration_delta < 0.0f) {
            std::cerr << "Ungueltiger Wert fuer Evo-Mutationsparameter\n";
            return 1;
        }
        if (opts.evo_age_decay <= 0.0f || opts.evo_age_decay > 1.0f) {
            std::cerr << "Ungueltiger Wert fuer --evo-age-decay\n";
            return 1;
        }
    }
    if (params.toxic_max_fraction < 0.0f || params.toxic_max_fraction > 1.0f) {
        std::cerr << "Ungueltiger Wert fuer --toxic-max-frac\n";
        return 1;
    }
    for (int i = 0; i < 4; ++i) {
        if (params.toxic_max_fraction_by_quadrant[i] < 0.0f || params.toxic_max_fraction_by_quadrant[i] > 1.0f) {
            std::cerr << "Ungueltiger Wert fuer --toxic-max-frac-quadrant\n";
            return 1;
        }
        if (params.toxic_max_fraction_by_species[i] < 0.0f || params.toxic_max_fraction_by_species[i] > 1.0f) {
            std::cerr << "Ungueltiger Wert fuer --toxic-max-frac-species\n";
            return 1;
        }
    }
    if (params.toxic_stride_min <= 0 || params.toxic_stride_max < params.toxic_stride_min) {
        std::cerr << "Ungueltige Werte fuer --toxic-stride-min/max\n";
        return 1;
    }
    if (params.toxic_iters_min < 0 || params.toxic_iters_max < params.toxic_iters_min) {
        std::cerr << "Ungueltige Werte fuer --toxic-iters-min/max\n";
        return 1;
    }
    if (opts.log_verbosity < 0 || opts.log_verbosity > 2) {
        std::cerr << "Ungueltiger Wert fuer --log-verbosity\n";
        return 1;
    }
    if (opts.dump_every < 0) {
        std::cerr << "Ungueltiger Wert fuer --dump-every\n";
        return 1;
    }
    if (params.info_metabolism_cost < 0.0f) {
        std::cerr << "Ungueltiger Wert fuer --info-cost\n";
        return 1;
    }
    if (opts.report_downsample < 0) {
        std::cerr << "Ungueltiger Wert fuer --report-downsample\n";
        return 1;
    }
    if (opts.report_hist_bins <= 0) {
        std::cerr << "Ungueltiger Wert fuer --report-hist-bins\n";
        return 1;
    }
    if (opts.global_spawn_frac < 0.0f || opts.global_spawn_frac > 1.0f) {
        std::cerr << "Ungueltiger Wert fuer --global-spawn-frac\n";
        return 1;
    }
    if (params.dna_global_capacity <= 0) {
        std::cerr << "Ungueltiger Wert fuer --dna-global-capacity\n";
        return 1;
    }
    {
        float sum = 0.0f;
        for (float f : opts.species_fracs) {
            if (f < 0.0f) {
                std::cerr << "Ungueltiger Wert fuer --species-fracs\n";
                return 1;
            }
            sum += f;
        }
        if (std::abs(sum - 1.0f) > 1e-3f) {
            std::cerr << "Ungueltige Summe fuer --species-fracs (muss ~1.0 sein)\n";
            return 1;
        }
    }
    if (opts.ocl_no_copyback && params.agent_count > 0) {
        std::cerr << "[OpenCL] ocl-no-copyback ist mit aktiven Agenten nicht kompatibel, erzwungenes Copyback.\n";
        opts.ocl_no_copyback = false;
    }
    if (!opts.dump_subdir.empty()) {
        std::filesystem::path base_dir = opts.dump_dir;
        std::filesystem::path sub_dir = opts.dump_subdir;
        opts.dump_dir = (base_dir / sub_dir).string();
        if (!opts.report_html_path.empty()) {
            std::filesystem::path report_path = opts.report_html_path;
            opts.report_html_path = (std::filesystem::path(opts.dump_dir) / report_path.filename()).string();
        }
    }

    GridData resources_data;
    GridData pheromone_data;
    GridData molecules_data;
    std::string error;

    auto apply_dataset = [&](const std::string &path, GridData &data, const char *label) -> bool {
        if (path.empty()) return true;
        if (!load_grid_csv(path, data, error)) {
            std::cerr << label << ": " << error << "\n";
            return false;
        }
        if (opts.width_set && data.width != params.width) {
            std::cerr << "Breite aus CSV passt nicht zu --width\n";
            return false;
        }
        if (opts.height_set && data.height != params.height) {
            std::cerr << "Hoehe aus CSV passt nicht zu --height\n";
            return false;
        }
        params.width = data.width;
        params.height = data.height;
        return true;
    };

    if (!apply_dataset(opts.resources_path, resources_data, "resources")) return 1;
    if (!apply_dataset(opts.pheromone_path, pheromone_data, "pheromone")) return 1;
    if (!apply_dataset(opts.molecules_path, molecules_data, "molecules")) return 1;

    if (params.logic_input_ax < 0 || params.logic_input_ay < 0 ||
        params.logic_input_bx < 0 || params.logic_input_by < 0) {
        params.logic_input_ax = params.width / 4;
        params.logic_input_ay = params.height / 4;
        params.logic_input_bx = params.width / 4;
        params.logic_input_by = (params.height * 3) / 4;
    }
    if (params.logic_output_x < 0 || params.logic_output_y < 0) {
        params.logic_output_x = (params.width * 3) / 4;
        params.logic_output_y = params.height / 2;
    }
    if (params.logic_mode < 0 || params.logic_mode > 3) {
        std::cerr << "Ungueltiger Wert fuer --logic-mode\n";
        return 1;
    }
    if (params.logic_pulse_period <= 0) {
        std::cerr << "Ungueltiger Wert fuer --logic-pulse-period\n";
        return 1;
    }
    if (params.logic_pulse_strength < 0.0f) {
        std::cerr << "Ungueltiger Wert fuer --logic-pulse-strength\n";
        return 1;
    }
    if (params.logic_mode != 0) {
        auto in_bounds = [&](int x, int y) -> bool {
            return x >= 0 && y >= 0 && x < params.width && y < params.height;
        };
        if (!in_bounds(params.logic_input_ax, params.logic_input_ay) ||
            !in_bounds(params.logic_input_bx, params.logic_input_by) ||
            !in_bounds(params.logic_output_x, params.logic_output_y)) {
            std::cerr << "Logic-Input/Output ausserhalb des Rasters\n";
            return 1;
        }
    }

    OpenCLStatus ocl_probe = probe_opencl();
    std::cout << "[OpenCL] " << ocl_probe.message << "\n";

    Environment env(params.width, params.height);
    if (!resources_data.values.empty()) {
        env.resources.data = resources_data.values;
    } else {
        env.seed_resources(rng);
    }

    // phero_food/phero_danger act as semantic channels Alpha/Beta (kept names for compatibility).
    GridField phero_food(params.width, params.height, 0.0f);
    GridField phero_danger(params.width, params.height, 0.0f);
    GridField phero_gamma(params.width, params.height, 0.0f);
    GridField molecules(params.width, params.height, 0.0f);
    MycelNetwork mycel(params.width, params.height);
    if (!pheromone_data.values.empty()) {
        phero_food.data = pheromone_data.values;
    }
    if (!molecules_data.values.empty()) {
        molecules.data = molecules_data.values;
    }

    std::array<DNAMemory, 4> dna_species;
    DNAMemory dna_global;
    EvoParams evo;
    evo.enabled = opts.evo_enable;
    evo.elite_frac = opts.evo_elite_frac;
    evo.mutation_sigma = opts.evo_mutation_sigma;
    evo.exploration_delta = opts.evo_exploration_delta;
    evo.fitness_window = opts.evo_fitness_window;
    evo.age_decay = opts.evo_age_decay;
    std::vector<Agent> agents;
    agents.reserve(params.agent_count);

    const int codon_max = 7;
    const int lws_min = 0;
    const int lws_max = 32;
    const int toxic_stride_min = std::max(1, params.toxic_stride_min);
    const int toxic_stride_max = std::max(toxic_stride_min, params.toxic_stride_max);
    const int toxic_iters_min = std::max(0, params.toxic_iters_min);
    const int toxic_iters_max = std::max(toxic_iters_min, params.toxic_iters_max);
    const bool toxic_enabled = params.toxic_enable != 0;
    auto randomize_codons = [&](Genome &g) {
        for (int i = 0; i < 4; ++i) {
            g.kernel_codons[i] = rng.uniform_int(0, codon_max);
        }
        g.lws_x = rng.uniform_int(lws_min, lws_max);
        g.lws_y = rng.uniform_int(lws_min, lws_max);
        g.toxic_stride = rng.uniform_int(toxic_stride_min, toxic_stride_max);
        g.toxic_iters = rng.uniform_int(toxic_iters_min, toxic_iters_max);
        if (!toxic_enabled) {
            g.toxic_iters = 0;
        }
    };
    auto clamp_codons = [&](Genome &g) {
        for (int i = 0; i < 4; ++i) {
            g.kernel_codons[i] = std::min(codon_max, std::max(0, g.kernel_codons[i]));
        }
        g.lws_x = std::min(lws_max, std::max(lws_min, g.lws_x));
        g.lws_y = std::min(lws_max, std::max(lws_min, g.lws_y));
        g.toxic_stride = std::min(toxic_stride_max, std::max(toxic_stride_min, g.toxic_stride));
        g.toxic_iters = std::min(toxic_iters_max, std::max(toxic_iters_min, g.toxic_iters));
        if (!toxic_enabled) {
            g.toxic_iters = 0;
        }
    };
    auto mutate_codons = [&](Genome &g, float prob) {
        if (prob <= 0.0f) return;
        for (int i = 0; i < 4; ++i) {
            if (rng.uniform(0.0f, 1.0f) < prob) {
                g.kernel_codons[i] = rng.uniform_int(0, codon_max);
            }
        }
        if (rng.uniform(0.0f, 1.0f) < prob) {
            g.lws_x = rng.uniform_int(lws_min, lws_max);
        }
        if (rng.uniform(0.0f, 1.0f) < prob) {
            g.lws_y = rng.uniform_int(lws_min, lws_max);
        }
        if (rng.uniform(0.0f, 1.0f) < prob) {
            g.toxic_stride = rng.uniform_int(toxic_stride_min, toxic_stride_max);
        }
        if (rng.uniform(0.0f, 1.0f) < prob) {
            g.toxic_iters = rng.uniform_int(toxic_iters_min, toxic_iters_max);
        }
    };

    auto random_genome = [&]() -> Genome {
        Genome g;
        g.sense_gain = rng.uniform(0.6f, 1.4f);
        g.pheromone_gain = rng.uniform(0.6f, 1.4f);
        g.exploration_bias = rng.uniform(0.2f, 0.8f);
        randomize_semantics(rng, g);
        clamp_semantics(g);
        randomize_codons(g);
        return g;
    };

    auto apply_role_mutation = [&](Genome &g, const SpeciesProfile &profile) {
        float sigma = evo.mutation_sigma * profile.mutation_sigma_mul;
        float delta = evo.exploration_delta * profile.exploration_delta_mul;
        if (sigma > 0.0f) {
            g.sense_gain *= rng.uniform(1.0f - sigma, 1.0f + sigma);
            g.pheromone_gain *= rng.uniform(1.0f - sigma, 1.0f + sigma);
        }
        if (delta > 0.0f) {
            g.exploration_bias += rng.uniform(-delta, delta);
        }
        g.response_matrix[0] += gaussian(rng, sigma);
        g.response_matrix[1] += gaussian(rng, sigma);
        g.response_matrix[2] += gaussian(rng, sigma);
        for (int i = 0; i < 4; ++i) {
            g.emission_matrix[i] += gaussian(rng, sigma);
        }
        mutate_codons(g, std::min(0.5f, sigma * 2.0f));
        g.sense_gain = std::min(3.0f, std::max(0.2f, g.sense_gain));
        g.pheromone_gain = std::min(3.0f, std::max(0.2f, g.pheromone_gain));
        g.exploration_bias = std::min(1.0f, std::max(0.0f, g.exploration_bias));
        clamp_semantics(g);
        clamp_codons(g);
    };

    auto sample_genome = [&](int species) -> Genome {
        const SpeciesProfile &profile = opts.species_profiles[species];
        bool use_dna = rng.uniform(0.0f, 1.0f) < profile.dna_binding;
        Genome g;
        if (use_dna) {
            if (opts.evo_enable && !dna_global.entries.empty() && rng.uniform(0.0f, 1.0f) < opts.global_spawn_frac) {
                g = dna_global.sample(rng, params, evo);
            } else {
                g = dna_species[species].sample(rng, params, evo);
            }
        } else {
            g = random_genome();
            apply_semantic_defaults(g, profile);
        }
        if (opts.evo_enable) {
            apply_role_mutation(g, profile);
        }
        return g;
    };

    const float global_epsilon = 1e-6f;
    auto maybe_add_global = [&](const Genome &genome, float fitness) {
        if (!opts.evo_enable) {
            return;
        }
        if (params.dna_global_capacity <= 0) {
            return;
        }
        if (dna_global.entries.size() < static_cast<size_t>(params.dna_global_capacity)) {
            dna_global.add(params, genome, fitness, evo, params.dna_global_capacity);
            return;
        }
        float worst = dna_global.entries.back().fitness;
        if (fitness > worst + global_epsilon) {
            dna_global.add(params, genome, fitness, evo, params.dna_global_capacity);
        }
    };

    for (int i = 0; i < params.agent_count; ++i) {
        Agent agent;
        agent.x = static_cast<float>(rng.uniform_int(0, params.width - 1));
        agent.y = static_cast<float>(rng.uniform_int(0, params.height - 1));
        agent.heading = rng.uniform(0.0f, 6.283185307f);
        agent.energy = rng.uniform(0.2f, 0.6f);
        agent.species = pick_species(rng, opts.species_fracs);
        agent.genome = sample_genome(agent.species);
        agents.push_back(agent);
    }

    FieldParams pheromone_params{params.pheromone_evaporation, params.pheromone_diffusion};
    FieldParams molecule_params{params.molecule_evaporation, params.molecule_diffusion};

    OpenCLRuntime ocl_runtime;
    bool ocl_active = false;
    if (opts.ocl_enable) {
        std::string ocl_error;
        if (!ocl_runtime.init(opts.ocl_platform, opts.ocl_device, ocl_error)) {
            std::cerr << "[OpenCL] init failed, fallback to CPU: " << ocl_error << "\n";
        } else if (!ocl_runtime.build_kernels(ocl_error)) {
            std::cerr << "[OpenCL] kernel build failed, fallback to CPU: " << ocl_error << "\n";
        } else if (!ocl_runtime.init_fields(phero_food, phero_danger, phero_gamma, molecules, ocl_error)) {
            std::cerr << "[OpenCL] buffer init failed, fallback to CPU: " << ocl_error << "\n";
        } else {
            std::cout << "[OpenCL] platform/device: " << ocl_runtime.device_info() << "\n";
            std::cout << "[OpenCL] kernels built\n";
            ocl_active = true;
        }
    }

    auto run_ocl_self_test = [&](OpenCLRuntime &runtime) -> bool {
        GridField pf(16, 16, 0.0f);
        GridField pd(16, 16, 0.0f);
        GridField m(16, 16, 0.0f);
        for (int y = 0; y < pf.height; ++y) {
            for (int x = 0; x < pf.width; ++x) {
                float v = rng.uniform(0.0f, 1.0f);
                pf.at(x, y) = v;
                pd.at(x, y) = 1.0f - v;
                m.at(x, y) = 1.0f - v;
            }
        }
        GridField cpu_pf = pf;
        GridField cpu_pd = pd;
        GridField cpu_m = m;
        GridField pg(pf.width, pf.height, 0.0f);
        GridField cpu_pg = pg;
        FieldParams fp{0.02f, 0.15f};
        FieldParams fm{0.35f, 0.25f};
        for (int i = 0; i < 5; ++i) {
            diffuse_and_evaporate(cpu_pf, fp);
            diffuse_and_evaporate(cpu_pd, fp);
            diffuse_and_evaporate(cpu_pg, fp);
            diffuse_and_evaporate(cpu_m, fm);
        }

        std::string error;
        if (!runtime.init_fields(pf, pd, pg, m, error)) {
            std::cerr << "[OpenCL] self-test init failed: " << error << "\n";
            return false;
        }
        for (int i = 0; i < 5; ++i) {
            if (!runtime.step_diffuse(fp, fm, true, pf, pd, pg, m, error)) {
                std::cerr << "[OpenCL] self-test step failed: " << error << "\n";
                return false;
            }
        }
        double mean_diff = 0.0;
        double max_abs = 0.0;
        for (size_t i = 0; i < pf.data.size(); ++i) {
            double d1 = std::abs(static_cast<double>(pf.data[i]) - cpu_pf.data[i]);
            double d2 = std::abs(static_cast<double>(pd.data[i]) - cpu_pd.data[i]);
            mean_diff += d1 + d2;
            if (d1 > max_abs) max_abs = d1;
            if (d2 > max_abs) max_abs = d2;
        }
        mean_diff /= static_cast<double>(pf.data.size() * 2);
        std::cout << "[OpenCL] self-test mean_diff=" << mean_diff << " max_abs=" << max_abs << "\n";
        if (max_abs > 1e-3) {
            std::cerr << "[OpenCL] self-test too large diff, fallback to CPU\n";
            return false;
        }
        return true;
    };

    if (ocl_active) {
        if (!run_ocl_self_test(ocl_runtime)) {
            ocl_active = false;
        } else {
            std::string ocl_error;
            if (!ocl_runtime.init_fields(phero_food, phero_danger, phero_gamma, molecules, ocl_error)) {
                std::cerr << "[OpenCL] buffer init failed, fallback to CPU: " << ocl_error << "\n";
                ocl_active = false;
            } else {
                std::cout << "[OpenCL] using GPU diffusion\n";
                if (opts.ocl_no_copyback) {
                    std::cout << "[OpenCL] no-copyback enabled\n";
                }
            }
        }
    }

    if (opts.dump_every > 0) {
        std::error_code ec;
        std::filesystem::create_directories(opts.dump_dir, ec);
        if (ec) {
            std::cerr << "Konnte Dump-Verzeichnis nicht erstellen: " << opts.dump_dir << "\n";
            return 1;
        }
    }

    auto dump_fields = [&](int step) -> bool {
        if (opts.dump_every <= 0) return true;
        if (step % opts.dump_every != 0) return true;

        std::ostringstream name;
        name << opts.dump_prefix << "_step" << std::setw(6) << std::setfill('0') << step;
        std::string base = name.str();

        std::string error;
        auto dump_one = [&](const std::string &suffix, const GridField &field) -> bool {
            std::filesystem::path path = std::filesystem::path(opts.dump_dir) / (base + suffix);
            if (!save_grid_csv(path.string(), field.width, field.height, field.data, error)) {
                std::cerr << error << "\n";
                return false;
            }
            return true;
        };

        if (!dump_one("_resources.csv", env.resources)) return false;
        if (!dump_one("_phero_food.csv", phero_food)) return false;
        if (!dump_one("_phero_danger.csv", phero_danger)) return false;
        if (!dump_one("_molecules.csv", molecules)) return false;
        if (!dump_one("_mycel.csv", mycel.density)) return false;
        return true;
    };

    bool stress_applied = false;
    Rng stress_rng(opts.stress_seed);
    std::vector<SystemMetrics> system_metrics;
    system_metrics.reserve(static_cast<size_t>(params.steps));
    bool last_physics_valid = true;
    auto field_sum = [](const GridField &field) -> double {
        double sum = 0.0;
        for (float v : field.data) {
            sum += static_cast<double>(v);
        }
        return sum;
    };
    auto compute_stagnation = [&]() -> float {
        if (!dna_global.entries.empty()) {
            return calculate_genetic_stagnation(dna_global.entries);
        }
        std::vector<DNAEntry> merged;
        for (const auto &pool : dna_species) {
            merged.insert(merged.end(), pool.entries.begin(), pool.entries.end());
        }
        if (merged.empty()) {
            return 1.0f;
        }
        return calculate_genetic_stagnation(merged);
    };
    auto inject_gamma = [&](float base, const float quad_ns[4]) {
        if (base > 0.0f) {
            for (float &v : phero_gamma.data) {
                v += base;
            }
        }
        int mid_x = params.width / 2;
        int mid_y = params.height / 2;
        struct Quad {
            int x0;
            int y0;
            int x1;
            int y1;
        };
        Quad quads[4] = {
            {0, 0, mid_x, mid_y},
            {mid_x, 0, params.width, mid_y},
            {0, mid_y, mid_x, params.height},
            {mid_x, mid_y, params.width, params.height}
        };
        const float scale = 1.0f / 1000000.0f;
        for (int q = 0; q < 4; ++q) {
            float v = clamp01(static_cast<float>(quad_ns[q]) * scale);
            if (v <= 0.0f) {
                continue;
            }
            for (int y = quads[q].y0; y < quads[q].y1; ++y) {
                for (int x = quads[q].x0; x < quads[q].x1; ++x) {
                    phero_gamma.at(x, y) += v;
                }
            }
        }
    };
    int logic_case = 0;
    int logic_active_case = 0;
    float logic_last_score = 0.5f;
    float logic_path_radius = std::max(2.0f, std::min(params.width, params.height) * 0.05f);
    auto sample_output = [&](const GridField &field) -> float {
        int x0 = std::max(0, params.logic_output_x - 1);
        int x1 = std::min(params.width - 1, params.logic_output_x + 1);
        int y0 = std::max(0, params.logic_output_y - 1);
        int y1 = std::min(params.height - 1, params.logic_output_y + 1);
        float sum = 0.0f;
        int count = 0;
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                sum += field.at(x, y);
                count++;
            }
        }
        return (count > 0) ? (sum / static_cast<float>(count)) : 0.0f;
    };

    for (int step = 0; step < params.steps; ++step) {
        bool dump_step = (opts.dump_every > 0 && step % opts.dump_every == 0);
        float quad_ns[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (ocl_active) {
            ocl_runtime.last_quadrant_exhaustion_ns(quad_ns);
        }
        float stagnation = compute_stagnation();
        inject_gamma(stagnation, quad_ns);
        if (params.logic_mode != 0 && (step % params.logic_pulse_period == 0)) {
            logic_active_case = logic_case;
            int a = (logic_active_case >> 0) & 1;
            int b = (logic_active_case >> 1) & 1;
            if (a) {
                phero_food.at(params.logic_input_ax, params.logic_input_ay) += params.logic_pulse_strength;
            }
            if (b) {
                phero_food.at(params.logic_input_bx, params.logic_input_by) += params.logic_pulse_strength;
            }
            logic_case = (logic_case + 1) & 3;
        }
        if (ocl_active && opts.ocl_no_copyback && dump_step) {
            std::string ocl_error;
            if (!ocl_runtime.copyback(phero_food, phero_danger, phero_gamma, molecules, ocl_error)) {
                std::cerr << "[OpenCL] copyback failed, fallback to CPU: " << ocl_error << "\n";
                ocl_active = false;
            }
        }
        if (opts.stress_enable && !stress_applied && step >= opts.stress_at_step) {
            if (opts.stress_block_rect_set) {
                env.apply_block_rect(opts.stress_block_x, opts.stress_block_y, opts.stress_block_w, opts.stress_block_h);
            }
            if (opts.stress_shift_set) {
                env.shift_hotspots(opts.stress_shift_dx, opts.stress_shift_dy);
            }
            stress_applied = true;
            std::cout << "[stress] applied at step=" << step << "\n";
        }
        if (!dump_fields(step)) {
            return 1;
        }
        for (auto &agent : agents) {
            const SpeciesProfile &profile = opts.species_profiles[agent.species];
            int fitness_window = (opts.evo_enable && params.logic_mode == 0) ? opts.evo_fitness_window : 0;
            agent.step(rng, params, fitness_window, profile, phero_food, phero_danger, phero_gamma, molecules, env.resources, mycel.density);
            if (opts.evo_enable && params.logic_mode != 0) {
                float dist_a = distance_to_segment(static_cast<float>(params.logic_input_ax),
                                                   static_cast<float>(params.logic_input_ay),
                                                   static_cast<float>(params.logic_output_x),
                                                   static_cast<float>(params.logic_output_y),
                                                   agent.x, agent.y);
                float dist_b = distance_to_segment(static_cast<float>(params.logic_input_bx),
                                                   static_cast<float>(params.logic_input_by),
                                                   static_cast<float>(params.logic_output_x),
                                                   static_cast<float>(params.logic_output_y),
                                                   agent.x, agent.y);
                float dist = std::min(dist_a, dist_b);
                float weight = 0.0f;
                if (dist <= logic_path_radius) {
                    weight = 1.0f - (dist / logic_path_radius);
                }
                agent.fitness_value = logic_last_score * weight;
            }
            if (opts.evo_enable) {
                if (agent.energy > opts.evo_min_energy_to_store) {
                    float fitness = agent.fitness_value;
                    if (ocl_active) {
                        float hw_penalty_ms = ocl_runtime.last_hardware_exhaustion_ns() / 1000000.0f;
                        fitness = agent.fitness_value / (hw_penalty_ms + 0.0001f);
                        if (!last_physics_valid) {
                            fitness *= 0.01f;
                        }
                    }
                    dna_species[agent.species].add(params, agent.genome, fitness, evo, params.dna_capacity);
                    maybe_add_global(agent.genome, fitness);
                    agent.energy *= 0.6f;
                }
            } else {
                if (agent.energy > 1.2f) {
                    dna_species[agent.species].add(params, agent.genome, agent.energy, evo, params.dna_capacity);
                    agent.energy *= 0.6f;
                }
            }
        }

        if (ocl_active && opts.evo_enable) {
            struct QuadPick {
                Genome genome;
                float score = -1.0f;
                bool has = false;
                bool from_global = false;
                int species = 0;
            };
            QuadPick picks[4];
            auto is_toxic_extra = [](int idx) -> bool {
                return idx >= 4;
            };
            const char *sum_names[] = {"standard", "mad", "alt", "sin_bias"};
            const char *neigh_names[] = {"h+v", "packed", "vector4", "skew"};
            const char *extra_names[] = {"none", "sin", "exp", "local_scatter", "local_atomic", "bank_conflict", "global_atomic", "unaligned_v4"};
            const char *out_names[] = {"clamp", "evap_sub", "ternary", "sinexp_mix"};

            int mid_x = params.width / 2;
            int mid_y = params.height / 2;
            for (const auto &agent : agents) {
                int q = 0;
                if (agent.x >= static_cast<float>(mid_x)) q += 1;
                if (agent.y >= static_cast<float>(mid_y)) q += 2;
                float score = (agent.fitness_value > 0.0f) ? agent.fitness_value : agent.energy;
                if (!picks[q].has || score > picks[q].score) {
                    picks[q].genome = agent.genome;
                    picks[q].score = score;
                    picks[q].has = true;
                    picks[q].from_global = false;
                    picks[q].species = agent.species;
                }
            }

            for (int q = 0; q < 4; ++q) {
                if (!picks[q].has) {
                    if (!dna_global.entries.empty()) {
                        picks[q].genome = dna_global.entries.front().genome;
                        picks[q].from_global = true;
                        picks[q].species = 0;
                    } else {
                        picks[q].genome = random_genome();
                        picks[q].from_global = false;
                        picks[q].species = 0;
                    }
                    picks[q].has = true;
                }
            }

            int lws[4][2] = {};
            for (int q = 0; q < 4; ++q) {
                lws[q][0] = picks[q].genome.lws_x;
                lws[q][1] = picks[q].genome.lws_y;
            }
            ocl_runtime.set_quadrant_lws(lws);

            if (step % 500 == 0) {
                int toxic_hits[4] = {0, 0, 0, 0};
                int total_counts[4] = {0, 0, 0, 0};
                for (int q = 0; q < 4; ++q) {
                    int codons[4] = {
                        picks[q].genome.kernel_codons[0],
                        picks[q].genome.kernel_codons[1],
                        picks[q].genome.kernel_codons[2],
                        picks[q].genome.kernel_codons[3]
                    };
                    total_counts[q] = 1;
                    bool toxic_allowed = (params.toxic_enable != 0) && (params.toxic_max_fraction > 0.0f);
                    float gate = params.toxic_max_fraction;
                    if (params.toxic_max_fraction_by_quadrant[q] < gate) {
                        gate = params.toxic_max_fraction_by_quadrant[q];
                    }
                    if (params.toxic_max_fraction_by_species[picks[q].species] < gate) {
                        gate = params.toxic_max_fraction_by_species[picks[q].species];
                    }
                    int toxic_stride = std::min(toxic_stride_max, std::max(toxic_stride_min, picks[q].genome.toxic_stride));
                    int toxic_iters = std::min(toxic_iters_max, std::max(toxic_iters_min, picks[q].genome.toxic_iters));
                    if (!toxic_allowed) {
                        toxic_iters = 0;
                    }
                    if (is_toxic_extra(codons[2])) {
                        if (!toxic_allowed || rng.uniform(0.0f, 1.0f) > gate) {
                            codons[2] = 0;
                        } else {
                            toxic_hits[q] += 1;
                        }
                    }
                    std::string build_err;
                    if (!ocl_runtime.assemble_evolved_kernel_quadrant(q,
                                                                      codons,
                                                                      toxic_stride,
                                                                      toxic_iters,
                                                                      build_err)) {
                        std::cerr << "[Hardware-Mutation-Error] quadrant=" << q << " " << build_err << "\n";
                        if (picks[q].from_global && !dna_global.entries.empty()) {
                            dna_global.entries.front().fitness *= 0.1f;
                            std::sort(dna_global.entries.begin(), dna_global.entries.end(), [](const DNAEntry &a, const DNAEntry &b) {
                                return a.fitness > b.fitness;
                            });
                        }
                    } else {
                        auto pick_name = [](const char *const *names, int count, int idx) -> const char * {
                            if (count <= 0) return "";
                            int fixed = idx % count;
                            if (fixed < 0) fixed += count;
                            return names[fixed];
                        };
                        if (opts.log_verbosity >= 1) {
                            std::cout << "[Hardware-Mutation] quadrant=" << q
                                      << " codons=["
                                      << codons[0] << ","
                                      << codons[1] << ","
                                      << codons[2] << ","
                                      << codons[3] << "]"
                                      << " lws=(" << picks[q].genome.lws_x << "x" << picks[q].genome.lws_y << ")"
                                      << " tox=(" << toxic_stride << "," << toxic_iters << ")"
                                      << " gate=" << gate
                                      << "\n";
                            if (opts.log_verbosity >= 2) {
                                std::cout << "[Hardware-Mutation-Map] quadrant=" << q
                                          << " sum=" << pick_name(sum_names, 4, codons[0])
                                          << " neigh=" << pick_name(neigh_names, 4, codons[1])
                                          << " extra=" << pick_name(extra_names, 8, codons[2])
                                          << " out=" << pick_name(out_names, 4, codons[3])
                                          << "\n";
                                std::cout << "[Semantics] quadrant=" << q
                                          << " response=[" << picks[q].genome.response_matrix[0] << "," << picks[q].genome.response_matrix[1]
                                          << "," << picks[q].genome.response_matrix[2] << "]"
                                          << " emit=[" << picks[q].genome.emission_matrix[0] << "," << picks[q].genome.emission_matrix[1]
                                          << "," << picks[q].genome.emission_matrix[2] << "," << picks[q].genome.emission_matrix[3] << "]"
                                          << "\n";
                            }
                        }
                    }
                }
                if (opts.log_verbosity >= 1) {
                    std::cout << "[Toxic-Hist] step=" << step
                              << " q0=" << toxic_hits[0] << "/" << total_counts[0]
                              << " q1=" << toxic_hits[1] << "/" << total_counts[1]
                              << " q2=" << toxic_hits[2] << "/" << total_counts[2]
                              << " q3=" << toxic_hits[3] << "/" << total_counts[3]
                              << "\n";
                }
            }
        }
        bool cpu_diffused = false;
        if (ocl_active) {
            double pre_food_sum = field_sum(phero_food);
            double pre_danger_sum = field_sum(phero_danger);
            double pre_mol_sum = field_sum(molecules);
            std::string ocl_error;
            if (!ocl_runtime.upload_fields(phero_food, phero_danger, phero_gamma, molecules, ocl_error)) {
                std::cerr << "[OpenCL] upload failed, fallback to CPU: " << ocl_error << "\n";
                ocl_active = false;
            } else {
                bool do_copyback = (!opts.ocl_no_copyback) || dump_step;
                if (!ocl_runtime.step_diffuse(pheromone_params, molecule_params, do_copyback, phero_food, phero_danger, phero_gamma, molecules, ocl_error)) {
                    std::cerr << "[OpenCL] diffuse failed, fallback to CPU: " << ocl_error << "\n";
                    ocl_active = false;
                    diffuse_and_evaporate(phero_food, pheromone_params);
                    diffuse_and_evaporate(phero_danger, pheromone_params);
                    diffuse_and_evaporate(phero_gamma, pheromone_params);
                    diffuse_and_evaporate(molecules, molecule_params);
                    cpu_diffused = true;
                } else if (do_copyback) {
                    auto valid_sum = [](double pre, double post, float evap) -> bool {
                        if (!std::isfinite(pre) || !std::isfinite(post)) return false;
                        double expected = pre * (1.0 - static_cast<double>(evap));
                        if (expected < 1e-6) {
                            return post >= -1e-3;
                        }
                        double min_allowed = expected * 0.5;
                        double max_allowed = pre * 1.1;
                        return post >= min_allowed && post <= max_allowed;
                    };
                    double post_food_sum = field_sum(phero_food);
                    double post_danger_sum = field_sum(phero_danger);
                    double post_mol_sum = field_sum(molecules);
                    bool ok_food = valid_sum(pre_food_sum, post_food_sum, pheromone_params.evaporation);
                    bool ok_danger = valid_sum(pre_danger_sum, post_danger_sum, pheromone_params.evaporation);
                    bool ok_mol = valid_sum(pre_mol_sum, post_mol_sum, molecule_params.evaporation);
                    last_physics_valid = ok_food && ok_danger && ok_mol;
                }
            }
        }
        if (!ocl_active && !cpu_diffused) {
            diffuse_and_evaporate(phero_food, pheromone_params);
            diffuse_and_evaporate(phero_danger, pheromone_params);
            diffuse_and_evaporate(phero_gamma, pheromone_params);
            diffuse_and_evaporate(molecules, molecule_params);
            last_physics_valid = true;
        }

        if (opts.stress_enable && stress_applied && opts.stress_pheromone_noise > 0.0f) {
            for (float &v : phero_food.data) {
                v += stress_rng.uniform(0.0f, opts.stress_pheromone_noise);
                if (v < 0.0f) v = 0.0f;
            }
            for (float &v : phero_danger.data) {
                v += stress_rng.uniform(0.0f, opts.stress_pheromone_noise);
                if (v < 0.0f) v = 0.0f;
            }
        }

        mycel.update(params, phero_food, env.resources);
        if (params.logic_mode != 0) {
            float measured = sample_output(mycel.density);
            int target = logic_target_for_case(params.logic_mode, logic_active_case);
            float score = 1.0f - std::abs(static_cast<float>(target) - clamp01(measured));
            logic_last_score = clamp01(score);
        }
        env.regenerate(params);
        for (auto &pool : dna_species) {
            pool.decay(evo);
        }
        dna_global.decay(evo);

        for (auto &agent : agents) {
            if (agent.energy <= 0.05f) {
                agent.x = static_cast<float>(rng.uniform_int(0, params.width - 1));
                agent.y = static_cast<float>(rng.uniform_int(0, params.height - 1));
                agent.heading = rng.uniform(0.0f, 6.283185307f);
                agent.energy = rng.uniform(0.2f, 0.5f);
                agent.last_energy = agent.energy;
                agent.fitness_accum = 0.0f;
                agent.fitness_ticks = 0;
                agent.fitness_value = 0.0f;
                agent.species = pick_species(rng, opts.species_fracs);
                agent.genome = sample_genome(agent.species);
            }
        }

        float avg_energy = 0.0f;
        float avg_cognitive_load = 0.0f;
        std::array<float, 4> energy_sum{0.0f, 0.0f, 0.0f, 0.0f};
        std::array<int, 4> energy_count{0, 0, 0, 0};
        for (const auto &agent : agents) {
            avg_energy += agent.energy;
            float cog = std::abs(agent.genome.response_matrix[0]) +
                        std::abs(agent.genome.response_matrix[1]) +
                        std::abs(agent.genome.response_matrix[2]) +
                        std::abs(agent.genome.emission_matrix[0]) +
                        std::abs(agent.genome.emission_matrix[1]) +
                        std::abs(agent.genome.emission_matrix[2]) +
                        std::abs(agent.genome.emission_matrix[3]);
            avg_cognitive_load += cog;
            if (agent.species >= 0 && agent.species < 4) {
                energy_sum[agent.species] += agent.energy;
                energy_count[agent.species] += 1;
            }
        }
        avg_energy /= static_cast<float>(agents.size());
        avg_cognitive_load /= static_cast<float>(agents.size());

        SystemMetrics m;
        m.step = step;
        m.avg_agent_energy = avg_energy;
        m.avg_cognitive_load = avg_cognitive_load;
        int dna_total = 0;
        for (int s = 0; s < 4; ++s) {
            m.dna_species_sizes[s] = static_cast<int>(dna_species[s].entries.size());
            dna_total += m.dna_species_sizes[s];
            if (energy_count[s] > 0) {
                m.avg_energy_by_species[s] = energy_sum[s] / static_cast<float>(energy_count[s]);
            } else {
                m.avg_energy_by_species[s] = 0.0f;
            }
        }
        m.dna_global_size = static_cast<int>(dna_global.entries.size());
        m.dna_pool_size = dna_total;
        system_metrics.push_back(m);

        if (step % 10 == 0) {
            float mycel_sum = 0.0f;
            for (float v : mycel.density.data) {
                mycel_sum += v;
            }
            float mycel_avg = mycel_sum / static_cast<float>(mycel.density.data.size());

            std::cout << "step=" << step
                      << " avg_energy=" << avg_energy
                      << " dna_pool=" << dna_total
                      << " mycel_avg=" << mycel_avg
                      << "\n";
        }
    }

    if (ocl_active && opts.ocl_no_copyback) {
        std::string ocl_error;
        if (!ocl_runtime.copyback(phero_food, phero_danger, phero_gamma, molecules, ocl_error)) {
            std::cerr << "[OpenCL] final copyback failed: " << ocl_error << "\n";
            return 1;
        }
    }

    if (opts.dump_every > 0) {
        ReportOptions report_opts;
        report_opts.dump_dir = opts.dump_dir;
        report_opts.dump_prefix = opts.dump_prefix;
        report_opts.report_html_path = opts.report_html_path;
        report_opts.downsample = opts.report_downsample;
        report_opts.paper_mode = opts.paper_mode;
        report_opts.global_normalization = opts.report_global_norm;
        report_opts.hist_bins = opts.report_hist_bins;
        report_opts.include_sparklines = opts.report_include_sparklines;
        report_opts.system_metrics = system_metrics;
        std::ostringstream scenario;
        bool has_scenario = false;
        if (opts.stress_enable) {
            scenario << "stress_enable=true";
            scenario << ", at_step=" << opts.stress_at_step;
            if (opts.stress_block_rect_set) {
                scenario << ", block_rect=" << opts.stress_block_x << "," << opts.stress_block_y << ","
                         << opts.stress_block_w << "," << opts.stress_block_h;
            }
            if (opts.stress_shift_set) {
                scenario << ", shift_hotspots=" << opts.stress_shift_dx << "," << opts.stress_shift_dy;
            }
            if (opts.stress_pheromone_noise > 0.0f) {
                scenario << ", pheromone_noise=" << opts.stress_pheromone_noise;
            }
            has_scenario = true;
        }
        const Genome *top = nullptr;
        float best_fit = -1.0f;
        if (!dna_global.entries.empty()) {
            top = &dna_global.entries.front().genome;
        } else {
            for (const auto &pool : dna_species) {
                for (const auto &entry : pool.entries) {
                    if (entry.fitness > best_fit) {
                        best_fit = entry.fitness;
                        top = &entry.genome;
                    }
                }
            }
        }
        if (top) {
            if (has_scenario) {
                scenario << " | ";
            }
            scenario << "top_semantics=response[" << top->response_matrix[0] << "," << top->response_matrix[1] << "," << top->response_matrix[2]
                     << "] emit[" << top->emission_matrix[0] << "," << top->emission_matrix[1] << ","
                     << top->emission_matrix[2] << "," << top->emission_matrix[3] << "]";
            has_scenario = true;
        }
        if (has_scenario) {
            report_opts.scenario_summary = scenario.str();
        }
        std::string report_error;
        if (!generate_dump_report_html(report_opts, report_error)) {
            std::cerr << "Report-Fehler: " << report_error << "\n";
            return 1;
        }
        std::filesystem::path report_path;
        if (opts.report_html_path.empty()) {
            report_path = std::filesystem::path(opts.dump_dir) / (opts.dump_prefix + "_report.html");
        } else {
            report_path = opts.report_html_path;
        }
        std::cout << "report=" << report_path.string() << "\n";
    }

    if (!opts.dna_export_path.empty()) {
        if (!export_dna_csv(opts.dna_export_path, dna_species, dna_global)) {
            std::cerr << "DNA-Export fehlgeschlagen: " << opts.dna_export_path << "\n";
            return 1;
        }
        std::cout << "dna_export=" << opts.dna_export_path << "\n";
    }

    std::cout << "done\n";
    return 0;
}
