# MycoDB Anleitung (Micro-Swarm)

Diese Anleitung beschreibt den kompletten Workflow: von MySQL Workbench ueber den SQL-Dump bis zur Ingestion, interaktiven Abfrage und Ausgabe in Micro-Swarm (MycoDB).

## 1a) Was MycoDB ist / was es nicht ist

MycoDB ist:
- raeumliches Clustering + lokales Retrieval von relationalen Payloads
- Shell-Navigation und ein SQL-Light Interpreter als Komfort-Layer
- optimiert fuer lokale Abfragen (Radius/Fokus); globale Abfragen sind moeglich, aber konzeptionell der Worst Case.

MycoDB ist nicht:
- keine ACID-DB, keine Transaktionen, keine Konkurrenzkontrolle
- kein Query-Optimizer/Kostenmodell im DB-Sinne
- keine vollstaendige SQL-Compliance
- kein Ersatz fuer Indexstrukturen mit garantierter Worst-Case-Komplexitaet

Diese Abgrenzung ist wichtig, weil "DB" sonst zu klassischen Postgres-Erwartungen fuehrt.

## 1) Voraussetzungen

- MySQL Workbench installiert
- Micro-Swarm gebaut (Release-Binary)
- Genug freier Speicher fuer Dump und .myco Datei

Empfohlene Struktur im Projektordner:

```
./
  build/Release/micro_swarm.exe
  data/
    dump.sql
    shop.myco
    clusters.ppm
```

## 1b) Datenvolumen und Komplexitaet (kurz)

Speicherkosten skalieren grob mit `payload_count`, der Grid-Groesse (`size * size`) und Ergebnis-Buffer groessen (Trefferlisten). `--size` erhoeht die Grid-Flaeche quadratisch (256 -> 512 = 4x Zellen). Lokale Queries skalieren mit Radius und Cluster-Dichte; `--db-radius` steuert die Suchflaeche naeherungsweise quadratisch mit dem Radius (doppelt so grosser Radius ~ 4x Flaeche). Worst Case ist ein globaler Scan (z. B. grosser Radius oder Fokus aus).

## 2) SQL-Dump in MySQL Workbench erstellen

1. MySQL Workbench starten.
2. Verbindung zur Datenbank oeffnen.
3. Menue: Server -> Data Export.
4. Schema auswaehlen.
5. Unter Export Options:
   - Dump Structure and Data aktivieren.
   - Export to Self-Contained File auswaehlen.
   - Include Column Names aktivieren (wichtig fuer saubere Queries).
6. Dateipfad waehlen, z. B. `C:\Users\<name>\Downloads\dump.sql`.
7. Start Export.

Hinweis: Micro-Swarm liest `INSERT INTO ... VALUES (...)` Statements. Mit Spaltenliste sind Queries ueber Spaltennamen moeglich.
Wenn der Dump `CREATE TABLE` enthaelt, werden Spaltennamen automatisch aus dem Schema gelesen.

## 3) SQL-Dump in Micro-Swarm einspielen (db_ingest)

Wechsle in das Release-Verzeichnis:

```powershell
cd C:\Users\<name>\Downloads\superpromt\micro_DB\build\Release
```

Starte die Ingestion:

```powershell
.\micro_swarm.exe --mode db_ingest --input C:\path\to\dump.sql --steps 5000 --agents 512 --size 256 --output shop.myco --db-dump clusters.ppm --db-dump-scale 6
```

Parameter-Erklaerung:
- `--mode db_ingest` aktiviert den Datenbank-Ingestion-Modus
- `--input` Pfad zum SQL-Dump
- `--steps` Anzahl der Schwarm-Schritte (mehr Schritte = staerkeres Clustering)
- `--agents` Anzahl Traeger-Agenten
- `--size` Grid-Groesse (z. B. 256x256)
- `--output` Ziel .myco Datei
- `--db-dump` optionales PPM-Bild der Cluster
- `--db-dump-scale` vergroessert das Bild (z. B. 6 = 6x groesser)

Erwartete Ausgabe:

```
ingest_done payloads=<N> tables=<T>
```

## 3a) Reproduzierbare Runs (golden path)

Empfohlene Konfiguration fuer reproduzierbare Reports:
- fixer SQL-Dump (gleiche Datei, gleiche Reihenfolge)
- feste Parameter: `--seed`, `--size`, `--agents`, `--steps`
- feste Query-Parameter: `--db-radius` bzw. `radius` in der Shell
- falls GPU/OpenCL aktiv ist, koennen numerische Abweichungen auftreten; fuer strikte Replays GPU nicht aktivieren oder toleranzbasiert vergleichen

Beispiel:

```powershell
.\micro_swarm.exe --mode db_ingest --input C:\path\to\dump.sql --steps 5000 --agents 512 --size 256 --seed 42 --output shop.myco
```

## 4) Cluster-Bild ansehen (optional)

