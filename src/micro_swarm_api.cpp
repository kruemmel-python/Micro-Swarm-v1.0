#include "micro_swarm_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>

#include "compute/opencl_runtime.h"
#include "sim/agent.h"
#include "sim/db_engine.h"
#include "sim/dna_memory.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/io.h"
#include "sim/mycel.h"
#include "sim/params.h"
#include "sim/rng.h"
#include "sim/db_sql.h"

namespace {
struct MicroSwarmContext {
    SimParams params;
    EvoParams evo;
    float evo_min_energy_to_store = 1.6f;
    float global_spawn_frac = 0.15f;
    std::array<SpeciesProfile, 4> profiles;
    std::array<float, 4> species_fracs{0.40f, 0.25f, 0.20f, 0.15f};

    uint32_t seed = 42;
    int step_index = 0;
    bool paused = false;

    Rng rng;
    Environment env;
    GridField phero_food;
    GridField phero_danger;
    GridField phero_gamma;
    GridField molecules;
    MycelNetwork mycel;

    std::array<DNAMemory, 4> dna_species;
    DNAMemory dna_global;
    std::vector<Agent> agents;

    OpenCLRuntime ocl;
    bool ocl_active = false;
    bool ocl_no_copyback = false;
    int ocl_platform = 0;
    int ocl_device = 0;
    bool last_physics_valid = true;
    int logic_case = 0;
    int logic_active_case = 0;
    float logic_last_score = 0.5f;
    float logic_path_radius = 4.0f;

