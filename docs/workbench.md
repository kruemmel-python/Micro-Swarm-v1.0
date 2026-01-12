# MycoDB Workbench Benutzerhandbuch (v1.3.x)

Dieses Handbuch erklaert die komplette Workbench-Oberflaeche: jedes Feld, jede Taste, jeder Tab und jeder Inhalt. Lies es einmal komplett durch, dann findest du spaeter schnell die Details wieder.

Hinweis: Umlaute wurden bewusst in ASCII geschrieben (ae/oe/ue), damit die Datei plattformneutral bleibt.

---

## 1) Erste Schritte (Start-Flow)

1) **API laden**
   - Pfad zur `micro_swarm.dll` (Windows) oder `libmicro_swarm.so` (Linux) setzen.
   - Button **API laden** klicken.
   - Statuszeile zeigt **API: ok** und die API-Version.

2) **DB oeffnen**
   - Pfad zu einer `.myco` Datei setzen.
   - Button **DB oeffnen** klicken.
   - Statuszeile zeigt **DB: ok**.
   - Links erscheinen die Tabellen im Navigator.

3) **Query ausfuehren**
   - In das Query-Feld eine Abfrage schreiben.
   - **Run** druecken oder Strg+Enter (je nach OS).
   - Ergebnis erscheint im Tab **SQL Result**.

---

## 2) Gesamtaufbau der Workbench

Die Workbench besteht aus 5 Hauptbereichen:

1) **Navigator (links)**
   - Tabellenliste
   - Schema-Sample

2) **Verbindung (oben)**
   - Pfade fuer API und DB
   - Buttons zum Laden

3) **Statuszeile (oben, unter Verbindung)**
   - API/DB-Status, API-Version
   - Favoriten-Buttons
   - Kurzstatus rechts (Treffer, Dauer, Fehler)

4) **Query-Bereich (mittig oben)**
   - Eingabe fuer SQL/Shell
   - Fokus- und Paging-Controls
   - Run/Cancel/Tools

5) **Ergebnisbereich (mittig unten)**
   - Tabs fuer SQL Result und Payload Debug
   - Kontextmenues fuer Zeilen/Spalten

6) **Tools (ganz unten)**
   - Queries, Diff, Filter, Export, Auto

7) **Log (unten, einklappbar)**
   - Aktuelle Session-Metadaten

---

## 3) Bereich "Verbindung"

### 3.1 API-Pfad
**Feld:** `Pfad zur micro_swarm.dll / libmicro_swarm.so`
- Hier wird die Workbench-API geladen.
- Windows: `...\build\Release\micro_swarm.dll`
- Linux: `...\build\libmicro_swarm.so`

### 3.2 DB-Pfad
**Feld:** `Pfad zur .myco Datei`
- Die Datenbank-Datei im Myco-Format.
- Beispiel: `chinook_optimized.myco`

### 3.3 API laden
**Button:** `API laden`
- Laedt die DLL/SO und initialisiert die API.
- Nach Erfolg: Badge **API: ok** und API-Version erscheint.

### 3.4 DB oeffnen
**Button:** `DB oeffnen`
- Laedt die `.myco`.
- Nach Erfolg: Badge **DB: ok**.
- Tabellenliste wird geladen.

---

## 4) Statuszeile (API/DB/Version/Favoriten/Status)

### 4.1 API-Status
**Badge:** `API: ok` oder `API: aus`
- Gruen = API geladen
- Rot = nicht geladen

### 4.2 DB-Status
**Badge:** `DB: ok` oder `DB: aus`
- Gruen = DB geoeffnet
- Rot = keine DB

### 4.3 API-Version
**Text:** `API vX.Y.Z`
- Zeigt die Version der geladenen API.

### 4.4 Favoritenleiste
**Button:** `Fav+`
- Speichert die aktuelle Query in den ersten freien Favoriten-Slot.

**Buttons:** `Fav` (6 Stueck)
- Fuehrt die gespeicherte Query aus.
- Ein Button ist aktiv, sobald er belegt ist.
- Label zeigt die gekuerzte Query.

### 4.5 Status-Text (rechts)
Beispiele:
- `Treffer: 50 | Dauer: 3 ms`
- `Query laeuft...`
- `Shell-SQL (csv) ausgefuehrt.`

---

## 5) Navigator (links)

### 5.1 Tabellenliste
Zeigt alle Tabellen der DB.

**Aktionen:**
- **Einfachklick**: laedt Schema-Sample der Tabelle.
- **Doppelklick**: fuellt Query mit `sql SELECT * FROM <Table> LIMIT 50`.

