#include "db_engine.h"

#include "params.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace {
std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return s.substr(start, end - start);
}

bool parse_int_value(const std::string &s, int &out) {
    try {
        size_t idx = 0;
        int v = std::stoi(s, &idx);
        if (idx == 0) {
            return false;
        }
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

std::string strip_quotes(const std::string &s) {
    if (s.size() >= 2) {
        char a = s.front();
        char b = s.back();
        if ((a == '\'' && b == '\'') || (a == '"' && b == '"')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

bool ieq_prefix(const std::string &s, const std::string &prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

bool ieq(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string escape_string(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string unescape_string(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[i + 1];
            switch (n) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '\\': out.push_back('\\'); break;
                default: out.push_back(n); break;
            }
            i++;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

bool ends_with_id(const std::string &name) {
    if (name.size() < 2) return false;
    std::string lower = to_lower(name);
    if (lower == "id") return false;
    if (lower.size() >= 3 && lower.substr(lower.size() - 3) == "_id") return true;
    if (lower.size() >= 2 && lower.substr(lower.size() - 2) == "id") return true;
    return false;
}

bool is_pk_column(const std::string &column, const std::string &table) {
    std::string col = to_lower(column);
    std::string tbl = to_lower(table);
    return col == "id" || col == tbl + "id" || col == tbl + "_id";
}

std::string strip_table_prefix(const std::string &column) {
    size_t dot = column.find('.');
    if (dot == std::string::npos) return column;
    return column.substr(dot + 1);
}

struct Token {
    std::string text;
};

std::vector<Token> tokenize_sql(const std::string &sql) {
    std::vector<Token> out;
    std::string cur;
    auto flush = [&]() {
        if (!cur.empty()) {
            out.push_back({cur});
            cur.clear();
        }
    };
    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            flush();
            continue;
        }
        if (c == '\'' || c == '"') {
            flush();
            char q = c;
            std::string val;
            i++;
            while (i < sql.size()) {
                char x = sql[i];
                if (x == q) {
                    if (i + 1 < sql.size() && sql[i + 1] == q) {
                        val.push_back(q);
                        i += 2;
                        continue;
                    }
                    break;
                }
                val.push_back(x);
                i++;
            }
            out.push_back({std::string(1, q) + val + std::string(1, q)});
            continue;
        }
        if (std::strchr("(),=*", c)) {
            flush();
            out.push_back({std::string(1, c)});
            continue;
        }
        cur.push_back(c);
    }
    flush();
    return out;
}

struct Parser {
    std::vector<Token> tokens;
    size_t pos = 0;

    bool eof() const { return pos >= tokens.size(); }
    const std::string &peek() const { static std::string empty; return eof() ? empty : tokens[pos].text; }
    bool match(const char *kw) {
        if (!eof() && ieq(tokens[pos].text, kw)) {
            pos++;
            return true;
        }
        return false;
    }
    bool match_symbol(const char *sym) {
        if (!eof() && tokens[pos].text == sym) {
            pos++;
            return true;
        }
        return false;
    }
    std::string consume() {
        if (eof()) return "";
        return tokens[pos++].text;
    }
};

std::string fk_table_from_column(const std::string &name) {
    std::string lower = to_lower(name);
    if (lower.size() >= 3 && lower.substr(lower.size() - 3) == "_id") {
        return name.substr(0, name.size() - 3);
    }
    if (lower.size() >= 2 && lower.substr(lower.size() - 2) == "id") {
        return name.substr(0, name.size() - 2);
    }
    return name;
}

struct SqlInsert {
    std::string table;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

bool parse_identifier(const std::string &s, size_t &i, std::string &out) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i >= s.size()) return false;
    char quote = 0;
    if (s[i] == '`' || s[i] == '"') {
        quote = s[i++];
        size_t start = i;
        while (i < s.size() && s[i] != quote) i++;
        if (i >= s.size()) return false;
        out = s.substr(start, i - start);
        i++;
        size_t save = i;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i < s.size() && s[i] == '.') {
            i++;
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
            if (i < s.size() && (s[i] == '`' || s[i] == '"')) {
                char q2 = s[i++];
                size_t s2 = i;
                while (i < s.size() && s[i] != q2) i++;
                if (i < s.size()) {
                    out = s.substr(s2, i - s2);
                    i++;
                }
            } else {
                i = save;
            }
        } else {
            i = save;
        }
        return true;
    }
    size_t start = i;
    while (i < s.size()) {
        char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ',' || c == ')') break;
        i++;
    }
    if (i <= start) return false;
    out = s.substr(start, i - start);
    size_t dot = out.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < out.size()) {
        out = out.substr(dot + 1);
    }
    return true;
}

bool parse_columns_list(const std::string &s, size_t &i, std::vector<std::string> &cols) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i >= s.size() || s[i] != '(') return false;
    i++;
    cols.clear();
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i < s.size() && s[i] == ')') {
            i++;
            return true;
        }
        std::string col;
        if (!parse_identifier(s, i, col)) return false;
        cols.push_back(col);
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i < s.size() && s[i] == ',') {
            i++;
        }
    }
    return false;
}

bool parse_value(const std::string &s, size_t &i, std::string &out) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i >= s.size()) return false;
    if (s[i] == '\'' || s[i] == '"') {
        char quote = s[i++];
        std::string val;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '\\' && i < s.size()) {
                char n = s[i++];
                val.push_back(n);
                continue;
            }
            if (c == quote) {
                if (i < s.size() && s[i] == quote) {
                    val.push_back(quote);
                    i++;
                    continue;
                }
                break;
            }
            val.push_back(c);
        }
        out = val;
        return true;
    }
    size_t start = i;
    while (i < s.size()) {
        char c = s[i];
        if (c == ',' || c == ')') break;
        i++;
    }
    out = trim(s.substr(start, i - start));
    return true;
}

bool parse_values_list(const std::string &s, size_t &i, std::vector<std::vector<std::string>> &rows) {
    rows.clear();
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i >= s.size()) return false;
        if (s[i] == ';') return true;
        if (s[i] != '(') {
            i++;
            continue;
        }
        i++;
        std::vector<std::string> row;
        while (i < s.size()) {
            std::string value;
            if (!parse_value(s, i, value)) return false;
            row.push_back(value);
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
            if (i < s.size() && s[i] == ',') {
                i++;
                continue;
            }
            if (i < s.size() && s[i] == ')') {
                i++;
                break;
            }
            if (i < s.size()) {
                return false;
            }
        }
        if (!row.empty()) {
            rows.push_back(std::move(row));
        }
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
        if (i < s.size() && s[i] == ',') {
            i++;
            continue;
        }
        if (i < s.size() && s[i] == ';') {
            return true;
        }
    }
    return true;
}

bool parse_insert_statement(const std::string &stmt, SqlInsert &out) {
    size_t i = 0;
    while (i < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[i]))) i++;
    if (!ieq_prefix(stmt.substr(i), "insert into")) {
        return false;
    }
    i += 11;
    std::string table;
    if (!parse_identifier(stmt, i, table)) {
        return false;
    }
    out.table = table;
    size_t save = i;
    if (!parse_columns_list(stmt, i, out.columns)) {
        out.columns.clear();
        i = save;
    }
    while (i < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[i]))) i++;
    if (!ieq_prefix(stmt.substr(i), "values")) {
        return false;
    }
    i += 6;
    return parse_values_list(stmt, i, out.rows);
}

bool parse_insert_statement_lenient(const std::string &stmt, SqlInsert &out) {
    std::string lower = to_lower(stmt);
    size_t pos = lower.find("insert");
    if (pos == std::string::npos) return false;
    pos = lower.find("into", pos);
    if (pos == std::string::npos) return false;
    pos += 4;
    while (pos < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[pos]))) pos++;
    if (pos >= stmt.size()) return false;
    std::string table;
    size_t i = pos;
    if (stmt[i] == '`' || stmt[i] == '"') {
        char q = stmt[i++];
        size_t s = i;
        while (i < stmt.size() && stmt[i] != q) i++;
        if (i >= stmt.size()) return false;
        table = stmt.substr(s, i - s);
        i++;
        while (i < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[i]))) i++;
        if (i < stmt.size() && stmt[i] == '.') {
            i++;
            while (i < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[i]))) i++;
            if (i < stmt.size() && (stmt[i] == '`' || stmt[i] == '"')) {
                char q2 = stmt[i++];
                size_t s2 = i;
                while (i < stmt.size() && stmt[i] != q2) i++;
                if (i < stmt.size()) {
                    table = stmt.substr(s2, i - s2);
                    i++;
                }
            }
        }
    } else {
        size_t s = i;
        while (i < stmt.size()) {
            char c = stmt[i];
            if (std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ';') break;
            if (c == '.') {
                s = i + 1;
            }
            i++;
        }
        if (i <= s) return false;
        table = stmt.substr(s, i - s);
    }
    if (table.empty()) return false;
    size_t values_pos = lower.find("values", i);
    if (values_pos == std::string::npos) return false;
    values_pos += 6;
    std::string tail = stmt.substr(values_pos);
    size_t ti = 0;
    while (ti < tail.size() && std::isspace(static_cast<unsigned char>(tail[ti]))) ti++;
    if (ti >= tail.size() || tail[ti] != '(') return false;
    out.table = table;
    out.columns.clear();
    out.rows.clear();
    return parse_values_list(tail, ti, out.rows);
}