    explicit MicroSwarmContext(uint32_t seed_in)
        : seed(seed_in),
          rng(seed_in),
          env(0, 0),
          phero_food(0, 0, 0.0f),
          phero_danger(0, 0, 0.0f),
          phero_gamma(0, 0, 0.0f),
          molecules(0, 0, 0.0f),
          mycel(0, 0) {}
};

struct MicroSwarmDbContext {
    DbWorld world;
    std::vector<int> last_results;
    std::string last_error;
    DbSqlResult last_sql_result;
    bool last_sql_valid = false;
    std::vector<std::string> delta_entries;
    std::vector<std::string> tombstone_entries;
    bool delta_cache_valid = false;
};

int copy_string(char *dst, int dst_size, const std::string &value) {
    if (!dst || dst_size <= 0) return 0;
    if (value.empty()) {
        dst[0] = '\0';
        return 1;
    }
    int copy_len = std::min(dst_size - 1, static_cast<int>(value.size()));
    std::memcpy(dst, value.data(), static_cast<size_t>(copy_len));
    dst[copy_len] = '\0';
    return 1;
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

void apply_semantic_defaults(Genome &g, const SpeciesProfile &profile) {
    g.response_matrix[0] = clamp_range(profile.food_attraction_mul, -1.5f, 1.5f);
    g.response_matrix[1] = clamp_range(-profile.danger_aversion_mul, -1.5f, 1.5f);
    g.response_matrix[2] = 0.0f;
    g.emission_matrix[0] = clamp_range(profile.deposit_food_mul, -1.5f, 1.5f);
    g.emission_matrix[1] = 0.0f;
    g.emission_matrix[2] = 0.0f;
    g.emission_matrix[3] = clamp_range(profile.deposit_danger_mul, -1.5f, 1.5f);
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

void invalidate_delta_cache(MicroSwarmDbContext *ctx) {
    if (!ctx) return;
    ctx->delta_cache_valid = false;
}

void build_delta_cache(MicroSwarmDbContext *ctx) {
    if (!ctx) return;
    ctx->delta_entries.clear();
    ctx->tombstone_entries.clear();
    const DbWorld &world = ctx->world;
    for (const auto &pair : world.delta_index_by_key) {
        int idx = pair.second;
        if (idx < 0 || idx >= static_cast<int>(world.payloads.size())) continue;
        const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
        if (p.table_id < 0 || p.table_id >= static_cast<int>(world.table_names.size())) continue;
        std::string entry = "UPSERT table=" + world.table_names[static_cast<size_t>(p.table_id)] +
                            " id=" + std::to_string(p.id) +
                            " data=\"" + p.raw_data + "\"";
        ctx->delta_entries.push_back(entry);
    }
    for (const auto &key : world.tombstones) {
        int table_id = static_cast<int>(key >> 32);
        int id = static_cast<int>(key & 0xffffffff);
        std::string tname = (table_id >= 0 && table_id < static_cast<int>(world.table_names.size()))
                                ? world.table_names[static_cast<size_t>(table_id)]
                                : "unknown";
        ctx->tombstone_entries.push_back("DELETE table=" + tname + " id=" + std::to_string(id));
    }
    ctx->delta_cache_valid = true;
}

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

void clamp_genome(Genome &g) {
    g.sense_gain = std::min(3.0f, std::max(0.2f, g.sense_gain));
    g.pheromone_gain = std::min(3.0f, std::max(0.2f, g.pheromone_gain));
    g.exploration_bias = std::min(1.0f, std::max(0.0f, g.exploration_bias));
    g.response_matrix[0] = std::min(2.0f, std::max(-2.0f, g.response_matrix[0]));
    g.response_matrix[1] = std::min(2.0f, std::max(-2.0f, g.response_matrix[1]));
    g.response_matrix[2] = std::min(2.0f, std::max(-2.0f, g.response_matrix[2]));
    for (int i = 0; i < 4; ++i) {
        g.emission_matrix[i] = std::min(2.0f, std::max(-2.0f, g.emission_matrix[i]));
    }
    for (int i = 0; i < 4; ++i) {
        g.kernel_codons[i] = std::min(7, std::max(0, g.kernel_codons[i]));
    }
    g.lws_x = std::min(32, std::max(0, g.lws_x));
    g.lws_y = std::min(32, std::max(0, g.lws_y));
    g.toxic_stride = std::min(64, std::max(1, g.toxic_stride));
    g.toxic_iters = std::min(256, std::max(0, g.toxic_iters));
}

struct FieldStatsLocal {
    float min = 0.0f;
    float max = 0.0f;
    float mean = 0.0f;
    float p95 = 0.0f;
    float entropy = 0.0f;
    float norm_entropy = 0.0f;
};

FieldStatsLocal compute_entropy_stats(const std::vector<float> &values, int bins) {
    FieldStatsLocal stats;
    if (values.empty()) {
        return stats;
    }
    stats.min = values.front();
    stats.max = values.front();
    double sum = 0.0;
    for (float v : values) {
        stats.min = std::min(stats.min, v);
        stats.max = std::max(stats.max, v);
        sum += v;
    }
    stats.mean = static_cast<float>(sum / static_cast<double>(values.size()));

    std::vector<float> sorted(values.begin(), values.end());
    size_t idx = static_cast<size_t>(std::floor(0.95 * (sorted.size() - 1)));
    std::nth_element(sorted.begin(), sorted.begin() + idx, sorted.end());
    stats.p95 = sorted[idx];

    if (bins <= 1 || stats.max <= stats.min) {
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

GridField *select_field(MicroSwarmContext *ctx, ms_field_kind kind) {
    switch (kind) {
        case MS_FIELD_RESOURCES: return &ctx->env.resources;
        case MS_FIELD_PHEROMONE_FOOD: return &ctx->phero_food;
        case MS_FIELD_PHEROMONE_DANGER: return &ctx->phero_danger;
        case MS_FIELD_MOLECULES: return &ctx->molecules;
        case MS_FIELD_MYCEL: return &ctx->mycel.density;
        default: return nullptr;
    }
}

void init_agents(MicroSwarmContext *ctx) {
    ctx->agents.clear();
    ctx->agents.reserve(ctx->params.agent_count);
    const int codon_max = 7;
    const int lws_min = 0;
    const int lws_max = 32;
    const int toxic_stride_min = std::max(1, ctx->params.toxic_stride_min);
    const int toxic_stride_max = std::max(toxic_stride_min, ctx->params.toxic_stride_max);
    const int toxic_iters_min = std::max(0, ctx->params.toxic_iters_min);
    const int toxic_iters_max = std::max(toxic_iters_min, ctx->params.toxic_iters_max);
    const bool toxic_enabled = ctx->params.toxic_enable != 0;
    auto randomize_codons = [&](Genome &g) {
        for (int i = 0; i < 4; ++i) {
            g.kernel_codons[i] = ctx->rng.uniform_int(0, codon_max);
        }
        g.lws_x = ctx->rng.uniform_int(lws_min, lws_max);
        g.lws_y = ctx->rng.uniform_int(lws_min, lws_max);
        g.toxic_stride = ctx->rng.uniform_int(toxic_stride_min, toxic_stride_max);
        g.toxic_iters = ctx->rng.uniform_int(toxic_iters_min, toxic_iters_max);
        if (!toxic_enabled) {
            g.toxic_iters = 0;
        }
    };
    auto mutate_codons = [&](Genome &g, float prob) {
        if (prob <= 0.0f) return;
        for (int i = 0; i < 4; ++i) {
            if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
                g.kernel_codons[i] = ctx->rng.uniform_int(0, codon_max);
            }
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.lws_x = ctx->rng.uniform_int(lws_min, lws_max);
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.lws_y = ctx->rng.uniform_int(lws_min, lws_max);
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.toxic_stride = ctx->rng.uniform_int(toxic_stride_min, toxic_stride_max);
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.toxic_iters = ctx->rng.uniform_int(toxic_iters_min, toxic_iters_max);
        }
        if (!toxic_enabled) {
            g.toxic_iters = 0;
        }
    };
    auto random_genome = [&]() -> Genome {
        Genome g;
        g.sense_gain = ctx->rng.uniform(0.6f, 1.4f);
        g.pheromone_gain = ctx->rng.uniform(0.6f, 1.4f);
        g.exploration_bias = ctx->rng.uniform(0.2f, 0.8f);
        randomize_semantics(ctx->rng, g);
        randomize_codons(g);
        clamp_genome(g);
        return g;
    };
    auto apply_role_mutation = [&](Genome &g, const SpeciesProfile &profile) {
        float sigma = ctx->evo.mutation_sigma * profile.mutation_sigma_mul;
        float delta = ctx->evo.exploration_delta * profile.exploration_delta_mul;
        if (sigma > 0.0f) {
            g.sense_gain *= ctx->rng.uniform(1.0f - sigma, 1.0f + sigma);
            g.pheromone_gain *= ctx->rng.uniform(1.0f - sigma, 1.0f + sigma);
        }
        if (delta > 0.0f) {
            g.exploration_bias += ctx->rng.uniform(-delta, delta);
        }
        g.response_matrix[0] += gaussian(ctx->rng, sigma);
        g.response_matrix[1] += gaussian(ctx->rng, sigma);
        g.response_matrix[2] += gaussian(ctx->rng, sigma);
        for (int i = 0; i < 4; ++i) {
            g.emission_matrix[i] += gaussian(ctx->rng, sigma);
        }
        mutate_codons(g, std::min(0.5f, sigma * 2.0f));
        clamp_genome(g);
    };
    auto sample_genome = [&](int species) -> Genome {
        const SpeciesProfile &profile = ctx->profiles[species];
        bool use_dna = ctx->rng.uniform(0.0f, 1.0f) < profile.dna_binding;
        Genome g;
        if (use_dna) {
            if (ctx->evo.enabled && !ctx->dna_global.entries.empty() &&
                ctx->rng.uniform(0.0f, 1.0f) < ctx->global_spawn_frac) {
                g = ctx->dna_global.sample(ctx->rng, ctx->params, ctx->evo);
            } else {
                g = ctx->dna_species[species].sample(ctx->rng, ctx->params, ctx->evo);
            }
        } else {
            g = random_genome();
            apply_semantic_defaults(g, profile);
        }
        if (ctx->evo.enabled) {
            apply_role_mutation(g, profile);
        }
        return g;
    };

    for (int i = 0; i < ctx->params.agent_count; ++i) {
        Agent agent;
        agent.x = static_cast<float>(ctx->rng.uniform_int(0, ctx->params.width - 1));
        agent.y = static_cast<float>(ctx->rng.uniform_int(0, ctx->params.height - 1));
        agent.heading = ctx->rng.uniform(0.0f, 6.283185307f);
        agent.energy = ctx->rng.uniform(0.2f, 0.6f);
        agent.last_energy = agent.energy;
        agent.fitness_accum = 0.0f;
        agent.fitness_ticks = 0;
        agent.fitness_value = 0.0f;
        agent.species = pick_species(ctx->rng, ctx->species_fracs);
        agent.genome = sample_genome(agent.species);
        ctx->agents.push_back(agent);
    }
}

void init_fields(MicroSwarmContext *ctx) {
    ctx->env = Environment(ctx->params.width, ctx->params.height);
    ctx->env.seed_resources(ctx->rng);
    // phero_food/phero_danger act as semantic channels Alpha/Beta (kept names for compatibility).
    ctx->phero_food = GridField(ctx->params.width, ctx->params.height, 0.0f);
    ctx->phero_danger = GridField(ctx->params.width, ctx->params.height, 0.0f);
    ctx->phero_gamma = GridField(ctx->params.width, ctx->params.height, 0.0f);
    ctx->molecules = GridField(ctx->params.width, ctx->params.height, 0.0f);
    ctx->mycel = MycelNetwork(ctx->params.width, ctx->params.height);
    if (ctx->params.logic_input_ax < 0 || ctx->params.logic_input_ay < 0 ||
        ctx->params.logic_input_bx < 0 || ctx->params.logic_input_by < 0) {
        ctx->params.logic_input_ax = ctx->params.width / 4;
        ctx->params.logic_input_ay = ctx->params.height / 4;
        ctx->params.logic_input_bx = ctx->params.width / 4;
        ctx->params.logic_input_by = (ctx->params.height * 3) / 4;
    }
    if (ctx->params.logic_output_x < 0 || ctx->params.logic_output_y < 0) {
        ctx->params.logic_output_x = (ctx->params.width * 3) / 4;
        ctx->params.logic_output_y = ctx->params.height / 2;
    }
    ctx->logic_case = 0;
    ctx->logic_active_case = 0;
    ctx->logic_last_score = 0.5f;
    ctx->logic_path_radius = std::max(2.0f, std::min(ctx->params.width, ctx->params.height) * 0.05f);
}

bool ensure_host_fields(MicroSwarmContext *ctx) {
    if (ctx->ocl_active && ctx->ocl_no_copyback) {
        std::string error;
        if (!ctx->ocl.copyback(ctx->phero_food, ctx->phero_danger, ctx->phero_gamma, ctx->molecules, error)) {
            return false;
        }
    }
    return true;
}
void step_once(MicroSwarmContext *ctx) {
    if (ctx->paused) {
        return;
    }
    FieldParams pheromone_params{ctx->params.pheromone_evaporation, ctx->params.pheromone_diffusion};
    FieldParams molecule_params{ctx->params.molecule_evaporation, ctx->params.molecule_diffusion};
    auto field_sum = [](const GridField &field) -> double {
        double sum = 0.0;
        for (float v : field.data) {
            sum += static_cast<double>(v);
        }
        return sum;
    };
    auto compute_stagnation = [&]() -> float {
        if (!ctx->dna_global.entries.empty()) {
            return calculate_genetic_stagnation(ctx->dna_global.entries);
        }
        std::vector<DNAEntry> merged;
        for (const auto &pool : ctx->dna_species) {
            merged.insert(merged.end(), pool.entries.begin(), pool.entries.end());
        }
        if (merged.empty()) {
            return 1.0f;
        }
        return calculate_genetic_stagnation(merged);
    };
    auto inject_gamma = [&](float base, const float quad_ns[4]) {
        if (base > 0.0f) {
            for (float &v : ctx->phero_gamma.data) {
                v += base;
            }
        }
        int mid_x = ctx->params.width / 2;
        int mid_y = ctx->params.height / 2;
        struct Quad {
            int x0;
            int y0;
            int x1;
            int y1;
        };
        Quad quads[4] = {
            {0, 0, mid_x, mid_y},
            {mid_x, 0, ctx->params.width, mid_y},
            {0, mid_y, mid_x, ctx->params.height},
            {mid_x, mid_y, ctx->params.width, ctx->params.height}
        };
        const float scale = 1.0f / 1000000.0f;
        for (int q = 0; q < 4; ++q) {
            float v = clamp01(static_cast<float>(quad_ns[q]) * scale);
            if (v <= 0.0f) {
                continue;
            }
            for (int y = quads[q].y0; y < quads[q].y1; ++y) {
                for (int x = quads[q].x0; x < quads[q].x1; ++x) {
                    ctx->phero_gamma.at(x, y) += v;
                }
            }
        }
    };
    const int codon_max = 7;
    const int lws_min = 0;
    const int lws_max = 32;
    const int toxic_stride_min = std::max(1, ctx->params.toxic_stride_min);
    const int toxic_stride_max = std::max(toxic_stride_min, ctx->params.toxic_stride_max);
    const int toxic_iters_min = std::max(0, ctx->params.toxic_iters_min);
    const int toxic_iters_max = std::max(toxic_iters_min, ctx->params.toxic_iters_max);
    const bool toxic_enabled = ctx->params.toxic_enable != 0;
    auto randomize_codons = [&](Genome &g) {
        for (int i = 0; i < 4; ++i) {
            g.kernel_codons[i] = ctx->rng.uniform_int(0, codon_max);
        }
        g.lws_x = ctx->rng.uniform_int(lws_min, lws_max);
        g.lws_y = ctx->rng.uniform_int(lws_min, lws_max);
        g.toxic_stride = ctx->rng.uniform_int(toxic_stride_min, toxic_stride_max);
        g.toxic_iters = ctx->rng.uniform_int(toxic_iters_min, toxic_iters_max);
        if (!toxic_enabled) {
            g.toxic_iters = 0;
        }
    };
    auto mutate_codons = [&](Genome &g, float prob) {
        if (prob <= 0.0f) return;
        for (int i = 0; i < 4; ++i) {
            if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
                g.kernel_codons[i] = ctx->rng.uniform_int(0, codon_max);
            }
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.lws_x = ctx->rng.uniform_int(lws_min, lws_max);
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.lws_y = ctx->rng.uniform_int(lws_min, lws_max);
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.toxic_stride = ctx->rng.uniform_int(toxic_stride_min, toxic_stride_max);
        }
        if (ctx->rng.uniform(0.0f, 1.0f) < prob) {
            g.toxic_iters = ctx->rng.uniform_int(toxic_iters_min, toxic_iters_max);
        }
        if (!toxic_enabled) {
            g.toxic_iters = 0;
        }
    };
    auto random_genome = [&]() -> Genome {
        Genome g;
        g.sense_gain = ctx->rng.uniform(0.6f, 1.4f);
        g.pheromone_gain = ctx->rng.uniform(0.6f, 1.4f);
        g.exploration_bias = ctx->rng.uniform(0.2f, 0.8f);
        randomize_semantics(ctx->rng, g);
        randomize_codons(g);
        clamp_genome(g);
        return g;
    };
    auto apply_role_mutation = [&](Genome &g, const SpeciesProfile &profile) {
        float sigma = ctx->evo.mutation_sigma * profile.mutation_sigma_mul;
        float delta = ctx->evo.exploration_delta * profile.exploration_delta_mul;
        if (sigma > 0.0f) {
            g.sense_gain *= ctx->rng.uniform(1.0f - sigma, 1.0f + sigma);
            g.pheromone_gain *= ctx->rng.uniform(1.0f - sigma, 1.0f + sigma);
        }
        if (delta > 0.0f) {
            g.exploration_bias += ctx->rng.uniform(-delta, delta);
        }
        g.response_matrix[0] += gaussian(ctx->rng, sigma);
        g.response_matrix[1] += gaussian(ctx->rng, sigma);
        g.response_matrix[2] += gaussian(ctx->rng, sigma);
        for (int i = 0; i < 4; ++i) {
            g.emission_matrix[i] += gaussian(ctx->rng, sigma);
        }
        mutate_codons(g, std::min(0.5f, sigma * 2.0f));
        clamp_genome(g);
    };
    auto sample_genome = [&](int species) -> Genome {
        const SpeciesProfile &profile = ctx->profiles[species];
        bool use_dna = ctx->rng.uniform(0.0f, 1.0f) < profile.dna_binding;
        Genome g;
        if (use_dna) {
            if (ctx->evo.enabled && !ctx->dna_global.entries.empty() &&
                ctx->rng.uniform(0.0f, 1.0f) < ctx->global_spawn_frac) {
                g = ctx->dna_global.sample(ctx->rng, ctx->params, ctx->evo);
            } else {
                g = ctx->dna_species[species].sample(ctx->rng, ctx->params, ctx->evo);
            }
        } else {
            g = random_genome();
            apply_semantic_defaults(g, profile);
        }
        if (ctx->evo.enabled) {
            apply_role_mutation(g, profile);
        }
        return g;
    };
    auto sample_output = [&](const GridField &field) -> float {
        int x0 = std::max(0, ctx->params.logic_output_x - 1);
        int x1 = std::min(ctx->params.width - 1, ctx->params.logic_output_x + 1);
        int y0 = std::max(0, ctx->params.logic_output_y - 1);
        int y1 = std::min(ctx->params.height - 1, ctx->params.logic_output_y + 1);
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

    float quad_ns[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (ctx->ocl_active) {
        ctx->ocl.last_quadrant_exhaustion_ns(quad_ns);
    }
    float stagnation = compute_stagnation();
    inject_gamma(stagnation, quad_ns);

    if (ctx->params.logic_mode != 0 && (ctx->step_index % ctx->params.logic_pulse_period == 0)) {
        ctx->logic_active_case = ctx->logic_case;
        int a = (ctx->logic_active_case >> 0) & 1;
        int b = (ctx->logic_active_case >> 1) & 1;
        if (a) {
            ctx->phero_food.at(ctx->params.logic_input_ax, ctx->params.logic_input_ay) += ctx->params.logic_pulse_strength;
        }
        if (b) {
            ctx->phero_food.at(ctx->params.logic_input_bx, ctx->params.logic_input_by) += ctx->params.logic_pulse_strength;
        }
        ctx->logic_case = (ctx->logic_case + 1) & 3;
    }

    for (auto &agent : ctx->agents) {
        const SpeciesProfile &profile = ctx->profiles[agent.species];
        int fitness_window = (ctx->evo.enabled && ctx->params.logic_mode == 0) ? ctx->evo.fitness_window : 0;
        agent.step(ctx->rng,
                   ctx->params,
                   fitness_window,
                   profile,
                   ctx->phero_food,
                   ctx->phero_danger,
                   ctx->phero_gamma,
                   ctx->molecules,
                   ctx->env.resources,
                   ctx->mycel.density);
        if (ctx->evo.enabled && ctx->params.logic_mode != 0) {
            float dist_a = distance_to_segment(static_cast<float>(ctx->params.logic_input_ax),
                                               static_cast<float>(ctx->params.logic_input_ay),
                                               static_cast<float>(ctx->params.logic_output_x),
                                               static_cast<float>(ctx->params.logic_output_y),
                                               agent.x, agent.y);
            float dist_b = distance_to_segment(static_cast<float>(ctx->params.logic_input_bx),
                                               static_cast<float>(ctx->params.logic_input_by),
                                               static_cast<float>(ctx->params.logic_output_x),
                                               static_cast<float>(ctx->params.logic_output_y),
                                               agent.x, agent.y);
            float dist = std::min(dist_a, dist_b);
            float weight = 0.0f;
            if (dist <= ctx->logic_path_radius) {
                weight = 1.0f - (dist / ctx->logic_path_radius);
            }
            agent.fitness_value = ctx->logic_last_score * weight;
        }
        if (ctx->evo.enabled) {
            if (agent.energy > ctx->evo_min_energy_to_store) {
                float fitness = agent.fitness_value;
                if (ctx->ocl_active) {
                    float hw_penalty_ms = ctx->ocl.last_hardware_exhaustion_ns() / 1000000.0f;
                    fitness = agent.fitness_value / (hw_penalty_ms + 0.0001f);
                    if (!ctx->last_physics_valid) {
                        fitness *= 0.01f;
                    }
                }
                ctx->dna_species[agent.species].add(ctx->params, agent.genome, fitness, ctx->evo, ctx->params.dna_capacity);
                float eps = 1e-6f;
                if (ctx->params.dna_global_capacity > 0) {
                    if (ctx->dna_global.entries.size() < static_cast<size_t>(ctx->params.dna_global_capacity) ||
                        fitness > ctx->dna_global.entries.back().fitness + eps) {
                        ctx->dna_global.add(ctx->params, agent.genome, fitness, ctx->evo, ctx->params.dna_global_capacity);
                    }
                }
                agent.energy *= 0.6f;
            }
        } else {
            if (agent.energy > 1.2f) {
                ctx->dna_species[agent.species].add(ctx->params, agent.genome, agent.energy, ctx->evo, ctx->params.dna_capacity);
                agent.energy *= 0.6f;
            }
        }
    }

    if (ctx->ocl_active && ctx->evo.enabled) {
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
        int mid_x = ctx->params.width / 2;
        int mid_y = ctx->params.height / 2;
        for (const auto &agent : ctx->agents) {
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
                if (!ctx->dna_global.entries.empty()) {
                    picks[q].genome = ctx->dna_global.entries.front().genome;
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
        ctx->ocl.set_quadrant_lws(lws);

        if (ctx->step_index % 500 == 0) {
            for (int q = 0; q < 4; ++q) {
                int codons[4] = {
                    picks[q].genome.kernel_codons[0],
                    picks[q].genome.kernel_codons[1],
                    picks[q].genome.kernel_codons[2],
                    picks[q].genome.kernel_codons[3]
                };
                bool toxic_allowed = (ctx->params.toxic_enable != 0) && (ctx->params.toxic_max_fraction > 0.0f);
                float gate = ctx->params.toxic_max_fraction;
                if (ctx->params.toxic_max_fraction_by_quadrant[q] < gate) {
                    gate = ctx->params.toxic_max_fraction_by_quadrant[q];
                }
                if (ctx->params.toxic_max_fraction_by_species[picks[q].species] < gate) {
                    gate = ctx->params.toxic_max_fraction_by_species[picks[q].species];
                }
                int toxic_stride = std::min(ctx->params.toxic_stride_max, std::max(ctx->params.toxic_stride_min, picks[q].genome.toxic_stride));
                int toxic_iters = std::min(ctx->params.toxic_iters_max, std::max(ctx->params.toxic_iters_min, picks[q].genome.toxic_iters));
                if (!toxic_allowed) {
                    toxic_iters = 0;
                }
                if (is_toxic_extra(codons[2])) {
                    if (!toxic_allowed || ctx->rng.uniform(0.0f, 1.0f) > gate) {
                        codons[2] = 0;
                    }
                }
                std::string build_err;
                if (!ctx->ocl.assemble_evolved_kernel_quadrant(q,
                                                               codons,
                                                               toxic_stride,
                                                               toxic_iters,
                                                               build_err)) {
                    if (picks[q].from_global && !ctx->dna_global.entries.empty()) {
                        ctx->dna_global.entries.front().fitness *= 0.1f;
                        std::sort(ctx->dna_global.entries.begin(), ctx->dna_global.entries.end(), [](const DNAEntry &a, const DNAEntry &b) {
                            return a.fitness > b.fitness;
                        });
                    }
                }
            }
        }
    }

    bool cpu_diffused = false;
    if (ctx->ocl_active) {
        double pre_food_sum = field_sum(ctx->phero_food);
        double pre_danger_sum = field_sum(ctx->phero_danger);
        double pre_mol_sum = field_sum(ctx->molecules);
        std::string error;
        if (!ctx->ocl.upload_fields(ctx->phero_food, ctx->phero_danger, ctx->phero_gamma, ctx->molecules, error)) {
            ctx->ocl_active = false;
        }
        if (ctx->ocl_active) {
            bool do_copyback = !ctx->ocl_no_copyback;
            if (!ctx->ocl.step_diffuse(pheromone_params, molecule_params, do_copyback, ctx->phero_food, ctx->phero_danger, ctx->phero_gamma, ctx->molecules, error)) {
                ctx->ocl_active = false;
                diffuse_and_evaporate(ctx->phero_food, pheromone_params);
                diffuse_and_evaporate(ctx->phero_danger, pheromone_params);
                diffuse_and_evaporate(ctx->phero_gamma, pheromone_params);
                diffuse_and_evaporate(ctx->molecules, molecule_params);
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
                double post_food_sum = field_sum(ctx->phero_food);
                double post_danger_sum = field_sum(ctx->phero_danger);
                double post_mol_sum = field_sum(ctx->molecules);
                bool ok_food = valid_sum(pre_food_sum, post_food_sum, pheromone_params.evaporation);
                bool ok_danger = valid_sum(pre_danger_sum, post_danger_sum, pheromone_params.evaporation);
                bool ok_mol = valid_sum(pre_mol_sum, post_mol_sum, molecule_params.evaporation);
                ctx->last_physics_valid = ok_food && ok_danger && ok_mol;
            }
        }
    }
    if (!ctx->ocl_active && !cpu_diffused) {
        diffuse_and_evaporate(ctx->phero_food, pheromone_params);
        diffuse_and_evaporate(ctx->phero_danger, pheromone_params);
        diffuse_and_evaporate(ctx->phero_gamma, pheromone_params);
        diffuse_and_evaporate(ctx->molecules, molecule_params);
        ctx->last_physics_valid = true;
    }

    ctx->mycel.update(ctx->params, ctx->phero_food, ctx->env.resources);
    if (ctx->params.logic_mode != 0) {
        float measured = sample_output(ctx->mycel.density);
        int target = logic_target_for_case(ctx->params.logic_mode, ctx->logic_active_case);
        float score = 1.0f - std::abs(static_cast<float>(target) - clamp01(measured));
        ctx->logic_last_score = clamp01(score);
    }
    ctx->env.regenerate(ctx->params);
    for (auto &pool : ctx->dna_species) {
        pool.decay(ctx->evo);
    }
    ctx->dna_global.decay(ctx->evo);

    for (auto &agent : ctx->agents) {
        if (agent.energy <= 0.05f) {
            agent.x = static_cast<float>(ctx->rng.uniform_int(0, ctx->params.width - 1));
            agent.y = static_cast<float>(ctx->rng.uniform_int(0, ctx->params.height - 1));
            agent.heading = ctx->rng.uniform(0.0f, 6.283185307f);
            agent.energy = ctx->rng.uniform(0.2f, 0.5f);
            agent.last_energy = agent.energy;
            agent.fitness_accum = 0.0f;
            agent.fitness_ticks = 0;
            agent.fitness_value = 0.0f;
            agent.species = pick_species(ctx->rng, ctx->species_fracs);
            agent.genome = sample_genome(agent.species);
        }
    }
    ctx->step_index += 1;
}

void fill_params(ms_params_t &out, const SimParams &params, const EvoParams &evo, float evo_min_energy_to_store, float global_spawn_frac) {
    out.width = params.width;
    out.height = params.height;
    out.agent_count = params.agent_count;
    out.steps = params.steps;
    out.pheromone_evaporation = params.pheromone_evaporation;
    out.pheromone_diffusion = params.pheromone_diffusion;
    out.molecule_evaporation = params.molecule_evaporation;
    out.molecule_diffusion = params.molecule_diffusion;
    out.resource_regen = params.resource_regen;
    out.resource_max = params.resource_max;
    out.mycel_decay = params.mycel_decay;
    out.mycel_growth = params.mycel_growth;
    out.mycel_transport = params.mycel_transport;
    out.mycel_drive_threshold = params.mycel_drive_threshold;
    out.mycel_drive_p = params.mycel_drive_p;
    out.mycel_drive_r = params.mycel_drive_r;
    out.mycel_inhibitor_weight = params.mycel_inhibitor_weight;
    out.mycel_inhibitor_gain = params.mycel_inhibitor_gain;
    out.mycel_inhibitor_decay = params.mycel_inhibitor_decay;
    out.mycel_inhibitor_threshold = params.mycel_inhibitor_threshold;
    out.agent_move_cost = params.agent_move_cost;
    out.agent_harvest = params.agent_harvest;
    out.agent_deposit_scale = params.agent_deposit_scale;
    out.agent_sense_radius = params.agent_sense_radius;
    out.agent_random_turn = params.agent_random_turn;
    out.info_metabolism_cost = params.info_metabolism_cost;
    out.dna_capacity = params.dna_capacity;
    out.dna_global_capacity = params.dna_global_capacity;
    out.dna_survival_bias = params.dna_survival_bias;
    out.phero_food_deposit_scale = params.phero_food_deposit_scale;
    out.phero_danger_deposit_scale = params.phero_danger_deposit_scale;
    out.danger_delta_threshold = params.danger_delta_threshold;
    out.danger_bounce_deposit = params.danger_bounce_deposit;
    out.evo_enable = evo.enabled ? 1 : 0;
    out.evo_elite_frac = evo.elite_frac;
    out.evo_min_energy_to_store = evo_min_energy_to_store;
    out.evo_mutation_sigma = evo.mutation_sigma;
    out.evo_exploration_delta = evo.exploration_delta;
    out.evo_fitness_window = evo.fitness_window;
    out.evo_age_decay = evo.age_decay;
    out.global_spawn_frac = global_spawn_frac;
    out.toxic_enable = params.toxic_enable;
    out.toxic_max_fraction = params.toxic_max_fraction;
    out.toxic_stride_min = params.toxic_stride_min;
    out.toxic_stride_max = params.toxic_stride_max;
    out.toxic_iters_min = params.toxic_iters_min;
    out.toxic_iters_max = params.toxic_iters_max;
    for (int i = 0; i < 4; ++i) {
        out.toxic_max_fraction_by_quadrant[i] = params.toxic_max_fraction_by_quadrant[i];
        out.toxic_max_fraction_by_species[i] = params.toxic_max_fraction_by_species[i];
    }
    out.logic_mode = params.logic_mode;
    out.logic_input_ax = params.logic_input_ax;
    out.logic_input_ay = params.logic_input_ay;
    out.logic_input_bx = params.logic_input_bx;
    out.logic_input_by = params.logic_input_by;
    out.logic_output_x = params.logic_output_x;
    out.logic_output_y = params.logic_output_y;
    out.logic_pulse_period = params.logic_pulse_period;
    out.logic_pulse_strength = params.logic_pulse_strength;
}

void set_params_from_api(MicroSwarmContext *ctx, const ms_params_t &p) {
    ctx->params.width = p.width;
    ctx->params.height = p.height;
    ctx->params.agent_count = p.agent_count;
    ctx->params.steps = p.steps;
    ctx->params.pheromone_evaporation = p.pheromone_evaporation;
    ctx->params.pheromone_diffusion = p.pheromone_diffusion;
    ctx->params.molecule_evaporation = p.molecule_evaporation;
    ctx->params.molecule_diffusion = p.molecule_diffusion;
    ctx->params.resource_regen = p.resource_regen;
    ctx->params.resource_max = p.resource_max;
    ctx->params.mycel_decay = p.mycel_decay;
    ctx->params.mycel_growth = p.mycel_growth;
    ctx->params.mycel_transport = p.mycel_transport;
    ctx->params.mycel_drive_threshold = p.mycel_drive_threshold;
    ctx->params.mycel_drive_p = p.mycel_drive_p;
    ctx->params.mycel_drive_r = p.mycel_drive_r;
    ctx->params.mycel_inhibitor_weight = p.mycel_inhibitor_weight;
    ctx->params.mycel_inhibitor_gain = p.mycel_inhibitor_gain;
    ctx->params.mycel_inhibitor_decay = p.mycel_inhibitor_decay;
    ctx->params.mycel_inhibitor_threshold = p.mycel_inhibitor_threshold;
    ctx->params.agent_move_cost = p.agent_move_cost;
    ctx->params.agent_harvest = p.agent_harvest;
    ctx->params.agent_deposit_scale = p.agent_deposit_scale;
    ctx->params.agent_sense_radius = p.agent_sense_radius;
    ctx->params.agent_random_turn = p.agent_random_turn;
    ctx->params.info_metabolism_cost = p.info_metabolism_cost;
    ctx->params.dna_capacity = p.dna_capacity;
    ctx->params.dna_global_capacity = p.dna_global_capacity;
    ctx->params.dna_survival_bias = p.dna_survival_bias;
    ctx->params.phero_food_deposit_scale = p.phero_food_deposit_scale;
    ctx->params.phero_danger_deposit_scale = p.phero_danger_deposit_scale;
    ctx->params.danger_delta_threshold = p.danger_delta_threshold;
    ctx->params.danger_bounce_deposit = p.danger_bounce_deposit;

    ctx->evo.enabled = p.evo_enable != 0;
    ctx->evo.elite_frac = p.evo_elite_frac;
    ctx->evo.mutation_sigma = p.evo_mutation_sigma;
    ctx->evo.exploration_delta = p.evo_exploration_delta;
    ctx->evo.fitness_window = p.evo_fitness_window;
    ctx->evo.age_decay = p.evo_age_decay;
    ctx->evo_min_energy_to_store = p.evo_min_energy_to_store;
    ctx->global_spawn_frac = p.global_spawn_frac;
    ctx->params.toxic_enable = p.toxic_enable;
    ctx->params.toxic_max_fraction = p.toxic_max_fraction;
    ctx->params.toxic_stride_min = p.toxic_stride_min;
    ctx->params.toxic_stride_max = p.toxic_stride_max;
    ctx->params.toxic_iters_min = p.toxic_iters_min;
    ctx->params.toxic_iters_max = p.toxic_iters_max;
    for (int i = 0; i < 4; ++i) {
        ctx->params.toxic_max_fraction_by_quadrant[i] = p.toxic_max_fraction_by_quadrant[i];
        ctx->params.toxic_max_fraction_by_species[i] = p.toxic_max_fraction_by_species[i];
    }
    ctx->params.logic_mode = p.logic_mode;
    ctx->params.logic_input_ax = p.logic_input_ax;
    ctx->params.logic_input_ay = p.logic_input_ay;
    ctx->params.logic_input_bx = p.logic_input_bx;
    ctx->params.logic_input_by = p.logic_input_by;
    ctx->params.logic_output_x = p.logic_output_x;
    ctx->params.logic_output_y = p.logic_output_y;
    ctx->params.logic_pulse_period = p.logic_pulse_period;
    ctx->params.logic_pulse_strength = p.logic_pulse_strength;

    if (ctx->params.toxic_stride_min <= 0) ctx->params.toxic_stride_min = 1;
    if (ctx->params.toxic_stride_max < ctx->params.toxic_stride_min) ctx->params.toxic_stride_max = ctx->params.toxic_stride_min;
    if (ctx->params.toxic_iters_min < 0) ctx->params.toxic_iters_min = 0;
    if (ctx->params.toxic_iters_max < ctx->params.toxic_iters_min) ctx->params.toxic_iters_max = ctx->params.toxic_iters_min;
    if (ctx->params.toxic_max_fraction < 0.0f) ctx->params.toxic_max_fraction = 0.0f;
    if (ctx->params.toxic_max_fraction > 1.0f) ctx->params.toxic_max_fraction = 1.0f;
    if (ctx->params.info_metabolism_cost < 0.0f) ctx->params.info_metabolism_cost = 0.0f;
    for (int i = 0; i < 4; ++i) {
        if (ctx->params.toxic_max_fraction_by_quadrant[i] < 0.0f) ctx->params.toxic_max_fraction_by_quadrant[i] = 0.0f;
        if (ctx->params.toxic_max_fraction_by_quadrant[i] > 1.0f) ctx->params.toxic_max_fraction_by_quadrant[i] = 1.0f;
        if (ctx->params.toxic_max_fraction_by_species[i] < 0.0f) ctx->params.toxic_max_fraction_by_species[i] = 0.0f;
        if (ctx->params.toxic_max_fraction_by_species[i] > 1.0f) ctx->params.toxic_max_fraction_by_species[i] = 1.0f;
    }
    if (ctx->params.logic_mode < 0) ctx->params.logic_mode = 0;
    if (ctx->params.logic_mode > 3) ctx->params.logic_mode = 3;
    if (ctx->params.logic_pulse_period <= 0) ctx->params.logic_pulse_period = 20;
    if (ctx->params.logic_pulse_strength < 0.0f) ctx->params.logic_pulse_strength = 0.0f;
    if (ctx->params.width > 0 && ctx->params.height > 0) {
        auto clamp_coord = [](int v, int max) -> int {
            if (v < 0) return v;
            if (v >= max) return max - 1;
            return v;
        };
        ctx->params.logic_input_ax = clamp_coord(ctx->params.logic_input_ax, ctx->params.width);
        ctx->params.logic_input_ay = clamp_coord(ctx->params.logic_input_ay, ctx->params.height);
        ctx->params.logic_input_bx = clamp_coord(ctx->params.logic_input_bx, ctx->params.width);
        ctx->params.logic_input_by = clamp_coord(ctx->params.logic_input_by, ctx->params.height);
        ctx->params.logic_output_x = clamp_coord(ctx->params.logic_output_x, ctx->params.width);
        ctx->params.logic_output_y = clamp_coord(ctx->params.logic_output_y, ctx->params.height);
    }
}

} // namespace

extern "C" {
ms_handle_t *ms_create(const ms_config_t *cfg) {
    uint32_t seed = 42;
    if (cfg) {
        seed = cfg->seed;
    }
    auto *ctx = new MicroSwarmContext(seed);
    ctx->profiles = default_species_profiles();
    SimParams params;
    EvoParams evo;
    if (cfg) {
        set_params_from_api(ctx, cfg->params);
    } else {
        ctx->params = params;
        ctx->evo = evo;
        ctx->global_spawn_frac = 0.15f;
    }
    init_fields(ctx);
    init_agents(ctx);
    return reinterpret_cast<ms_handle_t *>(ctx);
}

void ms_destroy(ms_handle_t *h) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    delete ctx;
}

ms_handle_t *ms_clone(const ms_handle_t *src) {
    if (!src) return nullptr;
    auto *ctx = reinterpret_cast<const MicroSwarmContext *>(src);
    auto *copy = new MicroSwarmContext(ctx->seed);
    *copy = *ctx;
    return reinterpret_cast<ms_handle_t *>(copy);
}

void ms_reset(ms_handle_t *h, uint32_t seed) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    ctx->seed = seed;
    ctx->rng = Rng(seed);
    ctx->step_index = 0;
    for (auto &pool : ctx->dna_species) pool.entries.clear();
    ctx->dna_global.entries.clear();
    init_fields(ctx);
    init_agents(ctx);
}

int ms_step(ms_handle_t *h, int steps) {
    if (!h || steps <= 0) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    for (int i = 0; i < steps; ++i) {
        step_once(ctx);
    }
    return steps;
}

int ms_run(ms_handle_t *h, int steps) {
    return ms_step(h, steps);
}

void ms_pause(ms_handle_t *h) {
    if (!h) return;
    reinterpret_cast<MicroSwarmContext *>(h)->paused = true;
}

void ms_resume(ms_handle_t *h) {
    if (!h) return;
    reinterpret_cast<MicroSwarmContext *>(h)->paused = false;
}

int ms_get_step_index(ms_handle_t *h) {
    if (!h) return 0;
    return reinterpret_cast<MicroSwarmContext *>(h)->step_index;
}

void ms_set_params(ms_handle_t *h, const ms_params_t *p) {
    if (!h || !p) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    set_params_from_api(ctx, *p);
    init_fields(ctx);
    init_agents(ctx);
}

void ms_get_params(ms_handle_t *h, ms_params_t *out) {
    if (!h || !out) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    fill_params(*out, ctx->params, ctx->evo, ctx->evo_min_energy_to_store, ctx->global_spawn_frac);
}

void ms_set_species_profiles(ms_handle_t *h, const ms_species_profile_t profiles[4]) {
    if (!h || !profiles) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    for (int i = 0; i < 4; ++i) {
        ctx->profiles[i].exploration_mul = profiles[i].exploration_mul;
        ctx->profiles[i].food_attraction_mul = profiles[i].food_attraction_mul;
        ctx->profiles[i].danger_aversion_mul = profiles[i].danger_aversion_mul;
        ctx->profiles[i].deposit_food_mul = profiles[i].deposit_food_mul;
        ctx->profiles[i].deposit_danger_mul = profiles[i].deposit_danger_mul;
        ctx->profiles[i].resource_weight_mul = profiles[i].resource_weight_mul;
        ctx->profiles[i].molecule_weight_mul = profiles[i].molecule_weight_mul;
        ctx->profiles[i].mycel_attraction_mul = profiles[i].mycel_attraction_mul;
        ctx->profiles[i].novelty_weight = profiles[i].novelty_weight;
        ctx->profiles[i].mutation_sigma_mul = profiles[i].mutation_sigma_mul;
        ctx->profiles[i].exploration_delta_mul = profiles[i].exploration_delta_mul;
        ctx->profiles[i].dna_binding = profiles[i].dna_binding;
        ctx->profiles[i].over_density_threshold = profiles[i].over_density_threshold;
        ctx->profiles[i].counter_deposit_mul = profiles[i].counter_deposit_mul;
    }
}

void ms_get_species_profiles(ms_handle_t *h, ms_species_profile_t out[4]) {
    if (!h || !out) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    for (int i = 0; i < 4; ++i) {
        out[i].exploration_mul = ctx->profiles[i].exploration_mul;
        out[i].food_attraction_mul = ctx->profiles[i].food_attraction_mul;
        out[i].danger_aversion_mul = ctx->profiles[i].danger_aversion_mul;
        out[i].deposit_food_mul = ctx->profiles[i].deposit_food_mul;
        out[i].deposit_danger_mul = ctx->profiles[i].deposit_danger_mul;
        out[i].resource_weight_mul = ctx->profiles[i].resource_weight_mul;
        out[i].molecule_weight_mul = ctx->profiles[i].molecule_weight_mul;
        out[i].mycel_attraction_mul = ctx->profiles[i].mycel_attraction_mul;
        out[i].novelty_weight = ctx->profiles[i].novelty_weight;
        out[i].mutation_sigma_mul = ctx->profiles[i].mutation_sigma_mul;
        out[i].exploration_delta_mul = ctx->profiles[i].exploration_delta_mul;
        out[i].dna_binding = ctx->profiles[i].dna_binding;
        out[i].over_density_threshold = ctx->profiles[i].over_density_threshold;
        out[i].counter_deposit_mul = ctx->profiles[i].counter_deposit_mul;
    }
}

void ms_set_species_fracs(ms_handle_t *h, const float fracs[4]) {
    if (!h || !fracs) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    for (int i = 0; i < 4; ++i) {
        ctx->species_fracs[i] = fracs[i];
    }
}

void ms_get_species_fracs(ms_handle_t *h, float out[4]) {
    if (!h || !out) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    for (int i = 0; i < 4; ++i) {
        out[i] = ctx->species_fracs[i];
    }
}

void ms_get_field_info(ms_handle_t *h, ms_field_kind kind, int *w, int *hgt) {
    if (!h || !w || !hgt) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    GridField *field = select_field(ctx, kind);
    if (!field) {
        *w = 0;
        *hgt = 0;
        return;
    }
    *w = field->width;
    *hgt = field->height;
}
int ms_copy_field_out(ms_handle_t *h, ms_field_kind kind, float *dst, int dst_count) {
    if (!h || !dst) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    if (!ensure_host_fields(ctx)) return 0;
    GridField *field = select_field(ctx, kind);
    if (!field) return 0;
    int count = field->width * field->height;
    if (dst_count < count) return 0;
    std::copy(field->data.begin(), field->data.end(), dst);
    return count;
}

int ms_copy_field_in(ms_handle_t *h, ms_field_kind kind, const float *src, int src_count) {
    if (!h || !src) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    GridField *field = select_field(ctx, kind);
    if (!field) return 0;
    int count = field->width * field->height;
    if (src_count < count) return 0;
    std::copy(src, src + count, field->data.begin());
    if (ctx->ocl_active) {
        std::string error;
        ctx->ocl.upload_fields(ctx->phero_food, ctx->phero_danger, ctx->phero_gamma, ctx->molecules, error);
    }
    return count;
}

void ms_clear_field(ms_handle_t *h, ms_field_kind kind, float value) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    GridField *field = select_field(ctx, kind);
    if (!field) return;
    field->fill(value);
    if (ctx->ocl_active) {
        std::string error;
        ctx->ocl.upload_fields(ctx->phero_food, ctx->phero_danger, ctx->phero_gamma, ctx->molecules, error);
    }
}

int ms_load_field_csv(ms_handle_t *h, ms_field_kind kind, const char *path) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    GridData data;
    std::string error;
    if (!load_grid_csv(path, data, error)) {
        return 0;
    }
    GridField *field = select_field(ctx, kind);
    if (!field) return 0;
    if (data.width != field->width || data.height != field->height) {
        return 0;
    }
    field->data = data.values;
    if (ctx->ocl_active) {
        ctx->ocl.upload_fields(ctx->phero_food, ctx->phero_danger, ctx->phero_gamma, ctx->molecules, error);
    }
    return 1;
}

int ms_save_field_csv(ms_handle_t *h, ms_field_kind kind, const char *path) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    if (!ensure_host_fields(ctx)) return 0;
    GridField *field = select_field(ctx, kind);
    if (!field) return 0;
    std::string error;
    if (!save_grid_csv(path, field->width, field->height, field->data, error)) {
        return 0;
    }
    return 1;
}

int ms_get_agent_count(ms_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    return static_cast<int>(ctx->agents.size());
}

int ms_get_agents(ms_handle_t *h, ms_agent_t *out, int max_agents) {
    if (!h || !out || max_agents <= 0) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    int count = std::min(max_agents, static_cast<int>(ctx->agents.size()));
    for (int i = 0; i < count; ++i) {
        const auto &a = ctx->agents[i];
        out[i].x = a.x;
        out[i].y = a.y;
        out[i].heading = a.heading;
        out[i].energy = a.energy;
        out[i].species = a.species;
        out[i].sense_gain = a.genome.sense_gain;
        out[i].pheromone_gain = a.genome.pheromone_gain;
        out[i].exploration_bias = a.genome.exploration_bias;
    }
    return count;
}

void ms_set_agents(ms_handle_t *h, const ms_agent_t *agents, int count) {
    if (!h || !agents || count <= 0) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    ctx->agents.clear();
    ctx->agents.reserve(count);
    for (int i = 0; i < count; ++i) {
        Agent a;
        a.x = agents[i].x;
        a.y = agents[i].y;
        a.heading = agents[i].heading;
        a.energy = agents[i].energy;
        a.last_energy = agents[i].energy;
        a.fitness_accum = 0.0f;
        a.fitness_ticks = 0;
        a.fitness_value = 0.0f;
        a.species = agents[i].species;
        a.genome.sense_gain = agents[i].sense_gain;
        a.genome.pheromone_gain = agents[i].pheromone_gain;
        a.genome.exploration_bias = agents[i].exploration_bias;
        clamp_genome(a.genome);
        ctx->agents.push_back(a);
    }
    ctx->params.agent_count = static_cast<int>(ctx->agents.size());
}

void ms_kill_agent(ms_handle_t *h, int agent_id) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    if (agent_id < 0 || agent_id >= static_cast<int>(ctx->agents.size())) return;
    ctx->agents[agent_id].energy = 0.0f;
}

void ms_spawn_agent(ms_handle_t *h, const ms_agent_t *agent) {
    if (!h || !agent) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    Agent a;
    a.x = agent->x;
    a.y = agent->y;
    a.heading = agent->heading;
    a.energy = agent->energy;
    a.last_energy = agent->energy;
    a.fitness_accum = 0.0f;
    a.fitness_ticks = 0;
    a.fitness_value = 0.0f;
    a.species = agent->species;
    a.genome.sense_gain = agent->sense_gain;
    a.genome.pheromone_gain = agent->pheromone_gain;
    a.genome.exploration_bias = agent->exploration_bias;
    clamp_genome(a.genome);
    ctx->agents.push_back(a);
    ctx->params.agent_count = static_cast<int>(ctx->agents.size());
}
void ms_get_dna_sizes(ms_handle_t *h, int out_species[4], int *out_global) {
    if (!h || !out_species || !out_global) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    for (int i = 0; i < 4; ++i) {
        out_species[i] = static_cast<int>(ctx->dna_species[i].entries.size());
    }
    *out_global = static_cast<int>(ctx->dna_global.entries.size());
}

void ms_get_dna_capacity(ms_handle_t *h, int *species_cap, int *global_cap) {
    if (!h || !species_cap || !global_cap) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    *species_cap = ctx->params.dna_capacity;
    *global_cap = ctx->params.dna_global_capacity;
}

void ms_set_dna_capacity(ms_handle_t *h, int species_cap, int global_cap) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    ctx->params.dna_capacity = species_cap;
    ctx->params.dna_global_capacity = global_cap;
    for (auto &pool : ctx->dna_species) {
        if (static_cast<int>(pool.entries.size()) > species_cap) {
            pool.entries.resize(species_cap);
        }
    }
    if (static_cast<int>(ctx->dna_global.entries.size()) > global_cap) {
        ctx->dna_global.entries.resize(global_cap);
    }
}

