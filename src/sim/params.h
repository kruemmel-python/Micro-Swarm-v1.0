#pragma once

#include <cstdint>

struct SimParams {
    int width = 128;
    int height = 128;
    int agent_count = 512;
    int steps = 200;

    float pheromone_evaporation = 0.02f;
    float pheromone_diffusion = 0.15f;
    float molecule_evaporation = 0.35f;
    float molecule_diffusion = 0.25f;

    float resource_regen = 0.0015f;
    float resource_max = 1.0f;

    float mycel_decay = 0.003f;
    float mycel_growth = 0.02f;
    float mycel_transport = 0.12f;
    float mycel_drive_threshold = 0.08f;
    float mycel_drive_p = 0.6f;
    float mycel_drive_r = 0.4f;
    float mycel_inhibitor_weight = 0.9f;
    float mycel_inhibitor_gain = 0.12f;
    float mycel_inhibitor_decay = 0.02f;
    float mycel_inhibitor_threshold = 0.45f;

    float agent_move_cost = 0.01f;
    float agent_harvest = 0.04f;
    float agent_deposit_scale = 0.8f;
    float agent_sense_radius = 2.5f;
    float agent_random_turn = 0.2f;
    float info_metabolism_cost = 0.005f;

    int dna_capacity = 256;
    int dna_global_capacity = 128;
    float dna_survival_bias = 0.7f;

    float phero_food_deposit_scale = 0.8f;
    float phero_danger_deposit_scale = 0.6f;
    float danger_delta_threshold = 0.05f;
    float danger_bounce_deposit = 0.02f;

    int toxic_enable = 1;
    float toxic_max_fraction = 1.0f;
    int toxic_stride_min = 1;
    int toxic_stride_max = 64;
    int toxic_iters_min = 0;
    int toxic_iters_max = 256;

    float toxic_max_fraction_by_quadrant[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float toxic_max_fraction_by_species[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    int logic_mode = 0;
    int logic_input_ax = -1;
    int logic_input_ay = -1;
    int logic_input_bx = -1;
    int logic_input_by = -1;
    int logic_output_x = -1;
    int logic_output_y = -1;
    int logic_pulse_period = 20;
    float logic_pulse_strength = 10.0f;
};