Das PPM-Bild zeigt die Tabellen-Cluster. Du kannst es oeffnen mit:

```powershell
start clusters.ppm
```

Oder mit einem Bildbetrachter deiner Wahl.

## 5) Interaktive Abfragen in Micro-Swarm (db_shell)

Starte die Shell:

```powershell
.\micro_swarm.exe --mode db_shell --db shop.myco --db-radius 5
```

Shell-Cheat-Sheet (12 Zeilen):

```
tables
schema <table>
goto <payload_id>
focus
radius <N>
unfocus
delta
merge
merge auto <N>
delta show
undo
history
last
redo
!n
save <name> [cmd]
run <name>
limit <N|off>
show <cols|off>
describe <table>
export <csv|json> <path>
explain
<Table> <PKValue>
<Table> <col>=<val>
show <col1,col2,...>
sql <statement>
sort <col|index> [asc|desc] [num][, <col|index> [asc|desc] [num] ...]
format <table|csv|json>
```

Beispieleingaben:

```
Album 1
Track AlbumId=1
goto 1234
radius 12
tables
stats
schema Track
Track AlbumId=1 show TrackId,Name,Milliseconds
TrackId=13
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1
sql SELECT TrackId,Name FROM Track WHERE TrackId=1
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 AND GenreId=1
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 OR AlbumId=2
sql SELECT TrackId,Name FROM Track WHERE NOT GenreId=1
sql SELECT Name FROM Artist WHERE ArtistId IN (1,2,3,4)
sql SELECT TrackId,Name FROM Track WHERE Milliseconds BETWEEN 200000 AND 300000
sql SELECT Name FROM Artist WHERE Name LIKE 'A%'
sql SELECT Name FROM Artist WHERE Name REGEXP '^A'
sql SELECT Name FROM Artist WHERE Name IS NOT NULL LIMIT 5
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 ORDER BY TrackId
sql SELECT TrackId,Name FROM Track WHERE AlbumId=1 ORDER BY TrackId DESC LIMIT 5
sql SELECT TrackId,Name FROM Track ORDER BY TrackId LIMIT 5 OFFSET 5
sql SELECT TrackId,Name FROM Track ORDER BY 1 LIMIT 5
sql SELECT DISTINCT GenreId FROM Track ORDER BY GenreId LIMIT 10
sql SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING C > 5 ORDER BY C DESC LIMIT 10
sql SELECT GenreId, AVG(Milliseconds) AS AvgMs FROM Track GROUP BY GenreId ORDER BY AvgMs DESC LIMIT 5
sql SELECT AlbumId, SUM(Milliseconds) AS SumMs FROM Track GROUP BY AlbumId ORDER BY SumMs DESC LIMIT 5
sql SELECT AlbumId, MIN(Milliseconds) AS MinMs, MAX(Milliseconds) AS MaxMs FROM Track GROUP BY AlbumId LIMIT 5
sql SELECT t.Name, a.Title FROM Track t JOIN Album a ON t.AlbumId=a.AlbumId WHERE t.TrackId=13
sql SELECT t.Name, a.Title FROM Track t LEFT JOIN Album a ON t.AlbumId=a.AlbumId WHERE a.AlbumId=1
sql SELECT t.Name, a.Title FROM Track t RIGHT JOIN Album a ON t.AlbumId=a.AlbumId WHERE a.AlbumId=1
sql SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5 ORDER BY C DESC LIMIT 5
sql SELECT LOWER(Name) AS n FROM Artist ORDER BY n LIMIT 5
sql SELECT UPPER(Name) AS n FROM Artist ORDER BY n LIMIT 5
sql SELECT LENGTH(Name) AS L FROM Artist ORDER BY L DESC LIMIT 5
sql SELECT SUBSTRING(Name,1,5) AS S FROM Artist ORDER BY S LIMIT 5
sql SELECT CONCAT(FirstName,' ',LastName) AS FullName FROM employee ORDER BY FullName LIMIT 5
sql SELECT Name FROM Artist a WHERE EXISTS (SELECT AlbumId FROM Album WHERE Album.ArtistId=a.ArtistId)
sql SELECT * FROM Track CROSS JOIN MediaType LIMIT 3
sql WITH top_albums AS (SELECT AlbumId FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5) SELECT AlbumId FROM top_albums ORDER BY AlbumId
sql SELECT AlbumId, COUNT(*) AS C FROM Track GROUP BY AlbumId HAVING COUNT(*) > 5 ORDER BY C DESC LIMIT 5
sql SELECT * FROM (SELECT ArtistId, Name FROM Artist) a WHERE a.ArtistId=1
sql SELECT TrackId,Name,Milliseconds FROM Track WHERE AlbumId=1
```

