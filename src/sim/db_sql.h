#pragma once

#include "db_engine.h"

#include <string>
#include <vector>

struct DbSqlResult {
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

bool db_execute_sql(DbWorld &world,
                    const std::string &sql,
                    bool use_focus,
                    int focus_x,
                    int focus_y,
                    int radius,
                    DbSqlResult &out,
                    std::string &error);