void ms_clear_dna_pools(ms_handle_t *h) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    for (auto &pool : ctx->dna_species) {
        pool.entries.clear();
    }
    ctx->dna_global.entries.clear();
}

int ms_export_dna_csv(ms_handle_t *h, const char *path) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    std::ofstream out(path);
    if (!out.is_open()) return 0;
    out << "pool,species,fitness,sense_gain,pheromone_gain,exploration_bias,"
           "response0,response1,response2,emit0,emit1,emit2,emit3,"
           "codon0,codon1,codon2,codon3,lws_x,lws_y,toxic_stride,toxic_iters\n";
    for (int s = 0; s < 4; ++s) {
        for (const auto &e : ctx->dna_species[s].entries) {
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
    for (const auto &e : ctx->dna_global.entries) {
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
    return 1;
}

int ms_import_dna_csv(ms_handle_t *h, const char *path) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    std::ifstream in(path);
    if (!in.is_open()) return 0;
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::vector<std::string> fields;
        std::string token;
        while (std::getline(ss, token, ',')) {
            fields.push_back(token);
        }
        if (fields.size() < 14) continue;
        std::string pool = fields[0];
        int species = std::stoi(fields[1]);
        float fitness = std::stof(fields[2]);
        Genome g;
        g.sense_gain = std::stof(fields[3]);
        g.pheromone_gain = std::stof(fields[4]);
        g.exploration_bias = std::stof(fields[5]);
        size_t idx = 6;
        if (fields.size() >= 21) {
            g.response_matrix[0] = std::stof(fields[idx++]);
            g.response_matrix[1] = std::stof(fields[idx++]);
            g.response_matrix[2] = std::stof(fields[idx++]);
            g.emission_matrix[0] = std::stof(fields[idx++]);
            g.emission_matrix[1] = std::stof(fields[idx++]);
            g.emission_matrix[2] = std::stof(fields[idx++]);
            g.emission_matrix[3] = std::stof(fields[idx++]);
        } else {
            g.response_matrix[0] = 1.0f;
            g.response_matrix[1] = -1.0f;
            g.response_matrix[2] = 0.0f;
            g.emission_matrix[0] = 1.0f;
            g.emission_matrix[1] = 0.0f;
            g.emission_matrix[2] = 0.0f;
            g.emission_matrix[3] = 1.0f;
        }
        if (fields.size() >= idx + 8) {
            g.kernel_codons[0] = std::stoi(fields[idx++]);
            g.kernel_codons[1] = std::stoi(fields[idx++]);
            g.kernel_codons[2] = std::stoi(fields[idx++]);
            g.kernel_codons[3] = std::stoi(fields[idx++]);
            g.lws_x = std::stoi(fields[idx++]);
            g.lws_y = std::stoi(fields[idx++]);
            g.toxic_stride = std::stoi(fields[idx++]);
            g.toxic_iters = std::stoi(fields[idx++]);
        } else {
            g.kernel_codons[0] = 0;
            g.kernel_codons[1] = 0;
            g.kernel_codons[2] = 0;
            g.kernel_codons[3] = 0;
            g.lws_x = 0;
            g.lws_y = 0;
            g.toxic_stride = 1;
            g.toxic_iters = 0;
        }
        clamp_genome(g);
        if (pool == "global") {
            ctx->dna_global.add(ctx->params, g, fitness, ctx->evo, ctx->params.dna_global_capacity);
        } else {
            if (species >= 0 && species < 4) {
                ctx->dna_species[species].add(ctx->params, g, fitness, ctx->evo, ctx->params.dna_capacity);
            }
        }
    }
    return 1;
}

