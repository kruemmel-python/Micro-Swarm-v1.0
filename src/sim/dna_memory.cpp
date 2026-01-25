#include "dna_memory.h"

#include <algorithm>
#include <cmath>
#include <array>

namespace {
constexpr int kKernelCodonCount = 4;
constexpr int kKernelCodonMax = 7;
constexpr int kLwsMin = 0;
constexpr int kLwsMax = 32;
constexpr int kToxicStrideMin = 1;
constexpr int kToxicStrideMax = 64;
constexpr int kToxicItersMin = 0;
constexpr int kToxicItersMax = 256;
constexpr float kResponseMin = -2.0f;
constexpr float kResponseMax = 2.0f;
constexpr float kEmissionMin = -2.0f;
constexpr float kEmissionMax = 2.0f;

void randomize_codons(Rng &rng,
                      Genome &g,
                      int stride_min,
                      int stride_max,
                      int iters_min,
                      int iters_max,
                      bool toxic_enabled) {
    for (int i = 0; i < kKernelCodonCount; ++i) {
        g.kernel_codons[i] = rng.uniform_int(0, kKernelCodonMax);
    }
    g.lws_x = rng.uniform_int(kLwsMin, kLwsMax);
    g.lws_y = rng.uniform_int(kLwsMin, kLwsMax);
    g.toxic_stride = rng.uniform_int(stride_min, stride_max);
    g.toxic_iters = rng.uniform_int(iters_min, iters_max);
    if (!toxic_enabled) {
        g.toxic_iters = 0;
    }
}

void clamp_codons(Genome &g) {
    for (int i = 0; i < kKernelCodonCount; ++i) {
        g.kernel_codons[i] = std::min(kKernelCodonMax, std::max(0, g.kernel_codons[i]));
    }
    g.lws_x = std::min(kLwsMax, std::max(kLwsMin, g.lws_x));
    g.lws_y = std::min(kLwsMax, std::max(kLwsMin, g.lws_y));
    g.toxic_stride = std::min(kToxicStrideMax, std::max(kToxicStrideMin, g.toxic_stride));
    g.toxic_iters = std::min(kToxicItersMax, std::max(kToxicItersMin, g.toxic_iters));
}

float clamp_range(float v, float lo, float hi) {
    return std::min(hi, std::max(lo, v));
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
    g.response_matrix[0] = clamp_range(g.response_matrix[0], kResponseMin, kResponseMax);
    g.response_matrix[1] = clamp_range(g.response_matrix[1], kResponseMin, kResponseMax);
    g.response_matrix[2] = clamp_range(g.response_matrix[2], kResponseMin, kResponseMax);
    for (int i = 0; i < 4; ++i) {
        g.emission_matrix[i] = clamp_range(g.emission_matrix[i], kEmissionMin, kEmissionMax);
    }
}

void mutate_semantics(Rng &rng, Genome &g, float sigma) {
    if (sigma <= 0.0f) return;
    g.response_matrix[0] = clamp_range(g.response_matrix[0] + gaussian(rng, sigma), kResponseMin, kResponseMax);
    g.response_matrix[1] = clamp_range(g.response_matrix[1] + gaussian(rng, sigma), kResponseMin, kResponseMax);
    g.response_matrix[2] = clamp_range(g.response_matrix[2] + gaussian(rng, sigma), kResponseMin, kResponseMax);
    for (int i = 0; i < 4; ++i) {
        g.emission_matrix[i] = clamp_range(g.emission_matrix[i] + gaussian(rng, sigma), kEmissionMin, kEmissionMax);
    }
}
} // namespace

void DNAMemory::add(const SimParams &params, const Genome &genome, float fitness, const EvoParams &evo, int capacity_override) {
    entries.push_back({genome, fitness, 0});
    std::sort(entries.begin(), entries.end(), [](const DNAEntry &a, const DNAEntry &b) {
        return a.fitness > b.fitness;
    });
    int capacity = (capacity_override > 0) ? capacity_override : params.dna_capacity;
    if (static_cast<int>(entries.size()) > capacity) {
        entries.resize(capacity);
    }
}