bool parse_create_table_statement(const std::string &stmt, std::string &table, std::vector<std::string> &columns) {
    size_t i = 0;
    while (i < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[i]))) i++;
    if (!ieq_prefix(stmt.substr(i), "create table")) {
        return false;
    }
    i += 12;
    {
        size_t tmp = i;
        while (tmp < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[tmp]))) tmp++;
        std::string rest = to_lower(stmt.substr(tmp));
        if (ieq_prefix(rest, "if not exists")) {
            tmp += 13;
            i = tmp;
        }
    }
    if (!parse_identifier(stmt, i, table)) {
        return false;
    }
    size_t open = stmt.find('(', i);
    size_t close = stmt.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return false;
    }
    std::string inner = stmt.substr(open + 1, close - open - 1);
    columns.clear();

    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    bool in_string = false;
    char string_quote = 0;
    for (size_t p = 0; p < inner.size(); ++p) {
        char c = inner[p];
        if ((c == '\'' || c == '"') && (!in_string || c == string_quote)) {
            if (in_string && c == string_quote) {
                in_string = false;
            } else if (!in_string) {
                in_string = true;
                string_quote = c;
            }
        }
        if (!in_string) {
            if (c == '(') depth++;
            if (c == ')') depth = std::max(0, depth - 1);
            if (c == ',' && depth == 0) {
                parts.push_back(current);
                current.clear();
                continue;
            }
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        parts.push_back(current);
    }

    for (auto &part : parts) {
        std::string item = trim(part);
        if (item.empty()) continue;
        std::string lower = to_lower(item);
        if (ieq_prefix(lower, "primary key") || ieq_prefix(lower, "foreign key") ||
            ieq_prefix(lower, "constraint") || ieq_prefix(lower, "unique") ||
            ieq_prefix(lower, "key") || ieq_prefix(lower, "index")) {
            continue;
        }
        size_t pos = 0;
        std::string col;
        if (parse_identifier(item, pos, col)) {
            columns.push_back(col);
        }
    }
    return !columns.empty();
}

int64_t make_payload_key(int table_id, int id) {
    return (static_cast<int64_t>(table_id) << 32) | static_cast<uint32_t>(id);
}

std::string build_raw_data(const std::vector<DbField> &fields) {
    std::ostringstream ss;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << fields[i].name << "=" << fields[i].value;
    }
    return ss.str();
}

struct IngestRule {
    std::string column;
    std::string pattern;
    double weight = 1.0;
    std::string type;
    bool pattern_rule = false;
    std::regex pattern_re;
};

struct IngestRules {
    std::vector<IngestRule> default_rules;
    std::unordered_map<std::string, std::vector<IngestRule>> table_rules;
};

enum class JsonTok {
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Colon,
    Comma,
    String,
    Number,
    Bare,
    End,
    Invalid
};

struct JsonToken {
    JsonTok type = JsonTok::Invalid;
    std::string text;
    double number = 0.0;
};

class JsonReader {
public:
    explicit JsonReader(const std::string &src) : s(src) {}

    JsonToken next() {
        skip_ws();
        if (i >= s.size()) return {JsonTok::End, {}, 0.0};
        char c = s[i];
        if (c == '{') { ++i; return {JsonTok::LBrace, "{", 0.0}; }
        if (c == '}') { ++i; return {JsonTok::RBrace, "}", 0.0}; }
        if (c == '[') { ++i; return {JsonTok::LBracket, "[", 0.0}; }
        if (c == ']') { ++i; return {JsonTok::RBracket, "]", 0.0}; }
        if (c == ':') { ++i; return {JsonTok::Colon, ":", 0.0}; }
        if (c == ',') { ++i; return {JsonTok::Comma, ",", 0.0}; }
        if (c == '"') {
            ++i;
            std::string out;
            while (i < s.size()) {
                char ch = s[i++];
                if (ch == '"') break;
                if (ch == '\\' && i < s.size()) {
                    char esc = s[i++];
                    switch (esc) {
                        case '\\': out.push_back('\\'); break;
                        case '"': out.push_back('"'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        default: out.push_back(esc); break;
                    }
                } else {
                    out.push_back(ch);
                }
            }
            return {JsonTok::String, out, 0.0};
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+') {
            size_t start = i;
            while (i < s.size()) {
                char ch = s[i];
                if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '.' && ch != 'e' && ch != 'E' &&
                    ch != '-' && ch != '+') {
                    break;
                }
                ++i;
            }
            std::string num = s.substr(start, i - start);
            char *endp = nullptr;
            double val = std::strtod(num.c_str(), &endp);
            if (!endp || endp == num.c_str()) {
                return {JsonTok::Invalid, num, 0.0};
            }
            return {JsonTok::Number, num, val};
        }
        if (std::isalpha(static_cast<unsigned char>(c))) {
            size_t start = i;
            while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) ++i;
            std::string word = s.substr(start, i - start);
            return {JsonTok::Bare, word, 0.0};
        }
        ++i;
        return {JsonTok::Invalid, std::string(1, c), 0.0};
    }

private:
    void skip_ws() {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }

    const std::string &s;
    size_t i = 0;
};

bool skip_json_value_with_first(JsonReader &reader, JsonToken tok);

bool skip_json_object(JsonReader &reader) {
    for (;;) {
        JsonToken key = reader.next();
        if (key.type == JsonTok::RBrace) return true;
        if (key.type != JsonTok::String && key.type != JsonTok::Bare) return false;
        if (reader.next().type != JsonTok::Colon) return false;
        if (!skip_json_value_with_first(reader, reader.next())) return false;
        JsonToken sep = reader.next();
        if (sep.type == JsonTok::RBrace) return true;
        if (sep.type != JsonTok::Comma) return false;
    }
}

bool skip_json_array(JsonReader &reader) {
    for (;;) {
        JsonToken next = reader.next();
        if (next.type == JsonTok::RBracket) return true;
        if (!skip_json_value_with_first(reader, next)) return false;
        JsonToken sep = reader.next();
        if (sep.type == JsonTok::RBracket) return true;
        if (sep.type != JsonTok::Comma) return false;
    }
}

bool skip_json_value_with_first(JsonReader &reader, JsonToken tok) {
    if (tok.type == JsonTok::LBrace) return skip_json_object(reader);
    if (tok.type == JsonTok::LBracket) return skip_json_array(reader);
    return tok.type == JsonTok::String || tok.type == JsonTok::Number || tok.type == JsonTok::Bare;
}

bool parse_rule_object_from_open(JsonReader &reader, IngestRule &out, std::string &error) {
    for (;;) {
        JsonToken key = reader.next();
        if (key.type == JsonTok::RBrace) break;
        if (key.type != JsonTok::String) {
            error = "rule key erwartet";
            return false;
        }
        if (reader.next().type != JsonTok::Colon) {
            error = "rule ':' erwartet";
            return false;
        }
        JsonToken val = reader.next();
        if (key.text == "column" && val.type == JsonTok::String) {
            out.column = val.text;
        } else if (key.text == "pattern" && val.type == JsonTok::String) {
            out.pattern = val.text;
            out.pattern_rule = true;
        } else if (key.text == "weight" && val.type == JsonTok::Number) {
            out.weight = val.number;
        } else if (key.text == "type" && val.type == JsonTok::String) {
            out.type = val.text;
        } else {
            if (!skip_json_value_with_first(reader, val)) {
                error = "rule value ungueltig";
                return false;
            }
        }
        JsonToken sep = reader.next();
        if (sep.type == JsonTok::RBrace) break;
        if (sep.type != JsonTok::Comma) {
            error = "rule ',' erwartet";
            return false;
        }
    }
    if (out.column.empty() && out.pattern.empty()) {
        error = "rule braucht column oder pattern";
        return false;
    }
    if (out.type.empty()) {
        out.type = out.pattern_rule ? "foreign_key" : "trait_cluster";
    }
    out.type = to_lower(out.type);
    if (out.pattern_rule) {
        try {
            out.pattern_re = std::regex(out.pattern, std::regex::icase);
        } catch (const std::regex_error &) {
            error = "ungueltiges Regex: " + out.pattern;
            return false;
        }
    }
    return true;
}

bool parse_rules_array(JsonReader &reader, std::vector<IngestRule> &out, std::string &error) {
    JsonToken tok = reader.next();
    if (tok.type != JsonTok::LBracket) {
        error = "Array erwartet";
        return false;
    }
    for (;;) {
        JsonToken next = reader.next();
        if (next.type == JsonTok::RBracket) break;
        if (next.type != JsonTok::LBrace) {
            error = "rule object erwartet";
            return false;
        }
        IngestRule rule;
        if (!parse_rule_object_from_open(reader, rule, error)) return false;
        out.push_back(std::move(rule));
        JsonToken sep = reader.next();
        if (sep.type == JsonTok::RBracket) break;
        if (sep.type != JsonTok::Comma) {
            error = "rule ',' erwartet";
            return false;
        }
    }
    return true;
}

