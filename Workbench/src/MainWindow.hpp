// MainWindow.hpp
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <wx/aui/aui.h>
#include <wx/aui/auibook.h>
#include <wx/checklst.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/grid.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/textctrl.h>

#include "MicroSwarmBridge.hpp"

class MainWindow : public wxFrame {
public:
  explicit MainWindow();
  ~MainWindow() override;

private:
  struct QueryResult {
    uint64_t token = 0;
    bool ok = true;
    std::string error;
    std::vector<PayloadRow> payloads;
    std::vector<std::string> sql_columns;
    std::vector<std::vector<std::string>> sql_rows;
    bool has_sql_table = false;
    long long duration_ms = 0;
  };
  void set_status(const std::string& text);
  void show_error(const std::string& text);
  void set_badge(wxStaticText* badge, bool ok, const std::string& text);
  void populate_payloads(const std::vector<PayloadRow>& payloads);
  void populate_sql_result(const std::vector<PayloadRow>& payloads);
  void clear_sql_result();
  void clear_debug_result();
  std::optional<int> selected_payload_id() const;
  void start_query(const std::string& query, int focus_x, int focus_y, int radius);
  void cancel_query();
  void load_tables();
  void update_schema_for_table(const std::string& table_name);
  void update_log();
  void set_result_text(const std::string& title, const std::vector<std::string>& lines);
  void apply_grid_data(wxGrid* grid, const std::vector<std::string>& columns,
                       const std::vector<std::vector<std::string>>& rows);
  bool handle_shell_command(const std::string& command);
  std::vector<std::string> run_shell_command(const std::string& command, const std::string& format = {});
  void start_query_task(const std::string& label, std::function<QueryResult()> task);
  void export_grid_to_csv(wxGrid* grid, const std::string& path);
  void export_grid_to_json(wxGrid* grid, const std::string& path);
  void apply_sort(const std::string& spec);
  void reset_sort();

  void on_connect(wxCommandEvent& event);
  void on_open_db(wxCommandEvent& event);
  void on_query_focus(wxCommandEvent& event);
  void on_table_count(wxCommandEvent& event);
  void on_focus_by_payload(wxCommandEvent& event);
  void on_clear_focus(wxCommandEvent& event);
  void on_focus_from_selection(wxCommandEvent& event);
  void on_query_complete(wxThreadEvent& event);
  void on_cancel_query(wxCommandEvent& event);
  void on_table_selected(wxCommandEvent& event);
  void on_table_activated(wxCommandEvent& event);
  void on_export_csv(wxCommandEvent& event);
  void on_export_json(wxCommandEvent& event);
  void on_copy_selection(wxCommandEvent& event);
  void on_query_key_down(wxKeyEvent& event);
  void on_page_prev(wxCommandEvent& event);
  void on_page_next(wxCommandEvent& event);
  void on_page_size_changed(wxCommandEvent& event);
  void on_grid_cell_menu(wxGridEvent& event);
  void on_grid_label_menu(wxGridEvent& event);
  void on_undo_delta(wxCommandEvent& event);
  void on_merge_delta(wxCommandEvent& event);
  void on_favorite_save(wxCommandEvent& event);
  void on_favorite_run(wxCommandEvent& event);
  void on_filter_changed(wxCommandEvent& event);
  void on_filter_clear(wxCommandEvent& event);
  void on_export_run(wxCommandEvent& event);
  void on_query_tab_close(wxAuiNotebookEvent& event);
  void update_filter_columns();
  void update_export_columns();
  void update_query_tab(const std::string& query,
                        const std::vector<std::string>& columns,
                        const std::vector<std::vector<std::string>>& rows);
  void update_diff_view();
  void refresh_tools_view();

  wxTextCtrl* library_path_ = nullptr;
  wxTextCtrl* db_path_ = nullptr;
  wxStaticText* status_label_ = nullptr;
  wxStaticText* api_badge_ = nullptr;
  wxStaticText* db_badge_ = nullptr;
  wxStaticText* api_version_label_ = nullptr;
  wxStyledTextCtrl* query_edit_ = nullptr;
  wxSpinCtrl* focus_x_ = nullptr;
  wxSpinCtrl* focus_y_ = nullptr;
  wxSpinCtrl* radius_ = nullptr;
  wxTextCtrl* focus_payload_id_ = nullptr;
  wxChoice* query_mode_ = nullptr;
  wxSpinCtrl* page_size_ctrl_ = nullptr;
  wxStaticText* page_label_ = nullptr;
  wxButton* page_prev_btn_ = nullptr;
  wxButton* page_next_btn_ = nullptr;
  wxButton* undo_btn_ = nullptr;
  wxButton* merge_btn_ = nullptr;
  wxGrid* sql_grid_ = nullptr;
  wxGrid* debug_grid_ = nullptr;
  wxAuiNotebook* result_tabs_ = nullptr;
  wxAuiNotebook* tools_tabs_ = nullptr;
  wxAuiNotebook* query_tabs_ = nullptr;
  wxTextCtrl* diff_view_ = nullptr;
  wxChoice* filter_column_ = nullptr;
  wxTextCtrl* filter_text_ = nullptr;
  wxButton* filter_clear_btn_ = nullptr;
  wxChoice* export_format_ = nullptr;
  wxCheckListBox* export_columns_ = nullptr;
  wxChoice* export_target_ = nullptr;
  wxButton* export_run_btn_ = nullptr;
  wxCheckBox* auto_explain_cb_ = nullptr;
  wxCheckBox* auto_stats_cb_ = nullptr;
  wxTextCtrl* auto_explain_view_ = nullptr;
  wxTextCtrl* auto_stats_view_ = nullptr;
  wxButton* fav_save_btn_ = nullptr;
  std::vector<wxButton*> fav_buttons_;
  wxAuiManager aui_manager_;
  wxListBox* tables_list_ = nullptr;
  wxTextCtrl* schema_view_ = nullptr;
  wxTextCtrl* log_view_ = nullptr;

  std::unique_ptr<MicroSwarmBridge> bridge_;
  bool api_ready_ = false;
  bool db_ready_ = false;
  std::atomic<uint64_t> query_token_{0};
  std::atomic<bool> query_running_{false};
  std::thread query_thread_;
  std::string last_query_;
  std::string last_error_;
  size_t last_hits_ = 0;
  long long last_duration_ms_ = 0;
  std::vector<std::string> query_history_;
  int history_index_ = -1;
  std::unordered_map<std::string, std::string> macros_;
  int default_limit_ = -1;
  std::vector<std::string> show_columns_;
  std::string output_format_{"table"};
  std::unordered_map<std::string, std::string> table_pk_cache_;
  std::vector<std::string> sql_columns_;
  std::vector<std::vector<std::string>> sql_rows_;
  std::vector<std::vector<std::string>> sql_rows_original_;
  std::vector<std::string> debug_columns_;
  std::vector<std::vector<std::string>> debug_rows_;
  std::vector<std::vector<std::string>> debug_rows_original_;
  bool bypass_shell_command_ = false;
  std::string shell_exe_path_;
  bool focus_set_ = false;
  std::string last_user_query_;
  int page_index_ = 0;
  bool last_query_paging_ = false;
  bool skip_history_next_ = false;
  bool keep_page_index_next_ = false;
  std::string last_exec_query_;
  std::vector<std::string> fav_queries_;
  struct QuerySnapshot {
    std::string label;
    std::string query;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    wxGrid* grid = nullptr;
  };
  std::vector<QuerySnapshot> query_tabs_data_;
};
