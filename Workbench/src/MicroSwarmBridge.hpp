// MicroSwarmBridge.hpp
#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#  define MS_CALL __cdecl
#else
#  define MS_CALL
#endif

#ifndef MS_API_VERSION_MAJOR
#  define MS_API_VERSION_MAJOR 1
#endif
#ifndef MS_API_VERSION_MINOR
#  define MS_API_VERSION_MINOR 0
#endif
#ifndef MS_API_VERSION_PATCH
#  define MS_API_VERSION_PATCH 0
#endif

struct ms_db_payload_t {
  int id;
  int table_id;
  int x;
  int y;
  int field_count;
  int fk_count;
};

struct MsApiVersion {
  int major;
  int minor;
  int patch;
};

class MycoDBException : public std::runtime_error {
public:
  explicit MycoDBException(const std::string& message) : std::runtime_error(message) {}
};

struct PayloadRow {
  int payload_index = 0;
  ms_db_payload_t payload{};
  std::string table_name;
  std::string raw_data;
};

class MicroSwarmBridge {
public:
  explicit MicroSwarmBridge(const std::string& library_path);
  ~MicroSwarmBridge();

  MicroSwarmBridge(const MicroSwarmBridge&) = delete;
  MicroSwarmBridge& operator=(const MicroSwarmBridge&) = delete;

  MicroSwarmBridge(MicroSwarmBridge&&) noexcept;
  MicroSwarmBridge& operator=(MicroSwarmBridge&&) noexcept;

  MsApiVersion api_version() const;

  void open_db(const std::string& path);
  void close_db();

  int get_payload_count();
  int get_table_count();
  std::string get_table_name(int table_id);
  std::string last_error_message() const;

  std::optional<PayloadRow> find_payload_by_id(int payload_id);
  std::vector<PayloadRow> query_focus(const std::string& query, int focus_x, int focus_y, int radius);
  bool query_sql_table(const std::string& query,
                       bool use_focus,
                       int focus_x,
                       int focus_y,
                       int radius,
                       std::vector<std::string>& columns,
                       std::vector<std::vector<std::string>>& rows);
  void exec_sql(const std::string& query,
                bool use_focus,
                int focus_x,
                int focus_y,
                int radius);
  void merge_delta(int agents, int steps, uint32_t seed);
  void undo_last_delta();
  int get_delta_count();
  int get_tombstone_count();
  std::string get_delta_entry(int index);
  std::string get_tombstone_entry(int index);

private:
  struct LibraryHandle;
  LibraryHandle* library_ = nullptr;
  void* db_handle_ = nullptr;
  MsApiVersion version_{};

  using ms_get_api_version_t = void (MS_CALL*)(int*, int*, int*);
  using ms_db_create_t = void* (MS_CALL*)();
  using ms_db_destroy_t = void (MS_CALL*)(void*);
  using ms_db_get_last_error_t = const char* (MS_CALL*)(void*);
  using ms_db_load_myco_t = int (MS_CALL*)(void*, const char*);
  using ms_db_get_payload_count_t = int (MS_CALL*)(void*);
  using ms_db_get_table_count_t = int (MS_CALL*)(void*);
  using ms_db_find_payload_by_id_t = int (MS_CALL*)(void*, int);
  using ms_db_get_payload_t = int (MS_CALL*)(void*, int, ms_db_payload_t*);
  using ms_db_get_payload_raw_t = int (MS_CALL*)(void*, int, char*, int);
  using ms_db_get_table_name_t = int (MS_CALL*)(void*, int, char*, int);
  using ms_db_get_result_count_t = int (MS_CALL*)(void*);
  using ms_db_get_result_indices_t = int (MS_CALL*)(void*, int*, int);
  using ms_db_query_sql_t = int (MS_CALL*)(void*, const char*, int);
  using ms_db_sql_exec_t = int (MS_CALL*)(void*, const char*, int, int, int, int);
  using ms_db_sql_get_column_count_t = int (MS_CALL*)(void*);
  using ms_db_sql_get_column_name_t = int (MS_CALL*)(void*, int, char*, int);
  using ms_db_sql_get_row_count_t = int (MS_CALL*)(void*);
  using ms_db_sql_get_cell_t = int (MS_CALL*)(void*, int, int, char*, int);
  using ms_db_merge_delta_t = int (MS_CALL*)(void*, int, int, uint32_t);
  using ms_db_undo_last_delta_t = int (MS_CALL*)(void*);
  using ms_db_get_delta_count_t = int (MS_CALL*)(void*);
  using ms_db_get_tombstone_count_t = int (MS_CALL*)(void*);
  using ms_db_get_delta_entry_t = int (MS_CALL*)(void*, int, char*, int);
  using ms_db_get_tombstone_entry_t = int (MS_CALL*)(void*, int, char*, int);
  using ms_db_query_simple_focus_t = int (MS_CALL*)(void*, const char*, const char*, const char*, int, int, int);
  using ms_db_query_by_id_focus_t = int (MS_CALL*)(void*, const char*, int, int, int, int);

  ms_get_api_version_t ms_get_api_version_ = nullptr;
  ms_db_create_t ms_db_create_ = nullptr;
  ms_db_destroy_t ms_db_destroy_ = nullptr;
  ms_db_get_last_error_t ms_db_get_last_error_ = nullptr;
  ms_db_load_myco_t ms_db_load_myco_ = nullptr;
  ms_db_get_payload_count_t ms_db_get_payload_count_ = nullptr;
  ms_db_get_table_count_t ms_db_get_table_count_ = nullptr;
  ms_db_find_payload_by_id_t ms_db_find_payload_by_id_ = nullptr;
  ms_db_get_payload_t ms_db_get_payload_ = nullptr;
  ms_db_get_payload_raw_t ms_db_get_payload_raw_ = nullptr;
  ms_db_get_table_name_t ms_db_get_table_name_ = nullptr;
  ms_db_get_result_count_t ms_db_get_result_count_ = nullptr;
  ms_db_get_result_indices_t ms_db_get_result_indices_ = nullptr;
  ms_db_query_sql_t ms_db_query_sql_ = nullptr;
  ms_db_sql_exec_t ms_db_sql_exec_ = nullptr;
  ms_db_sql_get_column_count_t ms_db_sql_get_column_count_ = nullptr;
  ms_db_sql_get_column_name_t ms_db_sql_get_column_name_ = nullptr;
  ms_db_sql_get_row_count_t ms_db_sql_get_row_count_ = nullptr;
  ms_db_sql_get_cell_t ms_db_sql_get_cell_ = nullptr;
  ms_db_merge_delta_t ms_db_merge_delta_ = nullptr;
  ms_db_undo_last_delta_t ms_db_undo_last_delta_ = nullptr;
  ms_db_get_delta_count_t ms_db_get_delta_count_ = nullptr;
  ms_db_get_tombstone_count_t ms_db_get_tombstone_count_ = nullptr;
  ms_db_get_delta_entry_t ms_db_get_delta_entry_ = nullptr;
  ms_db_get_tombstone_entry_t ms_db_get_tombstone_entry_ = nullptr;
  ms_db_query_simple_focus_t ms_db_query_simple_focus_ = nullptr;
  ms_db_query_by_id_focus_t ms_db_query_by_id_focus_ = nullptr;

  void load_symbols();
  void ensure_action(int result, const char* action);
  std::string last_error() const;
  PayloadRow fetch_payload_row(int payload_index);
  static void ensure_open(bool is_open);
};
