// MicroSwarmBridge.cpp
#include "MicroSwarmBridge.hpp"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

struct MicroSwarmBridge::LibraryHandle {
#ifdef _WIN32
  HMODULE handle = nullptr;
#else
  void* handle = nullptr;
#endif

  explicit LibraryHandle(const std::string& path) {
#ifdef _WIN32
    handle = LoadLibraryA(path.c_str());
#else
    handle = dlopen(path.c_str(), RTLD_NOW);
#endif
    if (!handle) {
      throw MycoDBException("Bibliothek konnte nicht geladen werden: " + path);
    }
  }

  ~LibraryHandle() {
#ifdef _WIN32
    if (handle) {
      FreeLibrary(handle);
    }
#else
    if (handle) {
      dlclose(handle);
    }
#endif
  }

  void* symbol(const char* name) const {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(handle, name));
#else
    return dlsym(handle, name);
#endif
  }
};

MicroSwarmBridge::MicroSwarmBridge(const std::string& library_path) {
  library_ = new LibraryHandle(library_path);
  load_symbols();

  int major = 0;
  int minor = 0;
  int patch = 0;
  ms_get_api_version_(&major, &minor, &patch);

  version_ = {major, minor, patch};
  if (version_.major != MS_API_VERSION_MAJOR) {
    throw MycoDBException("API-MAJOR inkompatibel: erwartete " +
                          std::to_string(MS_API_VERSION_MAJOR) +
                          ", gefunden " + std::to_string(version_.major));
  }
}

MicroSwarmBridge::~MicroSwarmBridge() {
  close_db();
  delete library_;
  library_ = nullptr;
}

MicroSwarmBridge::MicroSwarmBridge(MicroSwarmBridge&& other) noexcept {
  *this = std::move(other);
}

MicroSwarmBridge& MicroSwarmBridge::operator=(MicroSwarmBridge&& other) noexcept {
  if (this == &other) return *this;
  close_db();
  delete library_;

  library_ = other.library_;
  db_handle_ = other.db_handle_;
  version_ = other.version_;
  ms_get_api_version_ = other.ms_get_api_version_;
  ms_db_create_ = other.ms_db_create_;
  ms_db_destroy_ = other.ms_db_destroy_;
  ms_db_get_last_error_ = other.ms_db_get_last_error_;
  ms_db_load_myco_ = other.ms_db_load_myco_;
  ms_db_get_payload_count_ = other.ms_db_get_payload_count_;
  ms_db_get_table_count_ = other.ms_db_get_table_count_;
  ms_db_find_payload_by_id_ = other.ms_db_find_payload_by_id_;
  ms_db_get_payload_ = other.ms_db_get_payload_;
  ms_db_get_payload_raw_ = other.ms_db_get_payload_raw_;
  ms_db_get_table_name_ = other.ms_db_get_table_name_;
  ms_db_get_result_count_ = other.ms_db_get_result_count_;
  ms_db_get_result_indices_ = other.ms_db_get_result_indices_;
  ms_db_query_sql_ = other.ms_db_query_sql_;
  ms_db_sql_exec_ = other.ms_db_sql_exec_;
  ms_db_sql_get_column_count_ = other.ms_db_sql_get_column_count_;
  ms_db_sql_get_column_name_ = other.ms_db_sql_get_column_name_;
  ms_db_sql_get_row_count_ = other.ms_db_sql_get_row_count_;
  ms_db_sql_get_cell_ = other.ms_db_sql_get_cell_;
  ms_db_merge_delta_ = other.ms_db_merge_delta_;
  ms_db_undo_last_delta_ = other.ms_db_undo_last_delta_;
  ms_db_get_delta_count_ = other.ms_db_get_delta_count_;
  ms_db_get_tombstone_count_ = other.ms_db_get_tombstone_count_;
  ms_db_get_delta_entry_ = other.ms_db_get_delta_entry_;
  ms_db_get_tombstone_entry_ = other.ms_db_get_tombstone_entry_;
  ms_db_query_simple_focus_ = other.ms_db_query_simple_focus_;
  ms_db_query_by_id_focus_ = other.ms_db_query_by_id_focus_;

  other.library_ = nullptr;
  other.db_handle_ = nullptr;
  other.ms_get_api_version_ = nullptr;
  other.ms_db_create_ = nullptr;
  other.ms_db_destroy_ = nullptr;
  other.ms_db_get_last_error_ = nullptr;
  other.ms_db_load_myco_ = nullptr;
  other.ms_db_get_payload_count_ = nullptr;
  other.ms_db_get_table_count_ = nullptr;
  other.ms_db_find_payload_by_id_ = nullptr;
  other.ms_db_get_payload_ = nullptr;
  other.ms_db_get_payload_raw_ = nullptr;
  other.ms_db_get_table_name_ = nullptr;
  other.ms_db_get_result_count_ = nullptr;
  other.ms_db_get_result_indices_ = nullptr;
  other.ms_db_query_sql_ = nullptr;
  other.ms_db_sql_exec_ = nullptr;
  other.ms_db_sql_get_column_count_ = nullptr;
  other.ms_db_sql_get_column_name_ = nullptr;
  other.ms_db_sql_get_row_count_ = nullptr;
  other.ms_db_sql_get_cell_ = nullptr;
  other.ms_db_merge_delta_ = nullptr;
  other.ms_db_undo_last_delta_ = nullptr;
  other.ms_db_get_delta_count_ = nullptr;
  other.ms_db_get_tombstone_count_ = nullptr;
  other.ms_db_get_delta_entry_ = nullptr;
  other.ms_db_get_tombstone_entry_ = nullptr;
  other.ms_db_query_simple_focus_ = nullptr;
  other.ms_db_query_by_id_focus_ = nullptr;
  return *this;
}

