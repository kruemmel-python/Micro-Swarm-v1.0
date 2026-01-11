#include "db_sql.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace {
struct Row;
bool exec_sql_with_outer(const DbWorld &world,
                         const std::string &sql,
                         bool use_focus,
                         int focus_x,
                         int focus_y,
                         int radius,
                         const Row *outer,
                         DbSqlResult &out,
                         std::string &error);
struct Token {
    std::string text;
};

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
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

bool is_keyword(const std::string &t, const char *kw) {
    return ieq(t, kw);
}

std::vector<Token> tokenize(const std::string &sql) {
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
                // FIX: Escape-Handling für Backslashes in der Shell
                if (x == '\\' && i + 1 < sql.size()) {
                    val.push_back(x);
                    val.push_back(sql[i + 1]);
                    i += 2;
                    continue;
                }
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
        if (std::strchr("(),*", c)) {
            flush();
            out.push_back({std::string(1, c)});
            continue;
        }
        if (c == '=' || c == '!' || c == '<' || c == '>') {
            flush();
            std::string op(1, c);
            if (i + 1 < sql.size()) {
                char n = sql[i + 1];
                if ((c == '!' || c == '<' || c == '>') && n == '=') {
                    op.push_back(n);
                    i++;
                } else if (c == '<' && n == '>') {
                    op.push_back(n);
                    i++;
                }
            }
            out.push_back({op});
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
    const std::string &peek_lower() const { static std::string empty; return eof() ? empty : tokens[pos].text; }
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

struct Expr {
    enum Kind { VALUE, COMPARE, AND, OR, NOT, IN_LIST, IN_SUBQUERY, BETWEEN, LIKE, REGEXP, EXISTS, IS_NULL } kind = VALUE;
    std::string op;
    std::string value;
    std::string value2;
    std::vector<std::string> list;
    std::string subquery;
    bool negate = false;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
};

struct SelectItem {
    enum Kind { STAR, COLUMN, AGG, FUNC } kind = COLUMN;
    std::string table;
    std::string column;
    std::string func;
    std::string alias;
    std::string raw;
};

struct JoinClause {
    enum Kind { INNER, LEFT, RIGHT, CROSS } kind = INNER;
    std::string table;
    std::string alias;
    std::string left_col;
    std::string right_col;
};

struct SqlQuery {
    bool distinct = false;
    std::vector<std::string> distinct_on;
    std::vector<SelectItem> select_items;
    std::string from_table;
    std::string from_alias;
    std::string from_subquery;
    std::vector<JoinClause> joins;
    std::unique_ptr<Expr> where_expr;
    std::vector<std::string> group_by;
    std::unique_ptr<Expr> having_expr;
    struct OrderBy {
        std::string key;
        bool asc = true;
        bool nulls_last = false;
    };
    std::vector<OrderBy> order_by;
    int limit = -1;
    int offset = 0;
};

bool parse_identifier(Parser &p, std::string &out) {
    if (p.eof()) return false;
    std::string t = p.consume();
    if (t == "," || t == "(" || t == ")" || t == "*") return false;
    out = t;
    return true;
}

bool parse_select_list(Parser &p, std::vector<SelectItem> &out) {
    out.clear();
    while (!p.eof()) {
        SelectItem item;
        std::string t = p.peek();
        if (t == "*") {
            p.consume();
            item.kind = SelectItem::STAR;
            item.raw = "*";
            out.push_back(item);
        } else {
            std::string name = p.consume();
            std::string lower = to_lower(name);
            if (lower == "case") {
                std::string expr = name;
                int depth = 0;
                while (!p.eof()) {
                    std::string tok = p.consume();
                    if (ieq(tok, "case")) depth++;
                    if (ieq(tok, "end")) {
                        expr += " " + tok;
                        if (depth == 0) break;
                        depth--;
                        continue;
                    }
                    expr += " " + tok;
                }
                item.kind = SelectItem::FUNC;
                item.raw = expr;
                item.column = expr;
            } else if (!p.eof() && p.peek() == "(") {
                p.consume();
                std::string arglist;
                int depth = 1;
                while (!p.eof() && depth > 0) {
                    std::string tok = p.consume();
                    if (tok == "(") depth++;
                    if (tok == ")") {
                        depth--;
                        if (depth == 0) break;
                    }
                    if (!arglist.empty()) arglist.push_back(' ');
                    arglist += tok;
                }
                item.raw = lower + "(" + arglist + ")";
                if (lower == "count" || lower == "sum" || lower == "avg" || lower == "min" || lower == "max") {
                    item.kind = SelectItem::AGG;
                    item.func = lower;
                    item.column = arglist.empty() ? "*" : arglist;
                } else {
                    item.kind = SelectItem::FUNC;
                    item.column = item.raw;
                }
            } else {
                item.kind = SelectItem::COLUMN;
                item.column = name;
                item.raw = name;
            }
            if (p.match("as")) {
                std::string alias;
                if (!parse_identifier(p, alias)) return false;
                item.alias = alias;
            } else if (!p.eof() && p.peek() != "," && !ieq(p.peek(), "from")) {
                std::string alias;
                if (parse_identifier(p, alias)) {
                    item.alias = alias;
                }
            }
            out.push_back(item);
        }
        if (p.match_symbol(",")) {
            continue;
        }
        break;
    }
    return !out.empty();
}

std::unique_ptr<Expr> parse_expr(Parser &p);

std::unique_ptr<Expr> parse_primary(Parser &p) {
    if (p.match_symbol("(")) {
        auto inner = parse_expr(p);
        if (!p.match_symbol(")")) return nullptr;
        return inner;
    }
    if (p.match("exists")) {
        if (!p.match_symbol("(")) return nullptr;
        std::string sub;
        int depth = 1;
        while (!p.eof() && depth > 0) {
            std::string t = p.consume();
            if (t == "(") depth++;
            if (t == ")") {
                depth--;
                if (depth == 0) break;
            }
            if (!sub.empty()) sub.push_back(' ');
            sub += t;
        }
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::EXISTS;
        expr->subquery = sub;
        return expr;
    }
    if (p.match("not")) {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::NOT;
        expr->lhs = parse_primary(p);
        return expr;
    }
    if (p.eof()) return nullptr;
    std::string head = p.consume();
    if (!p.eof() && p.peek() == "(") {
        p.consume();
        std::string arglist;
        int depth = 1;
        while (!p.eof() && depth > 0) {
            std::string t = p.consume();
            if (t == "(") depth++;
            if (t == ")") {
                depth--;
                if (depth == 0) break;
            }
            if (!arglist.empty()) arglist.push_back(' ');
            arglist += t;
        }
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::VALUE;
        expr->value = to_lower(head) + "(" + arglist + ")";
        return expr;
    }
    auto expr = std::make_unique<Expr>();
    expr->kind = Expr::VALUE;
    expr->value = head;
    return expr;
}

std::unique_ptr<Expr> parse_compare(Parser &p) {
    auto left = parse_primary(p);
    if (!left) return nullptr;
    bool negated = false;
    if (p.match("not")) {
        negated = true;
    }
    if (p.match("is")) {
        bool is_not = false;
        if (p.match("not")) {
            is_not = true;
        }
        if (!p.match("null")) {
            return nullptr;
        }
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::IS_NULL;
        expr->lhs = std::move(left);
        expr->negate = is_not;
        if (negated) {
            auto wrap = std::make_unique<Expr>();
            wrap->kind = Expr::NOT;
            wrap->lhs = std::move(expr);
            return wrap;
        }
        return expr;
    }
    if (p.match("between")) {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::BETWEEN;
        expr->lhs = std::move(left);
        expr->value = p.consume();
        if (!p.match("and")) return nullptr;
        expr->value2 = p.consume();
        if (negated) {
            auto wrap = std::make_unique<Expr>();
            wrap->kind = Expr::NOT;
            wrap->lhs = std::move(expr);
            return wrap;
        }
        return expr;
    }
    if (p.match("in")) {
        if (!p.match_symbol("(")) return nullptr;
        if (!p.eof() && (ieq(p.peek(), "select") || ieq(p.peek(), "with"))) {
            std::string sub = p.consume();
            int depth = 1;
            while (!p.eof() && depth > 0) {
                std::string t = p.consume();
                if (t == "(") depth++;
                if (t == ")") {
                    depth--;
                    if (depth == 0) break;
                }
                sub += " ";
                sub += t;
            }
            auto expr = std::make_unique<Expr>();
            expr->kind = Expr::IN_SUBQUERY;
            expr->lhs = std::move(left);
            expr->subquery = sub;
            if (negated) {
                auto wrap = std::make_unique<Expr>();
                wrap->kind = Expr::NOT;
                wrap->lhs = std::move(expr);
                return wrap;
            }
            return expr;
        }
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::IN_LIST;
        expr->lhs = std::move(left);
        while (!p.eof()) {
            expr->list.push_back(p.consume());
            if (p.match_symbol(")")) break;
            if (!p.match_symbol(",")) return nullptr;
        }
        if (negated) {
            auto wrap = std::make_unique<Expr>();
            wrap->kind = Expr::NOT;
            wrap->lhs = std::move(expr);
            return wrap;
        }
        return expr;
    }
    if (p.match("like")) {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::LIKE;
        expr->lhs = std::move(left);
        expr->value = p.consume();
        if (negated) {
            auto wrap = std::make_unique<Expr>();
            wrap->kind = Expr::NOT;
            wrap->lhs = std::move(expr);
            return wrap;
        }
        return expr;
    }
    if (p.match("regexp")) {
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::REGEXP;
        expr->lhs = std::move(left);
        expr->value = p.consume();
        if (negated) {
            auto wrap = std::make_unique<Expr>();
            wrap->kind = Expr::NOT;
            wrap->lhs = std::move(expr);
            return wrap;
        }
        return expr;
    }
    if (p.eof()) return left;
    std::string op = p.peek();
    if (op == "=" || op == "!=" || op == "<>" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        p.consume();
        auto right = parse_primary(p);
        if (!right) return nullptr;
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::COMPARE;
        expr->op = op;
        expr->lhs = std::move(left);
        expr->rhs = std::move(right);
        if (negated) {
            auto wrap = std::make_unique<Expr>();
            wrap->kind = Expr::NOT;
            wrap->lhs = std::move(expr);
            return wrap;
        }
        return expr;
    }
    return left;
}

std::unique_ptr<Expr> parse_and(Parser &p) {
    auto left = parse_compare(p);
    while (p.match("and")) {
        auto right = parse_compare(p);
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::AND;
        expr->lhs = std::move(left);
        expr->rhs = std::move(right);
        left = std::move(expr);
    }
    return left;
}

std::unique_ptr<Expr> parse_expr(Parser &p) {
    auto left = parse_and(p);
    while (p.match("or")) {
        auto right = parse_and(p);
        auto expr = std::make_unique<Expr>();
        expr->kind = Expr::OR;
        expr->lhs = std::move(left);
        expr->rhs = std::move(right);
        left = std::move(expr);
    }
    return left;
}

bool parse_query(const std::string &sql, SqlQuery &out) {
    Parser p;
    p.tokens = tokenize(sql);
    out = SqlQuery{};
    if (!p.match("select")) {
        return false;
    }
    if (p.match("distinct")) {
        if (p.match("on")) {
            if (!p.match_symbol("(")) return false;
            while (!p.eof()) {
                std::string col = p.consume();
                if (col.empty()) return false;
                out.distinct_on.push_back(col);
                if (p.match_symbol(",")) continue;
                if (p.match_symbol(")")) break;
                return false;
            }
        } else {
            out.distinct = true;
        }
    }
    if (!parse_select_list(p, out.select_items)) return false;
    if (!p.match("from")) return false;
    if (p.match_symbol("(")) {
        std::string sub;
        int depth = 1;
        while (!p.eof() && depth > 0) {
            std::string t = p.consume();
            if (t == "(") depth++;
            if (t == ")") {
                depth--;
                if (depth == 0) break;
            }
            if (!sub.empty()) sub.push_back(' ');
            sub += t;
        }
        out.from_subquery = sub;
        out.from_table.clear();
    } else {
        if (!parse_identifier(p, out.from_table)) return false;
    }
    if (p.match("as")) {
        parse_identifier(p, out.from_alias);
    } else if (!p.eof() && !ieq(p.peek(), "join") && !ieq(p.peek(), "left") && !ieq(p.peek(), "where") &&
               !ieq(p.peek(), "right") && !ieq(p.peek(), "cross") && !ieq(p.peek(), "group") &&
               !ieq(p.peek(), "order") && !ieq(p.peek(), "limit") && !ieq(p.peek(), "offset")) {
        parse_identifier(p, out.from_alias);
    }
    while (!p.eof()) {
        JoinClause join;
        if (p.match("left")) {
            join.kind = JoinClause::LEFT;
            if (!p.match("join")) return false;
        } else if (p.match("right")) {
            join.kind = JoinClause::RIGHT;
            if (!p.match("join")) return false;
        } else if (p.match("cross")) {
            join.kind = JoinClause::CROSS;
            if (!p.match("join")) return false;
        } else if (p.match("join") || p.match("inner")) {
            join.kind = JoinClause::INNER;
            if (ieq(p.peek(), "join")) p.consume();
        } else {
            break;
        }
        if (!parse_identifier(p, join.table)) return false;
        if (p.match("as")) {
            parse_identifier(p, join.alias);
        } else if (!p.eof() && !ieq(p.peek(), "on")) {
            parse_identifier(p, join.alias);
        }
        if (join.kind == JoinClause::CROSS) {
            join.left_col.clear();
            join.right_col.clear();
        } else {
            if (!p.match("on")) return false;
            std::string left = p.consume();
            if (!p.match_symbol("=")) return false;
            std::string right = p.consume();
            join.left_col = left;
            join.right_col = right;
        }
        out.joins.push_back(join);
    }
    if (p.match("where")) {
        out.where_expr = parse_expr(p);
    }
    if (p.match("group")) {
        if (!p.match("by")) return false;
        while (!p.eof()) {
            std::string col = p.consume();
            out.group_by.push_back(col);
            if (p.match_symbol(",")) continue;
            break;
        }
    }
    if (p.match("having")) {
        out.having_expr = parse_expr(p);
    }
    if (p.match("order")) {
        if (!p.match("by")) return false;
        while (!p.eof()) {
            std::string col = p.consume();
            SqlQuery::OrderBy ob;
            ob.key = col;
            if (p.match("asc")) ob.asc = true;
            else if (p.match("desc")) ob.asc = false;
            if (p.match("nulls")) {
                if (p.match("last")) {
                    ob.nulls_last = true;
                } else if (p.match("first")) {
                    ob.nulls_last = false;
                } else {
                    return false;
                }
            }
            out.order_by.push_back(ob);
            if (p.match_symbol(",")) continue;
            break;
        }
    }
    if (p.match("limit")) {
        out.limit = std::stoi(p.consume());
    }
    if (p.match("offset")) {
        out.offset = std::stoi(p.consume());
    }
    return true;
}

struct Cell {
    std::string text;
    bool is_null = true;
    bool has_number = false;
    double number = 0.0;
};

struct Row {
    std::unordered_map<std::string, Cell> values;
};

std::vector<std::string> split_args(const std::string &s);

struct AggSpec {
    std::string raw;
    std::string func;
    std::string column;
};

bool parse_agg_spec(const std::string &raw, AggSpec &out) {
    size_t open = raw.find('(');
    size_t close = raw.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return false;
    }
    std::string fname = to_lower(raw.substr(0, open));
    if (fname != "count" && fname != "sum" && fname != "avg" && fname != "min" && fname != "max") {
        return false;
    }
    std::string args_str = raw.substr(open + 1, close - open - 1);
    std::vector<std::string> args = split_args(args_str);
    out.raw = raw;
    out.func = fname;
    out.column = args.empty() ? "*" : args[0];
    return true;
}

void collect_agg_specs(const Expr *expr, std::vector<AggSpec> &out) {
    if (!expr) return;
    if (expr->kind == Expr::VALUE) {
        AggSpec spec;
        if (parse_agg_spec(expr->value, spec)) {
            out.push_back(spec);
        }
        return;
    }
    if (expr->lhs) collect_agg_specs(expr->lhs.get(), out);
    if (expr->rhs) collect_agg_specs(expr->rhs.get(), out);
}

bool parse_number(const std::string &s, double &out) {
    try {
        size_t idx = 0;
        out = std::stod(s, &idx);
        return idx > 0;
    } catch (...) {
        return false;
    }
}

Cell make_cell(const std::string &val, bool is_null) {
    Cell c;
    c.text = val;
    c.is_null = is_null;
    double num = 0.0;
    if (!is_null && parse_number(val, num)) {
        c.has_number = true;
        c.number = num;
    }
    return c;
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

Cell get_value(const Row &row, const Row *outer, const std::string &name) {
    std::string key = to_lower(name);
    auto it = row.values.find(key);
    if (it != row.values.end()) {
        return it->second;
    }
    if (outer) {
        auto it2 = outer->values.find(key);
        if (it2 != outer->values.end()) {
            return it2->second;
        }
    }
    return Cell{"" , true, false, 0.0};
}

bool like_match(const std::string &text, const std::string &pattern) {
    std::string t = to_lower(text);
    std::string p = to_lower(pattern);
    size_t ti = 0, pi = 0, star = std::string::npos, match = 0;
    while (ti < t.size()) {
        if (pi < p.size() && (p[pi] == '_' || p[pi] == t[ti])) {
            pi++;
            ti++;
            continue;
        }
        if (pi < p.size() && p[pi] == '%') {
            star = pi++;
            match = ti;
            continue;
        }
        if (star != std::string::npos) {
            pi = star + 1;
            ti = ++match;
            continue;
        }
        return false;
    }
    while (pi < p.size() && p[pi] == '%') pi++;
    return pi == p.size();
}

bool eval_expr(const Expr *expr, const Row &row,
               const DbWorld &world,
               bool use_focus,
               int focus_x,
               int focus_y,
               int radius,
               std::string &error);

std::vector<std::string> split_args(const std::string &s) {
    std::vector<std::string> args;
    std::string cur;
    bool in_string = false;
    char quote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if ((c == '\'' || c == '"') && (!in_string || c == quote)) {
            if (in_string && c == quote) {
                in_string = false;
            } else if (!in_string) {
                in_string = true;
                quote = c;
            }
        }
        if (!in_string && c == ',') {
            args.push_back(cur);
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) args.push_back(cur);
    for (auto &a : args) {
        size_t start = 0;
        while (start < a.size() && std::isspace(static_cast<unsigned char>(a[start]))) start++;
        size_t end = a.size();
        while (end > start && std::isspace(static_cast<unsigned char>(a[end - 1]))) end--;
        a = a.substr(start, end - start);
    }
    return args;
}

Cell eval_function(const std::string &raw, const Row &row, const Row *outer) {
    size_t open = raw.find('(');
    size_t close = raw.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return Cell{"", true, false, 0.0};
    }
    std::string fname = to_lower(raw.substr(0, open));
    std::string args_str = raw.substr(open + 1, close - open - 1);
    std::vector<std::string> args = split_args(args_str);
    auto eval_arg = [&](const std::string &a) -> Cell {
        if (!a.empty() && (a.front() == '\'' || a.front() == '"')) {
            return make_cell(strip_quotes(a), false);
        }
        double num = 0.0;
        if (parse_number(a, num)) {
            return make_cell(a, false);
        }
        if (a.find('(') != std::string::npos && a.back() == ')') {
            return eval_function(a, row, outer);
        }
        return get_value(row, outer, a);
    };
    if (fname == "coalesce") {
        for (const auto &a : args) {
            Cell c = eval_arg(a);
            if (!c.is_null && !c.text.empty()) return c;
        }
        return Cell{"", true, false, 0.0};
    }
    if (fname == "ifnull") {
        if (args.size() < 2) return Cell{"", true, false, 0.0};
        Cell c = eval_arg(args[0]);
        if (!c.is_null && !c.text.empty()) return c;
        return eval_arg(args[1]);
    }
    if (fname == "nullif") {
        if (args.size() < 2) return Cell{"", true, false, 0.0};
        Cell a = eval_arg(args[0]);
        Cell b = eval_arg(args[1]);
        if (a.text == b.text) return Cell{"", true, false, 0.0};
        return a;
    }
    if (fname == "to_int") {
        if (args.empty()) return Cell{"", true, false, 0.0};
        Cell c = eval_arg(args[0]);
        double num = 0.0;
        if (!parse_number(c.text, num)) return Cell{"", true, false, 0.0};
        return make_cell(std::to_string(static_cast<int>(num)), false);
    }
    if (fname == "to_float") {
        if (args.empty()) return Cell{"", true, false, 0.0};
        Cell c = eval_arg(args[0]);
        double num = 0.0;
        if (!parse_number(c.text, num)) return Cell{"", true, false, 0.0};
        return make_cell(std::to_string(num), false);
    }
    if (fname == "cast") {
        std::string args_lower = to_lower(args_str);
        size_t as_pos = args_lower.find(" as ");
        if (as_pos == std::string::npos) {
            return Cell{"", true, false, 0.0};
        }
        std::string left = trim(args_str.substr(0, as_pos));
        std::string type = trim(args_lower.substr(as_pos + 4));
        Cell c = eval_arg(left);
        if (type == "int" || type == "integer") {
            double num = 0.0;
            if (!parse_number(c.text, num)) return Cell{"", true, false, 0.0};
            return make_cell(std::to_string(static_cast<int>(num)), false);
        }
        if (type == "float" || type == "real" || type == "double") {
            double num = 0.0;
            if (!parse_number(c.text, num)) return Cell{"", true, false, 0.0};
            return make_cell(std::to_string(num), false);
        }
        return make_cell(c.text, c.is_null);
    }
    if (fname == "lower") {
        if (args.empty()) return Cell{"", true, false, 0.0};
        Cell c = eval_arg(args[0]);
        std::string v = c.text;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return make_cell(v, false);
    }
    if (fname == "upper") {
        if (args.empty()) return Cell{"", true, false, 0.0};
        Cell c = eval_arg(args[0]);
        std::string v = c.text;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return make_cell(v, false);
    }
    if (fname == "length") {
        if (args.empty()) return Cell{"", true, false, 0.0};
        Cell c = eval_arg(args[0]);
        return make_cell(std::to_string(static_cast<int>(c.text.size())), false);
    }
    if (fname == "concat") {
        std::string out;
        for (const auto &a : args) {
            Cell c = eval_arg(a);
            out += c.text;
        }
        return make_cell(out, false);
    }
    if (fname == "substring" || fname == "substr") {
        if (args.size() < 2) return Cell{"", true, false, 0.0};
        Cell base = eval_arg(args[0]);
        int start = 1;
        int len = -1;
        if (!args[1].empty()) {
            try { start = std::stoi(args[1]); } catch (...) { start = 1; }
        }
        if (args.size() >= 3) {
            try { len = std::stoi(args[2]); } catch (...) { len = -1; }
        }
        if (start < 1) start = 1;
        size_t pos = static_cast<size_t>(start - 1);
        if (pos >= base.text.size()) return make_cell("", false);
        if (len < 0) {
            return make_cell(base.text.substr(pos), false);
        }
        return make_cell(base.text.substr(pos, static_cast<size_t>(len)), false);
    }
    return Cell{"", true, false, 0.0};
}

bool eval_case_condition(const std::vector<Token> &tokens,
                         size_t start,
                         size_t end,
                         const Row &row,
                         const Row *outer) {
    if (start >= end) return false;
    std::vector<std::string> parts;
    for (size_t i = start; i < end; ++i) {
        parts.push_back(tokens[i].text);
    }
    if (parts.size() >= 3 && ieq(parts[1], "is")) {
        bool is_not = false;
        size_t idx = 2;
        if (idx < parts.size() && ieq(parts[idx], "not")) {
            is_not = true;
            idx++;
        }
        if (idx < parts.size() && ieq(parts[idx], "null")) {
            Cell c = get_value(row, outer, parts[0]);
            bool is_null = c.is_null;
            return is_not ? !is_null : is_null;
        }
    }
    if (parts.size() < 3) return false;
    std::string lhs = parts[0];
    std::string op = to_lower(parts[1]);
    std::string rhs = parts[2];
    Cell a = get_value(row, outer, lhs);
    Cell b;
    if (!rhs.empty() && (rhs.front() == '\'' || rhs.front() == '"')) {
        b = make_cell(strip_quotes(rhs), false);
    } else {
        b = get_value(row, outer, rhs);
        if (b.is_null) {
            b = make_cell(rhs, false);
        }
    }
    double na = 0.0;
    double nb = 0.0;
    bool a_num = a.has_number || parse_number(a.text, na);
    bool b_num = b.has_number || parse_number(b.text, nb);
    if (a.has_number) na = a.number;
    if (b.has_number) nb = b.number;
    if (op == "=") return a.text == b.text;
    if (op == "!=" || op == "<>") return a.text != b.text;
    if (op == "like") return like_match(a.text, b.text);
    if (op == "regexp") {
        try {
            std::regex re(b.text, std::regex::icase);
            return std::regex_search(a.text, re);
        } catch (...) {
            return false;
        }
    }
    if (a_num && b_num) {
        if (op == "<") return na < nb;
        if (op == "<=") return na <= nb;
        if (op == ">") return na > nb;
        if (op == ">=") return na >= nb;
    }
    if (op == "<") return a.text < b.text;
    if (op == "<=") return a.text <= b.text;
    if (op == ">") return a.text > b.text;
    if (op == ">=") return a.text >= b.text;
    return false;
}

Cell eval_case_expr(const std::string &raw, const Row &row, const Row *outer) {
    Parser p;
    p.tokens = tokenize(raw);
    if (!p.match("case")) {
        return Cell{"", true, false, 0.0};
    }
    while (!p.eof()) {
        if (p.match("when")) {
            size_t cond_start = p.pos;
            while (!p.eof() && !ieq(p.peek(), "then")) {
                p.consume();
            }
            size_t cond_end = p.pos;
            if (!p.match("then")) return Cell{"", true, false, 0.0};
            size_t val_start = p.pos;
            while (!p.eof() && !ieq(p.peek(), "when") && !ieq(p.peek(), "else") && !ieq(p.peek(), "end")) {
                p.consume();
            }
            size_t val_end = p.pos;
            if (eval_case_condition(p.tokens, cond_start, cond_end, row, outer)) {
                std::string val;
                for (size_t i = val_start; i < val_end; ++i) {
                    if (!val.empty()) val.push_back(' ');
                    val += p.tokens[i].text;
                }
                if (val.empty()) return Cell{"", true, false, 0.0};
                if (!val.empty() && (val.front() == '\'' || val.front() == '"')) {
                    return make_cell(strip_quotes(val), false);
                }
                if (val.find('(') != std::string::npos && val.back() == ')') {
                    return eval_function(val, row, outer);
                }
                Cell c = get_value(row, outer, val);
                if (!c.is_null) return c;
                return make_cell(val, false);
            }
            continue;
        }
        if (p.match("else")) {
            std::string val;
            while (!p.eof() && !ieq(p.peek(), "end")) {
                if (!val.empty()) val.push_back(' ');
                val += p.consume();
            }
            if (!val.empty() && (val.front() == '\'' || val.front() == '"')) {
                return make_cell(strip_quotes(val), false);
            }
            if (val.find('(') != std::string::npos && val.back() == ')') {
                return eval_function(val, row, outer);
            }
            Cell c = get_value(row, outer, val);
            if (!c.is_null) return c;
            return make_cell(val, false);
        }
        if (p.match("end")) {
            break;
        }
        p.consume();
    }
    return Cell{"", true, false, 0.0};
}

Cell eval_value(const Expr *expr, const Row &row, const Row *outer) {
    if (!expr) return Cell{"", true, false, 0.0};
    if (expr->kind == Expr::VALUE) {
        std::string raw = expr->value;
        if (!raw.empty() && (raw.front() == '\'' || raw.front() == '"')) {
            return make_cell(strip_quotes(raw), false);
        }
        std::string lower = to_lower(raw);
        if (lower.rfind("case", 0) == 0 && lower.size() >= 3 &&
            lower.find(" end") != std::string::npos) {
            return eval_case_expr(raw, row, outer);
        }
        double num = 0.0;
        if (parse_number(raw, num)) {
            return make_cell(raw, false);
        }
        if (raw.find('(') != std::string::npos && raw.back() == ')') {
            Cell c = get_value(row, outer, raw);
            if (!c.is_null) {
                return c;
            }
            return eval_function(raw, row, outer);
        }
        return get_value(row, outer, raw);
    }
    return Cell{"", true, false, 0.0};
}

bool compare_cells(const Cell &a, const Cell &b, const std::string &op) {
    if (a.is_null || b.is_null) return false;
    
    double na, nb;
    bool a_num = a.has_number || parse_number(a.text, na);
    bool b_num = b.has_number || parse_number(b.text, nb);
    if (a.has_number) na = a.number;
    if (b.has_number) nb = b.number;

    if (a_num && b_num) {
        if (op == "=") return std::abs(na - nb) < 1e-9;
        if (op == "!=" || op == "<>") return std::abs(na - nb) > 1e-9;
        if (op == "<") return na < nb;
        if (op == "<=") return na <= nb;
        if (op == ">") return na > nb;
        if (op == ">=") return na >= nb;
    }
    
    // Fallback auf String-Vergleich
    if (op == "=") return ieq(a.text, b.text);
    if (op == "!=" || op == "<>") return !ieq(a.text, b.text);
    if (op == "<") return a.text < b.text;
    if (op == "<=") return a.text <= b.text;
    if (op == ">") return a.text > b.text;
    if (op == ">=") return a.text >= b.text;
    return false;
}

bool eval_expr(const Expr *expr,
               const Row &row,
               const Row *outer,
               const DbWorld &world,
               bool use_focus,
               int focus_x,
               int focus_y,
               int radius,
               std::string &error) {
    if (!expr) return true;
    switch (expr->kind) {
        case Expr::AND:
            return eval_expr(expr->lhs.get(), row, outer, world, use_focus, focus_x, focus_y, radius, error) &&
                   eval_expr(expr->rhs.get(), row, outer, world, use_focus, focus_x, focus_y, radius, error);
        case Expr::OR:
            return eval_expr(expr->lhs.get(), row, outer, world, use_focus, focus_x, focus_y, radius, error) ||
                   eval_expr(expr->rhs.get(), row, outer, world, use_focus, focus_x, focus_y, radius, error);
        case Expr::NOT:
			return !eval_expr(expr->lhs.get(), row, outer, world, use_focus, focus_x, focus_y, radius, error);
        case Expr::COMPARE: {
            Cell a = eval_value(expr->lhs.get(), row, outer);
            Cell b = eval_value(expr->rhs.get(), row, outer);
            return compare_cells(a, b, expr->op);
        }
        case Expr::BETWEEN: {
            Cell a = eval_value(expr->lhs.get(), row, outer);
            Cell b = make_cell(strip_quotes(expr->value), false);
            Cell c = make_cell(strip_quotes(expr->value2), false);
            if (a.is_null) return false;
            if (a.has_number && b.has_number && c.has_number) {
                return a.number >= b.number && a.number <= c.number;
            }
            return a.text >= b.text && a.text <= c.text;
        }
        case Expr::IN_LIST: {
            Cell a = eval_value(expr->lhs.get(), row, outer);
            if (a.is_null) return false;
            for (const auto &v : expr->list) {
                Cell b = make_cell(strip_quotes(v), false);
                if (compare_cells(a, b, "=")) return true;
            }
            return false;
        }
        case Expr::IN_SUBQUERY: {
            Cell a = eval_value(expr->lhs.get(), row, outer);
            if (a.is_null) return false;
            DbSqlResult sub;
            std::string sub_error;
            if (!exec_sql_with_outer(world, expr->subquery, use_focus, focus_x, focus_y, radius, &row, sub, sub_error)) {
                error = sub_error;
                return false;
            }
            if (sub.columns.empty()) return false;
            for (const auto &r : sub.rows) {
                if (r.empty()) continue;
                Cell b = make_cell(r.front(), false);
                if (compare_cells(a, b, "=")) return true;
            }
            return false;
        }
        case Expr::LIKE: {
            Cell a = eval_value(expr->lhs.get(), row, outer);
            if (a.is_null) return false;
            std::string pat = strip_quotes(expr->value);
            return like_match(a.text, pat);
        }
        case Expr::REGEXP: {
            Cell a = eval_value(expr->lhs.get(), row, outer);
            if (a.is_null) return false;
            std::string pat = strip_quotes(expr->value);
            try {
                std::regex re(pat, std::regex_constants::icase);
                return std::regex_search(a.text, re);
            } catch (...) {
                error = "REGEXP-Pattern ungueltig.";
                return false;
            }
        }
        case Expr::EXISTS: {
            DbSqlResult sub;
            std::string sub_error;
            if (!exec_sql_with_outer(world, expr->subquery, use_focus, focus_x, focus_y, radius, &row, sub, sub_error)) {
                error = sub_error;
                return false;
            }
            return !sub.rows.empty();
        }
        case Expr::IS_NULL: {
            Cell a = eval_value(expr->lhs.get(), row, outer);
            bool is_null = a.is_null || a.text.empty();
            return expr->negate ? !is_null : is_null;
        }
		case Expr::VALUE: {
			Cell v = eval_value(expr, row, outer);
			if (v.is_null) return false;
			if (v.has_number) return std::abs(v.number) > 1e-9;
			std::string s = to_lower(v.text);
			return !s.empty() && s != "0" && s != "false" && s != "null";
		}
	}
    return true;
}

struct AggState {
    int64_t count = 0;
    double sum = 0.0;
    int64_t count_num = 0;
    bool has_min = false;
    bool has_max = false;
    Cell min_val;
    Cell max_val;
};

void update_minmax(Cell &dst, const Cell &v, bool is_min) {
    if (v.is_null) return;
    if (!dst.is_null && dst.has_number && v.has_number) {
        if ((is_min && v.number < dst.number) || (!is_min && v.number > dst.number)) {
            dst = v;
        }
        return;
    }
    if ((is_min && (dst.is_null || v.text < dst.text)) || (!is_min && (dst.is_null || v.text > dst.text))) {
        dst = v;
    }
}

bool in_focus(const DbPayload &p, int focus_x, int focus_y, int radius) {
    if (!p.placed) return false;
    int dx = p.x - focus_x;
    int dy = p.y - focus_y;
    return (dx * dx + dy * dy) <= radius * radius;
}

Row make_row_for_payload(const DbWorld &world, const DbPayload &p, const std::string &alias) {
    Row row;
    std::string table = world.table_names[static_cast<size_t>(p.table_id)];
    std::string table_key = to_lower(table);
    std::string alias_key = to_lower(alias.empty() ? table : alias);
    for (const auto &f : p.fields) {
        std::string col_key = to_lower(f.name);
        Cell c = make_cell(f.value, false);
        row.values[col_key] = c;
        row.values[table_key + "." + col_key] = c;
        row.values[alias_key + "." + col_key] = c;
    }
    return row;
}

std::vector<Row> rows_for_table(const DbWorld &world,
                                const std::string &table_name,
                                const std::string &alias,
                                bool use_focus,
                                int focus_x,
                                int focus_y,
                                int radius,
                                const std::unordered_map<std::string, DbSqlResult> &cte_map) {
    std::vector<Row> rows;
    auto it_cte = cte_map.find(to_lower(table_name));
    if (it_cte != cte_map.end()) {
        const DbSqlResult &res = it_cte->second;
        for (const auto &r : res.rows) {
            Row row;
            for (size_t i = 0; i < r.size() && i < res.columns.size(); ++i) {
                Cell c = make_cell(r[i], false);
                std::string col_key = to_lower(res.columns[i]);
                row.values[col_key] = c;
                if (!alias.empty()) {
                    row.values[to_lower(alias) + "." + col_key] = c;
                }
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }
    int table_id = db_find_table(world, table_name);
    if (table_id < 0) return rows;
    for (const auto &p : world.payloads) {
        if (p.table_id != table_id) continue;
        int64_t key = db_payload_key(p.table_id, p.id);
        if (world.tombstones.find(key) != world.tombstones.end()) continue;
        if (!p.is_delta) {
            if (world.delta_index_by_key.find(key) != world.delta_index_by_key.end()) continue;
            if (use_focus && !in_focus(p, focus_x, focus_y, radius)) continue;
        }
        rows.push_back(make_row_for_payload(world, p, alias));
    }
    return rows;
}

Cell get_cell_by_name(const Row &row, const Row *outer, const std::string &name) {
    return get_value(row, outer, name);
}

std::string make_group_key(const Row &row, const Row *outer, const std::vector<std::string> &cols) {
    std::string key;
    for (size_t i = 0; i < cols.size(); ++i) {
        if (i > 0) key.push_back('|');
        Cell c = get_cell_by_name(row, outer, cols[i]);
        key += c.is_null ? "NULL" : c.text;
    }
    return key;
}

Cell resolve_order_cell(const std::vector<std::string> &columns,
                        const std::vector<std::string> &row_values,
                        const Row &row_meta,
                        const Row *outer,
                        const std::string &key) {
    bool digits = !key.empty() && std::all_of(key.begin(), key.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
    if (digits) {
        int idx = std::stoi(key);
        if (idx > 0 && static_cast<size_t>(idx) <= row_values.size()) {
            return make_cell(row_values[static_cast<size_t>(idx - 1)], false);
        }
    }
    std::string key_lower = to_lower(key);
    for (size_t i = 0; i < columns.size(); ++i) {
        if (to_lower(columns[i]) == key_lower) {
            if (i < row_values.size()) {
                return make_cell(row_values[i], false);
            }
            break;
        }
    }
    return get_cell_by_name(row_meta, outer, key);
}

bool apply_order(const std::vector<SqlQuery::OrderBy> &order_by,
                 const std::vector<std::string> &columns,
                 const std::vector<std::string> &a,
                 const std::vector<std::string> &b,
                 const Row &meta_a,
                 const Row *outer_a,
                 const Row &meta_b,
                 const Row *outer_b) {
    for (const auto &ob : order_by) {
        Cell ca = resolve_order_cell(columns, a, meta_a, outer_a, ob.key);
        Cell cb = resolve_order_cell(columns, b, meta_b, outer_b, ob.key);
        if (ob.nulls_last && ca.is_null != cb.is_null) {
            return ca.is_null ? false : true;
        }
        if (ca.is_null && cb.is_null) continue;
        double na = 0.0;
        double nb = 0.0;
        bool a_num = ca.has_number || parse_number(ca.text, na);
        bool b_num = cb.has_number || parse_number(cb.text, nb);
        if (ca.has_number) na = ca.number;
        if (cb.has_number) nb = cb.number;
        if (a_num && b_num) {
            if (na == nb) continue;
            return ob.asc ? (na < nb) : (na > nb);
        }
        if (ca.text == cb.text) continue;
        if (ob.asc) {
            return ca.text < cb.text;
        }
        return ca.text > cb.text;
    }
    return false;
}

std::string normalize_col(const std::string &col, const std::string &table, const std::string &alias) {
    if (col.find('.') != std::string::npos) {
        return col;
    }
    if (!alias.empty()) return alias + "." + col;
    return table + "." + col;
}
bool execute_single_sql(const DbWorld &world,
                        const std::string &sql,
                        bool use_focus,
                        int focus_x,
                        int focus_y,
                        int radius,
                        const std::unordered_map<std::string, DbSqlResult> &cte_map,
                        const Row *outer,
                        DbSqlResult &out,
                        std::vector<Row> &out_meta,
                        std::string &error) {
    SqlQuery q;
    if (!parse_query(sql, q)) {
        error = "SQL-Parser: ungueltige Query.";
        return false;
    }

    std::string from_alias = q.from_alias.empty() ? q.from_table : q.from_alias;
    std::vector<Row> rows;
    if (!q.from_subquery.empty()) {
        DbSqlResult sub_result;
        std::vector<Row> sub_meta;
        if (!execute_single_sql(world, q.from_subquery, use_focus, focus_x, focus_y, radius, cte_map, outer, sub_result, sub_meta, error)) {
            return false;
        }
        for (const auto &r : sub_result.rows) {
            Row row;
            for (size_t i = 0; i < r.size() && i < sub_result.columns.size(); ++i) {
                Cell c = make_cell(r[i], false);
                std::string col_key = to_lower(sub_result.columns[i]);
                row.values[col_key] = c;
                if (!from_alias.empty()) {
                    row.values[to_lower(from_alias) + "." + col_key] = c;
                }
            }
            rows.push_back(std::move(row));
        }
    } else {
        rows = rows_for_table(world, q.from_table, from_alias, use_focus, focus_x, focus_y, radius, cte_map);
    }
    if (rows.empty()) {
        out.columns.clear();
        out.rows.clear();
        return true;
    }

    for (const auto &join : q.joins) {
        std::string alias = join.alias.empty() ? join.table : join.alias;
        std::vector<Row> right_rows = rows_for_table(world, join.table, alias, use_focus, focus_x, focus_y, radius, cte_map);
        std::vector<Row> next;
        if (join.kind == JoinClause::CROSS) {
            for (const auto &lrow : rows) {
                for (const auto &rrow : right_rows) {
                    Row combined = lrow;
                    combined.values.insert(rrow.values.begin(), rrow.values.end());
                    next.push_back(std::move(combined));
                }
            }
        } else if (join.kind == JoinClause::RIGHT) {
            for (const auto &rrow : right_rows) {
                bool matched = false;
                Cell rv = get_cell_by_name(rrow, outer, join.right_col);
                for (const auto &lrow : rows) {
                    Cell lv = get_cell_by_name(lrow, outer, join.left_col);
                    if (compare_cells(lv, rv, "=")) {
                        Row combined = lrow;
                        combined.values.insert(rrow.values.begin(), rrow.values.end());
                        next.push_back(std::move(combined));
                        matched = true;
                    }
                }
                if (!matched) {
                    Row combined = rrow;
                    next.push_back(std::move(combined));
                }
            }
        } else {
            for (const auto &lrow : rows) {
                bool matched = false;
                Cell lv = get_cell_by_name(lrow, outer, join.left_col);
                for (const auto &rrow : right_rows) {
                    Cell rv = get_cell_by_name(rrow, outer, join.right_col);
                    if (compare_cells(lv, rv, "=")) {
                        Row combined = lrow;
                        combined.values.insert(rrow.values.begin(), rrow.values.end());
                        next.push_back(std::move(combined));
                        matched = true;
                    }
                }
                if (!matched && join.kind == JoinClause::LEFT) {
                    Row combined = lrow;
                    next.push_back(std::move(combined));
                }
            }
        }
        rows.swap(next);
    }

    if (q.where_expr) {
        std::vector<Row> filtered;
        for (const auto &row : rows) {
            if (eval_expr(q.where_expr.get(), row, outer, world, use_focus, focus_x, focus_y, radius, error)) {
                filtered.push_back(row);
            }
            if (!error.empty()) {
                return false;
            }
        }
        rows.swap(filtered);
    }

    std::vector<std::string> output_columns;
    std::vector<std::vector<std::string>> output_rows;
    std::vector<Row> output_meta;

    bool has_group = !q.group_by.empty();
    bool has_aggregate = false;
    bool has_nonagg_select = false;
    for (const auto &item : q.select_items) {
        if (item.kind == SelectItem::AGG) {
            has_aggregate = true;
        } else {
            has_nonagg_select = true;
        }
    }
    bool has_aggregate_only = !has_group && has_aggregate && !has_nonagg_select;
    std::vector<std::string> group_cols = q.group_by;
    if (has_group || has_aggregate_only) {
        if (has_group) {
            for (auto &gb : group_cols) {
                for (const auto &item : q.select_items) {
                    if (!item.alias.empty() && ieq(item.alias, gb) && item.kind == SelectItem::COLUMN) {
                        gb = item.column;
                    }
                }
            }
        }
        std::unordered_map<std::string, std::vector<Row>> groups;
        for (const auto &row : rows) {
            std::string key = make_group_key(row, outer, group_cols);
            groups[key].push_back(row);
        }
        if (has_aggregate_only && groups.empty()) {
            groups[""] = std::vector<Row>{};
        }
        for (const auto &item : q.select_items) {
            if (item.kind == SelectItem::STAR) {
                error = "SELECT * ist mit GROUP BY nicht erlaubt.";
                return false;
            }
        }
        output_columns.clear();
        for (const auto &item : q.select_items) {
            if (!item.alias.empty()) {
                output_columns.push_back(item.alias);
            } else if (item.kind == SelectItem::AGG) {
                output_columns.push_back(item.raw);
            } else {
                output_columns.push_back(item.column);
            }
        }

        for (const auto &pair : groups) {
            const auto &grows = pair.second;
            Row agg_row;
            std::unordered_map<std::string, AggState> agg;
            std::vector<AggSpec> agg_specs;
            {
                std::unordered_map<std::string, bool> seen;
                for (const auto &item : q.select_items) {
                    if (item.kind != SelectItem::AGG) continue;
                    AggSpec spec;
                    spec.raw = item.raw;
                    spec.func = item.func;
                    spec.column = item.column;
                    std::string key = to_lower(spec.raw);
                    if (!seen[key]) {
                        seen[key] = true;
                        agg_specs.push_back(spec);
                    }
                }
                if (q.having_expr) {
                    std::vector<AggSpec> having_specs;
                    collect_agg_specs(q.having_expr.get(), having_specs);
                    for (const auto &spec : having_specs) {
                        std::string key = to_lower(spec.raw);
                        if (!seen[key]) {
                            seen[key] = true;
                            agg_specs.push_back(spec);
                        }
                    }
                }
            }

            for (const auto &spec : agg_specs) {
                agg[spec.raw] = AggState{};
            }
            for (const auto &gb : group_cols) {
                Cell c = get_cell_by_name(grows.front(), outer, gb);
                agg_row.values[to_lower(gb)] = c;
            }

            for (const auto &row : grows) {
                for (const auto &spec : agg_specs) {
                    AggState &state = agg[spec.raw];
                    if (ieq(spec.func, "count")) {
                        if (spec.column == "*") {
                            state.count++;
                        } else {
                            Cell c = get_cell_by_name(row, outer, spec.column);
                            if (!c.is_null) state.count++;
                        }
                    } else if (ieq(spec.func, "sum") || ieq(spec.func, "avg")) {
                        Cell c = get_cell_by_name(row, outer, spec.column);
                        double val = 0.0;
                        // FIX: Versuche immer zu parsen, wenn has_number false ist
                        if (c.has_number) {
                            val = c.number;
                        } else {
                            parse_number(c.text, val);
                        }
                        state.sum += val;
                        state.count_num++;
                    } else if (ieq(spec.func, "min")) {
                        Cell c = get_cell_by_name(row, outer, spec.column);
                        update_minmax(state.min_val, c, true);
                        state.has_min = true;
                    } else if (ieq(spec.func, "max")) {
                        Cell c = get_cell_by_name(row, outer, spec.column);
                        update_minmax(state.max_val, c, false);
                        state.has_max = true;
                    }
                }
            }

            std::vector<std::string> out_row;
            out_row.reserve(q.select_items.size());
            for (const auto &item : q.select_items) {
                if (item.kind == SelectItem::AGG) {
                    const AggState &state = agg[item.raw];
                    if (ieq(item.func, "count")) {
                        out_row.push_back(std::to_string(state.count));
                        agg_row.values[to_lower(item.raw)] = make_cell(std::to_string(state.count), false);
                        if (!item.alias.empty()) {
                            agg_row.values[to_lower(item.alias)] = make_cell(std::to_string(state.count), false);
                        }
                    } else if (ieq(item.func, "sum")) {
                        out_row.push_back(std::to_string(state.sum));
                        agg_row.values[to_lower(item.raw)] = make_cell(std::to_string(state.sum), false);
                        if (!item.alias.empty()) {
                            agg_row.values[to_lower(item.alias)] = make_cell(std::to_string(state.sum), false);
                        }
                    } else if (ieq(item.func, "avg")) {
                        double avg = (state.count_num > 0) ? (state.sum / static_cast<double>(state.count_num)) : 0.0;
                        out_row.push_back(std::to_string(avg));
                        agg_row.values[to_lower(item.raw)] = make_cell(std::to_string(avg), false);
                        if (!item.alias.empty()) {
                            agg_row.values[to_lower(item.alias)] = make_cell(std::to_string(avg), false);
                        }
                    } else if (ieq(item.func, "min")) {
                        out_row.push_back(state.min_val.is_null ? "" : state.min_val.text);
                        agg_row.values[to_lower(item.raw)] = state.min_val;
                        if (!item.alias.empty()) {
                            agg_row.values[to_lower(item.alias)] = state.min_val;
                        }
                    } else if (ieq(item.func, "max")) {
                        out_row.push_back(state.max_val.is_null ? "" : state.max_val.text);
                        agg_row.values[to_lower(item.raw)] = state.max_val;
                        if (!item.alias.empty()) {
                            agg_row.values[to_lower(item.alias)] = state.max_val;
                        }
                    }
                } else {
                    Cell c;
                    if (item.kind == SelectItem::FUNC) {
                        std::string lower = to_lower(item.raw);
                        if (lower.rfind("case", 0) == 0) {
                            c = eval_case_expr(item.raw, grows.front(), outer);
                        } else {
                            c = eval_function(item.raw, grows.front(), outer);
                        }
                    } else {
                        c = get_cell_by_name(grows.front(), outer, item.column);
                    }
                    out_row.push_back(c.is_null ? "" : c.text);
                    agg_row.values[to_lower(item.column)] = c;
                    if (!item.alias.empty()) {
                        agg_row.values[to_lower(item.alias)] = c;
                    }
                }
            }
            for (const auto &spec : agg_specs) {
                const AggState &state = agg[spec.raw];
                std::string key = to_lower(spec.raw);
                if (agg_row.values.find(key) != agg_row.values.end()) continue;
                if (ieq(spec.func, "count")) {
                    agg_row.values[key] = make_cell(std::to_string(state.count), false);
                } else if (ieq(spec.func, "sum")) {
                    agg_row.values[key] = make_cell(std::to_string(state.sum), false);
                } else if (ieq(spec.func, "avg")) {
                    double avg = (state.count_num > 0) ? (state.sum / static_cast<double>(state.count_num)) : 0.0;
                    agg_row.values[key] = make_cell(std::to_string(avg), false);
                } else if (ieq(spec.func, "min")) {
                    agg_row.values[key] = state.min_val;
                } else if (ieq(spec.func, "max")) {
                    agg_row.values[key] = state.max_val;
                }
            }
            if (q.having_expr && !eval_expr(q.having_expr.get(), agg_row, outer, world, use_focus, focus_x, focus_y, radius, error)) {
                if (!error.empty()) {
                    return false;
                }
                continue;
            }
            output_rows.push_back(out_row);
            output_meta.push_back(agg_row);
        }
    } else {
        output_columns.clear();
        bool has_star = false;
        for (const auto &item : q.select_items) {
            if (item.kind == SelectItem::STAR) {
                has_star = true;
                break;
            }
        }
        if (has_star) {
            if (!rows.empty()) {
                // FIX: Filter eingebaut UND Klammern korrekt geschlossen
                for (const auto &pair : rows.front().values) {
                    if (pair.first.find('.') == std::string::npos) {
                        output_columns.push_back(pair.first);
                    }
                }
            }
        } else {
            for (const auto &item : q.select_items) {
                if (!item.alias.empty()) {
                    output_columns.push_back(item.alias);
                } else {
                    output_columns.push_back(item.raw);
                }
            }
        }
        for (const auto &row : rows) {
            std::vector<std::string> out_row;
            if (has_star) {
                out_row.reserve(output_columns.size());
                for (const auto &col : output_columns) {
                    Cell c = get_cell_by_name(row, outer, col);
                    out_row.push_back(c.is_null ? "" : c.text);
                }
            } else {
                for (const auto &item : q.select_items) {
                    if (item.kind == SelectItem::AGG) {
                        error = "Aggregates ohne GROUP BY nicht erlaubt.";
                        return false;
                    }
                    Cell c;
                    if (item.kind == SelectItem::FUNC) {
                        std::string lower = to_lower(item.raw);
                        if (lower.rfind("case", 0) == 0) {
                            c = eval_case_expr(item.raw, row, outer);
                        } else {
                            c = eval_function(item.raw, row, outer);
                        }
                    } else {
                        c = get_cell_by_name(row, outer, item.column);
                    }
                    out_row.push_back(c.is_null ? "" : c.text);
                }
            }
            output_rows.push_back(out_row);
            output_meta.push_back(row);
        }
    }

    if (q.distinct) {
        std::vector<std::vector<std::string>> unique_rows;
        std::unordered_map<std::string, bool> seen;
        for (const auto &r : output_rows) {
            std::string key;
            for (const auto &v : r) {
                key += v;
                key.push_back('|');
            }
            if (seen.find(key) == seen.end()) {
                seen[key] = true;
                unique_rows.push_back(r);
            }
        }
        output_rows.swap(unique_rows);
    }

    if (!q.order_by.empty()) {
        std::vector<size_t> order_idx(output_rows.size());
        for (size_t i = 0; i < order_idx.size(); ++i) order_idx[i] = i;
        std::sort(order_idx.begin(), order_idx.end(), [&](size_t ia, size_t ib) {
            return apply_order(q.order_by,
                               output_columns,
                               output_rows[ia],
                               output_rows[ib],
                               output_meta[ia],
                               outer,
                               output_meta[ib],
                               outer);
        });
        std::vector<std::vector<std::string>> sorted_rows;
        std::vector<Row> sorted_meta;
        sorted_rows.reserve(output_rows.size());
        sorted_meta.reserve(output_rows.size());
        for (size_t idx : order_idx) {
            sorted_rows.push_back(output_rows[idx]);
            sorted_meta.push_back(output_meta[idx]);
        }
        output_rows.swap(sorted_rows);
        output_meta.swap(sorted_meta);
    }

    if (!q.distinct_on.empty()) {
        std::vector<std::vector<std::string>> unique_rows;
        std::vector<Row> unique_meta;
        std::unordered_map<std::string, bool> seen;
        for (size_t r = 0; r < output_rows.size(); ++r) {
            std::string key;
            for (const auto &col : q.distinct_on) {
                Cell c = resolve_order_cell(output_columns, output_rows[r], output_meta[r], outer, col);
                key += c.is_null ? "NULL" : c.text;
                key.push_back('|');
            }
            if (seen.find(key) == seen.end()) {
                seen[key] = true;
                unique_rows.push_back(output_rows[r]);
                unique_meta.push_back(output_meta[r]);
            }
        }
        output_rows.swap(unique_rows);
        output_meta.swap(unique_meta);
    }

    int start = std::max(0, q.offset);
    int end = static_cast<int>(output_rows.size());
    int effective_limit = q.limit;
    if (effective_limit < 0 && world.default_limit >= 0) {
        effective_limit = world.default_limit;
    }
    if (effective_limit >= 0) {
        end = std::min(end, start + effective_limit);
    }
    if (start > 0 || end < static_cast<int>(output_rows.size())) {
        std::vector<std::vector<std::string>> sliced;
        std::vector<Row> sliced_meta;
        for (int i = start; i < end; ++i) {
            sliced.push_back(output_rows[static_cast<size_t>(i)]);
            sliced_meta.push_back(output_meta[static_cast<size_t>(i)]);
        }
        output_rows.swap(sliced);
        output_meta.swap(sliced_meta);
    }

    out.columns = output_columns;
    out.rows = output_rows;
    out_meta = output_meta;
    return true;
}

struct UnionPart {
    std::string sql;
    bool all = false;
};

bool split_union(const std::string &sql, std::vector<UnionPart> &parts) {
    Parser p;
    p.tokens = tokenize(sql);
    std::string current;
    parts.clear();
    int depth = 0;
    while (!p.eof()) {
        std::string t = p.consume();
        if (t == "(") {
            depth++;
        } else if (t == ")") {
            depth = std::max(0, depth - 1);
        }
        if (depth == 0 && ieq(t, "union")) {
            UnionPart part;
            part.sql = current;
            part.all = false;
            current.clear();
            if (p.match("all")) {
                part.all = true;
            }
            parts.push_back(part);
            continue;
        }
        if (!current.empty()) current.push_back(' ');
        current += t;
    }
    if (!current.empty()) {
        parts.push_back({current, false});
    }
    return parts.size() > 1;
}

bool exec_sql_with_outer(const DbWorld &world,
                         const std::string &sql,
                         bool use_focus,
                         int focus_x,
                         int focus_y,
                         int radius,
                         const Row *outer,
                         DbSqlResult &out,
                         std::string &error) {
    std::string input = sql;
    std::unordered_map<std::string, DbSqlResult> cte_map;
    if (input.size() >= 4 && ieq(input.substr(0, 4), "with")) {
        Parser p;
        p.tokens = tokenize(input);
        if (!p.match("with")) {
            error = "CTE-Parser: erwartet WITH.";
            return false;
        }
        while (!p.eof()) {
            std::string name = p.consume();
            if (name.empty()) {
                error = "CTE-Parser: Name fehlt.";
                return false;
            }
            if (!p.match("as")) {
                error = "CTE-Parser: AS fehlt.";
                return false;
            }
            if (!p.match_symbol("(")) {
                error = "CTE-Parser: Klammer fehlt.";
                return false;
            }
            std::string sub;
            int depth = 1;
            while (!p.eof() && depth > 0) {
                std::string t = p.consume();
                if (t == "(") depth++;
                if (t == ")") {
                    depth--;
                    if (depth == 0) break;
                }
                if (!sub.empty()) sub.push_back(' ');
                sub += t;
            }
            DbSqlResult sub_result;
            std::vector<Row> sub_meta;
            if (!execute_single_sql(world, sub, use_focus, focus_x, focus_y, radius, cte_map, outer, sub_result, sub_meta, error)) {
                return false;
            }
            cte_map[to_lower(name)] = sub_result;
            if (p.match_symbol(",")) {
                continue;
            }
            std::string rest;
            while (!p.eof()) {
                if (!rest.empty()) rest.push_back(' ');
                rest += p.consume();
            }
            input = rest;
            break;
        }
    }
    std::vector<UnionPart> parts;
    if (split_union(input, parts)) {
        DbSqlResult combined;
        std::vector<Row> combined_meta;
        for (size_t i = 0; i < parts.size(); ++i) {
            const auto &part = parts[i];
            DbSqlResult sub;
            std::vector<Row> sub_meta;
            if (!execute_single_sql(world, part.sql, use_focus, focus_x, focus_y, radius, cte_map, outer, sub, sub_meta, error)) {
                return false;
            }
            if (i == 0) {
                combined = sub;
                combined_meta = sub_meta;
            } else {
                if (sub.columns.size() != combined.columns.size()) {
                    error = "UNION: Spaltenanzahl passt nicht.";
                    return false;
                }
                combined.rows.insert(combined.rows.end(), sub.rows.begin(), sub.rows.end());
                combined_meta.insert(combined_meta.end(), sub_meta.begin(), sub_meta.end());
                bool do_distinct = !parts[i - 1].all;
                if (do_distinct) {
                    std::vector<std::vector<std::string>> unique_rows;
                    std::vector<Row> unique_meta;
                    std::unordered_map<std::string, bool> seen;
                    for (size_t r = 0; r < combined.rows.size(); ++r) {
                        std::string key;
                        for (const auto &v : combined.rows[r]) {
                            key += v;
                            key.push_back('|');
                        }
                        if (seen.find(key) == seen.end()) {
                            seen[key] = true;
                            unique_rows.push_back(combined.rows[r]);
                            unique_meta.push_back(combined_meta[r]);
                        }
                    }
                    combined.rows.swap(unique_rows);
                    combined_meta.swap(unique_meta);
                }
            }
        }
        out = combined;
        return true;
    }
    std::vector<Row> meta;
    return execute_single_sql(world, input, use_focus, focus_x, focus_y, radius, cte_map, outer, out, meta, error);
}

} // namespace

bool db_execute_sql(DbWorld &world,
                    const std::string &sql,
                    bool use_focus,
                    int focus_x,
                    int focus_y,
                    int radius,
                    DbSqlResult &out,
                    std::string &error) {
    auto lower_copy = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    };
    std::string trimmed = sql;
    trimmed.erase(trimmed.begin(),
                  std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c) { return !std::isspace(c); }));
    std::string lower = lower_copy(trimmed);
    int rows = 0;
    if (lower.rfind("set", 0) == 0) {
        Parser p;
        p.tokens = tokenize(sql);
        if (!p.match("set")) {
            error = "SET: ungueltig.";
            return false;
        }
        if (!p.match("limit")) {
            error = "SET: nur LIMIT unterstuetzt.";
            return false;
        }
        if (p.match("off")) {
            world.default_limit = -1;
        } else {
            std::string val = p.consume();
            if (val.empty()) {
                error = "SET LIMIT: Wert fehlt.";
                return false;
            }
            try {
                world.default_limit = std::stoi(val);
            } catch (...) {
                error = "SET LIMIT: ungueltiger Wert.";
                return false;
            }
        }
        out.columns = {"limit"};
        out.rows = {{std::to_string(world.default_limit)}};
        return true;
    }
    if (lower.rfind("insert", 0) == 0) {
        if (!db_apply_insert_sql(world, sql, rows, error)) return false;
        out.columns = {"rows_affected"};
        out.rows = {{std::to_string(rows)}};
        return true;
    }
    if (lower.rfind("update", 0) == 0) {
        if (!db_apply_update_sql(world, sql, rows, error)) return false;
        out.columns = {"rows_affected"};
        out.rows = {{std::to_string(rows)}};
        return true;
    }
    if (lower.rfind("delete", 0) == 0) {
        if (!db_apply_delete_sql(world, sql, rows, error)) return false;
        out.columns = {"rows_affected"};
        out.rows = {{std::to_string(rows)}};
        return true;
    }
    return exec_sql_with_outer(world, sql, use_focus, focus_x, focus_y, radius, nullptr, out, error);
}
