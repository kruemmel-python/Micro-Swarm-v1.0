#include "agent.h"

#include <cmath>

namespace {
float wrap_angle(float a) {
    const float two_pi = 6.283185307f;
    while (a < 0.0f) a += two_pi;
    while (a >= two_pi) a -= two_pi;
    return a;
}

float sample_field(const GridField &field, float fx, float fy) {
    int x = static_cast<int>(fx);
    int y = static_cast<int>(fy);
    if (x < 0 || y < 0 || x >= field.width || y >= field.height) {
        return 0.0f;
    }
    return field.at(x, y);
}
} // namespace

void Agent::step(Rng &rng,
                 const SimParams &params,
                 int fitness_window,
                 const SpeciesProfile &profile,
                 GridField &phero_food,
                 GridField &phero_danger,
                 const GridField &phero_gamma,
                 GridField &molecules,
                 GridField &resources,
                 const GridField &mycel) {
    last_energy = energy;
    const float sensor = params.agent_sense_radius * genome.sense_gain;
    const float turn = params.agent_random_turn * profile.exploration_mul;

    float angles[3] = {
        heading - 0.6f,
        heading,
        heading + 0.6f
    };
    float weights[3] = {};

    for (int i = 0; i < 3; ++i) {
        float nx = x + std::cos(angles[i]) * sensor;
        float ny = y + std::sin(angles[i]) * sensor;
        float alpha = sample_field(phero_food, nx, ny) * genome.pheromone_gain;
        float beta = sample_field(phero_danger, nx, ny) * genome.pheromone_gain;
        float gamma = sample_field(phero_gamma, nx, ny) * genome.pheromone_gain;
        float r = sample_field(resources, nx, ny) * profile.resource_weight_mul;
        float m = sample_field(molecules, nx, ny) * profile.molecule_weight_mul;
        float my = sample_field(mycel, nx, ny) * profile.mycel_attraction_mul;
        float signal_impact = alpha * genome.response_matrix[0] + beta * genome.response_matrix[1] + gamma * genome.response_matrix[2];
        float signal_strength = std::abs(signal_impact) + my;
        float novelty = 1.0f - std::min(1.0f, std::max(0.0f, signal_strength));
        float w = signal_impact + r + 0.25f * m + my + profile.novelty_weight * novelty;
        if (w < 0.001f) w = 0.001f;
        weights[i] = w;
    }

    float total = weights[0] + weights[1] + weights[2];
    float pick = rng.uniform(0.0f, total);
    int choice = 1;
    for (int i = 0; i < 3; ++i) {
        if (pick <= weights[i]) {
            choice = i;
            break;
        }
        pick -= weights[i];
    }

    heading = wrap_angle(angles[choice] + rng.uniform(-turn, turn) * genome.exploration_bias);

    float nx = x + std::cos(heading);
    float ny = y + std::sin(heading);

    bool bounced = false;
    if (nx >= 0.0f && ny >= 0.0f && nx < phero_food.width && ny < phero_food.height) {
        x = nx;
        y = ny;
    } else {
        heading = wrap_angle(heading + 3.1415926f);
        bounced = true;
    }

    int cx = static_cast<int>(x);
    int cy = static_cast<int>(y);
    if (cx >= 0 && cy >= 0 && cx < resources.width && cy < resources.height) {
        float &cell = resources.at(cx, cy);
        float harvested = std::min(cell, params.agent_harvest);
        cell -= harvested;
        energy += harvested;

        float deposit = params.phero_food_deposit_scale * harvested;
        float alpha_drop = deposit * genome.emission_matrix[0];
        float beta_drop = deposit * genome.emission_matrix[1];
        phero_food.at(cx, cy) = std::max(0.0f, phero_food.at(cx, cy) + alpha_drop);
        phero_danger.at(cx, cy) = std::max(0.0f, phero_danger.at(cx, cy) + beta_drop);
        molecules.at(cx, cy) += harvested * 0.5f;
    }

    float cognitive_load = std::abs(genome.response_matrix[0]) +
                           std::abs(genome.response_matrix[1]) +
                           std::abs(genome.response_matrix[2]) +
                           std::abs(genome.emission_matrix[0]) +
                           std::abs(genome.emission_matrix[1]) +
                           std::abs(genome.emission_matrix[2]) +
                           std::abs(genome.emission_matrix[3]);
    float info_cost = cognitive_load * params.info_metabolism_cost;
    energy -= params.agent_move_cost + info_cost;
    if (energy < 0.0f) {
        energy = 0.0f;
    }

    float delta = energy - last_energy;
    if (delta > 0.0f) {
        fitness_accum += delta;
    }
    fitness_ticks += 1;
    if (fitness_window > 0 && fitness_ticks >= fitness_window) {
        fitness_value = fitness_accum / static_cast<float>(fitness_ticks);
        fitness_accum = 0.0f;
        fitness_ticks = 0;
    }

    float danger_deposit = 0.0f;
    if (bounced) {
        danger_deposit += params.danger_bounce_deposit;
    }
    if (delta < -params.danger_delta_threshold) {
        danger_deposit += (-delta) * params.phero_danger_deposit_scale;
    }
    if (danger_deposit > 0.0f) {
        int dx = static_cast<int>(x);
        int dy = static_cast<int>(y);
        if (dx >= 0 && dy >= 0 && dx < phero_danger.width && dy < phero_danger.height) {
            float alpha_drop = danger_deposit * genome.emission_matrix[2];
            float beta_drop = danger_deposit * genome.emission_matrix[3];
            phero_food.at(dx, dy) = std::max(0.0f, phero_food.at(dx, dy) + alpha_drop);
            phero_danger.at(dx, dy) = std::max(0.0f, phero_danger.at(dx, dy) + beta_drop);
        }
    }

    if (profile.counter_deposit_mul > 0.0f) {
        int dx = static_cast<int>(x);
        int dy = static_cast<int>(y);
        if (dx >= 0 && dy >= 0 && dx < phero_food.width && dy < phero_food.height) {
            float local_food = phero_food.at(dx, dy);
            float local_mycel = sample_field(mycel, static_cast<float>(dx), static_cast<float>(dy));
            float density = local_food + local_mycel;
            if (density > profile.over_density_threshold) {
                float reduction = (density - profile.over_density_threshold) * profile.counter_deposit_mul;
                phero_food.at(dx, dy) = std::max(0.0f, local_food - reduction);
            }
        }
    }
}