MsApiVersion MicroSwarmBridge::api_version() const {
  return version_;
}

void MicroSwarmBridge::open_db(const std::string& path) {
  if (db_handle_) {
    close_db();
  }
  db_handle_ = ms_db_create_();
  if (!db_handle_) {
    throw MycoDBException("API-Fehler bei ms_db_create");
  }
  int result = ms_db_load_myco_(db_handle_, path.c_str());
  if (result <= 0) {
    std::string err = last_error();
    ms_db_destroy_(db_handle_);
    db_handle_ = nullptr;
    throw MycoDBException("API-Fehler bei ms_db_load_myco: " + err);
  }
}

void MicroSwarmBridge::close_db() {
  if (!db_handle_) return;
  ms_db_destroy_(db_handle_);
  db_handle_ = nullptr;
}

int MicroSwarmBridge::get_payload_count() {
  ensure_open(db_handle_ != nullptr);
  int count = ms_db_get_payload_count_(db_handle_);
  ensure_action(count, "ms_db_get_payload_count");
  return count;
}

int MicroSwarmBridge::get_table_count() {
  ensure_open(db_handle_ != nullptr);
  int count = ms_db_get_table_count_(db_handle_);
  ensure_action(count, "ms_db_get_table_count");
  return count;
}

std::string MicroSwarmBridge::get_table_name(int table_id) {
  ensure_open(db_handle_ != nullptr);
  char buffer[256] = {};
  int result = ms_db_get_table_name_(db_handle_, table_id, buffer, static_cast<int>(sizeof(buffer)));
  if (result <= 0) {
    return {};
  }
  return buffer;
}

std::string MicroSwarmBridge::last_error_message() const {
  return last_error();
}

std::optional<PayloadRow> MicroSwarmBridge::find_payload_by_id(int payload_id) {
  ensure_open(db_handle_ != nullptr);
  int payload_index = ms_db_find_payload_by_id_(db_handle_, payload_id);
  if (payload_index <= 0) {
    return std::nullopt;
  }
  return fetch_payload_row(payload_index);
}