bool load_ingest_rules(const std::string &path, IngestRules &rules, std::string &error) {
    std::ifstream in(path);
    if (!in.is_open()) {
        error = "Konnte ingest_rules nicht oeffnen: " + path;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    JsonReader reader(content);
    JsonToken root = reader.next();
    if (root.type != JsonTok::LBrace) {
        error = "JSON-Root muss Object sein";
        return false;
    }
    for (;;) {
        JsonToken key = reader.next();
        if (key.type == JsonTok::RBrace) break;
        if (key.type != JsonTok::String) {
            error = "JSON-Key erwartet";
            return false;
        }
        if (reader.next().type != JsonTok::Colon) {
            error = "JSON ':' erwartet";
            return false;
        }
        if (key.text == "default_rules") {
            if (!parse_rules_array(reader, rules.default_rules, error)) return false;
        } else if (key.text == "table_rules") {
            JsonToken obj = reader.next();
            if (obj.type != JsonTok::LBrace) {
                error = "table_rules erwartet Object";
                return false;
            }
            for (;;) {
                JsonToken tkey = reader.next();
                if (tkey.type == JsonTok::RBrace) break;
                if (tkey.type != JsonTok::String) {
                    error = "table name erwartet";
                    return false;
                }
                if (reader.next().type != JsonTok::Colon) {
                    error = "table_rules ':' erwartet";
                    return false;
                }
                std::vector<IngestRule> table_entries;
                if (!parse_rules_array(reader, table_entries, error)) return false;
                rules.table_rules[to_lower(tkey.text)] = std::move(table_entries);
                JsonToken sep = reader.next();
                if (sep.type == JsonTok::RBrace) break;
                if (sep.type != JsonTok::Comma) {
                    error = "table_rules ',' erwartet";
                    return false;
                }
            }
        } else {
            if (!skip_json_value_with_first(reader, reader.next())) {
                error = "JSON-Parsing fehlgeschlagen";
                return false;
            }
        }
        JsonToken sep = reader.next();
        if (sep.type == JsonTok::RBrace) break;
        if (sep.type != JsonTok::Comma) {
            error = "JSON ',' erwartet";
            return false;
        }
    }
    return true;
}

struct DbCarrierAgent {
    float x = 0.0f;
    float y = 0.0f;
    int payload_index = -1;
};

bool find_empty_near(const DbWorld &world, int cx, int cy, int radius, int &out_x, int &out_y) {
    int x0 = std::max(0, cx - radius);
    int x1 = std::min(world.width - 1, cx + radius);
    int y0 = std::max(0, cy - radius);
    int y1 = std::min(world.height - 1, cy + radius);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (world.cell_payload[static_cast<size_t>(y) * world.width + x] < 0) {
                out_x = x;
                out_y = y;
                return true;
            }
        }
    }
    return false;
}

bool match_field(const DbPayload &payload, const std::string &name, const std::string &value) {
    for (const auto &f : payload.fields) {
        if (ieq(f.name, name) && f.value == value) {
            return true;
        }
    }
    return false;
}

bool payload_tombstoned(const DbWorld &world, int64_t key) {
    return world.tombstones.find(key) != world.tombstones.end();
}

bool base_overridden(const DbWorld &world, int64_t key) {
    return world.delta_index_by_key.find(key) != world.delta_index_by_key.end();
}

int next_payload_id(const DbWorld &world, int table_id) {
    int max_id = 0;
    for (const auto &p : world.payloads) {
        if (p.table_id == table_id) {
            if (p.id > max_id) max_id = p.id;
        }
    }
    return max_id + 1;
}

void ensure_column(DbWorld &world, int table_id, const std::string &col) {
    if (table_id < 0) return;
    if (table_id >= static_cast<int>(world.table_columns.size())) return;
    auto &cols = world.table_columns[static_cast<size_t>(table_id)];
    for (const auto &c : cols) {
        if (ieq(c, col)) {
            return;
        }
    }
    cols.push_back(col);
}

void rebuild_foreign_keys(DbWorld &world, DbPayload &payload) {
    payload.foreign_keys.clear();
    for (const auto &f : payload.fields) {
        if (!ends_with_id(f.name)) continue;
        int fk_id = 0;
        if (!parse_int_value(f.value, fk_id)) continue;
        std::string fk_table = fk_table_from_column(f.name);
        int fk_table_id = db_find_table(world, fk_table);
        DbForeignKey fk;
        fk.table_id = fk_table_id;
        fk.id = fk_id;
        fk.column = f.name;
        payload.foreign_keys.push_back(fk);
    }
}

void deactivate_payload(DbPayload &payload) {
    payload.id = 0;
    payload.table_id = -1;
    payload.foreign_keys.clear();
    payload.fields.clear();
    payload.raw_data.clear();
    payload.x = -1;
    payload.y = -1;
    payload.placed = false;
    payload.is_delta = false;
}

bool apply_set_fields(DbWorld &world,
                      DbPayload &payload,
                      const std::vector<std::pair<std::string, std::string>> &sets,
                      const std::string &table,
                      std::string &error) {
    for (const auto &pair : sets) {
        std::string col = strip_table_prefix(pair.first);
        if (is_pk_column(col, table)) {
            error = "UPDATE auf Primary Key ist nicht unterstuetzt.";
            return false;
        }
    }
    for (const auto &pair : sets) {
        std::string col = strip_table_prefix(pair.first);
        std::string val = strip_quotes(pair.second);
        bool updated = false;
        for (auto &f : payload.fields) {
            if (ieq(f.name, col)) {
                f.value = val;
                updated = true;
                break;
            }
        }
        if (!updated) {
            DbField f;
            f.name = col;
            f.value = val;
            payload.fields.push_back(std::move(f));
        }
        ensure_column(world, payload.table_id, col);
    }
    rebuild_foreign_keys(world, payload);
    payload.raw_data = build_raw_data(payload.fields);
    return true;
}

bool build_payload_from_row(DbWorld &world,
                            const std::string &table,
                            const std::vector<std::string> &columns,
                            const std::vector<std::string> &row,
                            DbPayload &out,
                            std::string &error) {
    int table_id = db_add_table(world, table);
    if (!columns.empty() && row.size() != columns.size()) {
        error = "INSERT: Spaltenanzahl passt nicht.";
        return false;
    }
    std::vector<std::string> use_cols = columns;
    if (use_cols.empty()) {
        if (table_id >= 0 && table_id < static_cast<int>(world.table_columns.size()) &&
            !world.table_columns[static_cast<size_t>(table_id)].empty()) {
            use_cols = world.table_columns[static_cast<size_t>(table_id)];
        } else {
            use_cols.clear();
            for (size_t i = 0; i < row.size(); ++i) {
                use_cols.push_back("col" + std::to_string(i));
            }
        }
    }
    if (row.size() != use_cols.size()) {
        error = "INSERT: Werteanzahl passt nicht.";
        return false;
    }
    out = DbPayload{};
    out.table_id = table_id;
    out.is_delta = true;
    out.placed = false;
    out.x = -1;
    out.y = -1;
    out.fields.clear();
    for (size_t ci = 0; ci < use_cols.size(); ++ci) {
        DbField field;
        field.name = use_cols[ci];
        field.value = strip_quotes(row[ci]);
        out.fields.push_back(field);
        ensure_column(world, table_id, field.name);
    }
    int id_value = 0;
    bool found_id = false;
    for (const auto &f : out.fields) {
        if (ieq(f.name, "id") || is_pk_column(f.name, table)) {
            if (parse_int_value(f.value, id_value)) {
                found_id = true;
            }
            break;
        }
    }
    if (!found_id && !out.fields.empty()) {
        found_id = parse_int_value(out.fields.front().value, id_value);
    }
    if (!found_id) {
        id_value = next_payload_id(world, table_id);
    }
    out.id = id_value;
    rebuild_foreign_keys(world, out);
    out.raw_data = build_raw_data(out.fields);
    return true;
}

bool parse_update_statement(const std::string &stmt,
                            std::string &table,
                            std::vector<std::pair<std::string, std::string>> &sets,
                            std::string &where_col,
                            std::string &where_val) {
    Parser p;
    p.tokens = tokenize_sql(stmt);
    if (!p.match("update")) return false;
    table = p.consume();
    if (table.empty()) return false;
    if (!p.match("set")) return false;
    sets.clear();
    while (!p.eof() && !ieq(p.peek(), "where")) {
        std::string col = p.consume();
        if (col.empty()) return false;
        if (!p.match_symbol("=")) return false;
        std::string val = p.consume();
        if (val.empty()) return false;
        sets.push_back({col, val});
        if (p.match_symbol(",")) continue;
        if (ieq(p.peek(), "where")) break;
    }
    if (!p.match("where")) return false;
    where_col = p.consume();
    if (where_col.empty()) return false;
    if (!p.match_symbol("=")) return false;
    where_val = p.consume();
    return !where_val.empty() && !sets.empty();
}

bool parse_delete_statement(const std::string &stmt,
                            std::string &table,
                            std::string &where_col,
                            std::string &where_val) {
    Parser p;
    p.tokens = tokenize_sql(stmt);
    if (!p.match("delete")) return false;
    if (p.match("from")) {
        // ok
    }
    table = p.consume();
    if (table.empty()) return false;
    if (!p.match("where")) return false;
    where_col = p.consume();
    if (where_col.empty()) return false;
    if (!p.match_symbol("=")) return false;
    where_val = p.consume();
    return !where_val.empty();
}
} // namespace

