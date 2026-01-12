# Micro-Swarm v3.0: Emergent Intelligence & MycoDB

Vom biologischen Experiment zur raeumlichen Datenbank-Engine.

Micro-Swarm v3.0 ist ein High-Performance-System fuer kuenstliches Leben (ALife), das die Bruecke zwischen biologischer Simulation und datengetriebener Analyse schlaegt. Es nutzt lokale Regeln und ein mehrschichtiges Gedaechtnissystem, um globale Strukturen und adaptive Pfade zu erzeugen – ohne klassische neuronale Netze, Backpropagation oder Reinforcement Learning.

---

## Zentrale Features & Innovationen

### Vier-Schichten-Gedaechtnis
- Kurzzeit (Molekuele): fluechtige Reaktionssignale.
- Mittelzeit (Pheromone): Kommunikationskanaele fuer Stigmergie.
- Langzeit (Mycel): strukturelles Gedaechtnis mit logistischem Wachstum und Laplacian-Transport.
- Persistent (DNA): genetischer Pool erfolgreicher Strategien mit Fitness-gewichteter Auswahl.

### MycoDB: Emergent Data Clustering
- Ingestion: SQL-Dumps werden in Payloads uebersetzt und im 2D-Raum geclustert.
- Spatial Query: Fokus + Radius reduzieren globale Suchen auf lokale Nachbarschaft.
- SQL-Light: SELECT/INSERT/UPDATE/DELETE sowie JOINs und CTEs (WITH).
- Delta-Store: Aenderungen landen in Deltas und werden mit merge uebernommen (undo verfuegbar).

### Genetic Ingest Rules (neu in v3.0)
- Regelbasierte Trait-Cluster beim Ingest ueber `--ingest-rules`.
- Default-Instinkt: `.*_id$` als FK-Anziehung (Regex).
- JSON-Overrides fuer gezielte Cluster (trait_cluster, domain_cluster).

### Shell & Workbench (Qualitaet und Produktivitaet)
- Interaktive Shell mit Makros (save/run) und Persistenz (macros save/load).
- Shell-Option `ingest <sql_path> [rules_path]` zum Re-Import in der Session.
- Abfragezeit wird nach jeder Query ausgegeben.
- Workbench mit Auto-Paging, Favoriten, Query-Tabs, Diff/Filter/Export.

### Performance & Reproduzierbarkeit
- OpenCL-Beschleunigung fuer Diffusionsfelder (GPU, mit CPU-Fallback).
- Deterministischer Betrieb ueber Seeds.
- Benchmarking: Morningstar-Report zeigt 356 ms bei lokalem Radius (r=100) in einer 2048er Welt.

---

## Inhalt des Release-Pakets
- micro_swarm.exe (CLI, Simulation, db_shell, db_ingest, db_query)
- micro_swarm.dll (C-API)
- MycoDB Workbench (GUI)
- Dokumentation und Beispielkonfigurationen (z. B. ingest_rules.example.json)
- Beispiel-Myco-Dateien je nach Build/Projekt

---

## Quick Start

### 1) MycoDB Shell
```powershell
.\micro_swarm.exe --mode db_shell --db <database.myco> --db-radius 10
```

Beispiele: `tables`, `goto 1234`, `radius 50`, `sql SELECT ...`

### 2) Ingest mit Genetic Rules
```powershell
.\micro_swarm.exe --mode db_ingest --input data.sql --output data.myco --ingest-rules docs\ingest_rules.example.json --size 2048
```

### 3) Forschungs-Lauf (Baseline)
```powershell
.\micro_swarm.exe --steps 1000 --agents 1024 --gpu 1 --paper-mode --report-html reports\baseline.html
```

---

## Developer & Research Integration (C-API)

Micro-Swarm v3.0 ist als wiederverwendbare Engine konzipiert. Ueber die C-API laesst sich die Logik in Python, Rust, Unity oder Unreal integrieren.

Vorteile der DLL-Integration:
1) Visualisierung: eigene Frontends auf Basis der Mycel-Daten.
2) Daten-Pipelines: emergentes Clustering fuer Feature-Preprocessing.
3) Tooling: Workbench, Headless-Reports, CLI-Workflows.

Minimalbeispiel (Pseudo-C):
```c
void* engine = ms_create_engine(1024, 1024);
ms_spawn_agents(engine, 5000);
ms_step(engine, 100);
const char* json_result = ms_query_db(engine, "SELECT * FROM Tracks WHERE GenreId=1");
ms_destroy_engine(engine);
```

---

## Technische Parameter (Auszug)
- --mycel-growth / --mycel-decay: Steuerung des strukturellen Gedaechtnisses
- --evo-mutation-sigma: Mutationsstaerke der DNA-Ebene
- --species-fracs: Rollen-Mix (z. B. 0.4 0.2 0.2 0.2)
- --ocl-platform / --ocl-device: gezielte GPU-Auswahl
- --ingest-rules: Trait-Cluster beim Ingest

---

Status: Stabil / Forschungs- und Experimentalsystem  
Autor: Ralf Kruemmel  
Technologie: C++17 / MSVC 2022 / OpenCL / CMake >= 3.20