std::vector<PayloadRow> MicroSwarmBridge::query_focus(const std::string& query, int focus_x, int focus_y, int radius) {
  ensure_open(db_handle_ != nullptr);
  auto trim = [](const std::string& input) {
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return std::string();
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
  };
  auto to_lower = [](std::string value) {
    for (auto& ch : value) {
      if (ch >= 'A' && ch <= 'Z') {
        ch = static_cast<char>(ch - 'A' + 'a');
      }
    }
    return value;
  };

  const std::string trimmed = trim(query);
  if (trimmed.empty()) {
    return {};
  }

  const std::string lower = to_lower(trimmed);
  int query_result = 0;

  if (lower.rfind("sql ", 0) == 0) {
    const std::string sql = trim(trimmed.substr(4));
    query_result = ms_db_query_sql_(db_handle_, sql.c_str(), radius);
  } else if (lower.rfind("select", 0) == 0 || lower.rfind("with", 0) == 0) {
    query_result = ms_db_query_sql_(db_handle_, trimmed.c_str(), radius);
  } else {
    const auto space_pos = trimmed.find(' ');
    if (space_pos != std::string::npos) {
      const std::string table = trim(trimmed.substr(0, space_pos));
      const std::string rest = trim(trimmed.substr(space_pos + 1));
      const auto eq_pos = rest.find('=');
      if (eq_pos != std::string::npos) {
        const std::string column = trim(rest.substr(0, eq_pos));
        const std::string value = trim(rest.substr(eq_pos + 1));
        if (!table.empty() && !column.empty() && !value.empty()) {
          query_result = ms_db_query_simple_focus_(
            db_handle_, table.c_str(), column.c_str(), value.c_str(),
            focus_x, focus_y, radius);
        }
      } else {
        char* end_ptr = nullptr;
        long id_value = std::strtol(rest.c_str(), &end_ptr, 10);
        if (!table.empty() && end_ptr && *end_ptr == '\0') {
          query_result = ms_db_query_by_id_focus_(
            db_handle_, table.c_str(), static_cast<int>(id_value),
            focus_x, focus_y, radius);
        }
      }
    }
  }

  if (query_result <= 0) {
    return {};
  }

  const int result_count = ms_db_get_result_count_(db_handle_);
  if (result_count <= 0) {
    return {};
  }
  std::vector<int> indices(static_cast<size_t>(result_count));
  const int collected = ms_db_get_result_indices_(db_handle_, indices.data(), result_count);
  if (collected <= 0) {
    return {};
  }

  std::vector<PayloadRow> rows;
  rows.reserve(static_cast<size_t>(collected));
  for (int i = 0; i < collected; ++i) {
    rows.push_back(fetch_payload_row(indices[static_cast<size_t>(i)]));
  }
  return rows;
}

bool MicroSwarmBridge::query_sql_table(const std::string& query,
                                       bool use_focus,
                                       int focus_x,
                                       int focus_y,
                                       int radius,
                                       std::vector<std::string>& columns,
                                       std::vector<std::vector<std::string>>& rows) {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_sql_exec_ || !ms_db_sql_get_column_count_ || !ms_db_sql_get_column_name_ ||
      !ms_db_sql_get_row_count_ || !ms_db_sql_get_cell_) {
    throw MycoDBException("SQL-Table-API fehlt.");
  }
  const int result = ms_db_sql_exec_(db_handle_, query.c_str(), use_focus ? 1 : 0, focus_x, focus_y, radius);
  if (result <= 0) {
    const std::string err = last_error();
    if (!err.empty()) {
      throw MycoDBException(err);
    }
  }

  const int col_count = ms_db_sql_get_column_count_(db_handle_);
  columns.clear();
  rows.clear();
  if (col_count <= 0) {
    return true;
  }
  columns.resize(static_cast<size_t>(col_count));
  for (int c = 0; c < col_count; ++c) {
    char buf[512] = {};
    if (ms_db_sql_get_column_name_(db_handle_, c, buf, static_cast<int>(sizeof(buf))) > 0) {
      columns[static_cast<size_t>(c)] = buf;
    } else {
      columns[static_cast<size_t>(c)] = "col" + std::to_string(c + 1);
    }
  }
  const int row_count = ms_db_sql_get_row_count_(db_handle_);
  if (row_count <= 0) {
    return true;
  }
  rows.resize(static_cast<size_t>(row_count));
  for (int r = 0; r < row_count; ++r) {
    auto& row = rows[static_cast<size_t>(r)];
    row.resize(static_cast<size_t>(col_count));
    for (int c = 0; c < col_count; ++c) {
      char buf[8192] = {};
      if (ms_db_sql_get_cell_(db_handle_, r, c, buf, static_cast<int>(sizeof(buf))) > 0) {
        row[static_cast<size_t>(c)] = buf;
      } else {
        row[static_cast<size_t>(c)] = "";
      }
    }
  }
  return true;
}

void MicroSwarmBridge::exec_sql(const std::string& query,
                                bool use_focus,
                                int focus_x,
                                int focus_y,
                                int radius) {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_sql_exec_) {
    throw MycoDBException("SQL-Exec-API fehlt.");
  }
  const int result = ms_db_sql_exec_(db_handle_, query.c_str(), use_focus ? 1 : 0, focus_x, focus_y, radius);
  if (result <= 0) {
    const std::string err = last_error();
    if (!err.empty()) {
      throw MycoDBException(err);
    }
  }
}