void ms_get_system_metrics(ms_handle_t *h, ms_metrics_t *out) {
    if (!h || !out) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    out->step_index = ctx->step_index;
    out->dna_global_size = static_cast<int>(ctx->dna_global.entries.size());
    float avg_energy = 0.0f;
    std::array<float, 4> sums{0.0f, 0.0f, 0.0f, 0.0f};
    std::array<int, 4> counts{0, 0, 0, 0};
    for (const auto &a : ctx->agents) {
        avg_energy += a.energy;
        if (a.species >= 0 && a.species < 4) {
            sums[a.species] += a.energy;
            counts[a.species] += 1;
        }
    }
    avg_energy = ctx->agents.empty() ? 0.0f : avg_energy / static_cast<float>(ctx->agents.size());
    out->avg_energy = avg_energy;
    for (int i = 0; i < 4; ++i) {
        out->dna_species_sizes[i] = static_cast<int>(ctx->dna_species[i].entries.size());
        out->avg_energy_by_species[i] = counts[i] > 0 ? sums[i] / static_cast<float>(counts[i]) : 0.0f;
    }
}

void ms_get_energy_stats(ms_handle_t *h, float *avg, float *min, float *max) {
    if (!h || !avg || !min || !max) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    if (ctx->agents.empty()) {
        *avg = 0.0f;
        *min = 0.0f;
        *max = 0.0f;
        return;
    }
    float sum = 0.0f;
    float minv = ctx->agents.front().energy;
    float maxv = ctx->agents.front().energy;
    for (const auto &a : ctx->agents) {
        sum += a.energy;
        minv = std::min(minv, a.energy);
        maxv = std::max(maxv, a.energy);
    }
    *avg = sum / static_cast<float>(ctx->agents.size());
    *min = minv;
    *max = maxv;
}