Bedeutung:
- `Album 1` sucht nach Primary Key `AlbumId=1`.
- `Track AlbumId=1` sucht alle Tracks mit Foreign Key auf AlbumId.
- `goto 1234` setzt den Fokus auf Payload ID 1234.
- `radius 12` setzt den Suchradius fuer alle folgenden Anfragen.
- `tables` listet alle Tabellen.
- `schema <table>` zeigt bekannte Spalten.
- `delta` zeigt ausstehende Delta-Writes.
- `merge` fuehrt den Batch-Merge aus.
- `merge auto <N>` setzt Auto-Merge ab Delta-Size N.
- `delta show` zeigt Delta-Details.
- `undo` macht den letzten Delta-Schritt rueckgaengig.
- `history` zeigt die Kommando-Historie.
- `last`/`redo`/`!n` ruft Historie ab.
- `save <name> [cmd]` speichert ein Makro.
- `run <name>` fuehrt ein Makro aus.
- `limit <N|off>` setzt Default-Limit fuer Shell/SQL.
- `show <cols|off>` setzt globalen Show-Filter.
- `describe <table>` zeigt Schema + Beispielzeile.
- `export <csv|json> <path>` exportiert letztes Result.
- `explain` erklaert die letzte Query.
- `show ...` filtert die Ausgabe auf Spalten.
- `<Column>=<Value>` ohne Tabellennamen sucht global.
- `sql <statement>` fuehrt SQL-Light aus (Phase 1).
- `sort <col|index> [asc|desc] [num][, <col|index> [asc|desc] [num] ...]` sortiert das letzte SQL-Result oder das letzte Shell-Result.
- `sort reset` stellt das letzte SQL-Result wieder her.
- `format <table|csv|json>` setzt das SQL-Ausgabeformat.

Solange ein Fokus gesetzt ist, werden alle Suchanfragen lokal um diesen Punkt ausgefuehrt.

## 6) Query-Format (SQL-Light, optional)

Wenn du statt der Shell den Query-Modus nutzen willst:

```powershell
.\micro_swarm.exe --mode db_query --db shop.myco --query "SELECT * FROM Orders WHERE UserID=5" --db-radius 5
```

Optional:

```powershell
.\micro_swarm.exe --mode db_query --db shop.myco --query "SELECT Name FROM Artist ORDER BY ArtistId LIMIT 5" --sql-format csv
```

Formate:

```
SELECT * FROM <Table> WHERE <Column>=<Value>
```

SQL-Light (Phase 1):

```
SELECT <cols> FROM <table> [JOIN ...]
WHERE ... (AND/OR/NOT, IN, BETWEEN, LIKE)
GROUP BY ... HAVING ...
ORDER BY ... LIMIT ... OFFSET ...
DISTINCT, UNION, UNION ALL
DISTINCT ON (...)
RIGHT JOIN, CROSS JOIN
SUBQUERY: IN (SELECT ...), EXISTS (SELECT ...)
REGEXP, WITH (CTE)
IS NULL / IS NOT NULL
ORDER BY Spaltenindex
ORDER BY ... NULLS LAST
Funktionen: LOWER, UPPER, LENGTH, SUBSTRING, CONCAT
Funktionen: COALESCE, IFNULL, NULLIF, CAST, TO_INT, TO_FLOAT
CASE WHEN ... THEN ... ELSE ... END
FROM (SELECT ...) als Subquery
INSERT INTO <table> [(<col>, ...)] VALUES (...)
UPDATE <table> SET <col>=<val>[, ...] WHERE <col>=<val>
DELETE FROM <table> WHERE <col>=<val>
SET LIMIT <n|off>
```

Hinweise:
- Subqueries koennen aeussere Spalten referenzieren (korreliert).
- CTEs sind **nicht rekursiv**.
- Numerische Literale koennen ohne Quotes genutzt werden (z. B. `AlbumId=1`).
- `ORDER BY` sortiert numerisch, wenn beide Werte als Zahl parsebar sind.
- INSERT/UPDATE/DELETE schreiben in den Delta-Store; `merge` fuehrt die Batch-Re-Clusterung aus.
- `SET LIMIT <n|off>` setzt den Default-Limit fuer folgende Queries.

## 7) Fehlerdiagnose

- `hits=0`:
  - Pruefe Tabellennamen und Spaltennamen (Gross-/Kleinschreibung egal).
  - Falls der Dump ohne Spaltenliste ist, nutze `Album 1` (Primary Key) oder `col0`.
- `SQL-Fehler: Keine INSERT-Statements gefunden`:
  - Workbench Export erneut pruefen.
- `MYCO-Fehler`:
  - Dateipfade und Schreibrechte pruefen.

## 8) Tipps fuer gutes Clustering

- Hoehere `--steps` fuer bessere Clusterbildung.
- Groesseres Grid (`--size`) wenn viele Payloads vorhanden sind.
- `--agents` groesser als Anzahl Tabellen * 50 ist ein guter Start.

---

Ende.