void MicroSwarmBridge::merge_delta(int agents, int steps, uint32_t seed) {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_merge_delta_) {
    throw MycoDBException("Merge-API fehlt.");
  }
  const int ok = ms_db_merge_delta_(db_handle_, agents, steps, seed);
  if (ok <= 0) {
    const std::string err = last_error();
    if (!err.empty()) {
      throw MycoDBException(err);
    }
  }
}

void MicroSwarmBridge::undo_last_delta() {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_undo_last_delta_) {
    throw MycoDBException("Undo-API fehlt.");
  }
  const int ok = ms_db_undo_last_delta_(db_handle_);
  if (ok <= 0) {
    const std::string err = last_error();
    if (!err.empty()) {
      throw MycoDBException(err);
    }
  }
}

int MicroSwarmBridge::get_delta_count() {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_get_delta_count_) {
    throw MycoDBException("Delta-API fehlt.");
  }
  return ms_db_get_delta_count_(db_handle_);
}

int MicroSwarmBridge::get_tombstone_count() {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_get_tombstone_count_) {
    throw MycoDBException("Delta-API fehlt.");
  }
  return ms_db_get_tombstone_count_(db_handle_);
}

std::string MicroSwarmBridge::get_delta_entry(int index) {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_get_delta_entry_) {
    throw MycoDBException("Delta-API fehlt.");
  }
  char buf[4096] = {};
  if (ms_db_get_delta_entry_(db_handle_, index, buf, static_cast<int>(sizeof(buf))) > 0) {
    return buf;
  }
  return {};
}

std::string MicroSwarmBridge::get_tombstone_entry(int index) {
  ensure_open(db_handle_ != nullptr);
  if (!ms_db_get_tombstone_entry_) {
    throw MycoDBException("Delta-API fehlt.");
  }
  char buf[4096] = {};
  if (ms_db_get_tombstone_entry_(db_handle_, index, buf, static_cast<int>(sizeof(buf))) > 0) {
    return buf;
  }
  return {};
}