int db_add_table(DbWorld &world, const std::string &name) {
    std::string key = to_lower(name);
    auto it = world.table_lookup.find(key);
    if (it != world.table_lookup.end()) {
        return it->second;
    }
    int id = static_cast<int>(world.table_names.size());
    world.table_lookup[key] = id;
    world.table_names.push_back(name);
    world.table_columns.emplace_back();
    if (world.width > 0 && world.height > 0) {
        world.table_pheromones.emplace_back(world.width, world.height, 0.0f);
    }
    return id;
}

int64_t db_payload_key(int table_id, int id) {
    return make_payload_key(table_id, id);
}

size_t db_delta_count(const DbWorld &world) {
    size_t count = 0;
    for (const auto &pair : world.delta_index_by_key) {
        if (world.tombstones.find(pair.first) == world.tombstones.end()) {
            count++;
        }
    }
    return count;
}

bool db_has_pending_delta(const DbWorld &world) {
    return !world.tombstones.empty() || !world.delta_index_by_key.empty();
}

int db_find_table(const DbWorld &world, const std::string &name) {
    std::string key = to_lower(name);
    auto it = world.table_lookup.find(key);
    if (it != world.table_lookup.end()) {
        return it->second;
    }
    return -1;
}

void db_init_world(DbWorld &world, int width, int height) {
    world.width = width;
    world.height = height;
    world.cell_payload.assign(static_cast<size_t>(width) * height, -1);
    world.table_pheromones.clear();
    world.table_pheromones.reserve(world.table_names.size());
    for (size_t i = 0; i < world.table_names.size(); ++i) {
        world.table_pheromones.emplace_back(width, height, 0.0f);
    }
    world.data_density = GridField(width, height, 0.0f);
    world.mycel = MycelNetwork(width, height);
    world.payload_positions.clear();
}

bool db_place_payload(DbWorld &world, int payload_index, int x, int y) {
    if (payload_index < 0 || payload_index >= static_cast<int>(world.payloads.size())) {
        return false;
    }
    if (x < 0 || y < 0 || x >= world.width || y >= world.height) {
        return false;
    }
    size_t idx = static_cast<size_t>(y) * world.width + x;
    if (world.cell_payload[idx] >= 0) {
        return false;
    }
    DbPayload &payload = world.payloads[static_cast<size_t>(payload_index)];
    payload.x = x;
    payload.y = y;
    payload.placed = true;
    world.cell_payload[idx] = payload_index;
    world.data_density.at(x, y) = 1.0f;
    if (payload.table_id >= 0 && payload.table_id < static_cast<int>(world.table_pheromones.size())) {
        world.table_pheromones[static_cast<size_t>(payload.table_id)].at(x, y) += 1.0f;
    }
    world.payload_positions[make_payload_key(payload.table_id, payload.id)] = {x, y};
    return true;
}

bool db_load_sql(const std::string &path, DbWorld &world, std::string &error) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        error = "SQL-Datei konnte nicht geoeffnet werden: " + path;
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (content.empty()) {
        error = "SQL-Datei ist leer.";
        return false;
    }

    std::string stmt;
    bool in_string = false;
    char string_quote = 0;
    bool in_line_comment = false;
    bool in_block_comment = false;
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        char n = (i + 1 < content.size()) ? content[i + 1] : '\0';
        if (!in_string && !in_block_comment && c == '-' && n == '-') {
            in_line_comment = true;
            i++;
            continue;
        }
        if (!in_string && !in_line_comment && c == '/' && n == '*') {
            in_block_comment = true;
            i++;
            continue;
        }
        if (in_line_comment) {
            if (c == '\n' || c == '\r') {
                in_line_comment = false;
            }
            continue;
        }
        if (in_block_comment) {
            if (c == '*' && n == '/') {
                in_block_comment = false;
                i++;
            }
            continue;
        }

        if (in_string && c == '\\') {
            stmt.push_back(c);
            if (n != '\0') {
                stmt.push_back(n);
                i++;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            if (!in_string) {
                in_string = true;
                string_quote = c;
            } else if (c == string_quote) {
                if (n == string_quote) {
                    stmt.push_back(c);
                    stmt.push_back(n);
                    i++;
                    continue;
                }
                in_string = false;
            }
        }
        stmt.push_back(c);
        if (!in_string && c == ';') {
            std::string stmt_trim = trim(stmt);
            std::string table_name;
            std::vector<std::string> schema_cols;
            if (parse_create_table_statement(stmt_trim, table_name, schema_cols)) {
                int table_id = db_add_table(world, table_name);
                if (!schema_cols.empty()) {
                    world.table_columns[static_cast<size_t>(table_id)] = schema_cols;
                }
                stmt.clear();
                continue;
            }
            SqlInsert insert;
            if (parse_insert_statement(stmt, insert) || parse_insert_statement_lenient(stmt, insert)) {
                int table_id = db_add_table(world, insert.table);
                if (!insert.columns.empty() && world.table_columns[static_cast<size_t>(table_id)].empty()) {
                    world.table_columns[static_cast<size_t>(table_id)] = insert.columns;
                }
                for (const auto &row : insert.rows) {
                    DbPayload payload;
                    payload.table_id = table_id;
                    payload.fields.clear();
                    if (!insert.columns.empty()) {
                        if (row.size() != insert.columns.size()) {
                            continue;
                        }
                        for (size_t ci = 0; ci < insert.columns.size(); ++ci) {
                            DbField field;
                            field.name = insert.columns[ci];
                            field.value = strip_quotes(row[ci]);
                            payload.fields.push_back(field);
                        }
                    } else {
                        const auto &schema = world.table_columns[static_cast<size_t>(table_id)];
                        for (size_t ci = 0; ci < row.size(); ++ci) {
                            DbField field;
                            if (ci < schema.size()) {
                                field.name = schema[ci];
                            } else {
                                field.name = "col" + std::to_string(ci);
                            }
                            field.value = strip_quotes(row[ci]);
                            payload.fields.push_back(field);
                        }
                    }
                    int id_value = 0;
                    bool found_id = false;
                    for (const auto &f : payload.fields) {
                        if (ieq(f.name, "id")) {
                            if (parse_int_value(f.value, id_value)) {
                                found_id = true;
                            }
                            break;
                        }
                    }
                    if (!found_id) {
                        if (!payload.fields.empty()) {
                            found_id = parse_int_value(payload.fields.front().value, id_value);
                        }
                    }
                    if (!found_id) {
                        id_value = static_cast<int>(world.payloads.size()) + 1;
                    }
                    payload.id = id_value;
                    for (const auto &f : payload.fields) {
                        if (!ends_with_id(f.name)) continue;
                        int fk_id = 0;
                        if (!parse_int_value(f.value, fk_id)) continue;
                        std::string fk_table = fk_table_from_column(f.name);
                        int fk_table_id = db_find_table(world, fk_table);
                        if (fk_table_id >= 0) { // <--- WICHTIGE PRÜFUNG HINZUFÜGEN
                            DbForeignKey fk;
                            fk.table_id = fk_table_id;
                            fk.id = fk_id;
                            fk.column = f.name;
                            payload.foreign_keys.push_back(fk);
                        }
                    }
                    payload.raw_data = build_raw_data(payload.fields);
                    world.payloads.push_back(std::move(payload));
                }
            }
            stmt.clear();
        }
    }
    if (world.payloads.empty()) {
        error = "Keine INSERT-Statements gefunden.";
        return false;
    }
    return true;
}