void ms_get_energy_by_species(ms_handle_t *h, float out[4]) {
    if (!h || !out) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    std::array<float, 4> sums{0.0f, 0.0f, 0.0f, 0.0f};
    std::array<int, 4> counts{0, 0, 0, 0};
    for (const auto &a : ctx->agents) {
        if (a.species >= 0 && a.species < 4) {
            sums[a.species] += a.energy;
            counts[a.species] += 1;
        }
    }
    for (int i = 0; i < 4; ++i) {
        out[i] = counts[i] > 0 ? sums[i] / static_cast<float>(counts[i]) : 0.0f;
    }
}

void ms_get_entropy_metrics(ms_handle_t *h, ms_entropy_t *out) {
    if (!h || !out) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    if (!ensure_host_fields(ctx)) return;
    const int bins = 64;
    std::array<GridField *, 5> fields = {
        &ctx->env.resources,
        &ctx->phero_food,
        &ctx->phero_danger,
        &ctx->molecules,
        &ctx->mycel.density
    };
    for (int i = 0; i < 5; ++i) {
        FieldStatsLocal stats = compute_entropy_stats(fields[i]->data, bins);
        out->entropy[i] = stats.entropy;
        out->norm_entropy[i] = stats.norm_entropy;
        out->p95[i] = stats.p95;
    }
}