### 5.2 Schema (Sample)
Zeigt:
- **Spalten (Sample)**: Spaltennamen aus einer Beispielzeile.
- **Sample-Daten** falls keine Spalten erkannt wurden.

Hinweis: Die Spalteninfos stammen aus echten Beispieldaten (nicht aus einem separaten Metaschema).

---

## 6) Query-Editor

### 6.1 Eingabefeld
Mehrzeiliges Textfeld fuer:
- SQL
- Shell-Kommandos
- Makros

Beispiele:
- `sql SELECT * FROM artist LIMIT 50`
- `SELECT * FROM album`
- `tables`
- `describe track`

### 6.2 History (Shortcut)
**CTRL + Pfeil hoch/runter**
- Blaettert durch Query-Historie.

---

## 7) Query-Controls (Buttons + Felder)

### 7.1 Fokus-Felder
**X / Y / R**
- Fokus-Koordinaten (x,y) und Radius im Raum.
- Werden fuer fokusbasierte Abfragen verwendet.

### 7.2 payload_id
**Feld:** `payload_id`
- Eingabe fuer eine konkrete Payload-ID.
- Mit **Set Focus** wird der Fokus auf diese Payload gesetzt.

### 7.3 Query-Mode
**Dropdown:** `Auto | SQL | Shell`
- **Auto**: Workbench entscheidet je nach Query.
- **SQL**: erzwingt SQL-Interpretation.
- **Shell**: behandelt Input als Shell-Kommando.

### 7.4 Paging
**Page / Prev / Next / Page Size**
- Paging wird **automatisch** aktiviert, wenn es sich um SQL handelt **und** kein LIMIT/OFFSET im SQL steht.
- **Page** zeigt die aktuelle Seite.
- **Prev/Next** wechselt zwischen Seiten.
- **Page Size** bestimmt die Groesse.

Hinweise:
- SQL mit `LIMIT/OFFSET` deaktiviert das Auto-Paging.
- Der Workbench-Log zeigt "Exec Query" (die intern paginierte SQL).

### 7.5 Undo
**Button:** `Undo`
- Macht das letzte Delta rueckgaengig (API: `undo_last_delta`).
- Danach wird die letzte Query automatisch neu ausgefuehrt.

### 7.6 Merge
**Button:** `Merge`
- Fuehrt Delta-Merge + Re-Clustering aus.
- Zeigt eine Sicherheitsabfrage.
- Danach wird die letzte Query neu ausgefuehrt.

### 7.7 Fokus-Buttons
**Set Focus**
- Setzt Fokus auf `payload_id`.

**Clear Focus**
- Setzt Fokus zurueck auf (0,0) und deaktiviert Fokus.

**Use Selection**
- Setzt Fokus auf die in der Tabelle markierte Zeile (payload_id).

### 7.8 Run / Cancel
**Run**
- Fuehrt die aktuelle Query aus.

**Cancel**
- Bricht eine laufende Query ab (sofortige Abbruch-Anforderung).

### 7.9 Table Count
**Button:** `Table Count`
- Zeigt die Anzahl der Tabellen (API-Aufruf).

### 7.10 Export CSV / Export JSON
Diese Buttons exportieren direkt den aktuell aktiven Result-Tab.
Sie sind getrennt vom Export-Tab (Tools).

### 7.11 Copy
Kopiert die aktuelle Selektion in die Zwischenablage.
- Zeilen: tab-separiert
- Zellen: passend zur Auswahl

---

## 8) Ergebnis-Tabs

### 8.1 SQL Result
Standard-Tab fuer SQL-Ausgaben.
- Spalten basieren auf SQL-Result.
- Paging greift auf diesen Tab.

### 8.2 Payload Debug
Zeigt Debug-Daten aus dem Payload-Store:
- payload_id
- table_id
- id
- x / y
- field_count
- fk_count
- table_name
- raw_data

---

## 9) Kontextmenues (Rechtsklick)

### 9.1 Zellen / Zeilen (Grid)
Rechtsklick auf eine Zelle oeffnet Funktionen:

- **Edit Field**: Zellenwert per `UPDATE` aendern.
- **Set NULL**: Zelle auf NULL setzen.
- **Delete Row(s)**: Loescht markierte Zeilen.
- **Paste Row**: Fuegt eine Zeile aus der Zwischenablage ein.

Kopier-Optionen:
- **Copy Row**
- **Copy Row (with names)**
- **Copy Row (unquoted)**
- **Copy Row (with names, unquoted)**
- **Copy Row (tab separated)**
- **Copy Row (with names, tab separated)**
- **Copy Field**
- **Copy Field (unquoted)**

Wichtig:
- Editing ist nur bei **single-table SQL** erlaubt.
- JOINs sind editiergeschuetzt (kein Update/Delete).