bool db_run_ingest(DbWorld &world, const DbIngestConfig &cfg, std::string &error) {
    if (world.width <= 0 || world.height <= 0) {
        error = "Ungueltige Rastergroesse.";
        return false;
    }
    if (world.payloads.empty()) {
        error = "Keine Payloads vorhanden.";
        return false;
    }
    IngestRules ingest_rules;
    if (!cfg.rules_path.empty()) {
        std::string rules_error;
        if (!load_ingest_rules(cfg.rules_path, ingest_rules, rules_error)) {
            error = "Ingest-Regeln: " + rules_error;
            return false;
        }
    }
    if (ingest_rules.default_rules.empty()) {
        IngestRule rule;
        rule.pattern = ".*_id$";
        rule.pattern_rule = true;
        rule.weight = 1.0;
        rule.type = "foreign_key";
        rule.pattern_re = std::regex(rule.pattern, std::regex::icase);
        ingest_rules.default_rules.push_back(std::move(rule));
    }
    db_init_world(world, world.width, world.height);
    Rng rng(cfg.seed);
    int spawn_x = cfg.spawn_x >= 0 ? cfg.spawn_x : world.width / 2;
    int spawn_y = cfg.spawn_y >= 0 ? cfg.spawn_y : world.height / 2;

    std::vector<int> pending;
    pending.reserve(world.payloads.size());
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        pending.push_back(static_cast<int>(i));
    }
    size_t pending_index = 0;

    std::vector<DbCarrierAgent> agents;
    agents.reserve(cfg.agent_count);
    for (int i = 0; i < cfg.agent_count; ++i) {
        DbCarrierAgent a;
        a.x = static_cast<float>(spawn_x);
        a.y = static_cast<float>(spawn_y);
        agents.push_back(a);
    }

    struct TraitCenter {
        double sum_x = 0.0;
        double sum_y = 0.0;
        double sum_w = 0.0;
    };
    std::unordered_map<std::string, TraitCenter> trait_centers;

    auto trait_key = [&](const std::string &table, const std::string &column, const std::string &value) {
        return to_lower(table) + ":" + to_lower(column) + ":" + to_lower(value);
    };
    auto get_field_value = [&](const DbPayload &payload, const std::string &column, std::string &out) {
        for (const auto &f : payload.fields) {
            if (ieq(f.name, column)) {
                out = f.value;
                return true;
            }
        }
        return false;
    };
    auto resolve_domain = [](const std::string &value, std::string &out) {
        const auto at = value.find('@');
        if (at == std::string::npos || at + 1 >= value.size()) return false;
        out = value.substr(at + 1);
        return true;
    };

    GridField phero_accum(world.width, world.height, 0.0f);
    FieldParams pheromone_params{0.02f, 0.15f};

    for (int step = 0; step < cfg.steps; ++step) {
        for (auto &agent : agents) {
            if (agent.payload_index < 0) {
                if (pending_index < pending.size()) {
                    agent.payload_index = pending[pending_index++];
                } else {
                    continue;
                }
            }
            DbPayload &payload = world.payloads[static_cast<size_t>(agent.payload_index)];
            if (payload.placed) {
                agent.payload_index = -1;
                continue;
            }
            bool has_target = false;
            int tx = spawn_x;
            int ty = spawn_y;
            double sum_x = 0.0;
            double sum_y = 0.0;
            double sum_w = 0.0;
            auto add_target = [&](int x, int y, double weight) {
                if (weight <= 0.0) return;
                sum_x += static_cast<double>(x) * weight;
                sum_y += static_cast<double>(y) * weight;
                sum_w += weight;
            };
            for (const auto &fk : payload.foreign_keys) {
                int64_t key = make_payload_key(fk.table_id, fk.id);
                auto it = world.payload_positions.find(key);
                if (it != world.payload_positions.end()) {
                    add_target(it->second.first, it->second.second, 1.0);
                }
            }
            const std::string table_name = (payload.table_id >= 0 &&
                                            payload.table_id < static_cast<int>(world.table_names.size()))
                                             ? world.table_names[static_cast<size_t>(payload.table_id)]
                                             : std::string();
            auto table_it = ingest_rules.table_rules.find(to_lower(table_name));

            auto apply_rule = [&](const IngestRule &rule, const std::string &column, const std::string &value) {
                if (rule.type == "foreign_key") {
                    int fk_id = 0;
                    if (!parse_int_value(value, fk_id)) return;
                    std::string fk_table = fk_table_from_column(column);
                    int fk_table_id = db_find_table(world, fk_table);
                    if (fk_table_id < 0) return;
                    int64_t key = make_payload_key(fk_table_id, fk_id);
                    auto it = world.payload_positions.find(key);
                    if (it != world.payload_positions.end()) {
                        add_target(it->second.first, it->second.second, rule.weight);
                    }
                    return;
                }
                std::string key_value = value;
                if (rule.type == "domain_cluster") {
                    if (!resolve_domain(value, key_value)) return;
                }
                const std::string key = trait_key(table_name, column, key_value);
                auto it = trait_centers.find(key);
                if (it != trait_centers.end() && it->second.sum_w > 0.0) {
                    int cx = static_cast<int>(std::round(it->second.sum_x / it->second.sum_w));
                    int cy = static_cast<int>(std::round(it->second.sum_y / it->second.sum_w));
                    add_target(cx, cy, rule.weight);
                }
            };

            for (const auto &rule : ingest_rules.default_rules) {
                if (rule.pattern_rule) {
                    for (const auto &f : payload.fields) {
                        if (std::regex_match(f.name, rule.pattern_re)) {
                            apply_rule(rule, f.name, f.value);
                        }
                    }
                } else if (!rule.column.empty()) {
                    std::string value;
                    if (get_field_value(payload, rule.column, value)) {
                        apply_rule(rule, rule.column, value);
                    }
                }
            }
            if (table_it != ingest_rules.table_rules.end()) {
                for (const auto &rule : table_it->second) {
                    if (rule.pattern_rule) {
                        for (const auto &f : payload.fields) {
                            if (std::regex_match(f.name, rule.pattern_re)) {
                                apply_rule(rule, f.name, f.value);
                            }
                        }
                    } else if (!rule.column.empty()) {
                        std::string value;
                        if (get_field_value(payload, rule.column, value)) {
                            apply_rule(rule, rule.column, value);
                        }
                    }
                }
            }
            if (sum_w > 0.0) {
                tx = static_cast<int>(std::round(sum_x / sum_w));
                ty = static_cast<int>(std::round(sum_y / sum_w));
                has_target = true;
            }
            float dx = static_cast<float>(tx) - agent.x;
            float dy = static_cast<float>(ty) - agent.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.001f) {
                float step_len = 1.0f;
                float jitter = rng.uniform(-0.35f, 0.35f);
                agent.x += (dx / dist) * step_len + jitter;
                agent.y += (dy / dist) * step_len + jitter;
            } else {
                agent.x += rng.uniform(-1.0f, 1.0f);
                agent.y += rng.uniform(-1.0f, 1.0f);
            }
            int cx = std::max(0, std::min(world.width - 1, static_cast<int>(std::round(agent.x))));
            int cy = std::max(0, std::min(world.height - 1, static_cast<int>(std::round(agent.y))));
            int place_x = -1;
            int place_y = -1;
            bool allow_place = has_target ? (dist <= 2.5f) : (rng.uniform(0.0f, 1.0f) < 0.1f);
            if (allow_place && find_empty_near(world, cx, cy, 2, place_x, place_y)) {
                db_place_payload(world, agent.payload_index, place_x, place_y);
                for (const auto &rule : ingest_rules.default_rules) {
                    if (rule.type == "foreign_key") continue;
                    if (rule.pattern_rule) {
                        for (const auto &f : payload.fields) {
                            if (!std::regex_match(f.name, rule.pattern_re)) continue;
                            std::string key_value = f.value;
                            if (rule.type == "domain_cluster") {
                                if (!resolve_domain(f.value, key_value)) continue;
                            }
                            const std::string key = trait_key(table_name, f.name, key_value);
                            auto &center = trait_centers[key];
                            center.sum_x += place_x * rule.weight;
                            center.sum_y += place_y * rule.weight;
                            center.sum_w += rule.weight;
                        }
                    } else if (!rule.column.empty()) {
                        std::string value;
                        if (!get_field_value(payload, rule.column, value)) continue;
                        std::string key_value = value;
                        if (rule.type == "domain_cluster") {
                            if (!resolve_domain(value, key_value)) continue;
                        }
                        const std::string key = trait_key(table_name, rule.column, key_value);
                        auto &center = trait_centers[key];
                        center.sum_x += place_x * rule.weight;
                        center.sum_y += place_y * rule.weight;
                        center.sum_w += rule.weight;
                    }
                }
                if (table_it != ingest_rules.table_rules.end()) {
                    for (const auto &rule : table_it->second) {
                        if (rule.type == "foreign_key") continue;
                        if (rule.pattern_rule) {
                            for (const auto &f : payload.fields) {
                                if (!std::regex_match(f.name, rule.pattern_re)) continue;
                                std::string key_value = f.value;
                                if (rule.type == "domain_cluster") {
                                    if (!resolve_domain(f.value, key_value)) continue;
                                }
                                const std::string key = trait_key(table_name, f.name, key_value);
                                auto &center = trait_centers[key];
                                center.sum_x += place_x * rule.weight;
                                center.sum_y += place_y * rule.weight;
                                center.sum_w += rule.weight;
                            }
                        } else if (!rule.column.empty()) {
                            std::string value;
                            if (!get_field_value(payload, rule.column, value)) continue;
                            std::string key_value = value;
                            if (rule.type == "domain_cluster") {
                                if (!resolve_domain(value, key_value)) continue;
                            }
                            const std::string key = trait_key(table_name, rule.column, key_value);
                            auto &center = trait_centers[key];
                            center.sum_x += place_x * rule.weight;
                            center.sum_y += place_y * rule.weight;
                            center.sum_w += rule.weight;
                        }
                    }
                }
                agent.payload_index = -1;
            }
        }

        phero_accum.fill(0.0f);
        for (const auto &field : world.table_pheromones) {
            for (size_t i = 0; i < field.data.size(); ++i) {
                phero_accum.data[i] += field.data[i];
            }
        }
        diffuse_and_evaporate(phero_accum, pheromone_params);
        world.mycel.update(SimParams{}, phero_accum, world.data_density);
    }
    size_t placed_count = 0;
    for (const auto &p : world.payloads) {
        if (p.placed) {
            placed_count++;
        }
    }
    if (placed_count < world.payloads.size()) {
        std::vector<int> free_cells;
        free_cells.reserve(static_cast<size_t>(world.width) * world.height);
        for (int y = 0; y < world.height; ++y) {
            for (int x = 0; x < world.width; ++x) {
                if (world.cell_payload[static_cast<size_t>(y) * world.width + x] < 0) {
                    free_cells.push_back(y * world.width + x);
                }
            }
        }
        if (free_cells.size() < (world.payloads.size() - placed_count)) {
            error = "Nicht genug freie Zellen fuer alle Payloads.";
            return false;
        }
        for (size_t i = 0; i < world.payloads.size(); ++i) {
            DbPayload &payload = world.payloads[i];
            if (payload.placed) continue;
            int pick = rng.uniform_int(0, static_cast<int>(free_cells.size() - 1));
            int idx = free_cells[static_cast<size_t>(pick)];
            free_cells[static_cast<size_t>(pick)] = free_cells.back();
            free_cells.pop_back();
            int px = idx % world.width;
            int py = idx / world.width;
            if (!db_place_payload(world, static_cast<int>(i), px, py)) {
                error = "Konnte Payload nicht platzieren.";
                return false;
            }
        }
    }
    return true;
}

