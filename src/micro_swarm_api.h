#pragma once

#include <stdint.h>

#ifdef _WIN32
#if defined(MICRO_SWARM_DLL_EXPORT)
#define MICRO_SWARM_API __declspec(dllexport)
#else
#define MICRO_SWARM_API __declspec(dllimport)
#endif
#else
#define MICRO_SWARM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MS_API_VERSION_MAJOR 1
#define MS_API_VERSION_MINOR 3
#define MS_API_VERSION_PATCH 0

typedef struct ms_handle_t ms_handle_t;
typedef struct ms_db_handle_t ms_db_handle_t;

typedef enum ms_field_kind {
    MS_FIELD_RESOURCES = 0,
    MS_FIELD_PHEROMONE_FOOD = 1,
    MS_FIELD_PHEROMONE_DANGER = 2,
    MS_FIELD_MOLECULES = 3,
    MS_FIELD_MYCEL = 4
} ms_field_kind;

typedef struct ms_params_t {
    int width;
    int height;
    int agent_count;
    int steps;

    float pheromone_evaporation;
    float pheromone_diffusion;
    float molecule_evaporation;
    float molecule_diffusion;

    float resource_regen;
    float resource_max;

    float mycel_decay;
    float mycel_growth;
    float mycel_transport;
    float mycel_drive_threshold;
    float mycel_drive_p;
    float mycel_drive_r;

    float agent_move_cost;
    float agent_harvest;
    float agent_deposit_scale;
    float agent_sense_radius;
    float agent_random_turn;

    int dna_capacity;
    int dna_global_capacity;
    float dna_survival_bias;

    float phero_food_deposit_scale;
    float phero_danger_deposit_scale;
    float danger_delta_threshold;
    float danger_bounce_deposit;

    int evo_enable;
    float evo_elite_frac;
    float evo_min_energy_to_store;
    float evo_mutation_sigma;
    float evo_exploration_delta;
    int evo_fitness_window;
    float evo_age_decay;

    float global_spawn_frac;
} ms_params_t;

typedef struct ms_config_t {
    ms_params_t params;
    uint32_t seed;
} ms_config_t;

typedef struct ms_species_profile_t {
    float exploration_mul;
    float food_attraction_mul;
    float danger_aversion_mul;
    float deposit_food_mul;
    float deposit_danger_mul;
    float resource_weight_mul;
    float molecule_weight_mul;
    float mycel_attraction_mul;
    float novelty_weight;
    float mutation_sigma_mul;
    float exploration_delta_mul;
    float dna_binding;
    float over_density_threshold;
    float counter_deposit_mul;
} ms_species_profile_t;

typedef struct ms_agent_t {
    float x;
    float y;
    float heading;
    float energy;
    int species;
    float sense_gain;
    float pheromone_gain;
    float exploration_bias;
} ms_agent_t;

typedef struct ms_metrics_t {
    int step_index;
    int dna_global_size;
    int dna_species_sizes[4];
    float avg_energy;
    float avg_energy_by_species[4];
} ms_metrics_t;

typedef struct ms_entropy_t {
    float entropy[5];
    float norm_entropy[5];
    float p95[5];
} ms_entropy_t;

typedef struct ms_mycel_stats_t {
    float min_val;
    float max_val;
    float mean;
} ms_mycel_stats_t;

typedef struct ms_db_payload_t {
    int id;
    int table_id;
    int x;
    int y;
    int field_count;
    int fk_count;
} ms_db_payload_t;

MICRO_SWARM_API ms_handle_t *ms_create(const ms_config_t *cfg);
MICRO_SWARM_API void ms_destroy(ms_handle_t *h);
MICRO_SWARM_API ms_handle_t *ms_clone(const ms_handle_t *src);

MICRO_SWARM_API void ms_reset(ms_handle_t *h, uint32_t seed);
MICRO_SWARM_API int ms_step(ms_handle_t *h, int steps);
MICRO_SWARM_API int ms_run(ms_handle_t *h, int steps);
MICRO_SWARM_API void ms_pause(ms_handle_t *h);
MICRO_SWARM_API void ms_resume(ms_handle_t *h);
MICRO_SWARM_API int ms_get_step_index(ms_handle_t *h);

MICRO_SWARM_API void ms_set_params(ms_handle_t *h, const ms_params_t *p);
MICRO_SWARM_API void ms_get_params(ms_handle_t *h, ms_params_t *out);
MICRO_SWARM_API void ms_set_species_profiles(ms_handle_t *h, const ms_species_profile_t profiles[4]);
MICRO_SWARM_API void ms_get_species_profiles(ms_handle_t *h, ms_species_profile_t out[4]);
MICRO_SWARM_API void ms_set_species_fracs(ms_handle_t *h, const float fracs[4]);
MICRO_SWARM_API void ms_get_species_fracs(ms_handle_t *h, float out[4]);

MICRO_SWARM_API void ms_get_field_info(ms_handle_t *h, ms_field_kind kind, int *w, int *hgt);
MICRO_SWARM_API int ms_copy_field_out(ms_handle_t *h, ms_field_kind kind, float *dst, int dst_count);
MICRO_SWARM_API int ms_copy_field_in(ms_handle_t *h, ms_field_kind kind, const float *src, int src_count);
MICRO_SWARM_API void ms_clear_field(ms_handle_t *h, ms_field_kind kind, float value);