void MicroSwarmBridge::load_symbols() {
  ms_get_api_version_ = reinterpret_cast<ms_get_api_version_t>(library_->symbol("ms_get_api_version"));
  ms_db_create_ = reinterpret_cast<ms_db_create_t>(library_->symbol("ms_db_create"));
  ms_db_destroy_ = reinterpret_cast<ms_db_destroy_t>(library_->symbol("ms_db_destroy"));
  ms_db_get_last_error_ = reinterpret_cast<ms_db_get_last_error_t>(library_->symbol("ms_db_get_last_error"));
  ms_db_load_myco_ = reinterpret_cast<ms_db_load_myco_t>(library_->symbol("ms_db_load_myco"));
  ms_db_get_payload_count_ = reinterpret_cast<ms_db_get_payload_count_t>(library_->symbol("ms_db_get_payload_count"));
  ms_db_get_table_count_ = reinterpret_cast<ms_db_get_table_count_t>(library_->symbol("ms_db_get_table_count"));
  ms_db_find_payload_by_id_ = reinterpret_cast<ms_db_find_payload_by_id_t>(library_->symbol("ms_db_find_payload_by_id"));
  ms_db_get_payload_ = reinterpret_cast<ms_db_get_payload_t>(library_->symbol("ms_db_get_payload"));
  ms_db_get_payload_raw_ = reinterpret_cast<ms_db_get_payload_raw_t>(library_->symbol("ms_db_get_payload_raw"));
  ms_db_get_table_name_ = reinterpret_cast<ms_db_get_table_name_t>(library_->symbol("ms_db_get_table_name"));
  ms_db_get_result_count_ = reinterpret_cast<ms_db_get_result_count_t>(library_->symbol("ms_db_get_result_count"));
  ms_db_get_result_indices_ = reinterpret_cast<ms_db_get_result_indices_t>(library_->symbol("ms_db_get_result_indices"));
  ms_db_query_sql_ = reinterpret_cast<ms_db_query_sql_t>(library_->symbol("ms_db_query_sql"));
  ms_db_sql_exec_ = reinterpret_cast<ms_db_sql_exec_t>(library_->symbol("ms_db_sql_exec"));
  ms_db_sql_get_column_count_ = reinterpret_cast<ms_db_sql_get_column_count_t>(library_->symbol("ms_db_sql_get_column_count"));
  ms_db_sql_get_column_name_ = reinterpret_cast<ms_db_sql_get_column_name_t>(library_->symbol("ms_db_sql_get_column_name"));
  ms_db_sql_get_row_count_ = reinterpret_cast<ms_db_sql_get_row_count_t>(library_->symbol("ms_db_sql_get_row_count"));
  ms_db_sql_get_cell_ = reinterpret_cast<ms_db_sql_get_cell_t>(library_->symbol("ms_db_sql_get_cell"));
  ms_db_merge_delta_ = reinterpret_cast<ms_db_merge_delta_t>(library_->symbol("ms_db_merge_delta"));
  ms_db_undo_last_delta_ = reinterpret_cast<ms_db_undo_last_delta_t>(library_->symbol("ms_db_undo_last_delta"));
  ms_db_get_delta_count_ = reinterpret_cast<ms_db_get_delta_count_t>(library_->symbol("ms_db_get_delta_count"));
  ms_db_get_tombstone_count_ = reinterpret_cast<ms_db_get_tombstone_count_t>(library_->symbol("ms_db_get_tombstone_count"));
  ms_db_get_delta_entry_ = reinterpret_cast<ms_db_get_delta_entry_t>(library_->symbol("ms_db_get_delta_entry"));
  ms_db_get_tombstone_entry_ = reinterpret_cast<ms_db_get_tombstone_entry_t>(library_->symbol("ms_db_get_tombstone_entry"));
  ms_db_query_simple_focus_ = reinterpret_cast<ms_db_query_simple_focus_t>(library_->symbol("ms_db_query_simple_focus"));
  ms_db_query_by_id_focus_ = reinterpret_cast<ms_db_query_by_id_focus_t>(library_->symbol("ms_db_query_by_id_focus"));

  if (!ms_get_api_version_ || !ms_db_create_ || !ms_db_destroy_ || !ms_db_get_last_error_ ||
      !ms_db_load_myco_ || !ms_db_get_payload_count_ || !ms_db_get_table_count_ ||
      !ms_db_find_payload_by_id_ || !ms_db_get_payload_ || !ms_db_get_payload_raw_ ||
      !ms_db_get_table_name_ || !ms_db_get_result_count_ || !ms_db_get_result_indices_ ||
      !ms_db_query_sql_ || !ms_db_sql_exec_ || !ms_db_sql_get_column_count_ || !ms_db_sql_get_column_name_ ||
      !ms_db_sql_get_row_count_ || !ms_db_sql_get_cell_ || !ms_db_merge_delta_ || !ms_db_undo_last_delta_ ||
      !ms_db_get_delta_count_ || !ms_db_get_tombstone_count_ || !ms_db_get_delta_entry_ || !ms_db_get_tombstone_entry_ ||
      !ms_db_query_simple_focus_ || !ms_db_query_by_id_focus_) {
    throw MycoDBException("Erforderliche Symbole fehlen in der Bibliothek.");
  }
}

void MicroSwarmBridge::ensure_action(int result, const char* action) {
  if (result <= 0) {
    std::string err = last_error();
    if (!err.empty()) {
      throw MycoDBException(std::string("API-Fehler bei ") + action + ": " + err);
    }
    throw MycoDBException(std::string("API-Fehler bei ") + action);
  }
}

std::string MicroSwarmBridge::last_error() const {
  if (!db_handle_ || !ms_db_get_last_error_) {
    return {};
  }
  const char* message = ms_db_get_last_error_(db_handle_);
  return message ? std::string(message) : std::string();
}

PayloadRow MicroSwarmBridge::fetch_payload_row(int payload_index) {
  PayloadRow row{};
  row.payload_index = payload_index;
  ensure_action(ms_db_get_payload_(db_handle_, payload_index, &row.payload), "ms_db_get_payload");

  char table_buf[256] = {};
  if (ms_db_get_table_name_(db_handle_, row.payload.table_id, table_buf, static_cast<int>(sizeof(table_buf))) > 0) {
    row.table_name = table_buf;
  }

  char raw_buf[4096] = {};
  if (ms_db_get_payload_raw_(db_handle_, payload_index, raw_buf, static_cast<int>(sizeof(raw_buf))) > 0) {
    row.raw_data = raw_buf;
  }

  return row;
}

void MicroSwarmBridge::ensure_open(bool is_open) {
  if (!is_open) {
    throw MycoDBException("Keine offene Datenbankverbindung.");
  }
}