void ms_get_mycel_stats(ms_handle_t *h, ms_mycel_stats_t *out) {
    if (!h || !out) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    const auto &values = ctx->mycel.density.data;
    if (values.empty()) {
        out->min_val = 0.0f;
        out->max_val = 0.0f;
        out->mean = 0.0f;
        return;
    }
    float minv = values.front();
    float maxv = values.front();
    double sum = 0.0;
    for (float v : values) {
        minv = std::min(minv, v);
        maxv = std::max(maxv, v);
        sum += v;
    }
    out->min_val = minv;
    out->max_val = maxv;
    out->mean = static_cast<float>(sum / static_cast<double>(values.size()));
}

void ms_ocl_enable(ms_handle_t *h, int enable) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    if (!enable) {
        ctx->ocl_active = false;
        return;
    }
    std::string error;
    if (!ctx->ocl.init(ctx->ocl_platform, ctx->ocl_device, error)) {
        ctx->ocl_active = false;
        return;
    }
    if (!ctx->ocl.build_kernels(error)) {
        ctx->ocl_active = false;
        return;
    }
    if (!ctx->ocl.init_fields(ctx->phero_food, ctx->phero_danger, ctx->phero_gamma, ctx->molecules, error)) {
        ctx->ocl_active = false;
        return;
    }
    ctx->ocl_active = true;
}

