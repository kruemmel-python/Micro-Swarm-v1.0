
# 🐚 MycoDB Shell: Das Kommando-Referenzhandbuch

Dieses Dokument beschreibt die Interaktionsmöglichkeiten mit der **micro_swarm Engine** im `db_shell`-Modus.

## 📍 1. Räumliche Navigation (Spatial Control)
*Diese Befehle steuern den "physischen Fokus" innerhalb des Daten-Gitters.*

| Befehl | Beschreibung |
| :--- | :--- |
| **`goto <id>`** | Spricht den "Lesekopf" direkt auf die (x,y) Position der angegebenen Payload-ID an. |
| **`radius <n>`** | Definiert den Wirkungskreis für nachfolgende Abfragen. Ein kleinerer Radius bedeutet extreme Geschwindigkeit. |
| **`focus`** | Zeigt die aktuellen Koordinaten und den gesetzten Radius an. |
| **`unfocus`** | Entfernt den räumlichen Filter und kehrt zum globalen Scan zurück. |

## 📊 2. Inspektion & Metadaten
*Schnelle Einblicke in die Struktur der geladenen Welt.*

| Befehl | Beschreibung |
| :--- | :--- |
| **`tables`** | Listet alle Tabellen auf, die im `.myco` Abbild vorhanden sind. |
| **`stats`** | Zeigt die Anzahl der Datensätze (Payloads) pro Tabelle an (Sofort-Abruf). |
| **`schema <table>`** | Listet alle Spaltennamen der angegebenen Tabelle auf. |
| **`describe <table>`** | Kombiniert das Schema mit einem physischen Beispiel-Datensatz aus dem Gitter. |

## 🔍 3. Abfragen & Suche
*Vom intuitiven Schnellzugriff bis hin zum komplexen Standard-SQL.*

| Befehl | Beschreibung |
| :--- | :--- |
| **`<Table> <ID>`** | **PK-Shortcut:** Findet den Datensatz in Tabelle X mit der Primärschlüssel-ID Y. |
| **`<Table> <Col>=<Val>`** | **Column-Shortcut:** Findet alle Datensätze, die in einer bestimmten Spalte den Wert haben. |
| **`<Col>=<Val>`** | **Global-Shortcut:** Durchsucht alle Tabellen nach diesem Spaltenwert. |
| **`sql <statement>`** | Führt echtes SQL aus (`SELECT`, `INSERT`, `UPDATE`, `DELETE`). |
| **`explain`** | Analysiert die letzte Query: War sie lokal oder global? Wie viele Hits gab es? |

## 💾 4. Delta-Store & Transaktionen
*Änderungen sicher verwalten, ohne den Hauptspeicher sofort zu korrumpieren.*

| Befehl | Beschreibung |
| :--- | :--- |
| **`delta`** | Statusbericht: Wie viele Änderungen (Upserts/Tombstones) liegen im Wartebereich? |
| **`delta show`** | Listet alle ausstehenden Schreiboperationen im Detail auf. |
| **`merge`** | **Commit:** Schreibt das Delta permanent in das Gitter und löst ein Re-Clustering aus. |
| **`merge auto <n>`** | Aktiviert das automatische Mergen, sobald `n` Änderungen erreicht sind. |
| **`undo`** | Macht die letzte Änderung im Delta-Store rückgängig. |

## ⚙️ 5. Konfiguration & Ausgabe
*Steuerung der Darstellung und Verarbeitungsmenge.*

| Befehl | Beschreibung |
| :--- | :--- |
| **`limit <n\|off>`** | Begrenzt die Anzahl der angezeigten Zeilen (schont das Terminal). |
| **`show <cols\|off>`** | Setzt einen dauerhaften Spaltenfilter für alle kommenden Abfragen. |
| **`format <table\|csv\|json>`**| Wechselt den Ausgabestil (Tabelle für Menschen, CSV/JSON für Maschinen). |
| **`export <csv\|json> <path>`**| Speichert das letzte Abfrageergebnis direkt in eine Datei. |
| **`cls \| clear`** | Leert das Konsolenfenster. |

## 📥 6. Ingest & Import
*Lädt einen SQL-Dump und ersetzt die aktuelle Session.*

| Befehl | Beschreibung |
| :--- | :--- |
| **`ingest <sql_path> [rules_path]`** | Führt einen Ingest aus einer SQL-Datei aus. Optional mit `ingest_rules.json`. |

## 📝 7. Produktivität & Makros
*Automatisierung wiederkehrender Aufgaben.*

| Befehl | Beschreibung |
| :--- | :--- |
| **`history`** | Zeigt die Liste der zuletzt eingegebenen Befehle. |
| **`last \| redo \| !n`** | Wiederholt die letzte Query oder einen spezifischen Befehl aus der Historie. |
| **`save <name> [cmd]`** | Speichert den aktuellen oder einen angegebenen Befehl als Makro. |
| **`run <name>`** | Führt ein gespeichertes Makro aus. |
| **`macros save [path]`** | Exportiert alle Makros in eine JSON-Datei. |
| **`macros load <path>`** | Lädt Makros aus einer JSON-Datei nach. |

---

## ⚗️ 8. Genetic Ingest Rules (nur `db_ingest`)
*Regelbasierte Cluster-Bildung beim Ingest. Die Regeln wirken nur während des Ingest-Prozesses.*

CLI-Option:
```
--ingest-rules <path>
```

Beispiel:
```
.\micro_swarm.exe --mode db_ingest --input data.sql --output data.myco --ingest-rules docs/ingest_rules.example.json
```

Regeltypen:
- `foreign_key` (zielt auf FK-Cluster, Default: `.*_id$`)
- `trait_cluster` (clustert gleiche Werte)
- `domain_cluster` (clustert Werte nach der Domain hinter `@`)

Beispiel-JSON: `docs/ingest_rules.example.json`