bool db_save_myco(const std::string &path, const DbWorld &world, std::string &error) {
    if (db_has_pending_delta(world)) {
        error = "Delta-Writes ausstehend: bitte merge ausfuehren, bevor gespeichert wird.";
        return false;
    }
    std::ofstream out(path);
    if (!out.is_open()) {
        error = "MYCO-Datei konnte nicht geschrieben werden: " + path;
        return false;
    }
    out << "MYCO1\n";
    out << world.width << " " << world.height << "\n";
    out << "tables " << world.table_names.size() << "\n";
    for (size_t i = 0; i < world.table_names.size(); ++i) {
        out << i << "\t" << escape_string(world.table_names[i]) << "\n";
    }
    out << "columns " << world.table_names.size() << "\n";
    for (size_t i = 0; i < world.table_names.size(); ++i) {
        const auto &cols = (i < world.table_columns.size()) ? world.table_columns[i] : std::vector<std::string>{};
        out << i << "\t" << cols.size();
        for (const auto &c : cols) {
            out << "\t" << escape_string(c);
        }
        out << "\n";
    }
    out << "payloads " << world.payloads.size() << "\n";
    for (const auto &p : world.payloads) {
        out << p.id << "\t" << p.table_id << "\t" << p.x << "\t" << p.y << "\t"
            << p.fields.size() << "\t" << p.foreign_keys.size() << "\t" << escape_string(p.raw_data).size() << "\n";
        out << escape_string(p.raw_data) << "\n";
        for (const auto &f : p.fields) {
            out << escape_string(f.name) << "\t" << escape_string(f.value) << "\n";
        }
        for (const auto &fk : p.foreign_keys) {
            out << fk.table_id << "\t" << fk.id << "\t" << escape_string(fk.column) << "\n";
        }
    }
    return true;
}

bool db_load_myco(const std::string &path, DbWorld &world, std::string &error) {
    std::ifstream in(path);
    if (!in.is_open()) {
        error = "MYCO-Datei konnte nicht geoeffnet werden: " + path;
        return false;
    }
    std::string header;
    if (!std::getline(in, header) || trim(header) != "MYCO1") {
        error = "MYCO-Header ungueltig.";
        return false;
    }
    int width = 0;
    int height = 0;
    {
        std::string line;
        if (!std::getline(in, line)) {
            error = "MYCO-Dimension fehlt.";
            return false;
        }
        std::stringstream ss(line);
        ss >> width >> height;
        if (width <= 0 || height <= 0) {
            error = "MYCO-Dimension ungueltig.";
            return false;
        }
    }
    world = DbWorld{};
    world.width = width;
    world.height = height;

    std::string line;
    if (!std::getline(in, line)) {
        error = "MYCO-Tabellen fehlen.";
        return false;
    }
    {
        std::stringstream ss(line);
        std::string tag;
        size_t count = 0;
        ss >> tag >> count;
        if (tag != "tables") {
            error = "MYCO-Tabellen-Tag fehlt.";
            return false;
        }
        world.table_names.resize(count);
        world.table_columns.resize(count);
        for (size_t i = 0; i < count; ++i) {
            if (!std::getline(in, line)) {
                error = "MYCO-Tabellenliste unvollstaendig.";
                return false;
            }
            std::stringstream row(line);
            size_t id = 0;
            std::string name;
            if (!(row >> id)) {
                error = "MYCO-Tabellen-ID fehlt.";
                return false;
            }
            if (!std::getline(row, name)) {
                error = "MYCO-Tabellenname fehlt.";
                return false;
            }
            name = trim(name);
            if (!name.empty() && name[0] == '\t') {
                name.erase(0, 1);
            }
            if (id < world.table_names.size()) {
                world.table_names[id] = unescape_string(name);
            }
        }
    }
    for (size_t i = 0; i < world.table_names.size(); ++i) {
        world.table_lookup[to_lower(world.table_names[i])] = static_cast<int>(i);
    }
    world.table_pheromones.clear();
    for (size_t i = 0; i < world.table_names.size(); ++i) {
        world.table_pheromones.emplace_back(width, height, 0.0f);
    }

    if (!std::getline(in, line)) {
        error = "MYCO-Payload-Tag fehlt.";
        return false;
    }
    if (line.rfind("columns", 0) == 0) {
        std::stringstream ss(line);
        std::string tag;
        size_t count = 0;
        ss >> tag >> count;
        if (tag != "columns" || count != world.table_names.size()) {
            error = "MYCO-Columns-Tag ungueltig.";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            if (!std::getline(in, line)) {
                error = "MYCO-Columns-Liste unvollstaendig.";
                return false;
            }
            std::stringstream row(line);
            size_t id = 0;
            size_t col_count = 0;
            row >> id >> col_count;
            if (id >= world.table_columns.size()) {
                continue;
            }
            std::vector<std::string> cols;
            for (size_t c = 0; c < col_count; ++c) {
                std::string col;
                if (!(row >> col)) break;
                cols.push_back(unescape_string(col));
            }
            world.table_columns[id] = std::move(cols);
        }
        if (!std::getline(in, line)) {
            error = "MYCO-Payload-Tag fehlt.";
            return false;
        }
    }
    size_t payload_count = 0;
    {
        std::stringstream ss(line);
        std::string tag;
        ss >> tag >> payload_count;
        if (tag != "payloads") {
            error = "MYCO-Payload-Tag ungueltig.";
            return false;
        }
    }
    world.payloads.clear();
    world.payloads.reserve(payload_count);
    for (size_t i = 0; i < payload_count; ++i) {
        if (!std::getline(in, line)) {
            error = "MYCO-Payload-Header fehlt.";
            return false;
        }
        std::stringstream row(line);
        DbPayload p;
        size_t field_count = 0;
        size_t fk_count = 0;
        size_t raw_len = 0;
        row >> p.id >> p.table_id >> p.x >> p.y >> field_count >> fk_count >> raw_len;
        if (!std::getline(in, line)) {
            error = "MYCO-Payload-Daten fehlen.";
            return false;
        }
        p.raw_data = unescape_string(line);
        if (raw_len > 0 && p.raw_data.size() != raw_len) {
            // size mismatch is tolerated to keep forward compatibility
        }
        p.fields.reserve(field_count);
        for (size_t f = 0; f < field_count; ++f) {
            if (!std::getline(in, line)) {
                error = "MYCO-Feldzeile fehlt.";
                return false;
            }
            std::stringstream fr(line);
            std::string name;
            std::string value;
            if (!std::getline(fr, name, '\t')) {
                error = "MYCO-Feldname fehlt.";
                return false;
            }
            if (!std::getline(fr, value)) {
                value.clear();
            }
            DbField field;
            field.name = unescape_string(name);
            field.value = unescape_string(trim(value));
            p.fields.push_back(std::move(field));
        }
        p.foreign_keys.reserve(fk_count);
        for (size_t f = 0; f < fk_count; ++f) {
            if (!std::getline(in, line)) {
                error = "MYCO-FK-Zeile fehlt.";
                return false;
            }
            std::stringstream fr(line);
            DbForeignKey fk;
            fr >> fk.table_id >> fk.id;
            std::string col;
            if (std::getline(fr, col)) {
                col = trim(col);
                if (!col.empty() && col[0] == '\t') {
                    col.erase(0, 1);
                }
                fk.column = unescape_string(col);
            }
            p.foreign_keys.push_back(std::move(fk));
        }
        p.placed = (p.x >= 0 && p.y >= 0);
        world.payloads.push_back(std::move(p));
    }
    db_init_world(world, width, height);
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        DbPayload &p = world.payloads[i];
        if (p.placed && p.x >= 0 && p.y >= 0) {
            db_place_payload(world, static_cast<int>(i), p.x, p.y);
        }
    }
    return true;
}

bool db_parse_query(const std::string &query, DbQuery &out) {
    std::string q = query;
    std::string upper = to_lower(query);
    size_t from_pos = upper.find("from");
    size_t where_pos = upper.find("where");
    if (from_pos == std::string::npos || where_pos == std::string::npos || where_pos < from_pos) {
        return false;
    }
    std::string from_part = trim(q.substr(from_pos + 4, where_pos - (from_pos + 4)));
    std::string where_part = trim(q.substr(where_pos + 5));
    size_t eq = where_part.find('=');
    if (eq == std::string::npos) {
        return false;
    }
    out.table = trim(from_part);
    out.column = trim(where_part.substr(0, eq));
    out.value = trim(where_part.substr(eq + 1));
    out.value = strip_quotes(out.value);
    return !out.table.empty() && !out.column.empty() && !out.value.empty();
}