Genome DNAMemory::sample(Rng &rng, const SimParams &params, const EvoParams &evo) const {
    if (entries.empty()) {
        int stride_min = std::min(kToxicStrideMax, std::max(kToxicStrideMin, params.toxic_stride_min));
        int stride_max = std::min(kToxicStrideMax, std::max(stride_min, params.toxic_stride_max));
        int iters_min = std::min(kToxicItersMax, std::max(kToxicItersMin, params.toxic_iters_min));
        int iters_max = std::min(kToxicItersMax, std::max(iters_min, params.toxic_iters_max));
        bool toxic_enabled = params.toxic_enable != 0;
        Genome g;
        g.sense_gain = rng.uniform(0.6f, 1.4f);
        g.pheromone_gain = rng.uniform(0.6f, 1.4f);
        g.exploration_bias = rng.uniform(0.2f, 0.8f);
        randomize_semantics(rng, g);
        randomize_codons(rng, g, stride_min, stride_max, iters_min, iters_max, toxic_enabled);
        return g;
    }

    auto clamp01 = [](float v) {
        return std::min(1.0f, std::max(0.0f, v));
    };

    auto weighted_pick = [&](const std::vector<DNAEntry> &pool) -> Genome {
        float total = 0.0f;
        for (const auto &entry : pool) {
            total += entry.fitness * params.dna_survival_bias + 0.01f;
        }
        float pick = rng.uniform(0.0f, total);
        for (const auto &entry : pool) {
            float w = entry.fitness * params.dna_survival_bias + 0.01f;
            if (pick <= w) {
                return entry.genome;
            }
            pick -= w;
        }
        return pool.front().genome;
    };

    int stride_min = std::min(kToxicStrideMax, std::max(kToxicStrideMin, params.toxic_stride_min));
    int stride_max = std::min(kToxicStrideMax, std::max(stride_min, params.toxic_stride_max));
    int iters_min = std::min(kToxicItersMax, std::max(kToxicItersMin, params.toxic_iters_min));
    int iters_max = std::min(kToxicItersMax, std::max(iters_min, params.toxic_iters_max));
    bool toxic_enabled = params.toxic_enable != 0;
    Genome g;
    if (evo.enabled) {
        int elite_count = std::max(1, static_cast<int>(entries.size() * evo.elite_frac));
        bool from_elite = (rng.uniform(0.0f, 1.0f) < evo.elite_frac);
        if (from_elite && elite_count > 0) {
            std::vector<DNAEntry> elite(entries.begin(), entries.begin() + elite_count);
            g = weighted_pick(elite);
        } else {
            g = weighted_pick(entries);
        }
        g.sense_gain *= rng.uniform(1.0f - evo.mutation_sigma, 1.0f + evo.mutation_sigma);
        g.pheromone_gain *= rng.uniform(1.0f - evo.mutation_sigma, 1.0f + evo.mutation_sigma);
        g.exploration_bias = clamp01(g.exploration_bias + rng.uniform(-evo.exploration_delta, evo.exploration_delta));
        mutate_semantics(rng, g, evo.mutation_sigma);
        float codon_prob = std::min(0.5f, evo.mutation_sigma * 2.0f);
        if (codon_prob > 0.0f) {
            for (int i = 0; i < kKernelCodonCount; ++i) {
                if (rng.uniform(0.0f, 1.0f) < codon_prob) {
                    g.kernel_codons[i] = rng.uniform_int(0, kKernelCodonMax);
                }
            }
        }
        if (codon_prob > 0.0f) {
            if (rng.uniform(0.0f, 1.0f) < codon_prob) {
                g.lws_x = rng.uniform_int(kLwsMin, kLwsMax);
            }
            if (rng.uniform(0.0f, 1.0f) < codon_prob) {
                g.lws_y = rng.uniform_int(kLwsMin, kLwsMax);
            }
            if (rng.uniform(0.0f, 1.0f) < codon_prob) {
                g.toxic_stride = rng.uniform_int(stride_min, stride_max);
            }
            if (rng.uniform(0.0f, 1.0f) < codon_prob) {
                g.toxic_iters = rng.uniform_int(iters_min, iters_max);
            }
        }
    } else {
        g = weighted_pick(entries);
        g.sense_gain *= rng.uniform(0.9f, 1.1f);
        g.pheromone_gain *= rng.uniform(0.9f, 1.1f);
        g.exploration_bias = clamp01(g.exploration_bias + rng.uniform(-0.05f, 0.05f));
        mutate_semantics(rng, g, 0.05f);
        for (int i = 0; i < kKernelCodonCount; ++i) {
            if (rng.uniform(0.0f, 1.0f) < 0.05f) {
                g.kernel_codons[i] = rng.uniform_int(0, kKernelCodonMax);
            }
        }
        if (rng.uniform(0.0f, 1.0f) < 0.05f) {
            g.lws_x = rng.uniform_int(kLwsMin, kLwsMax);
        }
        if (rng.uniform(0.0f, 1.0f) < 0.05f) {
            g.lws_y = rng.uniform_int(kLwsMin, kLwsMax);
        }
        if (rng.uniform(0.0f, 1.0f) < 0.05f) {
            g.toxic_stride = rng.uniform_int(stride_min, stride_max);
        }
        if (rng.uniform(0.0f, 1.0f) < 0.05f) {
            g.toxic_iters = rng.uniform_int(iters_min, iters_max);
        }
    }

    g.sense_gain = clamp_range(g.sense_gain, 0.2f, 3.0f);
    g.pheromone_gain = clamp_range(g.pheromone_gain, 0.2f, 3.0f);
    g.exploration_bias = clamp01(g.exploration_bias);
    g.response_matrix[0] = clamp_range(g.response_matrix[0], kResponseMin, kResponseMax);
    g.response_matrix[1] = clamp_range(g.response_matrix[1], kResponseMin, kResponseMax);
    g.response_matrix[2] = clamp_range(g.response_matrix[2], kResponseMin, kResponseMax);
    for (int i = 0; i < 4; ++i) {
        g.emission_matrix[i] = clamp_range(g.emission_matrix[i], kEmissionMin, kEmissionMax);
    }
    clamp_codons(g);
    if (!toxic_enabled) {
        g.toxic_iters = 0;
    }
    return g;
}