MICRO_SWARM_API int ms_load_field_csv(ms_handle_t *h, ms_field_kind kind, const char *path);
MICRO_SWARM_API int ms_save_field_csv(ms_handle_t *h, ms_field_kind kind, const char *path);

MICRO_SWARM_API int ms_get_agent_count(ms_handle_t *h);
MICRO_SWARM_API int ms_get_agents(ms_handle_t *h, ms_agent_t *out, int max_agents);
MICRO_SWARM_API void ms_set_agents(ms_handle_t *h, const ms_agent_t *agents, int count);
MICRO_SWARM_API void ms_kill_agent(ms_handle_t *h, int agent_id);
MICRO_SWARM_API void ms_spawn_agent(ms_handle_t *h, const ms_agent_t *agent);

MICRO_SWARM_API void ms_get_dna_sizes(ms_handle_t *h, int out_species[4], int *out_global);
MICRO_SWARM_API void ms_get_dna_capacity(ms_handle_t *h, int *species_cap, int *global_cap);
MICRO_SWARM_API void ms_set_dna_capacity(ms_handle_t *h, int species_cap, int global_cap);
MICRO_SWARM_API void ms_clear_dna_pools(ms_handle_t *h);
MICRO_SWARM_API int ms_export_dna_csv(ms_handle_t *h, const char *path);
MICRO_SWARM_API int ms_import_dna_csv(ms_handle_t *h, const char *path);

MICRO_SWARM_API void ms_get_system_metrics(ms_handle_t *h, ms_metrics_t *out);
MICRO_SWARM_API void ms_get_energy_stats(ms_handle_t *h, float *avg, float *min, float *max);
MICRO_SWARM_API void ms_get_energy_by_species(ms_handle_t *h, float out[4]);
MICRO_SWARM_API void ms_get_entropy_metrics(ms_handle_t *h, ms_entropy_t *out);
MICRO_SWARM_API void ms_get_mycel_stats(ms_handle_t *h, ms_mycel_stats_t *out);

MICRO_SWARM_API void ms_ocl_enable(ms_handle_t *h, int enable);
MICRO_SWARM_API void ms_ocl_select_device(ms_handle_t *h, int platform, int device);
MICRO_SWARM_API void ms_ocl_print_devices(void);
MICRO_SWARM_API void ms_ocl_set_no_copyback(ms_handle_t *h, int enable);
MICRO_SWARM_API int ms_is_gpu_active(ms_handle_t *h);

MICRO_SWARM_API void ms_get_api_version(int *major, int *minor, int *patch);

MICRO_SWARM_API ms_db_handle_t *ms_db_create(void);
MICRO_SWARM_API void ms_db_destroy(ms_db_handle_t *h);
MICRO_SWARM_API const char *ms_db_get_last_error(ms_db_handle_t *h);

MICRO_SWARM_API int ms_db_load_sql(ms_db_handle_t *h, const char *path);
MICRO_SWARM_API int ms_db_run_ingest(ms_db_handle_t *h, int width, int height, int agents, int steps, uint32_t seed);
MICRO_SWARM_API int ms_db_save_myco(ms_db_handle_t *h, const char *path);
MICRO_SWARM_API int ms_db_load_myco(ms_db_handle_t *h, const char *path);
MICRO_SWARM_API int ms_db_save_cluster_ppm(ms_db_handle_t *h, const char *path, int scale);

MICRO_SWARM_API int ms_db_query_sql(ms_db_handle_t *h, const char *query, int radius);
MICRO_SWARM_API int ms_db_query_simple(ms_db_handle_t *h, const char *table, const char *column, const char *value, int radius);
MICRO_SWARM_API int ms_db_query_by_id(ms_db_handle_t *h, const char *table, int id, int radius);
MICRO_SWARM_API int ms_db_query_simple_focus(ms_db_handle_t *h, const char *table, const char *column, const char *value, int center_x, int center_y, int radius);
MICRO_SWARM_API int ms_db_query_by_id_focus(ms_db_handle_t *h, const char *table, int id, int center_x, int center_y, int radius);
MICRO_SWARM_API int ms_db_get_result_count(ms_db_handle_t *h);
MICRO_SWARM_API int ms_db_get_result_indices(ms_db_handle_t *h, int *out, int max_out);
MICRO_SWARM_API int ms_db_get_payload_count(ms_db_handle_t *h);
MICRO_SWARM_API int ms_db_find_payload_by_id(ms_db_handle_t *h, int payload_id);
MICRO_SWARM_API int ms_db_get_table_count(ms_db_handle_t *h);
MICRO_SWARM_API int ms_db_get_payload(ms_db_handle_t *h, int payload_index, ms_db_payload_t *out);
MICRO_SWARM_API int ms_db_get_payload_raw(ms_db_handle_t *h, int payload_index, char *dst, int dst_size);
MICRO_SWARM_API int ms_db_get_table_name(ms_db_handle_t *h, int table_id, char *dst, int dst_size);

#ifdef __cplusplus
}
#endif
