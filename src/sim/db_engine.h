#pragma once

#include "fields.h"
#include "mycel.h"
#include "rng.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct DbField {
    std::string name;
    std::string value;
};

struct DbForeignKey {
    int table_id = -1;
    int id = 0;
    std::string column;
};

struct DbPayload {
    int id = 0;
    int table_id = -1;
    std::vector<DbForeignKey> foreign_keys;
    std::vector<DbField> fields;
    std::string raw_data;
    int x = -1;
    int y = -1;
    bool placed = false;
    bool is_delta = false;
};

struct DbWorld {
    int width = 0;
    int height = 0;
    std::vector<int> cell_payload;
    std::vector<std::string> table_names;
    std::vector<std::vector<std::string>> table_columns;
    std::vector<GridField> table_pheromones;
    std::vector<DbPayload> payloads;
    GridField data_density;
    MycelNetwork mycel;
    std::unordered_map<std::string, int> table_lookup;
    std::unordered_map<int64_t, std::pair<int, int>> payload_positions;
    std::unordered_map<int64_t, int> delta_index_by_key;
    std::unordered_set<int64_t> tombstones;
};

struct DbIngestConfig {
    int agent_count = 256;
    int steps = 2000;
    uint32_t seed = 42;
    int spawn_x = -1;
    int spawn_y = -1;
};

struct DbQuery {
    std::string table;
    std::string column;
    std::string value;
};

int db_add_table(DbWorld &world, const std::string &name);
int db_find_table(const DbWorld &world, const std::string &name);
void db_init_world(DbWorld &world, int width, int height);
bool db_place_payload(DbWorld &world, int payload_index, int x, int y);

bool db_load_sql(const std::string &path, DbWorld &world, std::string &error);
bool db_run_ingest(DbWorld &world, const DbIngestConfig &cfg, std::string &error);

bool db_save_myco(const std::string &path, const DbWorld &world, std::string &error);
bool db_load_myco(const std::string &path, DbWorld &world, std::string &error);
bool db_save_cluster_ppm(const std::string &path, const DbWorld &world, int scale, std::string &error);

bool db_parse_query(const std::string &query, DbQuery &out);
std::vector<int> db_execute_query(const DbWorld &world, const DbQuery &q, int radius);
std::vector<int> db_execute_query_focus(const DbWorld &world, const DbQuery &q, int center_x, int center_y, int radius);

int64_t db_payload_key(int table_id, int id);
size_t db_delta_count(const DbWorld &world);
bool db_has_pending_delta(const DbWorld &world);
bool db_merge_delta(DbWorld &world, const DbIngestConfig &cfg, std::string &error);
bool db_apply_insert_sql(DbWorld &world, const std::string &stmt, int &rows, std::string &error);
bool db_apply_update_sql(DbWorld &world, const std::string &stmt, int &rows, std::string &error);
bool db_apply_delete_sql(DbWorld &world, const std::string &stmt, int &rows, std::string &error);