void DNAMemory::decay(const EvoParams &evo) {
    float decay = evo.enabled ? evo.age_decay : 0.995f;
    for (auto &entry : entries) {
        entry.age += 1;
        entry.fitness *= decay;
    }
}

float calculate_genetic_stagnation(const std::vector<DNAEntry> &entries) {
    if (entries.size() < 2) {
        return 1.0f;
    }
    const int kTop = 10;
    std::vector<const DNAEntry *> top;
    top.reserve(std::min(kTop, static_cast<int>(entries.size())));
    for (const auto &e : entries) {
        top.push_back(&e);
    }
    std::sort(top.begin(), top.end(), [](const DNAEntry *a, const DNAEntry *b) {
        return a->fitness > b->fitness;
    });
    if (top.size() > kTop) {
        top.resize(kTop);
    }
    if (top.size() < 2) {
        return 1.0f;
    }

    auto feature = [](const Genome &g, std::array<float, 18> &out) {
        out[0] = clamp_range((g.sense_gain - 0.2f) / 2.8f, 0.0f, 1.0f);
        out[1] = clamp_range((g.pheromone_gain - 0.2f) / 2.8f, 0.0f, 1.0f);
        out[2] = clamp_range(g.exploration_bias, 0.0f, 1.0f);
        out[3] = clamp_range((g.response_matrix[0] + 2.0f) / 4.0f, 0.0f, 1.0f);
        out[4] = clamp_range((g.response_matrix[1] + 2.0f) / 4.0f, 0.0f, 1.0f);
        out[5] = clamp_range((g.response_matrix[2] + 2.0f) / 4.0f, 0.0f, 1.0f);
        out[6] = clamp_range((g.emission_matrix[0] + 2.0f) / 4.0f, 0.0f, 1.0f);
        out[7] = clamp_range((g.emission_matrix[1] + 2.0f) / 4.0f, 0.0f, 1.0f);
        out[8] = clamp_range((g.emission_matrix[2] + 2.0f) / 4.0f, 0.0f, 1.0f);
        out[9] = clamp_range((g.emission_matrix[3] + 2.0f) / 4.0f, 0.0f, 1.0f);
        out[10] = clamp_range(static_cast<float>(g.kernel_codons[0]) / 7.0f, 0.0f, 1.0f);
        out[11] = clamp_range(static_cast<float>(g.kernel_codons[1]) / 7.0f, 0.0f, 1.0f);
        out[12] = clamp_range(static_cast<float>(g.kernel_codons[2]) / 7.0f, 0.0f, 1.0f);
        out[13] = clamp_range(static_cast<float>(g.kernel_codons[3]) / 7.0f, 0.0f, 1.0f);
        out[14] = clamp_range(static_cast<float>(g.lws_x) / 32.0f, 0.0f, 1.0f);
        out[15] = clamp_range(static_cast<float>(g.lws_y) / 32.0f, 0.0f, 1.0f);
        out[16] = clamp_range(static_cast<float>(g.toxic_stride - 1) / 63.0f, 0.0f, 1.0f);
        out[17] = clamp_range(static_cast<float>(g.toxic_iters) / 256.0f, 0.0f, 1.0f);
    };

    const float max_dist = std::sqrt(static_cast<float>(18));
    double sum = 0.0;
    int count = 0;
    for (size_t i = 0; i < top.size(); ++i) {
        std::array<float, 18> ai{};
        feature(top[i]->genome, ai);
        for (size_t j = i + 1; j < top.size(); ++j) {
            std::array<float, 18> bj{};
            feature(top[j]->genome, bj);
            double dist2 = 0.0;
            for (size_t k = 0; k < ai.size(); ++k) {
                double d = static_cast<double>(ai[k] - bj[k]);
                dist2 += d * d;
            }
            sum += std::sqrt(dist2);
            count++;
        }
    }
    if (count <= 0) {
        return 1.0f;
    }
    float avg = static_cast<float>(sum / static_cast<double>(count));
    float diversity = clamp_range(avg / max_dist, 0.0f, 1.0f);
    return 1.0f - diversity;
}
