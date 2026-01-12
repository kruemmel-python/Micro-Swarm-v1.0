# MycoDB SQL-Dialekt Uebersicht

Nur Syntax (keine Beispiele). Case-insensitive fuer Keywords, Tabellen und Spalten.

## SELECT

- `SELECT [DISTINCT] <select_list>`
- `SELECT DISTINCT ON (<cols>) <select_list>`
- `<select_list>`: `*` | `<item>` (`,` `<item>`)*
- `<item>`: `<column>` | `<table>.<column>` | `<function>(...)` | `<aggregate>(...)`
- Alias: `AS <alias>` oder `<item> <alias>`

## FROM

- `FROM <table> [AS <alias>]`
- `FROM (<subquery>) <alias>`

## JOIN

- `[INNER] JOIN <table> [AS <alias>] ON <expr>`
- `LEFT JOIN <table> [AS <alias>] ON <expr>`
- `RIGHT JOIN <table> [AS <alias>] ON <expr>`
- `CROSS JOIN <table> [AS <alias>]`

## WHERE / HAVING (Expressions)

- Logik: `AND`, `OR`, `NOT`
- Vergleich: `=`, `!=`, `<>`, `<`, `<=`, `>`, `>=`
- `BETWEEN <a> AND <b>`
- `IN (<value_list>)` oder `IN (<subquery>)`
- `LIKE <pattern>`
- `REGEXP <pattern>`
- `IS NULL`, `IS NOT NULL`
- `EXISTS (<subquery>)`

## GROUP BY

- `GROUP BY <column> (, <column>)*`

## ORDER BY

- `ORDER BY <column_or_index> [ASC|DESC] [NULLS LAST] (, <column_or_index> [ASC|DESC] [NULLS LAST])*`
- `<column_or_index>`: Spaltenname oder 1-basierter Spaltenindex
- Numerische Sortierung, wenn beide Werte als Zahl parsebar sind

## LIMIT / OFFSET

- `LIMIT <n> [OFFSET <m>]`

## UNION

- `<select> UNION <select>`
- `<select> UNION ALL <select>`

## CTE (WITH)

- `WITH <name> AS (<select>) [, <name> AS (<select>)]* <select>`
- Nicht rekursiv
- Muss im selben Statement direkt von einem SELECT gefolgt werden

## Funktionen

- `LOWER(<expr>)`
- `UPPER(<expr>)`
- `LENGTH(<expr>)`
- `SUBSTRING(<expr>, <start>, <len>)`
- `CONCAT(<expr>, <expr>[, ...])`
- `COALESCE(<expr>, <expr>[, ...])`
- `IFNULL(<expr>, <expr>)`
- `NULLIF(<expr>, <expr>)`
- `CAST(<expr> AS <type>)`
- `TO_INT(<expr>)`
- `TO_FLOAT(<expr>)`

## Aggregate

- `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`

## CASE (einfach)

- `CASE WHEN <expr> THEN <value> [WHEN ...] [ELSE <value>] END`

## Literale

- String: `'text'` oder `"text"`
- Numerisch: `123`, `123.45`, `-7`

## DML (Delta-Store)

- `INSERT INTO <table> [(<col>, ...)] VALUES (<val>, ...)[, (...)]`
- `UPDATE <table> SET <col>=<val>[, ...] WHERE <col>=<val>`
- `DELETE FROM <table> WHERE <col>=<val>`
- Writes landen im Delta-Store; `merge` fuehrt die Batch-Re-Clusterung aus.

## SET

- `SET LIMIT <n|off>` (Default-Limit fuer folgende Queries)

## Nicht unterstuetzt

- DDL (`CREATE`, `ALTER`, `DROP`)
- Transaktionen
- Stored Procedures