void ms_ocl_select_device(ms_handle_t *h, int platform, int device) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    ctx->ocl_platform = platform;
    ctx->ocl_device = device;
}

void ms_ocl_print_devices(void) {
    std::string output;
    std::string error;
    if (OpenCLRuntime::print_devices(output, error)) {
        std::cout << output;
    } else {
        std::cerr << "[OpenCL] " << error << "\n";
    }
}

void ms_ocl_set_no_copyback(ms_handle_t *h, int enable) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    if (enable && ctx->params.agent_count > 0) {
        ctx->ocl_no_copyback = false;
    } else {
        ctx->ocl_no_copyback = enable != 0;
    }
}

int ms_is_gpu_active(ms_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmContext *>(h);
    return ctx->ocl_active ? 1 : 0;
}

void ms_get_api_version(int *major, int *minor, int *patch) {
    if (major) *major = MS_API_VERSION_MAJOR;
    if (minor) *minor = MS_API_VERSION_MINOR;
    if (patch) *patch = MS_API_VERSION_PATCH;
}

ms_db_handle_t *ms_db_create(void) {
    auto *ctx = new MicroSwarmDbContext();
    return reinterpret_cast<ms_db_handle_t *>(ctx);
}

void ms_db_destroy(ms_db_handle_t *h) {
    if (!h) return;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    delete ctx;
}

const char *ms_db_get_last_error(ms_db_handle_t *h) {
    if (!h) return "";
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    return ctx->last_error.c_str();
}

int ms_db_load_sql(ms_db_handle_t *h, const char *path) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    ctx->world = DbWorld{};
    if (!db_load_sql(path, ctx->world, ctx->last_error)) {
        return 0;
    }
    invalidate_delta_cache(ctx);
    return 1;
}

int ms_db_run_ingest(ms_db_handle_t *h, int width, int height, int agents, int steps, uint32_t seed) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    if (width <= 0 || height <= 0) {
        ctx->last_error = "Ungueltige Rastergroesse";
        return 0;
    }
    ctx->world.width = width;
    ctx->world.height = height;
    DbIngestConfig cfg;
    cfg.agent_count = agents;
    cfg.steps = steps;
    cfg.seed = seed;
    if (!db_run_ingest(ctx->world, cfg, ctx->last_error)) {
        return 0;
    }
    invalidate_delta_cache(ctx);
    return 1;
}

int ms_db_save_myco(ms_db_handle_t *h, const char *path) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    if (!db_save_myco(path, ctx->world, ctx->last_error)) {
        return 0;
    }
    return 1;
}

int ms_db_load_myco(ms_db_handle_t *h, const char *path) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    if (!db_load_myco(path, ctx->world, ctx->last_error)) {
        return 0;
    }
    invalidate_delta_cache(ctx);
    return 1;
}

int ms_db_save_cluster_ppm(ms_db_handle_t *h, const char *path, int scale) {
    if (!h || !path) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    if (!db_save_cluster_ppm(path, ctx->world, scale, ctx->last_error)) {
        return 0;
    }
    return 1;
}