std::vector<int> db_execute_query(const DbWorld &world, const DbQuery &q, int radius) {
    std::vector<int> out;
    int table_id = db_find_table(world, q.table);
    if (table_id < 0) {
        return out;
    }
    std::string where_col = strip_table_prefix(q.column);
    bool fk_query = ends_with_id(q.column);
    int target_id = 0;
    if (fk_query && !parse_int_value(q.value, target_id)) {
        fk_query = false;
    }
    bool pk_query = false;
    if (fk_query) {
        std::string col_lower = to_lower(q.column);
        std::string table_lower = to_lower(q.table);
        if (col_lower == "id" || col_lower == table_lower + "id" || col_lower == table_lower + "_id") {
            pk_query = true;
        }
    }
    if (pk_query) {
        int64_t key = make_payload_key(table_id, target_id);
        auto dit = world.delta_index_by_key.find(key);
        if (dit != world.delta_index_by_key.end() && !payload_tombstoned(world, key)) {
            out.push_back(dit->second);
            return out;
        }
        auto it = world.payload_positions.find(key);
        if (it != world.payload_positions.end() && world.width > 0 && world.height > 0) {
            int px = it->second.first;
            int py = it->second.second;
            if (px >= 0 && py >= 0 && px < world.width && py < world.height) {
                int idx = world.cell_payload[static_cast<size_t>(py) * world.width + px];
                if (idx >= 0 && idx < static_cast<int>(world.payloads.size())) {
                    const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
                    if (!p.is_delta && p.table_id == table_id && p.id == target_id &&
                        !payload_tombstoned(world, key)) {
                        out.push_back(idx);
                        return out;
                    }
                }
            }
        }
    }
    std::vector<int> base_hits;
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        const DbPayload &p = world.payloads[i];
        if (!p.is_delta) continue;
        if (p.table_id != table_id) continue;
        int64_t key = make_payload_key(p.table_id, p.id);
        if (payload_tombstoned(world, key)) continue;
        if (pk_query && p.id == target_id) {
            out.push_back(static_cast<int>(i));
            continue;
        }
        if (match_field(p, where_col, q.value)) {
            out.push_back(static_cast<int>(i));
        }
    }
    if (fk_query) {
        std::string parent_table = fk_table_from_column(q.column);
        int parent_id = db_find_table(world, parent_table);
        if (parent_id >= 0) {
            int64_t key = make_payload_key(parent_id, target_id);
            auto it = world.payload_positions.find(key);
            if (it != world.payload_positions.end()) {
                int px = it->second.first;
                int py = it->second.second;
                int x0 = std::max(0, px - radius);
                int x1 = std::min(world.width - 1, px + radius);
                int y0 = std::max(0, py - radius);
                int y1 = std::min(world.height - 1, py + radius);
                for (int y = y0; y <= y1; ++y) {
                    for (int x = x0; x <= x1; ++x) {
                        int idx = world.cell_payload[static_cast<size_t>(y) * world.width + x];
                        if (idx < 0) continue;
                        const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
                        if (p.table_id != table_id) continue;
                        if (p.is_delta) continue;
                        int64_t pkey = make_payload_key(p.table_id, p.id);
                        if (payload_tombstoned(world, pkey) || base_overridden(world, pkey)) continue;
                        for (const auto &fk : p.foreign_keys) {
                            if (fk.table_id == parent_id && fk.id == target_id) {
                                out.push_back(idx);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        const DbPayload &p = world.payloads[i];
        if (p.is_delta) continue;
        if (p.table_id != table_id) continue;
        int64_t key = make_payload_key(p.table_id, p.id);
        if (payload_tombstoned(world, key) || base_overridden(world, key)) continue;
        if (pk_query && p.id == target_id) {
            out.push_back(static_cast<int>(i));
            continue;
        }
        if (match_field(p, where_col, q.value)) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

std::vector<int> db_execute_query_focus(const DbWorld &world, const DbQuery &q, int center_x, int center_y, int radius) {
    std::vector<int> out;
    int table_id = db_find_table(world, q.table);
    if (table_id < 0 || world.width <= 0 || world.height <= 0) {
        return out;
    }
    int x0 = std::max(0, center_x - radius);
    int x1 = std::min(world.width - 1, center_x + radius);
    int y0 = std::max(0, center_y - radius);
    int y1 = std::min(world.height - 1, center_y + radius);

    std::string where_col = strip_table_prefix(q.column);
    bool fk_query = ends_with_id(q.column);
    int target_id = 0;
    if (fk_query && !parse_int_value(q.value, target_id)) {
        fk_query = false;
    }
    bool pk_query = false;
    if (fk_query) {
        std::string col_lower = to_lower(q.column);
        std::string table_lower = to_lower(q.table);
        if (col_lower == "id" || col_lower == table_lower + "id" || col_lower == table_lower + "_id") {
            pk_query = true;
        }
    }
    int fk_table_id = -1;
    if (fk_query && !pk_query) {
        std::string parent_table = fk_table_from_column(q.column);
        fk_table_id = db_find_table(world, parent_table);
    }

    if (pk_query) {
        int64_t key = make_payload_key(table_id, target_id);
        auto dit = world.delta_index_by_key.find(key);
        if (dit != world.delta_index_by_key.end() && !payload_tombstoned(world, key)) {
            out.push_back(dit->second);
            return out;
        }
        auto it = world.payload_positions.find(key);
        if (it != world.payload_positions.end() && world.width > 0 && world.height > 0) {
            int px = it->second.first;
            int py = it->second.second;
            if (px >= 0 && py >= 0 && px < world.width && py < world.height) {
                int idx = world.cell_payload[static_cast<size_t>(py) * world.width + px];
                if (idx >= 0 && idx < static_cast<int>(world.payloads.size())) {
                    const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
                    if (!p.is_delta && p.table_id == table_id && p.id == target_id &&
                        !payload_tombstoned(world, key)) {
                        out.push_back(idx);
                        return out;
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        const DbPayload &p = world.payloads[i];
        if (!p.is_delta) continue;
        if (p.table_id != table_id) continue;
        int64_t key = make_payload_key(p.table_id, p.id);
        if (payload_tombstoned(world, key)) continue;
        if (pk_query && p.id == target_id) {
            out.push_back(static_cast<int>(i));
            continue;
        }
        if (fk_query && fk_table_id >= 0) {
            for (const auto &fk : p.foreign_keys) {
                if (fk.table_id == fk_table_id && fk.id == target_id) {
                    out.push_back(static_cast<int>(i));
                    break;
                }
            }
            continue;
        }
        if (match_field(p, where_col, q.value)) {
            out.push_back(static_cast<int>(i));
        }
    }

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int idx = world.cell_payload[static_cast<size_t>(y) * world.width + x];
            if (idx < 0 || idx >= static_cast<int>(world.payloads.size())) continue;
            const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
            if (p.table_id != table_id) continue;
            if (p.is_delta) continue;
            int64_t key = make_payload_key(p.table_id, p.id);
            if (payload_tombstoned(world, key) || base_overridden(world, key)) continue;
            if (pk_query && p.id == target_id) {
                out.push_back(idx);
                continue;
            }
            if (fk_query && fk_table_id >= 0) {
                for (const auto &fk : p.foreign_keys) {
                    if (fk.table_id == fk_table_id && fk.id == target_id) {
                        out.push_back(idx);
                        break;
                    }
                }
                continue;
            }
            if (match_field(p, where_col, q.value)) {
                out.push_back(idx);
            }
        }
    }
    return out;
}

bool db_apply_insert_sql(DbWorld &world, const std::string &stmt, int &rows, std::string &error) {
    rows = 0;
    SqlInsert insert;
    if (!parse_insert_statement(stmt, insert)) {
        error = "INSERT: ungueltiges Statement.";
        return false;
    }
    for (const auto &row : insert.rows) {
        DbPayload payload;
        if (!build_payload_from_row(world, insert.table, insert.columns, row, payload, error)) {
            return false;
        }
        int64_t key = make_payload_key(payload.table_id, payload.id);
        payload.is_delta = true;
        payload.placed = false;
        payload.x = -1;
        payload.y = -1;
        bool had_prev = false;
        DbPayload prev_payload;
        auto it_prev = world.delta_index_by_key.find(key);
        if (it_prev != world.delta_index_by_key.end()) {
            had_prev = true;
            prev_payload = world.payloads[static_cast<size_t>(it_prev->second)];
        }
        bool prev_tombstone = world.tombstones.find(key) != world.tombstones.end();
        world.tombstones.erase(key);
        auto it = world.delta_index_by_key.find(key);
        if (it != world.delta_index_by_key.end()) {
            world.payloads[static_cast<size_t>(it->second)] = std::move(payload);
        } else {
            int idx = static_cast<int>(world.payloads.size());
            world.payloads.push_back(std::move(payload));
            world.delta_index_by_key[key] = idx;
        }
        DbDeltaOp op;
        op.kind = DbDeltaOp::INSERT;
        op.key = key;
        op.had_prev = had_prev;
        op.prev_payload = std::move(prev_payload);
        op.prev_tombstone = prev_tombstone;
        world.delta_history.push_back(std::move(op));
        rows++;
    }
    return true;
}

bool db_apply_update_sql(DbWorld &world, const std::string &stmt, int &rows, std::string &error) {
    rows = 0;
    std::string table;
    std::vector<std::pair<std::string, std::string>> sets;
    std::string where_col;
    std::string where_val;
    if (!parse_update_statement(stmt, table, sets, where_col, where_val)) {
        error = "UPDATE: ungueltiges Statement.";
        return false;
    }
    where_col = strip_table_prefix(where_col);
    where_val = strip_quotes(where_val);
    int table_id = db_find_table(world, table);
    if (table_id < 0) {
        error = "UPDATE: Tabelle nicht gefunden.";
        return false;
    }
    bool pk_query = is_pk_column(where_col, table);
    int target_id = 0;
    if (pk_query && !parse_int_value(where_val, target_id)) {
        pk_query = false;
    }
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        DbPayload &p = world.payloads[i];
        if (!p.is_delta) continue;
        if (p.table_id != table_id) continue;
        int64_t key = make_payload_key(p.table_id, p.id);
        if (payload_tombstoned(world, key)) continue;
        bool match = pk_query ? (p.id == target_id) : match_field(p, where_col, where_val);
        if (!match) continue;
        DbPayload prev_payload = p;
        if (!apply_set_fields(world, p, sets, table, error)) {
            return false;
        }
        DbDeltaOp op;
        op.kind = DbDeltaOp::UPDATE;
        op.key = key;
        op.had_prev = true;
        op.prev_payload = std::move(prev_payload);
        op.prev_tombstone = payload_tombstoned(world, key);
        world.delta_history.push_back(std::move(op));
        rows++;
    }
    std::vector<int> base_hits;
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        const DbPayload &p = world.payloads[i];
        if (p.is_delta) continue;
        if (p.table_id != table_id) continue;
        int64_t key = make_payload_key(p.table_id, p.id);
        if (payload_tombstoned(world, key) || base_overridden(world, key)) continue;
        bool match = pk_query ? (p.id == target_id) : match_field(p, where_col, where_val);
        if (!match) continue;
        base_hits.push_back(static_cast<int>(i));
    }
    for (int idx : base_hits) {
        const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
        int64_t key = make_payload_key(p.table_id, p.id);
        DbPayload updated = p;
        updated.is_delta = true;
        updated.placed = false;
        updated.x = -1;
        updated.y = -1;
        if (!apply_set_fields(world, updated, sets, table, error)) {
            return false;
        }
        auto it = world.delta_index_by_key.find(key);
        if (it != world.delta_index_by_key.end()) {
            world.payloads[static_cast<size_t>(it->second)] = std::move(updated);
        } else {
            int idx = static_cast<int>(world.payloads.size());
            world.payloads.push_back(std::move(updated));
            world.delta_index_by_key[key] = idx;
        }
        DbDeltaOp op;
        op.kind = DbDeltaOp::UPDATE;
        op.key = key;
        op.had_prev = false;
        op.prev_tombstone = payload_tombstoned(world, key);
        world.delta_history.push_back(std::move(op));
        rows++;
    }
    return true;
}

bool db_apply_delete_sql(DbWorld &world, const std::string &stmt, int &rows, std::string &error) {
    rows = 0;
    std::string table;
    std::string where_col;
    std::string where_val;
    if (!parse_delete_statement(stmt, table, where_col, where_val)) {
        error = "DELETE: ungueltiges Statement.";
        return false;
    }
    where_col = strip_table_prefix(where_col);
    where_val = strip_quotes(where_val);
    int table_id = db_find_table(world, table);
    if (table_id < 0) {
        error = "DELETE: Tabelle nicht gefunden.";
        return false;
    }
    bool pk_query = is_pk_column(where_col, table);
    int target_id = 0;
    if (pk_query && !parse_int_value(where_val, target_id)) {
        pk_query = false;
    }
    for (size_t i = 0; i < world.payloads.size(); ++i) {
        const DbPayload &p = world.payloads[i];
        if (p.table_id != table_id) continue;
        int64_t key = make_payload_key(p.table_id, p.id);
        if (payload_tombstoned(world, key)) continue;
        if (!p.is_delta && base_overridden(world, key)) continue;
        bool match = pk_query ? (p.id == target_id) : match_field(p, where_col, where_val);
        if (!match) continue;
        DbDeltaOp op;
        op.kind = DbDeltaOp::DELETE;
        op.key = key;
        op.had_prev = false;
        op.prev_tombstone = payload_tombstoned(world, key);
        world.delta_history.push_back(std::move(op));
        world.tombstones.insert(key);
        rows++;
    }
    return true;
}

bool db_merge_delta(DbWorld &world, const DbIngestConfig &cfg, std::string &error) {
    if (!db_has_pending_delta(world)) {
        return true;
    }
    if (cfg.agent_count <= 0 || cfg.steps <= 0) {
        error = "Merge-Config ungueltig (agents/steps).";
        return false;
    }
    std::unordered_set<int64_t> delta_keys;
    delta_keys.reserve(world.delta_index_by_key.size());
    for (const auto &pair : world.delta_index_by_key) {
        delta_keys.insert(pair.first);
    }
    std::vector<DbPayload> merged;
    merged.reserve(world.payloads.size());
    for (const auto &p : world.payloads) {
        int64_t key = make_payload_key(p.table_id, p.id);
        if (payload_tombstoned(world, key)) continue;
        if (!p.is_delta) {
            if (delta_keys.find(key) != delta_keys.end()) continue;
            merged.push_back(p);
            continue;
        }
        DbPayload copy = p;
        copy.is_delta = false;
        copy.placed = false;
        copy.x = -1;
        copy.y = -1;
        merged.push_back(std::move(copy));
    }
    world.payloads.swap(merged);
    world.delta_index_by_key.clear();
    world.tombstones.clear();
    world.delta_history.clear();
    return db_run_ingest(world, cfg, error);
}

bool db_undo_last_delta(DbWorld &world, std::string &error) {
    if (world.delta_history.empty()) {
        error = "Kein Undo verfuegbar.";
        return false;
    }
    DbDeltaOp op = world.delta_history.back();
    world.delta_history.pop_back();
    if (op.kind == DbDeltaOp::INSERT) {
        auto it = world.delta_index_by_key.find(op.key);
        if (it != world.delta_index_by_key.end()) {
            if (op.had_prev) {
                world.payloads[static_cast<size_t>(it->second)] = op.prev_payload;
            } else {
                deactivate_payload(world.payloads[static_cast<size_t>(it->second)]);
                world.delta_index_by_key.erase(it);
            }
        }
        if (op.prev_tombstone) {
            world.tombstones.insert(op.key);
        } else {
            world.tombstones.erase(op.key);
        }
        return true;
    }
    if (op.kind == DbDeltaOp::UPDATE) {
        auto it = world.delta_index_by_key.find(op.key);
        if (op.had_prev) {
            if (it != world.delta_index_by_key.end()) {
                world.payloads[static_cast<size_t>(it->second)] = op.prev_payload;
            }
        } else {
            if (it != world.delta_index_by_key.end()) {
                deactivate_payload(world.payloads[static_cast<size_t>(it->second)]);
                world.delta_index_by_key.erase(it);
            }
        }
        if (op.prev_tombstone) {
            world.tombstones.insert(op.key);
        } else {
            world.tombstones.erase(op.key);
        }
        return true;
    }
    if (op.kind == DbDeltaOp::DELETE) {
        if (op.prev_tombstone) {
            world.tombstones.insert(op.key);
        } else {
            world.tombstones.erase(op.key);
        }
        return true;
    }
    error = "Undo fehlgeschlagen.";
    return false;
}

bool db_save_cluster_ppm(const std::string &path, const DbWorld &world, int scale, std::string &error) {
    if (world.width <= 0 || world.height <= 0) {
        error = "Ungueltige Rastergroesse fuer PPM.";
        return false;
    }
    if (scale <= 0) {
        error = "Ungueltiger PPM-Scale.";
        return false;
    }
    std::ofstream out(path);
    if (!out.is_open()) {
        error = "PPM-Datei konnte nicht geschrieben werden: " + path;
        return false;
    }
    static const int palette[][3] = {
        {30, 30, 30},
        {220, 60, 60},
        {60, 200, 90},
        {70, 120, 220},
        {220, 200, 60},
        {200, 80, 200},
        {60, 200, 200},
        {200, 140, 60},
        {160, 160, 160}
    };
    const int palette_size = static_cast<int>(sizeof(palette) / sizeof(palette[0]));

    out << "P3\n" << world.width * scale << " " << world.height * scale << "\n255\n";
    for (int y = 0; y < world.height; ++y) {
        for (int sy = 0; sy < scale; ++sy) {
            for (int x = 0; x < world.width; ++x) {
                int idx = world.cell_payload[static_cast<size_t>(y) * world.width + x];
                int color = 0;
                if (idx >= 0 && idx < static_cast<int>(world.payloads.size())) {
                    const DbPayload &p = world.payloads[static_cast<size_t>(idx)];
                    color = 1 + (p.table_id % (palette_size - 1));
                }
                for (int sx = 0; sx < scale; ++sx) {
                    out << palette[color][0] << " " << palette[color][1] << " " << palette[color][2] << " ";
                }
            }
            out << "\n";
        }
    }
    return true;
}