### 9.2 Spalten-Header (Label)
Rechtsklick auf Header bietet:
- **Copy Field Name**
- **Copy All Field Names**
- **Reset Sorting**
- **Reset Column Widths**

---

## 10) Tools-Tabs (unten)

### 10.1 Queries
Speichert einen Snapshot jeder Query:
- Query-Text
- Ergebnis-Grid
- Maximal 10 Tabs (aelteste fliegen raus)

### 10.2 Diff
Zeigt Delta-Infos:
- Anzahl Delta-Entries
- Anzahl Tombstones
- Liste der ersten 50 Eintraege (Delta + Tombstones)

Nutzen:
Sehr hilfreich, um sofort zu sehen, welche Aenderungen sich im Delta-Store befinden.

### 10.3 Filter
Client-seitiger Filter fuer das aktive Grid.

**Filter-Input**
- Text, der im Result gesucht wird.

**Filter-Spalte**
- Auswahl einer Spalte oder **All**.

**Clear**
- Entfernt den Filter und stellt das volle Ergebnis her.

### 10.4 Export
Exportiert gezielt aus SQL Result oder Payload Debug.

**Target**
- SQL Result oder Payload Debug.

**Format**
- csv oder json.

**Spaltenliste**
- Checkboxen fuer Spaltenauswahl.
- Wenn nichts markiert ist, werden alle exportiert.

**Export...**
- Oeffnet Dateidialog, schreibt die Datei.

### 10.5 Auto
Automatische Infos nach jeder Query.

**Auto Explain**
- Zeigt Query, Exec Query (Paging), Fokus und Timing.

**Auto Stats**
- Zaehlt alle Tabellen via `SELECT COUNT(*)` und zeigt die Ergebnisse.

Hinweis: Auto Stats kann bei sehr grossen DBs langsamer sein.

---

## 11) Log (unten)

Das Log fasst den Session-Status zusammen:
- API/DB Status
- API Version
- Fokus (x,y,r)
- Limit/Show/Format
- Shell-Pfad
- Last Query + Exec Query
- Hits / Duration
- Last Error

Das Log ist die erste Anlaufstelle fuer Debugging.

---

## 12) SQL, Shell und Makros (Kurzreferenz)

### 12.1 SQL
Akzeptierte SQL-Formen:
- `sql SELECT ...`
- `SELECT ...` (Auto-Modus erkennt SQL)
- `INSERT/UPDATE/DELETE`

### 12.2 Shell-Kommandos
Beispiele:
- `tables`
- `describe track`
- `schema artist`
- `stats`
- `delta`
- `merge`
- `undo`
- `format table|csv|json`

### 12.3 Show/Limit/Sort
Im Shell-Stil:
- `show Col1,Col2`
- `limit 50`
- `sort col asc`
- `sort reset`

---

## 13) Typische Fehler und Loesungen

### 13.1 "API: aus"
API wurde nicht geladen.
Loesung: DLL/SO-Pfad korrekt setzen und **API laden**.

### 13.2 "DB: aus"
DB wurde nicht geoeffnet.
Loesung: `.myco` Pfad setzen und **DB oeffnen**.

### 13.3 "Query ungueltig"
SQL Syntax oder Shell-Befehl fehlerhaft.
Loesung: SQL prüfen oder mit `sql` prefix starten.

### 13.4 Keine Treffer
Eventuell Fokus zu klein oder SQL-Filter zu strikt.
Loesung: Fokus vergroessern (R) oder Filter pruefen.

---

## 14) Tipps fuer fluessiges Arbeiten

1) Nutze **Auto-Paging** fuer grosse Joins.
2) Nutze **Filter** fuer schnelle UI-Suche.
3) Speichere wiederkehrende Abfragen als Favoriten.
4) Pruefe **Diff**, bevor du Merge machst.
5) Nutze **Query Tabs** als Verlauf mit Snapshot.

---

## 15) Mini-FAQ

**Q: Warum zeigt SQL Result eine andere Spaltenreihenfolge?**  
A: Die Reihenfolge kommt aus dem SQL-Result. Payload Debug hat feste Felder.

**Q: Wieso sind JOIN-Resultate nicht editierbar?**  
A: Updates/Deletes sind nur sicher auf single-table SQL moeglich.

**Q: Ich sehe alle Zeilen statt Paging.**  
A: Wenn du selbst `LIMIT` oder `OFFSET` angibst, wird Auto-Paging deaktiviert.

---

## 16) Kontakt / Erweiterungen

Wenn du neue Funktionen willst (Custom Toolbar, Visual Query Builder, Charting, etc.), sag Bescheid.