int ms_db_query_sql(ms_db_handle_t *h, const char *query, int radius) {
    if (!h || !query) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    DbQuery q;
    if (!db_parse_query(query, q)) {
        ctx->last_error = "Query ungueltig";
        return 0;
    }
    ctx->last_results = db_execute_query(ctx->world, q, radius);
    return static_cast<int>(ctx->last_results.size());
}

int ms_db_sql_exec(ms_db_handle_t *h, const char *query, int use_focus, int focus_x, int focus_y, int radius) {
    if (!h || !query) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    DbSqlResult result;
    std::string error;
    const bool focus_enabled = use_focus != 0;
    if (!db_execute_sql(ctx->world, query, focus_enabled, focus_x, focus_y, radius, result, error)) {
        ctx->last_error = error;
        ctx->last_sql_valid = false;
        ctx->last_sql_result = DbSqlResult{};
        return 0;
    }
    ctx->last_sql_result = std::move(result);
    ctx->last_sql_valid = true;
    invalidate_delta_cache(ctx);
    return static_cast<int>(ctx->last_sql_result.rows.size());
}

int ms_db_sql_get_column_count(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->last_sql_valid) return 0;
    return static_cast<int>(ctx->last_sql_result.columns.size());
}

int ms_db_sql_get_column_name(ms_db_handle_t *h, int index, char *dst, int dst_size) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->last_sql_valid) return 0;
    if (index < 0 || index >= static_cast<int>(ctx->last_sql_result.columns.size())) return 0;
    return copy_string(dst, dst_size, ctx->last_sql_result.columns[static_cast<size_t>(index)]);
}

int ms_db_sql_get_row_count(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->last_sql_valid) return 0;
    return static_cast<int>(ctx->last_sql_result.rows.size());
}

int ms_db_sql_get_cell(ms_db_handle_t *h, int row, int col, char *dst, int dst_size) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->last_sql_valid) return 0;
    if (row < 0 || row >= static_cast<int>(ctx->last_sql_result.rows.size())) return 0;
    const auto &r = ctx->last_sql_result.rows[static_cast<size_t>(row)];
    if (col < 0 || col >= static_cast<int>(r.size())) {
        return copy_string(dst, dst_size, "");
    }
    return copy_string(dst, dst_size, r[static_cast<size_t>(col)]);
}

int ms_db_merge_delta(ms_db_handle_t *h, int agents, int steps, uint32_t seed) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    DbIngestConfig cfg;
    cfg.agent_count = agents;
    cfg.steps = steps;
    cfg.seed = seed;
    std::string error;
    if (!db_merge_delta(ctx->world, cfg, error)) {
        ctx->last_error = error;
        return 0;
    }
    invalidate_delta_cache(ctx);
    return 1;
}

int ms_db_undo_last_delta(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    std::string error;
    if (!db_undo_last_delta(ctx->world, error)) {
        ctx->last_error = error;
        return 0;
    }
    invalidate_delta_cache(ctx);
    return 1;
}

int ms_db_get_delta_count(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->delta_cache_valid) {
        build_delta_cache(ctx);
    }
    return static_cast<int>(ctx->delta_entries.size());
}

int ms_db_get_tombstone_count(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->delta_cache_valid) {
        build_delta_cache(ctx);
    }
    return static_cast<int>(ctx->tombstone_entries.size());
}

int ms_db_get_delta_entry(ms_db_handle_t *h, int index, char *dst, int dst_size) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->delta_cache_valid) {
        build_delta_cache(ctx);
    }
    if (index < 0 || index >= static_cast<int>(ctx->delta_entries.size())) return 0;
    return copy_string(dst, dst_size, ctx->delta_entries[static_cast<size_t>(index)]);
}

int ms_db_get_tombstone_entry(ms_db_handle_t *h, int index, char *dst, int dst_size) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (!ctx->delta_cache_valid) {
        build_delta_cache(ctx);
    }
    if (index < 0 || index >= static_cast<int>(ctx->tombstone_entries.size())) return 0;
    return copy_string(dst, dst_size, ctx->tombstone_entries[static_cast<size_t>(index)]);
}

int ms_db_query_simple(ms_db_handle_t *h, const char *table, const char *column, const char *value, int radius) {
    if (!h || !table || !column || !value) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    DbQuery q;
    q.table = table;
    q.column = column;
    q.value = value;
    ctx->last_results = db_execute_query(ctx->world, q, radius);
    return static_cast<int>(ctx->last_results.size());
}

int ms_db_query_by_id(ms_db_handle_t *h, const char *table, int id, int radius) {
    if (!h || !table) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    DbQuery q;
    q.table = table;
    q.column = std::string(table) + "Id";
    q.value = std::to_string(id);
    ctx->last_results = db_execute_query(ctx->world, q, radius);
    return static_cast<int>(ctx->last_results.size());
}

int ms_db_query_simple_focus(ms_db_handle_t *h, const char *table, const char *column, const char *value, int center_x, int center_y, int radius) {
    if (!h || !table || !column || !value) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    DbQuery q;
    q.table = table;
    q.column = column;
    q.value = value;
    ctx->last_results = db_execute_query_focus(ctx->world, q, center_x, center_y, radius);
    return static_cast<int>(ctx->last_results.size());
}

int ms_db_query_by_id_focus(ms_db_handle_t *h, const char *table, int id, int center_x, int center_y, int radius) {
    if (!h || !table) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    ctx->last_error.clear();
    DbQuery q;
    q.table = table;
    q.column = std::string(table) + "Id";
    q.value = std::to_string(id);
    ctx->last_results = db_execute_query_focus(ctx->world, q, center_x, center_y, radius);
    return static_cast<int>(ctx->last_results.size());
}

int ms_db_get_result_count(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    return static_cast<int>(ctx->last_results.size());
}

int ms_db_get_result_indices(ms_db_handle_t *h, int *out, int max_out) {
    if (!h || !out || max_out <= 0) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    int count = std::min(max_out, static_cast<int>(ctx->last_results.size()));
    for (int i = 0; i < count; ++i) {
        out[i] = ctx->last_results[static_cast<size_t>(i)];
    }
    return count;
}

int ms_db_get_payload_count(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    return static_cast<int>(ctx->world.payloads.size());
}

int ms_db_find_payload_by_id(ms_db_handle_t *h, int payload_id) {
    if (!h) return -1;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    for (const auto &pair : ctx->world.delta_index_by_key) {
        int idx = pair.second;
        if (idx < 0 || idx >= static_cast<int>(ctx->world.payloads.size())) continue;
        const DbPayload &p = ctx->world.payloads[static_cast<size_t>(idx)];
        int64_t key = db_payload_key(p.table_id, p.id);
        if (ctx->world.tombstones.find(key) != ctx->world.tombstones.end()) continue;
        if (p.id == payload_id) {
            return idx;
        }
    }
    for (size_t i = 0; i < ctx->world.payloads.size(); ++i) {
        const DbPayload &p = ctx->world.payloads[i];
        int64_t key = db_payload_key(p.table_id, p.id);
        if (ctx->world.tombstones.find(key) != ctx->world.tombstones.end()) continue;
        if (!p.is_delta && ctx->world.delta_index_by_key.find(key) != ctx->world.delta_index_by_key.end()) continue;
        if (p.id == payload_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int ms_db_get_table_count(ms_db_handle_t *h) {
    if (!h) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    return static_cast<int>(ctx->world.table_names.size());
}

int ms_db_get_payload(ms_db_handle_t *h, int payload_index, ms_db_payload_t *out) {
    if (!h || !out) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (payload_index < 0 || payload_index >= static_cast<int>(ctx->world.payloads.size())) return 0;
    const DbPayload &p = ctx->world.payloads[static_cast<size_t>(payload_index)];
    out->id = p.id;
    out->table_id = p.table_id;
    out->x = p.x;
    out->y = p.y;
    out->field_count = static_cast<int>(p.fields.size());
    out->fk_count = static_cast<int>(p.foreign_keys.size());
    return 1;
}

int ms_db_get_payload_raw(ms_db_handle_t *h, int payload_index, char *dst, int dst_size) {
    if (!h || !dst || dst_size <= 0) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (payload_index < 0 || payload_index >= static_cast<int>(ctx->world.payloads.size())) return 0;
    const DbPayload &p = ctx->world.payloads[static_cast<size_t>(payload_index)];
    if (p.raw_data.empty()) {
        dst[0] = '\0';
        return 1;
    }
    int copy_len = std::min(dst_size - 1, static_cast<int>(p.raw_data.size()));
    std::memcpy(dst, p.raw_data.data(), static_cast<size_t>(copy_len));
    dst[copy_len] = '\0';
    return 1;
}

int ms_db_get_table_name(ms_db_handle_t *h, int table_id, char *dst, int dst_size) {
    if (!h || !dst || dst_size <= 0) return 0;
    auto *ctx = reinterpret_cast<MicroSwarmDbContext *>(h);
    if (table_id < 0 || table_id >= static_cast<int>(ctx->world.table_names.size())) return 0;
    const std::string &name = ctx->world.table_names[static_cast<size_t>(table_id)];
    int copy_len = std::min(dst_size - 1, static_cast<int>(name.size()));
    std::memcpy(dst, name.data(), static_cast<size_t>(copy_len));
    dst[copy_len] = '\0';
    return 1;
}

} // extern "C"
