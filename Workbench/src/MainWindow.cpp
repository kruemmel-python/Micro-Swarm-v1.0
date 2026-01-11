// MainWindow.cpp
#include "MainWindow.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/thread.h>
#include <wx/textdlg.h>

namespace {
constexpr int kIdLoadApi = 1001;
constexpr int kIdOpenDb = 1002;
constexpr int kIdQuery = 1003;
constexpr int kIdCancel = 1004;
constexpr int kIdTableCount = 1005;
constexpr int kIdFocusByPayload = 1006;
constexpr int kIdClearFocus = 1007;
constexpr int kIdFocusFromSelection = 1008;
constexpr int kIdTablesList = 1009;
constexpr int kIdExportCsv = 1010;
constexpr int kIdExportJson = 1011;
constexpr int kIdCopySelection = 1012;
constexpr int kIdPagePrev = 1013;
constexpr int kIdPageNext = 1014;
constexpr int kIdPageSize = 1015;
constexpr int kIdMenuCopyRow = 1100;
constexpr int kIdMenuCopyRowWithNames = 1101;
constexpr int kIdMenuCopyRowUnquoted = 1102;
constexpr int kIdMenuCopyRowWithNamesUnquoted = 1103;
constexpr int kIdMenuCopyRowTabs = 1104;
constexpr int kIdMenuCopyRowWithNamesTabs = 1105;
constexpr int kIdMenuCopyField = 1110;
constexpr int kIdMenuCopyFieldUnquoted = 1111;
constexpr int kIdMenuCopyFieldName = 1112;
constexpr int kIdMenuEditField = 1120;
constexpr int kIdMenuSetNull = 1121;
constexpr int kIdMenuDeleteRows = 1122;
constexpr int kIdMenuPasteRow = 1123;
constexpr int kIdMenuCopyAllFieldNames = 1130;
constexpr int kIdMenuResetSort = 1131;
constexpr int kIdMenuResetColWidths = 1132;
constexpr int kIdUndoDelta = 1140;
constexpr int kIdMergeDelta = 1141;
}  // namespace

wxDEFINE_EVENT(EVT_QUERY_COMPLETE, wxThreadEvent);

namespace {
std::string trim_copy(const std::string& input) {
  const auto start = input.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return {};
  }
  const auto end = input.find_last_not_of(" \t\r\n");
  return input.substr(start, end - start + 1);
}

std::string to_lower_ascii(std::string value) {
  for (auto& ch : value) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return value;
}

bool is_sql_like(const std::string& input) {
  const std::string lower = to_lower_ascii(trim_copy(input));
  return lower.rfind("sql ", 0) == 0 || lower.rfind("select", 0) == 0 || lower.rfind("with", 0) == 0 ||
         lower.rfind("insert", 0) == 0 || lower.rfind("update", 0) == 0 || lower.rfind("delete", 0) == 0;
}

bool sql_has_limit_offset(const std::string& sql) {
  const std::string lower = to_lower_ascii(sql);
  if (lower.rfind("limit ", 0) == 0 || lower.rfind("offset ", 0) == 0) {
    return true;
  }
  if (lower.find(" limit ") != std::string::npos || lower.find(" offset ") != std::string::npos) {
    return true;
  }
  return false;
}

std::string strip_sql_prefix(const std::string& input) {
  const std::string trimmed = trim_copy(input);
  const std::string lower = to_lower_ascii(trimmed);
  if (lower.rfind("sql ", 0) == 0) {
    return trim_copy(trimmed.substr(4));
  }
  return trimmed;
}

bool sql_selects_all_no_limit(const std::string& sql) {
  const std::string lower = to_lower_ascii(strip_sql_prefix(sql));
  if (sql_has_limit_offset(lower)) {
    return false;
  }
  size_t pos = lower.find("select");
  if (pos == std::string::npos) {
    return false;
  }
  pos += 6;
  while (pos < lower.size() && std::isspace(static_cast<unsigned char>(lower[pos]))) {
    ++pos;
  }
  return pos < lower.size() && lower[pos] == '*';
}

bool is_number_literal(const std::string& value) {
  if (value.empty()) return false;
  char* end_ptr = nullptr;
  std::strtod(value.c_str(), &end_ptr);
  return end_ptr && *end_ptr == '\0';
}

std::string escape_sql_value(const std::string& value) {
  if (value.empty()) {
    return "''";
  }
  std::string out;
  out.reserve(value.size() + 4);
  for (char ch : value) {
    if (ch == '\'') {
      out.push_back('\'');
      out.push_back('\'');
    } else {
      out.push_back(ch);
    }
  }
  return "'" + out + "'";
}

std::string format_sql_value(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty()) return "''";
  const std::string lower = to_lower_ascii(trimmed);
  if (lower == "null") return "NULL";
  if (is_number_literal(trimmed)) return trimmed;
  return escape_sql_value(trimmed);
}

std::string parse_single_table(const std::string& sql) {
  const std::string lower = to_lower_ascii(sql);
  if (lower.find(" join ") != std::string::npos) return {};
  if (lower.find(" cross ") != std::string::npos) return {};
  if (lower.find(" union ") != std::string::npos) return {};
  if (lower.find(" from ") == std::string::npos && lower.rfind("from ", 0) != 0) return {};
  size_t from_pos = lower.find("from");
  if (from_pos == std::string::npos) return {};
  std::string tail = trim_copy(sql.substr(from_pos + 4));
  if (tail.empty()) return {};
  std::string table;
  std::stringstream ss(tail);
  ss >> table;
  if (table.empty()) return {};
  if (table == "(") return {};
  return table;
}

std::string find_pk_column(const std::string& table, const std::vector<std::string>& columns) {
  const std::string lower_table = to_lower_ascii(table);
  std::string best;
  for (const auto& col : columns) {
    const std::string lc = to_lower_ascii(col);
    if (lc == "id") return col;
    if (lc == lower_table + "id") return col;
    if (lc == lower_table + "_id") return col;
    if (lc.size() >= 2 && lc.substr(lc.size() - 2) == "id") {
      best = col;
    }
  }
  return best;
}

bool parse_csv_line(const std::string& line, std::vector<std::string>& fields) {
  fields.clear();
  std::string field;
  bool in_quotes = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char ch = line[i];
    if (in_quotes) {
      if (ch == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          in_quotes = false;
        }
      } else {
        field.push_back(ch);
      }
      continue;
    }
    if (ch == '"') {
      in_quotes = true;
    } else if (ch == ',') {
      fields.push_back(field);
      field.clear();
    } else {
      field.push_back(ch);
    }
  }
  fields.push_back(field);
  return true;
}

bool parse_csv_lines(const std::vector<std::string>& lines,
                     std::vector<std::string>& columns,
                     std::vector<std::vector<std::string>>& rows,
                     std::string& error) {
  columns.clear();
  rows.clear();
  error.clear();
  if (lines.empty()) {
    error = "CSV: leere Ausgabe.";
    return false;
  }
  if (lines.front().rfind("SQL-Fehler:", 0) == 0) {
    error = lines.front();
    return false;
  }
  std::vector<std::string> parsed;
  parse_csv_line(lines.front(), columns);
  if (columns.empty()) {
    error = "CSV: keine Spalten.";
    return false;
  }
  size_t max_cols = columns.size();
  for (size_t i = 1; i < lines.size(); ++i) {
    parse_csv_line(lines[i], parsed);
    if (parsed.size() > max_cols) {
      max_cols = parsed.size();
    }
    rows.push_back(parsed);
  }
  if (max_cols > columns.size()) {
    for (size_t i = columns.size(); i < max_cols; ++i) {
      columns.push_back("col" + std::to_string(i + 1));
    }
  }
  for (auto& row : rows) {
    if (row.size() < max_cols) {
      row.resize(max_cols);
    }
  }
  return true;
}

std::vector<std::string> split_by_any(const std::string& input, const std::string& separators) {
  std::vector<std::string> out;
  std::string token;
  for (char ch : input) {
    if (separators.find(ch) != std::string::npos) {
      if (!token.empty()) {
        out.push_back(trim_copy(token));
        token.clear();
      }
    } else {
      token.push_back(ch);
    }
  }
  if (!token.empty()) {
    out.push_back(trim_copy(token));
  }
  return out;
}
}  // namespace

MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, "MycoDB Workbench", wxDefaultPosition, wxSize(1280, 780)) {
  aui_manager_.SetManagedWindow(this);

  auto* content = new wxPanel(this);
  auto* root = new wxBoxSizer(wxVERTICAL);

  auto* navigator = new wxPanel(this);
  auto* nav_root = new wxBoxSizer(wxVERTICAL);
  tables_list_ = new wxListBox(navigator, kIdTablesList);
  schema_view_ = new wxTextCtrl(navigator, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  schema_view_->SetMinSize(wxSize(220, 180));
  nav_root->Add(new wxStaticText(navigator, wxID_ANY, "Tabellen"), 0, wxLEFT | wxRIGHT | wxTOP, 6);
  nav_root->Add(tables_list_, 1, wxEXPAND | wxALL, 6);
  nav_root->Add(new wxStaticText(navigator, wxID_ANY, "Schema (Sample)"), 0, wxLEFT | wxRIGHT | wxTOP, 6);
  nav_root->Add(schema_view_, 1, wxEXPAND | wxALL, 6);
  navigator->SetSizer(nav_root);

  auto* connect_box = new wxStaticBoxSizer(wxHORIZONTAL, content, "Verbindung");
  library_path_ = new wxTextCtrl(content, wxID_ANY);
  library_path_->SetHint("Pfad zur micro_swarm.dll / libmicro_swarm.so");
  db_path_ = new wxTextCtrl(content, wxID_ANY);
  db_path_->SetHint("Pfad zur .myco Datei");
  auto* load_btn = new wxButton(content, kIdLoadApi, "API laden");
  auto* open_btn = new wxButton(content, kIdOpenDb, "DB oeffnen");
  open_btn->Enable(false);

  connect_box->Add(library_path_, 2, wxEXPAND | wxALL, 4);
  connect_box->Add(db_path_, 2, wxEXPAND | wxALL, 4);
  connect_box->Add(load_btn, 0, wxEXPAND | wxALL, 4);
  connect_box->Add(open_btn, 0, wxEXPAND | wxALL, 4);

  auto* status_row = new wxBoxSizer(wxHORIZONTAL);
  api_badge_ = new wxStaticText(content, wxID_ANY, "API: aus");
  db_badge_ = new wxStaticText(content, wxID_ANY, "DB: aus");
  api_version_label_ = new wxStaticText(content, wxID_ANY, "API v-.-.-");
  status_label_ = new wxStaticText(content, wxID_ANY, "Nicht verbunden.");
  api_badge_->SetWindowStyle(wxBORDER_SIMPLE);
  db_badge_->SetWindowStyle(wxBORDER_SIMPLE);

  status_row->Add(api_badge_, 0, wxALL, 4);
  status_row->Add(db_badge_, 0, wxALL, 4);
  status_row->Add(api_version_label_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
  auto* fav_row = new wxBoxSizer(wxHORIZONTAL);
  fav_save_btn_ = new wxButton(content, wxID_ANY, "Fav+");
  fav_row->Add(fav_save_btn_, 0, wxLEFT | wxRIGHT, 4);
  for (int i = 0; i < 6; ++i) {
    auto* btn = new wxButton(content, wxID_ANY, "Fav");
    fav_buttons_.push_back(btn);
    fav_row->Add(btn, 0, wxLEFT | wxRIGHT, 2);
  }
  status_row->Add(fav_row, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
  status_row->AddStretchSpacer();
  status_row->Add(status_label_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

  auto* query_box = new wxStaticBoxSizer(wxVERTICAL, content, "Query");
  query_edit_ = new wxStyledTextCtrl(content, wxID_ANY, wxDefaultPosition, wxSize(-1, 120));
  query_edit_->StyleSetFont(wxSTC_STYLE_DEFAULT,
                            wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
  query_edit_->StyleClearAll();
  query_edit_->SetWrapMode(wxSTC_WRAP_WORD);
  query_edit_->Bind(wxEVT_KEY_DOWN, &MainWindow::on_query_key_down, this);

  auto* query_controls = new wxBoxSizer(wxHORIZONTAL);
  focus_x_ = new wxSpinCtrl(content, wxID_ANY);
  focus_x_->SetRange(-100000, 100000);
  focus_x_->SetValue(0);
  focus_y_ = new wxSpinCtrl(content, wxID_ANY);
  focus_y_->SetRange(-100000, 100000);
  focus_y_->SetValue(0);
  radius_ = new wxSpinCtrl(content, wxID_ANY);
  radius_->SetRange(0, 100000);
  radius_->SetValue(5500);
  focus_payload_id_ = new wxTextCtrl(content, wxID_ANY);
  focus_payload_id_->SetHint("payload_id");
  query_mode_ = new wxChoice(content, wxID_ANY);
  query_mode_->Append("Auto");
  query_mode_->Append("SQL");
  query_mode_->Append("Shell");
  query_mode_->SetSelection(0);
  page_size_ctrl_ = new wxSpinCtrl(content, kIdPageSize);
  page_size_ctrl_->SetRange(10, 100000);
  page_size_ctrl_->SetValue(200);
  page_label_ = new wxStaticText(content, wxID_ANY, "Page 1");
  page_prev_btn_ = new wxButton(content, kIdPagePrev, "Prev");
  page_next_btn_ = new wxButton(content, kIdPageNext, "Next");
  undo_btn_ = new wxButton(content, kIdUndoDelta, "Undo");
  merge_btn_ = new wxButton(content, kIdMergeDelta, "Merge");
  page_prev_btn_->Enable(false);
  page_next_btn_->Enable(false);
  auto* query_btn = new wxButton(content, kIdQuery, "Run");
  auto* cancel_btn = new wxButton(content, kIdCancel, "Cancel");
  auto* table_count_btn = new wxButton(content, kIdTableCount, "Table Count");
  auto* export_csv_btn = new wxButton(content, kIdExportCsv, "Export CSV");
  auto* export_json_btn = new wxButton(content, kIdExportJson, "Export JSON");
  auto* copy_btn = new wxButton(content, kIdCopySelection, "Copy");
  auto* focus_btn = new wxButton(content, kIdFocusByPayload, "Set Focus");
  auto* clear_focus_btn = new wxButton(content, kIdClearFocus, "Clear Focus");
  auto* focus_sel_btn = new wxButton(content, kIdFocusFromSelection, "Use Selection");
  cancel_btn->Enable(false);

  query_controls->Add(new wxStaticText(content, wxID_ANY, "X"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
  query_controls->Add(focus_x_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(new wxStaticText(content, wxID_ANY, "Y"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
  query_controls->Add(focus_y_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(new wxStaticText(content, wxID_ANY, "R"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
  query_controls->Add(radius_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(focus_payload_id_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(query_mode_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(page_label_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
  query_controls->Add(page_prev_btn_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(page_next_btn_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(page_size_ctrl_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(undo_btn_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(merge_btn_, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(focus_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(clear_focus_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(focus_sel_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->AddStretchSpacer();
  query_controls->Add(query_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(cancel_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(table_count_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(export_csv_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(export_json_btn, 0, wxEXPAND | wxALL, 4);
  query_controls->Add(copy_btn, 0, wxEXPAND | wxALL, 4);

  query_box->Add(query_edit_, 0, wxEXPAND | wxALL, 4);
  query_box->Add(query_controls, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

  result_tabs_ = new wxAuiNotebook(content, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_TAB_MOVE);
  sql_grid_ = new wxGrid(result_tabs_, wxID_ANY);
  sql_grid_->CreateGrid(0, 1);
  sql_grid_->SetColLabelValue(0, "raw");
  sql_grid_->EnableEditing(false);
  sql_grid_->EnableGridLines(true);

  debug_grid_ = new wxGrid(result_tabs_, wxID_ANY);
  debug_grid_->CreateGrid(0, 9);
  debug_grid_->SetColLabelValue(0, "payload_id");
  debug_grid_->SetColLabelValue(1, "table_id");
  debug_grid_->SetColLabelValue(2, "id");
  debug_grid_->SetColLabelValue(3, "x");
  debug_grid_->SetColLabelValue(4, "y");
  debug_grid_->SetColLabelValue(5, "field_count");
  debug_grid_->SetColLabelValue(6, "fk_count");
  debug_grid_->SetColLabelValue(7, "table_name");
  debug_grid_->SetColLabelValue(8, "raw_data");
  debug_grid_->EnableEditing(false);
  debug_grid_->EnableGridLines(true);

  result_tabs_->AddPage(sql_grid_, "SQL Result", true);
  result_tabs_->AddPage(debug_grid_, "Payload Debug", false);

  tools_tabs_ = new wxAuiNotebook(content, wxID_ANY, wxDefaultPosition, wxSize(-1, 220),
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_TAB_MOVE);

  auto* queries_panel = new wxPanel(tools_tabs_);
  auto* queries_root = new wxBoxSizer(wxVERTICAL);
  query_tabs_ = new wxAuiNotebook(queries_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_TAB_MOVE | wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
  queries_root->Add(query_tabs_, 1, wxEXPAND | wxALL, 4);
  queries_panel->SetSizer(queries_root);
  tools_tabs_->AddPage(queries_panel, "Queries", true);

  auto* diff_panel = new wxPanel(tools_tabs_);
  auto* diff_root = new wxBoxSizer(wxVERTICAL);
  diff_view_ = new wxTextCtrl(diff_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  diff_root->Add(diff_view_, 1, wxEXPAND | wxALL, 4);
  diff_panel->SetSizer(diff_root);
  tools_tabs_->AddPage(diff_panel, "Diff", false);

  auto* filter_panel = new wxPanel(tools_tabs_);
  auto* filter_root = new wxBoxSizer(wxHORIZONTAL);
  filter_column_ = new wxChoice(filter_panel, wxID_ANY);
  filter_text_ = new wxTextCtrl(filter_panel, wxID_ANY);
  filter_clear_btn_ = new wxButton(filter_panel, wxID_ANY, "Clear");
  filter_root->Add(new wxStaticText(filter_panel, wxID_ANY, "Filter"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
  filter_root->Add(filter_column_, 0, wxEXPAND | wxALL, 4);
  filter_root->Add(filter_text_, 1, wxEXPAND | wxALL, 4);
  filter_root->Add(filter_clear_btn_, 0, wxEXPAND | wxALL, 4);
  filter_panel->SetSizer(filter_root);
  tools_tabs_->AddPage(filter_panel, "Filter", false);

  auto* export_panel = new wxPanel(tools_tabs_);
  auto* export_root = new wxBoxSizer(wxVERTICAL);
  auto* export_top = new wxBoxSizer(wxHORIZONTAL);
  export_target_ = new wxChoice(export_panel, wxID_ANY);
  export_target_->Append("SQL Result");
  export_target_->Append("Payload Debug");
  export_target_->SetSelection(0);
  export_format_ = new wxChoice(export_panel, wxID_ANY);
  export_format_->Append("csv");
  export_format_->Append("json");
  export_format_->SetSelection(0);
  export_run_btn_ = new wxButton(export_panel, wxID_ANY, "Export...");
  export_top->Add(new wxStaticText(export_panel, wxID_ANY, "Target"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
  export_top->Add(export_target_, 0, wxEXPAND | wxALL, 4);
  export_top->Add(new wxStaticText(export_panel, wxID_ANY, "Format"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
  export_top->Add(export_format_, 0, wxEXPAND | wxALL, 4);
  export_top->Add(export_run_btn_, 0, wxEXPAND | wxALL, 4);
  export_columns_ = new wxCheckListBox(export_panel, wxID_ANY);
  export_root->Add(export_top, 0, wxEXPAND | wxALL, 2);
  export_root->Add(export_columns_, 1, wxEXPAND | wxALL, 4);
  export_panel->SetSizer(export_root);
  tools_tabs_->AddPage(export_panel, "Export", false);

  auto* auto_panel = new wxPanel(tools_tabs_);
  auto* auto_root = new wxBoxSizer(wxHORIZONTAL);
  auto_explain_cb_ = new wxCheckBox(auto_panel, wxID_ANY, "Auto Explain");
  auto_stats_cb_ = new wxCheckBox(auto_panel, wxID_ANY, "Auto Stats");
  auto_explain_view_ = new wxTextCtrl(auto_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                      wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  auto_stats_view_ = new wxTextCtrl(auto_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  auto_root->Add(auto_explain_cb_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
  auto_root->Add(auto_stats_cb_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
  auto_root->Add(auto_explain_view_, 1, wxEXPAND | wxALL, 4);
  auto_root->Add(auto_stats_view_, 1, wxEXPAND | wxALL, 4);
  auto_panel->SetSizer(auto_root);
  tools_tabs_->AddPage(auto_panel, "Auto", false);

  root->Add(connect_box, 0, wxEXPAND | wxALL, 6);
  root->Add(status_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);
  root->Add(query_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
  root->Add(result_tabs_, 1, wxEXPAND | wxALL, 6);
  root->Add(tools_tabs_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

  content->SetSizer(root);
  aui_manager_.AddPane(content, wxAuiPaneInfo().CenterPane());
  aui_manager_.AddPane(navigator, wxAuiPaneInfo().Left().Caption("Navigator").BestSize(260, -1).MinSize(220, 200));

  log_view_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 160),
                             wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  aui_manager_.AddPane(log_view_, wxAuiPaneInfo().Bottom().Caption("Log").BestSize(-1, 180).MinSize(200, 120));
  aui_manager_.Update();

  set_badge(api_badge_, false, "API: aus");
  set_badge(db_badge_, false, "DB: aus");
  update_log();

  Bind(wxEVT_BUTTON, &MainWindow::on_connect, this, kIdLoadApi);
  Bind(wxEVT_BUTTON, &MainWindow::on_open_db, this, kIdOpenDb);
  Bind(wxEVT_BUTTON, &MainWindow::on_query_focus, this, kIdQuery);
  Bind(wxEVT_BUTTON, &MainWindow::on_cancel_query, this, kIdCancel);
  Bind(wxEVT_BUTTON, &MainWindow::on_table_count, this, kIdTableCount);
  Bind(wxEVT_BUTTON, &MainWindow::on_focus_by_payload, this, kIdFocusByPayload);
  Bind(wxEVT_BUTTON, &MainWindow::on_clear_focus, this, kIdClearFocus);
  Bind(wxEVT_BUTTON, &MainWindow::on_focus_from_selection, this, kIdFocusFromSelection);
  Bind(wxEVT_BUTTON, &MainWindow::on_export_csv, this, kIdExportCsv);
  Bind(wxEVT_BUTTON, &MainWindow::on_export_json, this, kIdExportJson);
  Bind(wxEVT_BUTTON, &MainWindow::on_copy_selection, this, kIdCopySelection);
  Bind(wxEVT_BUTTON, &MainWindow::on_page_prev, this, kIdPagePrev);
  Bind(wxEVT_BUTTON, &MainWindow::on_page_next, this, kIdPageNext);
  Bind(wxEVT_SPINCTRL, &MainWindow::on_page_size_changed, this, kIdPageSize);
  Bind(wxEVT_TEXT, &MainWindow::on_page_size_changed, this, kIdPageSize);
  Bind(wxEVT_BUTTON, &MainWindow::on_undo_delta, this, kIdUndoDelta);
  Bind(wxEVT_BUTTON, &MainWindow::on_merge_delta, this, kIdMergeDelta);
  sql_grid_->Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &MainWindow::on_grid_cell_menu, this);
  sql_grid_->Bind(wxEVT_GRID_LABEL_RIGHT_CLICK, &MainWindow::on_grid_label_menu, this);
  debug_grid_->Bind(wxEVT_GRID_CELL_RIGHT_CLICK, &MainWindow::on_grid_cell_menu, this);
  debug_grid_->Bind(wxEVT_GRID_LABEL_RIGHT_CLICK, &MainWindow::on_grid_label_menu, this);
  if (result_tabs_) {
    result_tabs_->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED,
                       [this](wxAuiNotebookEvent&) {
                         update_filter_columns();
                         update_export_columns();
                       });
  }
  Bind(EVT_QUERY_COMPLETE, &MainWindow::on_query_complete, this);
  Bind(wxEVT_LISTBOX, &MainWindow::on_table_selected, this, kIdTablesList);
  Bind(wxEVT_LISTBOX_DCLICK, &MainWindow::on_table_activated, this, kIdTablesList);

  if (fav_save_btn_) {
    fav_save_btn_->Bind(wxEVT_BUTTON, &MainWindow::on_favorite_save, this);
  }
  for (size_t i = 0; i < fav_buttons_.size(); ++i) {
    if (fav_buttons_[i]) {
      fav_buttons_[i]->SetClientData(reinterpret_cast<void*>(i));
      fav_buttons_[i]->Bind(wxEVT_BUTTON, &MainWindow::on_favorite_run, this);
      fav_buttons_[i]->Enable(false);
    }
  }
  if (filter_text_) {
    filter_text_->Bind(wxEVT_TEXT, &MainWindow::on_filter_changed, this);
  }
  if (filter_column_) {
    filter_column_->Bind(wxEVT_CHOICE, &MainWindow::on_filter_changed, this);
  }
  if (filter_clear_btn_) {
    filter_clear_btn_->Bind(wxEVT_BUTTON, &MainWindow::on_filter_clear, this);
  }
  if (export_target_) {
    export_target_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { update_export_columns(); });
  }
  if (auto_explain_cb_) {
    auto_explain_cb_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { refresh_tools_view(); });
  }
  if (auto_stats_cb_) {
    auto_stats_cb_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { refresh_tools_view(); });
  }
  if (export_run_btn_) {
    export_run_btn_->Bind(wxEVT_BUTTON, &MainWindow::on_export_run, this);
  }
  if (query_tabs_) {
    query_tabs_->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &MainWindow::on_query_tab_close, this);
  }
}

MainWindow::~MainWindow() {
  cancel_query();
  if (query_thread_.joinable()) {
    query_thread_.join();
  }
  aui_manager_.UnInit();
}

void MainWindow::set_status(const std::string& text) {
  status_label_->SetLabel(text);
}

void MainWindow::show_error(const std::string& text) {
  wxMessageBox(text, "Fehler", wxOK | wxICON_ERROR, this);
}

void MainWindow::set_badge(wxStaticText* badge, bool ok, const std::string& text) {
  badge->SetLabel(text);
  if (ok) {
    badge->SetBackgroundColour(wxColour(56, 132, 80));
    badge->SetForegroundColour(wxColour(255, 255, 255));
  } else {
    badge->SetBackgroundColour(wxColour(148, 55, 52));
    badge->SetForegroundColour(wxColour(255, 255, 255));
  }
}

void MainWindow::populate_payloads(const std::vector<PayloadRow>& payloads) {
  debug_columns_ = {
    "payload_id",
    "table_id",
    "id",
    "x",
    "y",
    "field_count",
    "fk_count",
    "table_name",
    "raw_data"
  };
  debug_rows_.clear();
  debug_rows_.reserve(payloads.size());
  for (const auto& row : payloads) {
    debug_rows_.push_back({
      std::to_string(row.payload_index),
      std::to_string(row.payload.table_id),
      std::to_string(row.payload.id),
      std::to_string(row.payload.x),
      std::to_string(row.payload.y),
      std::to_string(row.payload.field_count),
      std::to_string(row.payload.fk_count),
      row.table_name,
      row.raw_data
    });
  }
  debug_rows_original_ = debug_rows_;
  apply_grid_data(debug_grid_, debug_columns_, debug_rows_);
}

void MainWindow::clear_sql_result() {
  if (sql_grid_->GetNumberRows() > 0) {
    sql_grid_->DeleteRows(0, sql_grid_->GetNumberRows());
  }
  if (sql_grid_->GetNumberCols() > 0) {
    sql_grid_->DeleteCols(0, sql_grid_->GetNumberCols());
  }
  sql_columns_.clear();
  sql_rows_.clear();
  sql_rows_original_.clear();
}

void MainWindow::clear_debug_result() {
  if (debug_grid_->GetNumberRows() > 0) {
    debug_grid_->DeleteRows(0, debug_grid_->GetNumberRows());
  }
  if (debug_grid_->GetNumberCols() > 0) {
    debug_grid_->DeleteCols(0, debug_grid_->GetNumberCols());
  }
  debug_columns_.clear();
  debug_rows_.clear();
  debug_rows_original_.clear();
}

void MainWindow::populate_sql_result(const std::vector<PayloadRow>& payloads) {
  clear_sql_result();
  if (payloads.empty()) {
    sql_columns_.assign(1, "raw");
    sql_rows_.clear();
    sql_rows_original_.clear();
    apply_grid_data(sql_grid_, sql_columns_, sql_rows_);
    return;
  }

  std::vector<std::string> columns;
  columns.push_back("payload_id");
  std::vector<std::vector<std::string>> rows;
  rows.reserve(payloads.size());

  bool has_kv = false;
  for (const auto& row : payloads) {
    if (row.raw_data.find('=') != std::string::npos) {
      has_kv = true;
      break;
    }
  }

  if (has_kv) {
    for (const auto& row : payloads) {
      std::vector<std::string> row_values;
      row_values.resize(columns.size());
      row_values[0] = std::to_string(row.payload_index);

      auto tokens = split_by_any(row.raw_data, ",;|");
      for (const auto& token : tokens) {
        const auto eq_pos = token.find('=');
        if (eq_pos == std::string::npos) {
          continue;
        }
        const std::string key = trim_copy(token.substr(0, eq_pos));
        const std::string value = trim_copy(token.substr(eq_pos + 1));
        if (key.empty()) {
          continue;
        }
        auto it = std::find(columns.begin(), columns.end(), key);
        if (it == columns.end()) {
          columns.push_back(key);
          for (auto& existing_row : rows) {
            existing_row.push_back("");
          }
          row_values.push_back(value);
        } else {
          const size_t index = static_cast<size_t>(std::distance(columns.begin(), it));
          if (index >= row_values.size()) {
            row_values.resize(columns.size());
          }
          row_values[index] = value;
        }
      }
      rows.push_back(row_values);
    }
  } else {
    for (const auto& row : payloads) {
      std::vector<std::string> cells;
      cells.push_back(std::to_string(row.payload_index));
      if (row.raw_data.find('|') != std::string::npos) {
        auto parts = split_by_any(row.raw_data, "|");
        for (const auto& part : parts) {
          cells.push_back(part);
        }
        if (columns.size() < cells.size()) {
          for (size_t i = columns.size(); i < cells.size(); ++i) {
            std::ostringstream label;
            label << "col" << i;
            columns.push_back(label.str());
          }
        }
      } else {
        cells.push_back(row.raw_data);
        if (columns.size() < 2) {
          columns.push_back("raw");
        }
      }
      rows.push_back(cells);
    }
  }

  sql_columns_ = columns;
  sql_rows_ = rows;
  sql_rows_original_ = rows;
  apply_grid_data(sql_grid_, sql_columns_, sql_rows_);
}

std::optional<int> MainWindow::selected_payload_id() const {
  if (result_tabs_->GetSelection() == 0) {
    const int row = sql_grid_->GetGridCursorRow();
    if (row >= 0 && sql_grid_->GetNumberCols() > 0) {
      const wxString value = sql_grid_->GetCellValue(row, 0);
      long payload_id = 0;
      if (value.ToLong(&payload_id)) {
        return static_cast<int>(payload_id);
      }
    }
  } else {
    const int row = debug_grid_->GetGridCursorRow();
    if (row >= 0 && debug_grid_->GetNumberCols() > 0) {
      const wxString value = debug_grid_->GetCellValue(row, 0);
      long payload_id = 0;
      if (value.ToLong(&payload_id)) {
        return static_cast<int>(payload_id);
      }
    }
  }
  return std::nullopt;
}

void MainWindow::start_query(const std::string& query, int focus_x, int focus_y, int radius) {
  if (!bridge_ || !db_ready_) {
    show_error("Bitte zuerst API laden und DB oeffnen.");
    return;
  }
  if (query_running_.load()) {
    set_status("Query laeuft bereits.");
    return;
  }

  std::string actual = trim_copy(query);
  if (actual.empty()) {
    show_error("Query ist leer.");
    return;
  }

  if (!bypass_shell_command_ && handle_shell_command(actual)) {
    return;
  }
  const int mode = query_mode_ ? query_mode_->GetSelection() : 0;
  if (mode == 1) {
    const std::string lower = [](std::string value) {
      for (auto& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
          ch = static_cast<char>(ch - 'A' + 'a');
        }
      }
      return value;
    }(actual);
    if (lower.rfind("sql ", 0) != 0 && lower.rfind("select", 0) != 0 && lower.rfind("with", 0) != 0) {
      actual = "sql " + actual;
    }
  } else if (mode == 2) {
    const std::string prefix = "sql ";
    if (actual.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), actual.begin(),
                   [](char a, char b) { return a == static_cast<char>(std::tolower(static_cast<unsigned char>(b))); })) {
      actual = trim_copy(actual.substr(4));
    }
  }

  if (is_sql_like(actual) && sql_selects_all_no_limit(actual)) {
    wxMessageDialog confirm(
      this,
      "WARNUNG: SELECT * ohne LIMIT/OFFSET kann bei grossen Tabellen sehr langsam sein oder das System "
      "instabil machen.\n"
      "Empfehlung: nutze LIMIT/OFFSET oder Paging.\n\n"
      "Trotzdem ausfuehren?",
      "Grosses Ergebnis",
      wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    if (confirm.ShowModal() != wxID_YES) {
      set_status("Query abgebrochen (ohne LIMIT/OFFSET).");
      return;
    }
  }

  if (default_limit_ > 0 && !is_sql_like(actual)) {
    std::string lower_actual = actual;
    for (auto& ch : lower_actual) {
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    if (lower_actual.find(" limit ") == std::string::npos &&
        lower_actual.rfind("limit ", 0) != 0 &&
        lower_actual.find(" set limit ") == std::string::npos) {
      actual += " LIMIT " + std::to_string(default_limit_);
    }
  }

  if (!skip_history_next_) {
    if (!query_history_.empty()) {
      if (query_history_.back() != actual) {
        query_history_.push_back(actual);
      }
    } else {
      query_history_.push_back(actual);
    }
  }
  history_index_ = static_cast<int>(query_history_.size());
  last_query_ = actual;
  last_error_.clear();
  if (!skip_history_next_) {
    last_user_query_ = actual;
  }
  skip_history_next_ = false;

  const uint64_t token = ++query_token_;
  query_running_.store(true);
  set_status("Query laeuft...");

  if (query_thread_.joinable()) {
    query_thread_.join();
  }

  auto* query_btn = FindWindow(kIdQuery);
  auto* cancel_btn = FindWindow(kIdCancel);
  if (query_btn) query_btn->Enable(false);
  if (cancel_btn) cancel_btn->Enable(true);

  const bool sql_table = is_sql_like(actual);
  std::string sql_for_exec = actual;
  if (sql_table) {
    const std::string lower = to_lower_ascii(sql_for_exec);
    if (lower.rfind("sql ", 0) == 0) {
      sql_for_exec = trim_copy(sql_for_exec.substr(4));
    }
  }
  bool apply_paging = sql_table && !sql_has_limit_offset(sql_for_exec);
  if (apply_paging) {
    if (!keep_page_index_next_ && last_user_query_ != actual) {
      page_index_ = 0;
    }
  } else {
    page_index_ = 0;
  }
  keep_page_index_next_ = false;

  int page_size = page_size_ctrl_ ? page_size_ctrl_->GetValue() : 200;
  if (page_size < 1) page_size = 200;
  std::string paged_sql = sql_for_exec;
  if (apply_paging) {
    const int offset = page_index_ * page_size;
    paged_sql += " LIMIT " + std::to_string(page_size) + " OFFSET " + std::to_string(offset);
  }
  last_query_paging_ = apply_paging;
  if (page_label_) {
    if (apply_paging) {
      page_label_->SetLabel("Page " + std::to_string(page_index_ + 1));
    } else {
      page_label_->SetLabel("Page -");
    }
  }
  if (page_prev_btn_) {
    page_prev_btn_->Enable(apply_paging && page_index_ > 0);
  }
  if (page_next_btn_) {
    page_next_btn_->Enable(apply_paging);
  }
  if (sql_table) {
    last_exec_query_ = "sql " + paged_sql;
  } else {
    last_exec_query_ = paged_sql;
  }

  start_query_task(actual, [this, paged_sql, sql_table, apply_paging, focus_x, focus_y, radius]() {
    QueryResult result;
    if (sql_table) {
      bridge_->query_sql_table(paged_sql, focus_set_, focus_x, focus_y, radius, result.sql_columns, result.sql_rows);
      result.has_sql_table = true;
    } else {
      result.payloads = bridge_->query_focus(paged_sql, focus_x, focus_y, radius);
    }
    return result;
  });
}

void MainWindow::cancel_query() {
  if (!query_running_.load()) {
    return;
  }
  ++query_token_;
  set_status("Abbruch angefordert. Warte auf Query.");
  last_error_ = "Query abgebrochen.";
  update_log();
}

void MainWindow::start_query_task(const std::string& label, std::function<QueryResult()> task) {
  if (query_thread_.joinable()) {
    query_thread_.join();
  }

  (void)label;
  const uint64_t token = query_token_.load();
  query_thread_ = std::thread([this, token, label, task]() {
    QueryResult result;
    result.token = token;
    const auto start = std::chrono::steady_clock::now();
    try {
      result = task();
      result.token = token;
    } catch (const std::exception& ex) {
      result.ok = false;
      result.error = ex.what();
    }
    const auto end = std::chrono::steady_clock::now();
    result.duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    auto* event = new wxThreadEvent(EVT_QUERY_COMPLETE);
    event->SetPayload(result);
    wxQueueEvent(this, event);
  });
}

void MainWindow::on_connect(wxCommandEvent&) {
  try {
    bridge_ = std::make_unique<MicroSwarmBridge>(library_path_->GetValue().ToStdString());
    const auto version = bridge_->api_version();
    api_ready_ = true;
    db_ready_ = false;
    set_badge(api_badge_, true, "API: ok");
    set_badge(db_badge_, false, "DB: aus");
    api_version_label_->SetLabel(
      "API v" + std::to_string(version.major) + "." +
      std::to_string(version.minor) + "." + std::to_string(version.patch));
    set_status("API geladen.");
    last_error_.clear();
    update_log();

    const std::filesystem::path lib_path = std::filesystem::path(library_path_->GetValue().ToStdString());
    const std::filesystem::path candidate = lib_path.parent_path() /
#ifdef _WIN32
      "micro_swarm.exe";
#else
      "micro_swarm";
#endif
    if (std::filesystem::exists(candidate)) {
      shell_exe_path_ = candidate.string();
    }

    auto* open_btn = FindWindow(kIdOpenDb);
    if (open_btn) {
      open_btn->Enable(true);
    }
  } catch (const std::exception& ex) {
    api_ready_ = false;
    db_ready_ = false;
    set_badge(api_badge_, false, "API: aus");
    set_badge(db_badge_, false, "DB: aus");
    api_version_label_->SetLabel("API v-.-.-");
    last_error_ = ex.what();
    update_log();
    show_error(ex.what());
  }
}

void MainWindow::on_open_db(wxCommandEvent&) {
  if (!bridge_ || !api_ready_) {
    show_error("Bitte zuerst API laden.");
    return;
  }
  try {
    bridge_->open_db(db_path_->GetValue().ToStdString());
    db_ready_ = true;
    set_badge(db_badge_, true, "DB: ok");
    set_status("DB geladen.");
    last_error_.clear();
    load_tables();
    update_log();
  } catch (const std::exception& ex) {
    db_ready_ = false;
    set_badge(db_badge_, false, "DB: aus");
    last_error_ = ex.what();
    update_log();
    show_error(ex.what());
  }
}

void MainWindow::on_query_focus(wxCommandEvent&) {
  start_query(query_edit_->GetText().ToStdString(),
              focus_x_->GetValue(),
              focus_y_->GetValue(),
              radius_->GetValue());
}

void MainWindow::on_table_count(wxCommandEvent&) {
  if (!bridge_ || !db_ready_) {
    show_error("Bitte zuerst API laden und DB oeffnen.");
    return;
  }
  try {
    int count = bridge_->get_table_count();
    set_status("Tabellen: " + std::to_string(count));
    update_log();
  } catch (const std::exception& ex) {
    last_error_ = ex.what();
    update_log();
    show_error(ex.what());
  }
}

void MainWindow::on_focus_by_payload(wxCommandEvent&) {
  if (!bridge_ || !db_ready_) {
    show_error("Bitte zuerst API laden und DB oeffnen.");
    return;
  }
  const std::string text = focus_payload_id_->GetValue().ToStdString();
  if (text.empty()) {
    show_error("payload_id fehlt.");
    return;
  }
  try {
    const int payload_id = std::stoi(text);
    auto payload = bridge_->find_payload_by_id(payload_id);
    if (!payload) {
      show_error("payload_id nicht gefunden.");
      return;
    }
    focus_x_->SetValue(payload->payload.x);
    focus_y_->SetValue(payload->payload.y);
    focus_set_ = true;
    set_status("Fokus gesetzt: payload_id " + std::to_string(payload_id));
    update_log();
  } catch (const std::exception& ex) {
    last_error_ = ex.what();
    update_log();
    show_error(ex.what());
  }
}

void MainWindow::on_clear_focus(wxCommandEvent&) {
  focus_x_->SetValue(0);
  focus_y_->SetValue(0);
  focus_set_ = false;
  set_status("Fokus zurueckgesetzt.");
  update_log();
}

void MainWindow::on_focus_from_selection(wxCommandEvent&) {
  auto payload_id = selected_payload_id();
  if (!payload_id) {
    show_error("Keine Auswahl mit payload_id.");
    return;
  }
  focus_payload_id_->SetValue(std::to_string(*payload_id));
  wxCommandEvent evt;
  on_focus_by_payload(evt);
}

void MainWindow::on_query_complete(wxThreadEvent& event) {
  QueryResult result = event.GetPayload<QueryResult>();

  query_running_.store(false);
  auto* query_btn = FindWindow(kIdQuery);
  auto* cancel_btn = FindWindow(kIdCancel);
  if (query_btn) query_btn->Enable(true);
  if (cancel_btn) cancel_btn->Enable(false);

  if (result.token != query_token_.load()) {
    set_status("Query abgebrochen.");
    if (query_thread_.joinable()) {
      query_thread_.join();
    }
    update_log();
    return;
  }
  if (!result.ok) {
    last_error_ = result.error;
    update_log();
    std::string fallback_cmd = last_query_;
    std::string lower = fallback_cmd;
    for (auto& ch : lower) {
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    if (lower.rfind("sql ", 0) != 0 && (lower.rfind("select", 0) == 0 || lower.rfind("with", 0) == 0)) {
      fallback_cmd = "sql " + fallback_cmd;
    }
    auto lines = run_shell_command(fallback_cmd);
    set_result_text("shell", lines);
    set_status("Shell-Fallback ausgefuehrt.");
    last_error_.clear();
    update_log();
    return;
  }

  if (result.has_sql_table) {
    sql_columns_ = result.sql_columns;
    sql_rows_ = result.sql_rows;
    sql_rows_original_ = result.sql_rows;
    apply_grid_data(sql_grid_, sql_columns_, sql_rows_);
    result_tabs_->SetSelection(0);

    std::ostringstream status;
    status << "Treffer: " << result.sql_rows.size()
           << " | Dauer: " << result.duration_ms << " ms";
    set_status(status.str());
    last_hits_ = result.sql_rows.size();
    last_duration_ms_ = result.duration_ms;
    last_error_.clear();
    if (last_query_paging_ && page_size_ctrl_ && page_next_btn_) {
      const int page_size = page_size_ctrl_->GetValue();
      page_next_btn_->Enable(static_cast<int>(result.sql_rows.size()) >= page_size);
    }
    update_query_tab(last_query_, sql_columns_, sql_rows_);
    refresh_tools_view();
    update_log();
    return;
  }

  if (result.payloads.empty() && bridge_ && db_ready_ && is_sql_like(last_query_)) {
    const std::string api_error = bridge_->last_error_message();
    if (!api_error.empty()) {
      std::string fallback_cmd = last_query_;
      std::string lower = to_lower_ascii(fallback_cmd);
      if (lower.rfind("sql ", 0) != 0) {
        fallback_cmd = "sql " + fallback_cmd;
      }
      auto lines = run_shell_command(fallback_cmd, "csv");
      std::vector<std::string> columns;
      std::vector<std::vector<std::string>> rows;
      std::string parse_error;
      if (parse_csv_lines(lines, columns, rows, parse_error)) {
        sql_columns_ = columns;
        sql_rows_ = rows;
        sql_rows_original_ = rows;
        apply_grid_data(sql_grid_, sql_columns_, sql_rows_);
        result_tabs_->SetSelection(0);
        set_status("Shell-SQL (csv) ausgefuehrt.");
        last_hits_ = rows.size();
        last_duration_ms_ = result.duration_ms;
        last_error_.clear();
        update_query_tab(last_query_, sql_columns_, sql_rows_);
        refresh_tools_view();
        update_log();
        return;
      }
      set_result_text("shell", lines);
      set_status("Shell-SQL (raw) ausgefuehrt.");
      last_error_ = parse_error.empty() ? api_error : parse_error;
      refresh_tools_view();
      update_log();
      return;
    }
  }

  populate_payloads(result.payloads);
  populate_sql_result(result.payloads);

  std::ostringstream status;
  status << "Treffer: " << result.payloads.size()
         << " | Dauer: " << result.duration_ms << " ms";
  set_status(status.str());
  last_hits_ = result.payloads.size();
  last_duration_ms_ = result.duration_ms;
  last_error_.clear();
  update_query_tab(last_query_, sql_columns_, sql_rows_);
  refresh_tools_view();
  update_log();

  if (result.payloads.empty()) {
    const std::string api_error = bridge_ ? bridge_->last_error_message() : std::string();
    std::string lower_err = api_error;
    for (auto& ch : lower_err) {
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    if (lower_err.find("ungueltig") != std::string::npos) {
      std::string fallback_cmd = last_query_;
      std::string lower = fallback_cmd;
      for (auto& ch : lower) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
      }
      if (lower.rfind("sql ", 0) != 0 && (lower.rfind("select", 0) == 0 || lower.rfind("with", 0) == 0)) {
        fallback_cmd = "sql " + fallback_cmd;
      }
      auto lines = run_shell_command(fallback_cmd);
      set_result_text("shell", lines);
      set_status("Shell-Fallback ausgefuehrt.");
      last_error_.clear();
      update_log();
    }
  }

  if (query_thread_.joinable()) {
    query_thread_.join();
  }
}

void MainWindow::on_cancel_query(wxCommandEvent&) {
  cancel_query();
}

void MainWindow::on_export_csv(wxCommandEvent&) {
  wxGrid* grid = (result_tabs_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
  if (!grid || grid->GetNumberCols() == 0) {
    show_error("Keine Daten zum Export.");
    return;
  }
  std::string default_name = (result_tabs_->GetSelection() == 0) ? "sql_result.csv" : "payload_debug.csv";
  wxFileDialog dialog(this, "CSV exportieren", "", default_name,
                      "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK) {
    return;
  }
  export_grid_to_csv(grid, dialog.GetPath().ToStdString());
}

void MainWindow::export_grid_to_csv(wxGrid* grid, const std::string& path) {
  std::ofstream out(path);
  if (!out) {
    show_error("Datei konnte nicht geschrieben werden.");
    return;
  }

  auto escape_csv = [](const std::string& value) {
    bool needs_quotes = false;
    std::string out = value;
    size_t pos = 0;
    while ((pos = out.find('"', pos)) != std::string::npos) {
      out.insert(pos, "\"");
      pos += 2;
      needs_quotes = true;
    }
    if (out.find(',') != std::string::npos || out.find('\n') != std::string::npos || out.find('\r') != std::string::npos) {
      needs_quotes = true;
    }
    if (needs_quotes) {
      return "\"" + out + "\"";
    }
    return out;
  };

  for (int c = 0; c < grid->GetNumberCols(); ++c) {
    if (c > 0) out << ",";
    out << escape_csv(grid->GetColLabelValue(c).ToStdString());
  }
  out << "\n";

  for (int r = 0; r < grid->GetNumberRows(); ++r) {
    for (int c = 0; c < grid->GetNumberCols(); ++c) {
      if (c > 0) out << ",";
      out << escape_csv(grid->GetCellValue(r, c).ToStdString());
    }
    out << "\n";
  }
  set_status("CSV exportiert.");
}

void MainWindow::on_export_json(wxCommandEvent&) {
  wxGrid* grid = (result_tabs_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
  if (!grid || grid->GetNumberCols() == 0) {
    show_error("Keine Daten zum Export.");
    return;
  }
  std::string default_name = (result_tabs_->GetSelection() == 0) ? "sql_result.json" : "payload_debug.json";
  wxFileDialog dialog(this, "JSON exportieren", "", default_name,
                      "JSON files (*.json)|*.json|All files (*.*)|*.*",
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK) {
    return;
  }
  export_grid_to_json(grid, dialog.GetPath().ToStdString());
}

void MainWindow::export_grid_to_json(wxGrid* grid, const std::string& path) {
  std::ofstream out(path);
  if (!out) {
    show_error("Datei konnte nicht geschrieben werden.");
    return;
  }

  auto escape_json = [](const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
      switch (ch) {
        case '\\': out += "\\\\"; break;
        case '\"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
      }
    }
    return out;
  };

  std::vector<std::string> headers;
  headers.reserve(static_cast<size_t>(grid->GetNumberCols()));
  for (int c = 0; c < grid->GetNumberCols(); ++c) {
    headers.push_back(grid->GetColLabelValue(c).ToStdString());
  }

  out << "[\n";
  for (int r = 0; r < grid->GetNumberRows(); ++r) {
    out << "  {";
    for (int c = 0; c < grid->GetNumberCols(); ++c) {
      if (c > 0) out << ", ";
      out << "\"" << escape_json(headers[c]) << "\": "
          << "\"" << escape_json(grid->GetCellValue(r, c).ToStdString()) << "\"";
    }
    out << "}";
    if (r + 1 < grid->GetNumberRows()) out << ",";
    out << "\n";
  }
  out << "]\n";
  set_status("JSON exportiert.");
}

void MainWindow::on_favorite_save(wxCommandEvent&) {
  std::string query = trim_copy(query_edit_->GetText().ToStdString());
  if (query.empty()) {
    show_error("Keine Query zum Speichern.");
    return;
  }
  if (fav_queries_.size() < fav_buttons_.size()) {
    fav_queries_.resize(fav_buttons_.size());
  }
  size_t slot = fav_queries_.size();
  for (size_t i = 0; i < fav_queries_.size(); ++i) {
    if (fav_queries_[i].empty()) {
      slot = i;
      break;
    }
  }
  if (slot >= fav_queries_.size()) {
    slot = 0;
  }
  fav_queries_[slot] = query;

  if (slot < fav_buttons_.size() && fav_buttons_[slot]) {
    std::string label = query;
    const std::string lower = to_lower_ascii(label);
    if (lower.rfind("sql ", 0) == 0) {
      label = trim_copy(label.substr(4));
    }
    if (label.size() > 18) {
      label = label.substr(0, 18) + "...";
    }
    if (label.empty()) {
      label = "Fav " + std::to_string(slot + 1);
    }
    fav_buttons_[slot]->SetLabel(label);
    fav_buttons_[slot]->Enable(true);
  }
  set_status("Favorit gespeichert (" + std::to_string(slot + 1) + ").");
}

void MainWindow::on_favorite_run(wxCommandEvent& event) {
  size_t slot = 0;
  auto* btn = dynamic_cast<wxButton*>(event.GetEventObject());
  if (btn && btn->GetClientData()) {
    slot = reinterpret_cast<size_t>(btn->GetClientData());
  } else {
    for (size_t i = 0; i < fav_buttons_.size(); ++i) {
      if (fav_buttons_[i] == btn) {
        slot = i;
        break;
      }
    }
  }
  if (slot >= fav_queries_.size() || fav_queries_[slot].empty()) {
    show_error("Favorit ist leer.");
    return;
  }
  query_edit_->SetText(fav_queries_[slot]);
  start_query(fav_queries_[slot], focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
}

void MainWindow::on_filter_changed(wxCommandEvent&) {
  if (!filter_text_ || !filter_column_) return;
  wxGrid* grid = (result_tabs_ && result_tabs_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
  const bool is_sql = (grid == sql_grid_);
  auto& columns = is_sql ? sql_columns_ : debug_columns_;
  auto& rows = is_sql ? sql_rows_ : debug_rows_;
  auto& original = is_sql ? sql_rows_original_ : debug_rows_original_;

  std::string filter = trim_copy(filter_text_->GetValue().ToStdString());
  if (filter.empty()) {
    rows = original;
    apply_grid_data(grid, columns, rows);
    set_status("Filter entfernt.");
    return;
  }

  const std::string filter_lc = to_lower_ascii(filter);
  const int col_sel = filter_column_->GetSelection();
  const int col_index = (col_sel > 0) ? (col_sel - 1) : -1;

  std::vector<std::vector<std::string>> filtered;
  filtered.reserve(original.size());
  for (const auto& row : original) {
    bool match = false;
    if (col_index >= 0 && static_cast<size_t>(col_index) < row.size()) {
      match = to_lower_ascii(row[static_cast<size_t>(col_index)]).find(filter_lc) != std::string::npos;
    } else {
      for (const auto& cell : row) {
        if (to_lower_ascii(cell).find(filter_lc) != std::string::npos) {
          match = true;
          break;
        }
      }
    }
    if (match) {
      filtered.push_back(row);
    }
  }

  rows = std::move(filtered);
  apply_grid_data(grid, columns, rows);
  set_status("Filter: " + std::to_string(rows.size()) + " Treffer.");
}

void MainWindow::on_filter_clear(wxCommandEvent&) {
  if (!filter_text_ || !filter_column_) return;
  filter_text_->SetValue("");
  filter_column_->SetSelection(0);
  wxCommandEvent evt;
  on_filter_changed(evt);
}

void MainWindow::on_export_run(wxCommandEvent&) {
  if (!export_format_ || !export_target_) return;
  wxGrid* grid = (export_target_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
  if (!grid || grid->GetNumberCols() == 0) {
    show_error("Keine Daten zum Export.");
    return;
  }

  std::vector<int> cols;
  if (export_columns_ && export_columns_->GetCount() > 0) {
    for (size_t i = 0; i < export_columns_->GetCount(); ++i) {
      if (export_columns_->IsChecked(i)) {
        cols.push_back(static_cast<int>(i));
      }
    }
  }
  if (cols.empty()) {
    for (int c = 0; c < grid->GetNumberCols(); ++c) {
      cols.push_back(c);
    }
  }

  const std::string format = export_format_->GetStringSelection().ToStdString();
  std::string default_name = (export_target_->GetSelection() == 0) ? "sql_result" : "payload_debug";
  default_name += (format == "json") ? ".json" : ".csv";
  const std::string filter =
    (format == "json") ? "JSON files (*.json)|*.json|All files (*.*)|*.*"
                       : "CSV files (*.csv)|*.csv|All files (*.*)|*.*";
  wxFileDialog dialog(this, "Exportieren", "", default_name, filter,
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dialog.ShowModal() != wxID_OK) {
    return;
  }

  if (format == "json") {
    std::ofstream out(dialog.GetPath().ToStdString());
    if (!out) {
      show_error("Datei konnte nicht geschrieben werden.");
      return;
    }
    auto escape_json = [](const std::string& value) {
      std::string out;
      out.reserve(value.size());
      for (char ch : value) {
        switch (ch) {
          case '\\': out += "\\\\"; break;
          case '\"': out += "\\\""; break;
          case '\n': out += "\\n"; break;
          case '\r': out += "\\r"; break;
          case '\t': out += "\\t"; break;
          default: out += ch; break;
        }
      }
      return out;
    };

    out << "[\n";
    for (int r = 0; r < grid->GetNumberRows(); ++r) {
      out << "  {";
      for (size_t i = 0; i < cols.size(); ++i) {
        const int c = cols[i];
        if (i > 0) out << ", ";
        out << "\"" << escape_json(grid->GetColLabelValue(c).ToStdString()) << "\": "
            << "\"" << escape_json(grid->GetCellValue(r, c).ToStdString()) << "\"";
      }
      out << "}";
      if (r + 1 < grid->GetNumberRows()) out << ",";
      out << "\n";
    }
    out << "]\n";
    set_status("JSON exportiert.");
    return;
  }

  std::ofstream out(dialog.GetPath().ToStdString());
  if (!out) {
    show_error("Datei konnte nicht geschrieben werden.");
    return;
  }
  auto escape_csv = [](const std::string& value) {
    bool needs_quotes = false;
    std::string out = value;
    size_t pos = 0;
    while ((pos = out.find('"', pos)) != std::string::npos) {
      out.insert(pos, "\"");
      pos += 2;
      needs_quotes = true;
    }
    if (out.find(',') != std::string::npos || out.find('\n') != std::string::npos || out.find('\r') != std::string::npos) {
      needs_quotes = true;
    }
    if (needs_quotes) {
      return "\"" + out + "\"";
    }
    return out;
  };

  for (size_t i = 0; i < cols.size(); ++i) {
    if (i > 0) out << ",";
    out << escape_csv(grid->GetColLabelValue(cols[i]).ToStdString());
  }
  out << "\n";
  for (int r = 0; r < grid->GetNumberRows(); ++r) {
    for (size_t i = 0; i < cols.size(); ++i) {
      if (i > 0) out << ",";
      out << escape_csv(grid->GetCellValue(r, cols[i]).ToStdString());
    }
    out << "\n";
  }
  set_status("CSV exportiert.");
}

void MainWindow::on_query_tab_close(wxAuiNotebookEvent& event) {
  if (!query_tabs_) return;
  const int page = event.GetSelection();
  if (page == wxNOT_FOUND) {
    event.Skip();
    return;
  }
  wxWindow* page_window = query_tabs_->GetPage(page);
  if (page_window) {
    auto it = std::find_if(query_tabs_data_.begin(), query_tabs_data_.end(),
                           [page_window](const QuerySnapshot& snap) {
                             return snap.grid && snap.grid->GetParent() == page_window;
                           });
    if (it != query_tabs_data_.end()) {
      query_tabs_data_.erase(it);
    }
  }
  event.Skip();
}

void MainWindow::update_filter_columns() {
  if (!filter_column_) return;
  const bool is_sql = (result_tabs_ && result_tabs_->GetSelection() == 0);
  const auto& columns = is_sql ? sql_columns_ : debug_columns_;
  const std::string prev = filter_column_->GetStringSelection().ToStdString();
  filter_column_->Clear();
  filter_column_->Append("All");
  for (const auto& col : columns) {
    filter_column_->Append(col);
  }
  int selection = 0;
  if (!prev.empty()) {
    const int found = filter_column_->FindString(prev);
    if (found != wxNOT_FOUND) {
      selection = found;
    }
  }
  filter_column_->SetSelection(selection);
  if (filter_text_ && !filter_text_->GetValue().IsEmpty()) {
    wxCommandEvent evt;
    on_filter_changed(evt);
  }
}

void MainWindow::update_export_columns() {
  if (!export_columns_ || !export_target_) return;
  const bool use_sql = (export_target_->GetSelection() == 0);
  const auto& columns = use_sql ? sql_columns_ : debug_columns_;
  export_columns_->Clear();
  for (const auto& col : columns) {
    export_columns_->Append(col);
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    export_columns_->Check(i, true);
  }
}

void MainWindow::update_query_tab(const std::string& query,
                                  const std::vector<std::string>& columns,
                                  const std::vector<std::vector<std::string>>& rows) {
  if (!query_tabs_) return;
  if (columns.empty()) return;

  constexpr size_t kMaxTabs = 10;
  if (query_tabs_data_.size() >= kMaxTabs) {
    query_tabs_->DeletePage(0);
    query_tabs_data_.erase(query_tabs_data_.begin());
  }

  auto* panel = new wxPanel(query_tabs_);
  auto* sizer = new wxBoxSizer(wxVERTICAL);
  auto* query_view = new wxTextCtrl(panel, wxID_ANY, query, wxDefaultPosition, wxDefaultSize,
                                    wxTE_READONLY | wxTE_MULTILINE | wxTE_RICH2);
  query_view->SetMinSize(wxSize(-1, 50));
  auto* grid = new wxGrid(panel, wxID_ANY);
  grid->CreateGrid(0, 1);
  grid->EnableEditing(false);
  grid->EnableGridLines(true);
  apply_grid_data(grid, columns, rows);
  sizer->Add(query_view, 0, wxEXPAND | wxALL, 4);
  sizer->Add(grid, 1, wxEXPAND | wxALL, 4);
  panel->SetSizer(sizer);

  std::string label = query;
  if (to_lower_ascii(label).rfind("sql ", 0) == 0) {
    label = trim_copy(label.substr(4));
  }
  if (label.size() > 22) {
    label = label.substr(0, 22) + "...";
  }
  if (label.empty()) {
    label = "Query";
  }
  query_tabs_->AddPage(panel, label, true);
  query_tabs_data_.push_back({label, query, columns, rows, grid});
}

void MainWindow::update_diff_view() {
  if (!diff_view_) return;
  if (!bridge_ || !db_ready_) {
    diff_view_->SetValue("Keine DB geladen.");
    return;
  }
  try {
    const int delta_count = bridge_->get_delta_count();
    const int tomb_count = bridge_->get_tombstone_count();
    std::ostringstream out;
    out << "Delta: " << delta_count << "\n";
    out << "Tombstones: " << tomb_count << "\n";

    const int show_delta = std::min(delta_count, 50);
    const int show_tomb = std::min(tomb_count, 50);
    if (show_delta > 0) {
      out << "\nDelta-Entries:\n";
      for (int i = 0; i < show_delta; ++i) {
        out << "- " << bridge_->get_delta_entry(i) << "\n";
      }
    }
    if (show_tomb > 0) {
      out << "\nTombstones:\n";
      for (int i = 0; i < show_tomb; ++i) {
        out << "- " << bridge_->get_tombstone_entry(i) << "\n";
      }
    }
    diff_view_->SetValue(out.str());
  } catch (const std::exception& ex) {
    diff_view_->SetValue(std::string("Diff konnte nicht geladen werden: ") + ex.what());
  }
}

void MainWindow::refresh_tools_view() {
  update_filter_columns();
  update_export_columns();
  update_diff_view();

  if (auto_explain_view_) {
    if (auto_explain_cb_ && auto_explain_cb_->IsChecked()) {
      std::ostringstream out;
      out << "Query: " << last_query_ << "\n";
      if (!last_exec_query_.empty() && last_exec_query_ != last_query_) {
        out << "Exec: " << last_exec_query_ << "\n";
      }
      out << "Focus: x=" << focus_x_->GetValue()
          << " y=" << focus_y_->GetValue()
          << " r=" << radius_->GetValue() << "\n";
      if (last_query_paging_ && page_size_ctrl_) {
        out << "Paging: on (page " << (page_index_ + 1)
            << ", size " << page_size_ctrl_->GetValue() << ")\n";
      } else {
        out << "Paging: off\n";
      }
      out << "Hits: " << last_hits_ << " | Duration: " << last_duration_ms_ << " ms\n";
      auto_explain_view_->SetValue(out.str());
    } else {
      auto_explain_view_->Clear();
    }
  }

  if (auto_stats_view_) {
    if (auto_stats_cb_ && auto_stats_cb_->IsChecked() && bridge_ && db_ready_ && tables_list_) {
      std::ostringstream out;
      out << "Table counts:\n";
      for (unsigned int i = 0; i < tables_list_->GetCount(); ++i) {
        const std::string table = tables_list_->GetString(i).ToStdString();
        std::vector<std::string> cols;
        std::vector<std::vector<std::string>> rows;
        try {
          bridge_->query_sql_table("SELECT COUNT(*) AS count FROM " + table,
                                   focus_set_,
                                   focus_x_->GetValue(),
                                   focus_y_->GetValue(),
                                   radius_->GetValue(),
                                   cols,
                                   rows);
        } catch (const std::exception&) {
        }
        std::string count = "?";
        if (!rows.empty() && !rows.front().empty()) {
          count = rows.front().front();
        }
        out << "- " << table << ": " << count << "\n";
      }
      auto_stats_view_->SetValue(out.str());
    } else {
      auto_stats_view_->Clear();
    }
  }
}

void MainWindow::on_copy_selection(wxCommandEvent&) {
  wxGrid* grid = (result_tabs_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
  if (!grid || grid->GetNumberCols() == 0) {
    show_error("Keine Daten zum Kopieren.");
    return;
  }

  std::vector<int> rows = grid->GetSelectedRows();
  std::ostringstream out;
  if (!rows.empty()) {
    for (size_t i = 0; i < rows.size(); ++i) {
      int r = rows[i];
      if (i > 0) out << "\n";
      for (int c = 0; c < grid->GetNumberCols(); ++c) {
        if (c > 0) out << "\t";
        out << grid->GetCellValue(r, c).ToStdString();
      }
    }
  } else if (!grid->GetSelectionBlockTopLeft().empty()) {
    const auto& tl = grid->GetSelectionBlockTopLeft().front();
    const auto& br = grid->GetSelectionBlockBottomRight().front();
    for (int r = tl.GetRow(); r <= br.GetRow(); ++r) {
      if (r > tl.GetRow()) out << "\n";
      for (int c = tl.GetCol(); c <= br.GetCol(); ++c) {
        if (c > tl.GetCol()) out << "\t";
        out << grid->GetCellValue(r, c).ToStdString();
      }
    }
  } else if (!grid->GetSelectedCells().empty()) {
    auto cells = grid->GetSelectedCells();
    std::sort(cells.begin(), cells.end(),
              [](const wxGridCellCoords& a, const wxGridCellCoords& b) {
                if (a.GetRow() == b.GetRow()) return a.GetCol() < b.GetCol();
                return a.GetRow() < b.GetRow();
              });
    int current_row = cells.front().GetRow();
    bool first = true;
    for (const auto& cell : cells) {
      if (cell.GetRow() != current_row) {
        out << "\n";
        current_row = cell.GetRow();
        first = true;
      }
      if (!first) out << "\t";
      out << grid->GetCellValue(cell.GetRow(), cell.GetCol()).ToStdString();
      first = false;
    }
  } else {
    int r = grid->GetGridCursorRow();
    int c = grid->GetGridCursorCol();
    if (r >= 0 && c >= 0) {
      out << grid->GetCellValue(r, c).ToStdString();
    }
  }

  if (!wxTheClipboard->Open()) {
    show_error("Clipboard nicht verfuegbar.");
    return;
  }
  wxTheClipboard->SetData(new wxTextDataObject(out.str()));
  wxTheClipboard->Close();
  set_status("In Zwischenablage kopiert.");
}

void MainWindow::on_query_key_down(wxKeyEvent& event) {
  if (!event.ControlDown() && !event.AltDown()) {
    event.Skip();
    return;
  }
  if (query_history_.empty()) {
    event.Skip();
    return;
  }

  if (event.GetKeyCode() == WXK_UP) {
    if (history_index_ <= 0) {
      history_index_ = static_cast<int>(query_history_.size()) - 1;
    } else {
      --history_index_;
    }
    query_edit_->SetText(query_history_[static_cast<size_t>(history_index_)]);
    query_edit_->GotoPos(query_edit_->GetTextLength());
    return;
  }
  if (event.GetKeyCode() == WXK_DOWN) {
    if (history_index_ < 0) {
      history_index_ = static_cast<int>(query_history_.size()) - 1;
    } else if (history_index_ >= static_cast<int>(query_history_.size()) - 1) {
      history_index_ = -1;
      query_edit_->SetText("");
      return;
    } else {
      ++history_index_;
    }
    query_edit_->SetText(query_history_[static_cast<size_t>(history_index_)]);
    query_edit_->GotoPos(query_edit_->GetTextLength());
    return;
  }
  event.Skip();
}

void MainWindow::on_page_prev(wxCommandEvent&) {
  if (!last_query_paging_ || last_user_query_.empty()) {
    return;
  }
  if (page_index_ > 0) {
    page_index_ -= 1;
  }
  skip_history_next_ = true;
  keep_page_index_next_ = true;
  start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
}

void MainWindow::on_page_next(wxCommandEvent&) {
  if (!last_query_paging_ || last_user_query_.empty()) {
    return;
  }
  page_index_ += 1;
  skip_history_next_ = true;
  keep_page_index_next_ = true;
  start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
}

void MainWindow::on_page_size_changed(wxCommandEvent&) {
  if (page_size_ctrl_) {
    if (page_size_ctrl_->GetValue() < 1) {
      page_size_ctrl_->SetValue(200);
    }
  }
  page_index_ = 0;
  if (!last_query_paging_ || last_user_query_.empty()) {
    if (page_label_) {
      page_label_->SetLabel("Page 1");
    }
    return;
  }
  skip_history_next_ = true;
  keep_page_index_next_ = true;
  start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
}

void MainWindow::on_grid_cell_menu(wxGridEvent& event) {
  wxGrid* grid = dynamic_cast<wxGrid*>(event.GetEventObject());
  if (!grid) return;
  const bool is_sql = (grid == sql_grid_);
  const int row = event.GetRow();
  const int col = event.GetCol();
  if (row < 0 || col < 0) return;

  std::vector<std::string>& columns = is_sql ? sql_columns_ : debug_columns_;
  std::vector<std::vector<std::string>>& rows = is_sql ? sql_rows_ : debug_rows_;
  if (rows.empty() || columns.empty()) return;

  std::string base_sql = strip_sql_prefix(last_user_query_);
  std::string table = is_sql ? parse_single_table(base_sql) : std::string();
  const std::string pk_col = is_sql ? find_pk_column(table, columns) : std::string();
  const bool can_edit = is_sql && !table.empty() && !pk_col.empty();

  wxMenu menu;
  menu.Append(kIdMenuEditField, "Open Value in Editor");
  menu.Append(kIdMenuSetNull, "Set Field to NULL");
  menu.AppendSeparator();
  menu.Append(kIdMenuDeleteRows, "Delete Row(s)");
  menu.AppendSeparator();
  menu.Append(kIdMenuPasteRow, "Paste Row");
  menu.AppendSeparator();
  menu.Append(kIdMenuCopyRow, "Copy Row");
  menu.Append(kIdMenuCopyRowWithNames, "Copy Row (with names)");
  menu.Append(kIdMenuCopyRowUnquoted, "Copy Row (unquoted)");
  menu.Append(kIdMenuCopyRowWithNamesUnquoted, "Copy Row (with names, unquoted)");
  menu.Append(kIdMenuCopyRowWithNamesTabs, "Copy Row (with names, tab separated)");
  menu.Append(kIdMenuCopyRowTabs, "Copy Row (tab separated)");
  menu.AppendSeparator();
  menu.Append(kIdMenuCopyField, "Copy Field");
  menu.Append(kIdMenuCopyFieldUnquoted, "Copy Field (unquoted)");
  menu.Append(kIdMenuCopyFieldName, "Copy Field Name");

  menu.Enable(kIdMenuEditField, can_edit);
  menu.Enable(kIdMenuSetNull, can_edit);
  menu.Enable(kIdMenuDeleteRows, can_edit);
  menu.Enable(kIdMenuPasteRow, can_edit);

  const int selection = grid->GetPopupMenuSelectionFromUser(menu);
  if (selection == wxID_NONE) return;

  auto get_cell = [&](int r, int c) -> std::string {
    if (r < 0 || c < 0) return {};
    if (static_cast<size_t>(r) >= rows.size()) return {};
    const auto& row_vals = rows[static_cast<size_t>(r)];
    if (static_cast<size_t>(c) >= row_vals.size()) return {};
    return row_vals[static_cast<size_t>(c)];
  };

  auto copy_text = [&](const std::string& text) {
    if (!wxTheClipboard->Open()) return;
    wxTheClipboard->SetData(new wxTextDataObject(text));
    wxTheClipboard->Close();
  };

  auto build_row_text = [&](int r, bool with_names, bool unquoted, bool tabs) -> std::string {
    std::ostringstream out;
    const auto& row_vals = rows[static_cast<size_t>(r)];
    for (size_t c = 0; c < columns.size(); ++c) {
      if (c > 0) out << (tabs ? "\t" : ", ");
      std::string val = (c < row_vals.size()) ? row_vals[c] : "";
      if (unquoted) {
        // keep raw
      }
      if (with_names) {
        out << columns[c] << "=" << val;
      } else {
        out << val;
      }
    }
    return out.str();
  };

  if (selection == kIdMenuCopyRow || selection == kIdMenuCopyRowWithNames ||
      selection == kIdMenuCopyRowUnquoted || selection == kIdMenuCopyRowWithNamesUnquoted ||
      selection == kIdMenuCopyRowTabs || selection == kIdMenuCopyRowWithNamesTabs) {
    bool with_names = (selection == kIdMenuCopyRowWithNames || selection == kIdMenuCopyRowWithNamesUnquoted ||
                       selection == kIdMenuCopyRowWithNamesTabs);
    bool unquoted = (selection == kIdMenuCopyRowUnquoted || selection == kIdMenuCopyRowWithNamesUnquoted);
    bool tabs = (selection == kIdMenuCopyRowTabs || selection == kIdMenuCopyRowWithNamesTabs);
    copy_text(build_row_text(row, with_names, unquoted, tabs));
    return;
  }

  if (selection == kIdMenuCopyField || selection == kIdMenuCopyFieldUnquoted) {
    copy_text(get_cell(row, col));
    return;
  }

  if (selection == kIdMenuCopyFieldName) {
    if (static_cast<size_t>(col) < columns.size()) {
      copy_text(columns[static_cast<size_t>(col)]);
    }
    return;
  }

  if (!can_edit) {
    return;
  }

  auto find_pk_value = [&](int r) -> std::string {
    for (size_t c = 0; c < columns.size(); ++c) {
      if (to_lower_ascii(columns[c]) == to_lower_ascii(pk_col)) {
        return get_cell(r, static_cast<int>(c));
      }
    }
    return {};
  };

  if (selection == kIdMenuEditField) {
    const std::string col_name = (static_cast<size_t>(col) < columns.size()) ? columns[static_cast<size_t>(col)] : "";
    if (col_name.empty()) return;
    const std::string pk_val = find_pk_value(row);
    if (pk_val.empty()) return;
    wxTextEntryDialog dlg(this, "Neuer Wert:", "Edit Field", get_cell(row, col),
                          wxOK | wxCANCEL | wxTE_MULTILINE);
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string new_value = dlg.GetValue().ToStdString();
    const std::string sql = "UPDATE " + table + " SET " + col_name + "=" + format_sql_value(new_value) +
                            " WHERE " + pk_col + "=" + format_sql_value(pk_val);
    try {
      bridge_->exec_sql(sql, false, 0, 0, 0);
      skip_history_next_ = true;
      keep_page_index_next_ = true;
      start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    } catch (const std::exception& ex) {
      show_error(ex.what());
    }
    return;
  }

  if (selection == kIdMenuSetNull) {
    const std::string col_name = (static_cast<size_t>(col) < columns.size()) ? columns[static_cast<size_t>(col)] : "";
    if (col_name.empty()) return;
    const std::string pk_val = find_pk_value(row);
    if (pk_val.empty()) return;
    const std::string sql = "UPDATE " + table + " SET " + col_name + "=NULL WHERE " + pk_col + "=" +
                            format_sql_value(pk_val);
    try {
      bridge_->exec_sql(sql, false, 0, 0, 0);
      skip_history_next_ = true;
      keep_page_index_next_ = true;
      start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    } catch (const std::exception& ex) {
      show_error(ex.what());
    }
    return;
  }

  if (selection == kIdMenuDeleteRows) {
    std::vector<int> target_rows = grid->GetSelectedRows();
    if (target_rows.empty()) {
      target_rows.push_back(row);
    }
    if (wxMessageBox("Zeile(n) wirklich loeschen?", "Delete Row(s)", wxYES_NO | wxICON_WARNING, this) != wxYES) {
      return;
    }
    try {
      for (int r : target_rows) {
        const std::string pk_val = find_pk_value(r);
        if (pk_val.empty()) continue;
        const std::string sql = "DELETE FROM " + table + " WHERE " + pk_col + "=" + format_sql_value(pk_val);
        bridge_->exec_sql(sql, false, 0, 0, 0);
      }
      skip_history_next_ = true;
      keep_page_index_next_ = true;
      start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    } catch (const std::exception& ex) {
      show_error(ex.what());
    }
    return;
  }

  if (selection == kIdMenuPasteRow) {
    if (!wxTheClipboard->Open()) return;
    wxTextDataObject data;
    if (!wxTheClipboard->GetData(data)) {
      wxTheClipboard->Close();
      return;
    }
    wxTheClipboard->Close();
    const std::string clip = data.GetText().ToStdString();
    if (clip.empty()) return;

    std::vector<std::string> cols;
    std::vector<std::string> vals;
    if (clip.find('=') != std::string::npos) {
      auto tokens = split_by_any(clip, ",;|");
      for (const auto& token : tokens) {
        const auto eq = token.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim_copy(token.substr(0, eq));
        std::string val = trim_copy(token.substr(eq + 1));
        if (!key.empty()) {
          cols.push_back(key);
          vals.push_back(val);
        }
      }
    } else {
      auto parts = split_by_any(clip, "\t");
      for (size_t i = 0; i < parts.size() && i < columns.size(); ++i) {
        const std::string col_name = columns[i];
        if (to_lower_ascii(col_name) == "payload_id") continue;
        cols.push_back(col_name);
        vals.push_back(parts[i]);
      }
    }
    if (cols.empty()) return;
    std::ostringstream sql;
    sql << "INSERT INTO " << table << " (";
    for (size_t i = 0; i < cols.size(); ++i) {
      if (i > 0) sql << ",";
      sql << cols[i];
    }
    sql << ") VALUES (";
    for (size_t i = 0; i < vals.size(); ++i) {
      if (i > 0) sql << ",";
      sql << format_sql_value(vals[i]);
    }
    sql << ")";
    try {
      bridge_->exec_sql(sql.str(), false, 0, 0, 0);
      skip_history_next_ = true;
      keep_page_index_next_ = true;
      start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    } catch (const std::exception& ex) {
      show_error(ex.what());
    }
    return;
  }
}

void MainWindow::on_grid_label_menu(wxGridEvent& event) {
  wxGrid* grid = dynamic_cast<wxGrid*>(event.GetEventObject());
  if (!grid) return;
  const bool is_sql = (grid == sql_grid_);
  const int col = event.GetCol();
  const int row = event.GetRow();
  if (row >= 0) {
    return;
  }
  std::vector<std::string>& columns = is_sql ? sql_columns_ : debug_columns_;
  if (columns.empty()) return;

  wxMenu menu;
  if (col >= 0 && static_cast<size_t>(col) < columns.size()) {
    menu.Append(kIdMenuCopyFieldName, "Copy Field Name");
  }
  menu.Append(kIdMenuCopyAllFieldNames, "Copy All Field Names");
  if (is_sql) {
    menu.AppendSeparator();
    menu.Append(kIdMenuResetSort, "Reset Sorting");
  }
  menu.Append(kIdMenuResetColWidths, "Reset Column Widths");

  const int selection = grid->GetPopupMenuSelectionFromUser(menu);
  if (selection == wxID_NONE) return;

  auto copy_text = [&](const std::string& text) {
    if (!wxTheClipboard->Open()) return;
    wxTheClipboard->SetData(new wxTextDataObject(text));
    wxTheClipboard->Close();
  };

  if (selection == kIdMenuCopyFieldName && col >= 0 && static_cast<size_t>(col) < columns.size()) {
    copy_text(columns[static_cast<size_t>(col)]);
    return;
  }
  if (selection == kIdMenuCopyAllFieldNames) {
    std::ostringstream out;
    for (size_t i = 0; i < columns.size(); ++i) {
      if (i > 0) out << ",";
      out << columns[i];
    }
    copy_text(out.str());
    return;
  }
  if (selection == kIdMenuResetSort) {
    reset_sort();
    return;
  }
  if (selection == kIdMenuResetColWidths) {
    grid->AutoSizeColumns(false);
    return;
  }
}

void MainWindow::on_undo_delta(wxCommandEvent&) {
  if (!bridge_ || !db_ready_) {
    show_error("Bitte zuerst API laden und DB oeffnen.");
    return;
  }
  try {
    bridge_->undo_last_delta();
    set_status("Undo ausgefuehrt.");
    skip_history_next_ = true;
    keep_page_index_next_ = true;
    if (!last_user_query_.empty()) {
      start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    } else {
      update_diff_view();
      update_log();
    }
  } catch (const std::exception& ex) {
    show_error(ex.what());
  }
}

void MainWindow::on_merge_delta(wxCommandEvent&) {
  if (!bridge_ || !db_ready_) {
    show_error("Bitte zuerst API laden und DB oeffnen.");
    return;
  }
  if (wxMessageBox("Delta-Store jetzt mergen und neu clustern?", "Merge & Re-Cluster",
                   wxYES_NO | wxICON_QUESTION, this) != wxYES) {
    return;
  }
  const int agents = 256;
  const int steps = 2000;
  const uint32_t seed = 42;
  try {
    bridge_->merge_delta(agents, steps, seed);
    set_status("Merge ok.");
    skip_history_next_ = true;
    keep_page_index_next_ = true;
    if (!last_user_query_.empty()) {
      start_query(last_user_query_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    } else {
      update_diff_view();
      update_log();
    }
  } catch (const std::exception& ex) {
    show_error(ex.what());
  }
}

void MainWindow::apply_sort(const std::string& spec) {
  wxGrid* grid = (result_tabs_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
  const bool is_sql = (grid == sql_grid_);
  auto& columns = is_sql ? sql_columns_ : debug_columns_;
  auto& rows = is_sql ? sql_rows_ : debug_rows_;
  if (columns.empty() || rows.empty()) {
    show_error("Kein Result zum Sortieren.");
    return;
  }

  struct SortKey {
    int index = -1;
    bool desc = false;
    bool numeric = false;
  };

  std::vector<SortKey> keys;
  auto parts = split_by_any(spec, ",");
  for (auto part : parts) {
    auto tokens = split_by_any(part, " \t");
    if (tokens.empty()) continue;
    SortKey key;
    const std::string col_token = tokens[0];
    char* end_ptr = nullptr;
    long col_index = std::strtol(col_token.c_str(), &end_ptr, 10);
    if (end_ptr && *end_ptr == '\0') {
      key.index = static_cast<int>(col_index) - 1;
    } else {
      auto to_lower = [](std::string value) {
        for (auto& ch : value) {
          if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        return value;
      };
      const std::string target = to_lower(col_token);
      auto it = std::find_if(columns.begin(), columns.end(),
                             [&](const std::string& col) { return to_lower(col) == target; });
      if (it == columns.end()) {
        show_error("Spalte nicht gefunden: " + col_token);
        return;
      }
      key.index = static_cast<int>(std::distance(columns.begin(), it));
    }
    for (size_t i = 1; i < tokens.size(); ++i) {
      const std::string lower = [t = tokens[i]]() {
        std::string out = t;
        for (auto& ch : out) {
          if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        return out;
      }();
      if (lower == "desc") key.desc = true;
      if (lower == "num") key.numeric = true;
    }
    keys.push_back(key);
  }
  if (keys.empty()) {
    show_error("Sort-Spezifikation fehlt.");
    return;
  }

  std::stable_sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) {
    for (const auto& key : keys) {
      if (key.index < 0 || key.index >= static_cast<int>(columns.size())) {
        continue;
      }
      const std::string& av = key.index < static_cast<int>(a.size()) ? a[key.index] : std::string();
      const std::string& bv = key.index < static_cast<int>(b.size()) ? b[key.index] : std::string();
      if (key.numeric) {
        const double ad = std::strtod(av.c_str(), nullptr);
        const double bd = std::strtod(bv.c_str(), nullptr);
        if (ad < bd) return !key.desc;
        if (ad > bd) return key.desc;
      } else {
        if (av < bv) return !key.desc;
        if (av > bv) return key.desc;
      }
    }
    return false;
  });

  apply_grid_data(grid, columns, rows);
  set_status("Sortiert.");
}

void MainWindow::reset_sort() {
  wxGrid* grid = (result_tabs_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
  const bool is_sql = (grid == sql_grid_);
  auto& columns = is_sql ? sql_columns_ : debug_columns_;
  auto& rows = is_sql ? sql_rows_ : debug_rows_;
  auto& original = is_sql ? sql_rows_original_ : debug_rows_original_;
  if (columns.empty() || original.empty()) {
    show_error("Kein Result zum Zuruecksetzen.");
    return;
  }
  rows = original;
  apply_grid_data(grid, columns, rows);
  set_status("Sort reset.");
}

void MainWindow::load_tables() {
  if (!bridge_ || !db_ready_) {
    return;
  }
  tables_list_->Clear();
  try {
    int count = bridge_->get_table_count();
    for (int i = 0; i < count; ++i) {
      std::string name = bridge_->get_table_name(i);
      if (name.empty()) {
        name = "table_" + std::to_string(i);
      }
      tables_list_->Append(name);
    }
    if (count > 0) {
      tables_list_->SetSelection(0);
      update_schema_for_table(tables_list_->GetString(0).ToStdString());
    }
  } catch (const std::exception& ex) {
    last_error_ = ex.what();
    update_log();
  }
}

void MainWindow::update_schema_for_table(const std::string& table_name) {
  if (!bridge_ || !db_ready_) {
    schema_view_->SetValue("Keine DB geoeffnet.");
    return;
  }
  if (query_running_.load()) {
    schema_view_->SetValue("Schema nicht verfuegbar waehrend Query.");
    return;
  }
  if (table_name.empty()) {
    schema_view_->SetValue("Keine Tabelle ausgewaehlt.");
    return;
  }

  std::string query = "sql SELECT * FROM " + table_name + " LIMIT 1";
  std::vector<PayloadRow> payloads;
  std::vector<std::string> columns;
  std::string sample_row;
  bool used_sql_table = false;
  try {
    std::vector<std::vector<std::string>> sql_rows;
    const std::string sql = "SELECT * FROM " + table_name + " LIMIT 1";
    if (bridge_->query_sql_table(sql, focus_set_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue(),
                                 columns, sql_rows)) {
      used_sql_table = !columns.empty();
      if (!sql_rows.empty() && !columns.empty()) {
        std::ostringstream sample;
        const auto& row = sql_rows.front();
        for (size_t i = 0; i < columns.size(); ++i) {
          if (i > 0) sample << ", ";
          const std::string value = (i < row.size()) ? row[i] : "";
          sample << columns[i] << "=" << value;
        }
        sample_row = sample.str();
      }
    }
  } catch (const std::exception& ex) {
    used_sql_table = false;
  }

  if (!used_sql_table) {
    try {
      payloads = bridge_->query_focus(query, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    } catch (const std::exception& ex) {
      schema_view_->SetValue("Schema konnte nicht geladen werden.");
      last_error_ = ex.what();
      update_log();
      return;
    }

    bool has_kv = false;
    for (const auto& row : payloads) {
      if (row.raw_data.find('=') != std::string::npos) {
        has_kv = true;
        break;
      }
    }

    if (has_kv) {
      for (const auto& row : payloads) {
        auto tokens = split_by_any(row.raw_data, ",;|");
        for (const auto& token : tokens) {
          const auto eq_pos = token.find('=');
          if (eq_pos == std::string::npos) {
            continue;
          }
          const std::string key = trim_copy(token.substr(0, eq_pos));
          if (key.empty()) {
            continue;
          }
          if (std::find(columns.begin(), columns.end(), key) == columns.end()) {
            columns.push_back(key);
          }
        }
      }
    } else if (!payloads.empty()) {
      if (payloads.front().raw_data.find('|') != std::string::npos) {
        auto parts = split_by_any(payloads.front().raw_data, "|");
        for (const auto& part : parts) {
          if (!part.empty()) {
            columns.push_back(part);
          }
        }
      }
    }
    if (sample_row.empty() && !payloads.empty()) {
      sample_row = payloads.front().raw_data;
    }
  }

  std::ostringstream out;
  out << "Tabelle: " << table_name << "\n";
  if (!columns.empty()) {
    out << "Spalten (Sample):\n";
    for (const auto& col : columns) {
      out << "- " << col << "\n";
    }
    table_pk_cache_[table_name] = columns.front();
  } else if (!sample_row.empty()) {
    out << "Sample:\n" << sample_row << "\n";
  } else {
    out << "Keine Daten fuer Schema-Sample.\n";
  }
  schema_view_->SetValue(out.str());
}

void MainWindow::update_log() {
  std::ostringstream out;
  out << "API: " << (api_ready_ ? "ok" : "aus") << "\n";
  out << "DB: " << (db_ready_ ? "ok" : "aus") << "\n";
  out << api_version_label_->GetLabel().ToStdString() << "\n";
  out << "Focus: x=" << focus_x_->GetValue()
      << " y=" << focus_y_->GetValue()
      << " r=" << radius_->GetValue() << "\n";
  out << "Limit: " << (default_limit_ > 0 ? std::to_string(default_limit_) : "off") << "\n";
  if (!show_columns_.empty()) {
    std::string cols;
    for (size_t i = 0; i < show_columns_.size(); ++i) {
      if (i > 0) cols += ",";
      cols += show_columns_[i];
    }
    out << "Show: " << cols << "\n";
  } else {
    out << "Show: off\n";
  }
  out << "Format: " << output_format_ << "\n";
  if (!shell_exe_path_.empty()) {
    out << "Shell: " << shell_exe_path_ << "\n";
  }
  if (!last_query_.empty()) {
    out << "Last Query: " << last_query_ << "\n";
  }
  if (!last_exec_query_.empty() && last_exec_query_ != last_query_) {
    out << "Exec Query: " << last_exec_query_ << "\n";
  }
  out << "Hits: " << last_hits_ << "\n";
  out << "Duration: " << last_duration_ms_ << " ms\n";
  if (!last_error_.empty()) {
    out << "Last Error: " << last_error_ << "\n";
  } else if (bridge_ && db_ready_) {
    const std::string api_err = bridge_->last_error_message();
    if (!api_err.empty()) {
      out << "Last Error: " << api_err << "\n";
    }
  }
  log_view_->SetValue(out.str());
}

std::vector<std::string> MainWindow::run_shell_command(const std::string& command, const std::string& format) {
  std::vector<std::string> lines;
  if (!db_ready_) {
    lines.push_back("Keine DB geoeffnet.");
    return lines;
  }

  std::string shell = shell_exe_path_;
#ifdef _WIN32
  if (shell.empty()) {
    shell = "micro_swarm.exe";
  }
#else
  if (shell.empty()) {
    shell = "./micro_swarm";
  }
#endif
  if (!shell_exe_path_.empty()) {
    const std::filesystem::path candidate(shell);
    if (candidate.has_parent_path() && !std::filesystem::exists(candidate)) {
#ifdef _WIN32
      shell = "micro_swarm.exe";
#else
      shell = "./micro_swarm";
#endif
    }
  }

  const std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path in_path = temp_dir / ("myco_shell_in_" + std::to_string(stamp) + ".txt");
  const std::filesystem::path out_path = temp_dir / ("myco_shell_out_" + std::to_string(stamp) + ".txt");

  {
    std::ofstream in_file(in_path.string());
    in_file << command << "\n";
    in_file << "exit\n";
  }

  std::ostringstream cmd;
  const std::string format_arg = format.empty() ? "" : (" --sql-format " + format);
#ifdef _WIN32
  cmd << "cmd /c \"\\\"" << shell << "\\\" --mode db_shell --db \\\"" << db_path_->GetValue().ToStdString()
      << "\\\" --db-radius " << radius_->GetValue() << format_arg
      << " < \\\"" << in_path.string() << "\\\" > \\\"" << out_path.string() << "\\\" 2>&1\"";
#else
  cmd << "sh -c '\"" << shell << "\" --mode db_shell --db \"" << db_path_->GetValue().ToStdString()
      << "\" --db-radius " << radius_->GetValue() << format_arg
      << " < \"" << in_path.string() << "\" > \"" << out_path.string() << "\" 2>&1'";
#endif

  const int result = std::system(cmd.str().c_str());
  std::vector<std::string> output_lines;
  {
    std::ifstream out_file(out_path.string());
    std::string line;
    while (std::getline(out_file, line)) {
      if (!line.empty()) {
        output_lines.push_back(line);
      }
    }
  }
  if (result != 0) {
    lines.push_back("Shell-Fallback fehlgeschlagen. Exit-Code: " + std::to_string(result));
    lines.insert(lines.end(), output_lines.begin(), output_lines.end());
  } else {
    lines = std::move(output_lines);
  }

  std::error_code ec;
  std::filesystem::remove(in_path, ec);
  std::filesystem::remove(out_path, ec);
  if (lines.empty()) {
    lines.push_back("(keine Ausgabe)");
  }
  return lines;
}

static std::vector<std::string> tokenize_command(const std::string& input) {
  std::vector<std::string> tokens;
  std::string current;
  bool in_quotes = false;
  for (size_t i = 0; i < input.size(); ++i) {
    char ch = input[i];
    if (ch == '"') {
      in_quotes = !in_quotes;
      continue;
    }
    if (!in_quotes && std::isspace(static_cast<unsigned char>(ch))) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(ch);
    }
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

bool MainWindow::handle_shell_command(const std::string& command) {
  const std::string cmd = trim_copy(command);
  if (cmd.empty()) {
    return false;
  }

  std::string lower = cmd;
  for (auto& ch : lower) {
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }

  if (lower == "help") {
    auto lines = run_shell_command("help");
    set_result_text("help", lines);
    return true;
  }

  if (lower == "cls" || lower == "clear") {
    clear_sql_result();
    clear_debug_result();
    if (log_view_) {
      log_view_->Clear();
    }
    if (auto_explain_view_) {
      auto_explain_view_->Clear();
    }
    if (auto_stats_view_) {
      auto_stats_view_->Clear();
    }
    set_status("Ansicht geleert.");
    return true;
  }

  if (lower == "tables") {
    std::vector<std::string> lines;
    for (unsigned int i = 0; i < tables_list_->GetCount(); ++i) {
      lines.push_back(tables_list_->GetString(i).ToStdString());
    }
    if (lines.empty()) {
      lines.push_back("(keine Tabellen)");
    }
    set_result_text("tables", lines);
    return true;
  }

  if (lower.rfind("schema ", 0) == 0) {
    const std::string table_name = trim_copy(cmd.substr(7));
    update_schema_for_table(table_name);
    set_result_text("schema", split_by_any(schema_view_->GetValue().ToStdString(), "\n"));
    return true;
  }

  if (lower.rfind("describe ", 0) == 0) {
    const std::string table_name = trim_copy(cmd.substr(9));
    update_schema_for_table(table_name);
    std::vector<std::string> lines = split_by_any(schema_view_->GetValue().ToStdString(), "\n");
    lines.push_back("Sample:");
    std::string sample_query = "SELECT * FROM " + table_name + " LIMIT 1";
    try {
      std::vector<std::string> columns;
      std::vector<std::vector<std::string>> rows;
      bridge_->query_sql_table(sample_query, focus_set_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue(),
                               columns, rows);
      if (!rows.empty() && !columns.empty()) {
        std::ostringstream sample;
        const auto& row = rows.front();
        for (size_t i = 0; i < columns.size(); ++i) {
          if (i > 0) sample << ", ";
          const std::string value = (i < row.size()) ? row[i] : "";
          sample << columns[i] << "=" << value;
        }
        lines.push_back(sample.str());
      } else {
        lines.push_back("(keine Daten)");
      }
    } catch (const std::exception& ex) {
      lines.push_back(std::string("(Fehler: ") + ex.what() + ")");
    }
    set_result_text("describe", lines);
    return true;
  }

  if (lower.rfind("limit ", 0) == 0) {
    const std::string value = trim_copy(lower.substr(6));
    if (value == "off") {
      default_limit_ = -1;
      set_status("limit off");
    } else {
      default_limit_ = std::stoi(value);
      set_status("limit " + std::to_string(default_limit_));
    }
    update_log();
    return true;
  }

  if (lower.rfind("show ", 0) == 0) {
    const std::string value = trim_copy(cmd.substr(5));
    if (value == "off") {
      show_columns_.clear();
      set_status("show off");
    } else {
      show_columns_ = split_by_any(value, ",");
      set_status("show " + value);
    }
    update_log();
    return true;
  }

  if (lower == "focus") {
    set_result_text("focus", { "x=" + std::to_string(focus_x_->GetValue()) +
                               " y=" + std::to_string(focus_y_->GetValue()) +
                               " r=" + std::to_string(radius_->GetValue()) });
    return true;
  }

  if (lower == "explain") {
    auto lines = run_shell_command("explain");
    set_result_text("explain", lines);
    return true;
  }

  if (lower == "exit") {
    Close(true);
    return true;
  }

  if (lower == "unfocus") {
    focus_x_->SetValue(0);
    focus_y_->SetValue(0);
    focus_set_ = false;
    set_status("Fokus entfernt.");
    update_log();
    return true;
  }

  if (lower.rfind("radius ", 0) == 0) {
    const std::string value = trim_copy(lower.substr(7));
    radius_->SetValue(std::stoi(value));
    set_status("radius " + value);
    update_log();
    return true;
  }

  if (lower.rfind("goto ", 0) == 0) {
    const std::string value = trim_copy(lower.substr(5));
    focus_payload_id_->SetValue(value);
    wxCommandEvent evt;
    on_focus_by_payload(evt);
    return true;
  }

  if (lower == "history") {
    set_result_text("history", query_history_);
    return true;
  }

  if (lower == "last" || lower == "redo") {
    if (!query_history_.empty()) {
      start_query(query_history_.back(), focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    }
    return true;
  }

  if (lower.size() > 1 && lower[0] == '!') {
    int index = std::stoi(lower.substr(1));
    if (index > 0 && index <= static_cast<int>(query_history_.size())) {
      start_query(query_history_[static_cast<size_t>(index - 1)], focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
    }
    return true;
  }

  if (lower.rfind("save ", 0) == 0) {
    auto tokens = tokenize_command(cmd);
    if (tokens.size() >= 2) {
      std::string name = tokens[1];
      std::string stored;
      if (tokens.size() >= 3) {
        stored = trim_copy(cmd.substr(cmd.find(name) + name.size()));
        stored = trim_copy(stored);
      } else {
        stored = query_edit_->GetText().ToStdString();
      }
      macros_[name] = stored;
      set_status("Makro gespeichert: " + name);
    }
    return true;
  }

  if (lower.rfind("run ", 0) == 0) {
    auto tokens = tokenize_command(cmd);
    if (tokens.size() >= 2) {
      auto it = macros_.find(tokens[1]);
      if (it != macros_.end()) {
        start_query(it->second, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
      } else {
        show_error("Makro nicht gefunden.");
      }
    }
    return true;
  }

  if (lower.rfind("export ", 0) == 0) {
    auto tokens = tokenize_command(cmd);
    if (tokens.size() >= 3) {
      wxGrid* grid = (result_tabs_->GetSelection() == 0) ? sql_grid_ : debug_grid_;
      if (tokens[1] == "csv") {
        export_grid_to_csv(grid, tokens[2]);
      } else if (tokens[1] == "json") {
        export_grid_to_json(grid, tokens[2]);
      }
    }
    return true;
  }

  if (lower.rfind("format ", 0) == 0) {
    output_format_ = trim_copy(lower.substr(7));
    set_status("format " + output_format_);
    update_log();
    return true;
  }

  if (lower == "sort reset") {
    reset_sort();
    return true;
  }

  if (lower.rfind("sort ", 0) == 0) {
    apply_sort(cmd.substr(5));
    return true;
  }

  if (lower == "stats") {
    std::vector<std::string> lines;
    for (unsigned int i = 0; i < tables_list_->GetCount(); ++i) {
      const std::string name = tables_list_->GetString(i).ToStdString();
      std::string query = "SELECT COUNT(*) AS C FROM " + name;
      try {
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> rows;
        bridge_->query_sql_table(query, focus_set_, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue(),
                                 columns, rows);
        std::string count = "0";
        if (!rows.empty()) {
          if (!rows.front().empty()) {
            count = rows.front().front();
          }
          for (size_t c = 0; c < columns.size(); ++c) {
            if (to_lower_ascii(columns[c]) == "c" && c < rows.front().size()) {
              count = rows.front()[c];
              break;
            }
          }
        }
        lines.push_back(name + " -> " + count);
      } catch (...) {
        lines.push_back(name + " -> (Fehler)");
      }
    }
    set_result_text("stats", lines);
    return true;
  }

  if (lower == "delta" || lower == "merge" || lower.rfind("merge auto", 0) == 0 ||
      lower == "delta show" || lower == "undo") {
    auto lines = run_shell_command(cmd);
    set_result_text(cmd, lines);
    return true;
  }

  if (lower.rfind("sql ", 0) == 0 || lower.rfind("select", 0) == 0 || lower.rfind("with", 0) == 0) {
    return false;
  }

  auto is_number = [](const std::string& value) {
    if (value.empty()) return false;
    char* end_ptr = nullptr;
    std::strtod(value.c_str(), &end_ptr);
    return end_ptr && *end_ptr == '\0';
  };

  auto quote_value = [&](const std::string& value) {
    std::string trimmed = trim_copy(value);
    if (trimmed.empty()) return trimmed;
    if (trimmed.front() == '\'' || trimmed.front() == '"') return trimmed;
    if (is_number(trimmed)) return trimmed;
    return "'" + trimmed + "'";
  };

  std::string base = cmd;
  std::string show_inline;
  const auto show_pos = lower.find(" show ");
  if (show_pos != std::string::npos) {
    base = trim_copy(cmd.substr(0, show_pos));
    show_inline = trim_copy(cmd.substr(show_pos + 6));
  }

  auto select_cols = [&](const std::string& table) {
    if (!show_inline.empty()) return show_inline;
    if (!show_columns_.empty()) {
      std::string cols;
      for (size_t i = 0; i < show_columns_.size(); ++i) {
        if (i > 0) cols += ",";
        cols += show_columns_[i];
      }
      return cols;
    }
    return std::string("*");
  };

  if (base.find('=') != std::string::npos && base.find(' ') == std::string::npos) {
    const auto eq_pos = base.find('=');
    const std::string column = trim_copy(base.substr(0, eq_pos));
    const std::string value = quote_value(base.substr(eq_pos + 1));
    if (!column.empty() && !value.empty()) {
      std::string label = base;
      if (default_limit_ > 0) {
        label += " LIMIT " + std::to_string(default_limit_);
      }
      if (!query_history_.empty()) {
        if (query_history_.back() != label) {
          query_history_.push_back(label);
        }
      } else {
        query_history_.push_back(label);
      }
      history_index_ = static_cast<int>(query_history_.size());
      last_query_ = label;
      last_error_.clear();

      ++query_token_;
      query_running_.store(true);
      set_status("Query laeuft...");

      auto* query_btn = FindWindow(kIdQuery);
      auto* cancel_btn = FindWindow(kIdCancel);
      if (query_btn) query_btn->Enable(false);
      if (cancel_btn) cancel_btn->Enable(true);

      start_query_task(label, [this, column, value]() {
        QueryResult result;
        for (unsigned int i = 0; i < tables_list_->GetCount(); ++i) {
          const std::string table = tables_list_->GetString(i).ToStdString();
          std::string select_cols = "*";
          if (!show_columns_.empty()) {
            select_cols.clear();
            for (size_t c = 0; c < show_columns_.size(); ++c) {
              if (c > 0) select_cols += ",";
              select_cols += show_columns_[c];
            }
          }
          std::string query = "sql SELECT " + select_cols + " FROM " + table +
                              " WHERE " + column + "=" + value;
          if (default_limit_ > 0) {
            query += " LIMIT " + std::to_string(default_limit_);
          }
          auto payloads = bridge_->query_focus(query, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
          result.payloads.insert(result.payloads.end(), payloads.begin(), payloads.end());
        }
        return result;
      });
      return true;
    }
  }

  const auto space_pos = base.find(' ');
  if (space_pos != std::string::npos) {
    const std::string table = trim_copy(base.substr(0, space_pos));
    const std::string rest = trim_copy(base.substr(space_pos + 1));
    if (!table.empty() && !rest.empty()) {
      if (rest.find('=') != std::string::npos) {
        const auto eq_pos = rest.find('=');
        const std::string column = trim_copy(rest.substr(0, eq_pos));
        const std::string value = quote_value(rest.substr(eq_pos + 1));
        if (!column.empty() && !value.empty()) {
          std::string sql = "sql SELECT " + select_cols(table) + " FROM " + table +
                            " WHERE " + column + "=" + value;
          bypass_shell_command_ = true;
          start_query(sql, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
          bypass_shell_command_ = false;
          return true;
        }
      } else if (is_number(rest)) {
        std::string pk = table + "Id";
        auto it = table_pk_cache_.find(table);
        if (it != table_pk_cache_.end() && !it->second.empty()) {
          pk = it->second;
        } else {
          update_schema_for_table(table);
          it = table_pk_cache_.find(table);
          if (it != table_pk_cache_.end() && !it->second.empty()) {
            pk = it->second;
          }
        }
        std::string sql = "sql SELECT " + select_cols(table) + " FROM " + table +
                          " WHERE " + pk + "=" + rest;
        bypass_shell_command_ = true;
        start_query(sql, focus_x_->GetValue(), focus_y_->GetValue(), radius_->GetValue());
        bypass_shell_command_ = false;
        return true;
      }
    }
  }

  return false;
}

void MainWindow::set_result_text(const std::string& title, const std::vector<std::string>& lines) {
  sql_columns_.assign(1, title);
  sql_rows_.clear();
  sql_rows_.reserve(lines.size());
  for (const auto& line : lines) {
    sql_rows_.push_back({line});
  }
  sql_rows_original_ = sql_rows_;
  apply_grid_data(sql_grid_, sql_columns_, sql_rows_);
  result_tabs_->SetSelection(0);
  update_filter_columns();
  update_export_columns();
}

void MainWindow::apply_grid_data(wxGrid* grid, const std::vector<std::string>& columns,
                                 const std::vector<std::vector<std::string>>& rows) {
  if (!grid) return;
  if (grid->GetNumberRows() > 0) {
    grid->DeleteRows(0, grid->GetNumberRows());
  }
  if (grid->GetNumberCols() > 0) {
    grid->DeleteCols(0, grid->GetNumberCols());
  }
  grid->AppendCols(static_cast<int>(columns.size()));
  for (size_t c = 0; c < columns.size(); ++c) {
    grid->SetColLabelValue(static_cast<int>(c), columns[c]);
  }
  grid->AppendRows(static_cast<int>(rows.size()));
  for (size_t r = 0; r < rows.size(); ++r) {
    for (size_t c = 0; c < rows[r].size(); ++c) {
      grid->SetCellValue(static_cast<int>(r), static_cast<int>(c), rows[r][c]);
    }
  }
  grid->AutoSizeColumns(false);
}

void MainWindow::on_table_selected(wxCommandEvent& event) {
  const int selection = event.GetSelection();
  if (selection == wxNOT_FOUND) {
    return;
  }
  update_schema_for_table(tables_list_->GetString(selection).ToStdString());
}

void MainWindow::on_table_activated(wxCommandEvent& event) {
  const int selection = event.GetSelection();
  if (selection == wxNOT_FOUND) {
    return;
  }
  const std::string table_name = tables_list_->GetString(selection).ToStdString();
  std::string template_query = "sql SELECT * FROM " + table_name + " LIMIT 50";
  query_edit_->SetText(template_query);
}
