# Micro-Swarm  
**Biologisch inspiriertes, agentenbasiertes Schwarm- und Gedächtnissystem (C++17)**

Micro-Swarm ist ein experimentelles Artificial-Life-System, das untersucht, ob aus **lokalen Regeln** und **mehrschichtigen Gedächtnisformen** globale Struktur, Anpassung und stabile Pfade entstehen können – **ohne klassische neuronale Netze**, ohne Backpropagation und ohne Reinforcement-Learning-Frameworks.

Der Fokus liegt auf:
- Emergenz statt Optimierung
- Kausal nachvollziehbaren Mechanismen
- Trennung von Kurzzeit-, Mittelzeit- und Langzeitgedächtnis

---

## Zentrale Konzepte

### 1. Agenten
Mobile Einheiten mit lokalem Zustand:
- Position, Richtung, Energie
- Genome (Parametervektor)

Agenten handeln ausschließlich lokal und besitzen **keine globale Sicht**.

### 2. Felder (Environment)
Alle Felder sind reguläre 2D-Raster (`GridField`):

- **Ressourcenfeld**  
  Langsam regenerierend, begrenzt (`resource_max`).

- **Pheromonfeld (Food)**  
  Diffusiv, verdampfend. Attraktiver Kommunikationskanal.

- **Pheromonfeld (Danger)**  
  Diffusiv, verdampfend. Repulsiver Kommunikationskanal.

- **Molekülfeld (Kurzzeitgedächtnis)**  
  Stark verdampfend, lokal, schnelle Reaktion auf aktuelle Ereignisse.

### 3. Mycel-Netzwerk (Strukturelles Gedächtnis)
Ein langsames Dichtefeld, das **dauerhafte Aktivität** speichert und stabilisiert.

**Aktuelle Dynamik (wichtig):**
- Normalisierter Aktivitäts-Drive aus Pheromon + Ressourcen
- Aktivitätsschwelle (unterhalb kein Wachstum)
- **Logistisches Wachstum** (kein globales Aufpumpen)
- **Diffusionsartiger Transport (Laplacian)** statt positiver Rückkopplung
- Expliziter Decay

→ verhindert globale Sättigung (`mycel_avg != 1.0` Dauerzustand)

### 4. DNA-Gedächtnis (Langzeit)
- Pool erfolgreicher Genome
- Fitness-gewichtetes Sampling
- Mutationen bei Reproduktion
- Alterung (Fitness-Decay)

DNA speichert **Strategien**, nicht Zustände.

---

## Agentenlogik (lokal, pro Schritt)

1. **Sensorik**
   - Drei Richtungen (links / vorne / rechts)
   - Gewichtung aus:
     - Pheromon Food (attraktiv)
     - Pheromon Danger (repulsiv)
     - Ressourcen
     - Molekülen
     - Exploration-Bias

2. **Bewegung**
   - Stochastische Wahl proportional zu lokalen Gewichten
   - Kein globales Ziel

3. **Interaktion**
   - Ressourcenaufnahme
   - Energiegewinn
   - Pheromon- und Molekül-Emission

4. **Selektion**
   - Hohe Energie → Eintrag ins DNA-Gedächtnis
   - Niedrige Energie → Reinitialisierung aus DNA-Pool

---

## Build & Toolchain (Windows / MSVC)

Das Projekt wird **nativ mit Visual Studio 2022** kompiliert und nutzt **CMake ≥ 3.20**.

### Build-Workflow (bewährt)

```powershell
# 1. Hängenden Prozess hart beenden
Get-Process "micro_swarm" -ErrorAction SilentlyContinue | Stop-Process -Force; `
# 2. Kurz warten (Windows gibt Handles frei)
Start-Sleep -Seconds 2; `
# 3. Rebuild
$CMake="C:\Program Files\CMake\bin\cmake.exe"; `
if(Test-Path build){
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
}; `
& $CMake -S . -B build -G "Visual Studio 17 2022" -A x64; `
& $CMake --build build --config Release -j 8
````

---

## Ausführung

```powershell
.\build\Release\micro_swarm.exe
```

---

## CLI-Parameter (vollständig)

### Basis

```
--mode NAME        sim | db_ingest | db_query | db_shell
--input PATH       SQL-Input fuer db_ingest
--output PATH      MYCO-Output fuer db_ingest
--db-dump PATH     Cluster-PPM-Output fuer db_ingest
--db-dump-scale N  Skalierung fuer PPM-Output (Default 4)
--db PATH          MYCO-Input fuer db_query
--query TEXT       Query fuer db_query
--db-radius N      Radius fuer db_query (Default 5)
--db-merge-agents N    Agentenanzahl fuer Merge (Default 256)
--db-merge-steps N     Schritte fuer Merge (Default 2000)
--db-merge-seed N      Seed fuer Merge (Default 42)
--db-merge-threshold N Auto-Merge ab Delta-Size N (0=aus)
--sql-format F     Output-Format fuer SQL (table|csv|json)
--width N          (Alias: --wight)
--height N         (Alias: --hight)
--size N
--agents N
--steps N
--seed N
```

### Startfelder (CSV)

```
--resources  resources.csv
--pheromone  pheromone.csv
--molecules  molecules.csv
--resource-regen F
```

CSV-Format:

* Zeilen = Rasterzeilen
* Kommagetrennte Floats
* `#` am Zeilenanfang = Kommentar

Hinweis: `--pheromone` initialisiert nur den **Food**-Kanal. Danger startet bei 0.

---

## MycoDB (Data Clustering via Emergence)

Der neue Datenbankmodus nutzt den Schwarm, um Payloads raeumlich zu clustern.

### Ingestion (SQL -> .myco)

```powershell
.\micro_swarm.exe --mode db_ingest --input shop_data.sql --steps 5000 --agents 512 --size 256 --output shop.myco --db-dump shop_clusters.ppm --db-dump-scale 6
```

Unterstuetzt werden `INSERT INTO ... VALUES (...)` und `INSERT INTO ... (columns) VALUES (...)`,
auch ueber mehrere Zeilen und mit SQL-Kommentaren.
Foreign Keys werden ueber Spaltennamen erkannt, die auf `*_id` oder `Id` enden.

### Query (lokale Suche)

```powershell
.\micro_swarm.exe --mode db_query --db shop.myco --query "SELECT * FROM Orders WHERE UserID=5" --db-radius 5
```

Die Query nutzt zuerst die raeumliche Naehe um das Parent-Cluster, faellt sonst auf Feldsuche zurueck.

### Interaktive Shell (db_shell)

```powershell
.\micro_swarm.exe --mode db_shell --db shop.myco --db-radius 5
```

Die Chinook-Referenz definiert den vollständigen MycoDB-SQL-Dialekt.

Write-Pfad (Delta-Store):
- INSERT/UPDATE/DELETE schreiben in den Delta-Store (Merge on read).
- `delta` zeigt ausstehende Writes/Tombstones.
- `merge` re-clustert und schreibt den Delta-Store dauerhaft ein.
- Optional: `--db-merge-threshold N` fuer Auto-Merge ab Delta-Size N.

Beispiele:

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
format csv
```

SQL-Light (Shell):
```
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

`goto` setzt einen Fokuspunkt. Alle folgenden Anfragen nutzen den Fokus als Zentrum fuer den Radius.

---

### Mycel-Tuning / Pheromon-Danger (neu)

```
--mycel-growth F
--mycel-decay F
--mycel-transport F
--mycel-threshold F
--mycel-drive-p F
--mycel-drive-r F
--phero-food-deposit F
--phero-danger-deposit F
--danger-delta-threshold F
--danger-bounce-deposit F
```

Diese Parameter erlauben **Live-Tuning**, ohne Recompile.

---

### Feld-Dumps (Diagnose / Analyse)

```
--dump-every N        # 0 = aus
--dump-dir PATH       # Default: dumps
--dump-prefix NAME    # Default: swarm
[subdir]             # Optional: letzter freier Parameter = Unterordner in dump-dir
```

Beispiel:

```powershell
.\micro_swarm.exe --steps 500 --dump-every 50 --dump-dir dumps --dump-prefix test
```

Erzeugt:

```
dumps/
 ├─ test_step000000_resources.csv
 ├─ test_step000000_phero_food.csv
 ├─ test_step000000_phero_danger.csv
 ├─ test_step000000_molecules.csv
 ├─ test_step000000_mycel.csv
 ├─ test_step000050_...
```

Warum: CSV-Dumps erlauben externe Visualisierung, Debugging und Paper-Auswertung.

---

### GPU / OpenCL (Diffusion auf der GPU)

OpenCL ist optional und faellt bei Problemen automatisch auf CPU zurueck.
GPU-Beschleunigung gilt fuer **Pheromon Food**, **Pheromon Danger** und **Molekuel**.

```
--ocl-enable
--ocl-platform N
--ocl-device N
--ocl-print-devices
--ocl-no-copyback
--gpu N           # Alias fuer OpenCL (0=aus, 1=an)
```

Beispiele:

```powershell
# Geraete auflisten
.\micro_swarm.exe --ocl-print-devices

# GPU-Diffusion aktivieren (Platform 0, Device 0)
.\micro_swarm.exe --steps 200 --agents 512 --ocl-enable --dump-every 50 --dump-dir dumps --dump-prefix gpu --report-html dumps\gpu_report.html

# Alias
.\micro_swarm.exe --steps 200 --agents 512 --gpu 1 --dump-every 50 --dump-dir dumps --dump-prefix gpu --report-html dumps\gpu_report.html
```

Hinweis: Mit `--ocl-no-copyback` werden Host-Daten nur bei Dump-Schritten und am Ende aktualisiert.
Wenn Agenten aktiv sind, wird Copyback erzwungen (Sensorsignale benoetigen aktuelle Felder).

---

### Stress-Test

```
--stress-enable
--stress-at-step N
--stress-block-rect x y w h
--stress-shift-hotspots dx dy
--stress-pheromone-noise F
--stress-seed N
```

---

### Evolution

```
--evo-enable
--evo-elite-frac F
--evo-min-energy-to-store F
--evo-mutation-sigma F
--evo-exploration-delta F
--evo-fitness-window N
--evo-age-decay F
--dna-global-capacity N
--global-spawn-frac F
```

---

### Spezies / Profile

```
--species-fracs f0 f1 f2 f3
--species-profile S exploration food_attraction danger_aversion dep_food dep_danger
```

#### Rollen (Species 0..3)

Die vier Spezies sind feste Rollen mit unterschiedlichen lokalen Praeferenzen. Default-Zuordnung:

| Species | Rollenname   | Prinzip |
|--------:|-------------|---------|
| 0       | Explorator  | Neugier / Informationsgewinn |
| 1       | Integrator  | Verdichtung / Strukturaufbau |
| 2       | Regulator   | Stabilitaet / Kontrolle |
| 3       | Innovator   | Variation / Strategiebruch |

**Explorator**
- Hoher Exploration-Bias
- Geringe Pheromon-Bindung
- Starke Reaktion auf neue Gradienten (Ressourcen + Molekuele)
- Niedriger Deposit-Scale

**Integrator**
- Hohe Gewichtung bestehender Pheromon- und Mycel-Strukturen
- Niedrige Exploration
- Hoher Deposit-Scale fuer Food-Pheromone
- Stabilisiert Pfade (Gedaechtnisbildung)

**Regulator**
- Starke Sensitivitaet auf Danger-Pheromone
- Erhoehte Aversion bei lokalen Ueberdichten
- Negatives Feedback (Counter-Deposits / Abschwaechung)

**Innovator**
- Erhoehte Mutationsraten im Genome
- Schwaechere DNA-Bindung (haeufigere Neuinitialisierung)
- Bevorzugt Randbereiche hoher Entropie

Hinweis: `--species-profile` ueberschreibt pro Spezies nur die Basiswerte
(exploration, food_attraction, danger_aversion, dep_food, dep_danger).
Die rollenspezifischen Zusatzgewichte bleiben aktiv.

---

### Report

```
--report-html PATH
--report-downsample N
--paper-mode
--report-global-norm
--report-hist-bins N
--report-no-sparklines
```

Wenn Dumps aktiv sind, wird automatisch ein **Offline-HTML-Report** erzeugt.
Standardpfad: `<dump-dir>/<dump-prefix>_report.html` (falls nicht gesetzt).

Der Report enthaelt:
* Step-Weise Statistiken (min/max/mean/stddev/p95/entropy)
* Heatmap-Previews (optional Downsample)
* Summary-Sparklines ueber die Zeit
* Scenario-Sektion (z. B. Stress-Parameter)
* Systemmetriken (DNA-Pool-Groessen, Energie je Spezies)

Im Paper-Modus entsteht zusaetzlich:
* `<prefix>_metrics.csv` (metrische Zeitreihen fuer Auswertung/Plotting)

---

## 1) **Baseline / Paper-Run**

```powershell
.\micro_swarm.exe --steps 500 --agents 512 --seed 42 --size 128 --dump-every 50 --dump-dir dumps --dump-prefix baseline --report-html dumps\baseline_report.html --report-downsample 32 --report-hist-bins 64 --paper-mode --report-global-norm --species-fracs 0.40 0.25 0.20 0.15 --global-spawn-frac 0.15 Baseline-Paper_Run
```

**Was das ist:**
Der **Referenzlauf** des Systems ohne externe Störungen und ohne Evolution-Tuning.

**Was er macht:**

* Führt die Simulation deterministisch aus (Seed = 42)
* Schreibt alle 50 Schritte Feld-Dumps (Ressourcen, Pheromone, Moleküle, Mycel)
* Erzeugt einen **wissenschaftlichen HTML-Report**:

  * globale Normalisierung (zeitlich vergleichbare Heatmaps)
  * Entropie, p95, nonzero_ratio
  * Sparklines über die Zeit
  * zusätzlich `baseline_metrics.csv` (Paper-Modus)

**Wofür gedacht:**

* **Ground Truth**
* Vergleichsbasis für Stress- und Evolutionsläufe
* Analyse von Emergenz *ohne* Eingriffe

---

## 2) **Stress-Test / Adaptionslauf**

```powershell
.\micro_swarm.exe --steps 500 --agents 512 --seed 42 --size 128 --dump-every 50 --dump-dir dumps --dump-prefix stress --report-html dumps\stress_report.html --report-downsample 32 --report-hist-bins 64 --stress-enable --stress-at-step 120 --stress-block-rect 40 40 30 30 --stress-pheromone-noise 0.004 --stress-seed 1337 --species-fracs 0.40 0.25 0.20 0.15 --global-spawn-frac 0.15 Stress-Test_Adaptionslauf
```

**Was das ist:**
Ein **Umwelt-Störungstest**, der die Robustheit und Adaptionsfähigkeit des Schwarms prüft.

**Was er macht:**

* Läuft zunächst identisch zur Baseline
* Ab Schritt **120**:

  * blockiert ein Ressourcen-Rechteck (40,40 – 30×30)
  * injiziert kontinuierliches Pheromon-Rauschen
* Alle Effekte werden im Report als *Scenario* dokumentiert

**Wofür gedacht:**

* Prüfen, ob:

  * Mycel-Pfade umlernen
  * Agenten neue Strategien finden
  * das System stabil bleibt (keine Explosion / kein Kollaps)
* **Resilienz-Analyse**

---

## 3) **Evolution / Selektion scharf gestellt**

```powershell
.\micro_swarm.exe --gpu 1 --steps 2000 --agents 512 --seed 42 --size 128 --dump-every 200 --dump-dir dumps --dump-prefix evo_turbo --report-html dumps\evo_turbo_report.html --report-downsample 32 --report-hist-bins 64 --evo-enable --evo-elite-frac 0.20 --evo-min-energy-to-store 1.2 --evo-mutation-sigma 0.10 --evo-exploration-delta 0.10 --evo-fitness-window 50 --evo-age-decay 0.995 --dna-global-capacity 128 --global-spawn-frac 0.15 --species-fracs 0.40 0.25 0.20 0.15 --species-profile 0 1.0 1.2 0.8 1.0 0.7 --species-profile 1 1.1 1.0 1.0 0.9 0.9 --species-profile 2 0.9 1.3 0.7 1.1 0.6 --species-profile 3 1.2 0.9 1.2 0.8 1.1 Turbo-Evolution_Run_gpu_1
```

**Was das ist:**
Ein **selektiver Evolutionslauf**, bei dem DNA nicht mehr „nebenbei“, sondern gezielt entsteht.

**Was er macht:**

* Aktiviert Elite-Selektion (Top 20 %)
* Speichert Genome nur bei *echtem* Fitness-Gewinn
* Fitness basiert auf **Energiezuwachs über Zeitfenster**, nicht auf Momentwerten
* Mutation ist kontrolliert, keine Drift-Explosion
* Report zeigt:

  * DNA-Pool-Dynamik
  * Energieverteilung
  * Entropie-Änderungen

**Wofür gedacht:**

* Beobachtung echter **Strategie-Evolution**
* Nachweis, dass DNA ein **Langzeitgedächtnis für Verhalten** ist
* Trennung von kurzfristigem Erfolg vs. nachhaltiger Fitness

---

## Kurz gesagt

| Run       | Zweck                         | Frage, die er beantwortet       |
| --------- | ----------------------------- | ------------------------------- |
| Baseline  | Referenz / Paper              | „Was passiert ohne Eingriffe?“  |
| Stress    | Robustheit / Adaptation       | „Kann das System umlernen?“     |
| Evolution | Selektion / Gedächtnisbildung | „Entstehen bessere Strategien?“ |

---

## Erwartetes Systemverhalten

* **Mycel** bildet stabile Pfade, aber keine globale Sättigung
* **Pheromone** reagieren schnell, sind flüchtig
* **DNA-Pool** wächst selektiv, nicht explosionsartig
* **Agenten** zeigen:

  * Pfadbildung
  * Lokale Spezialisierung
  * Anpassung bei Umweltänderung

---

## Was Micro-Swarm bewusst nicht ist

* Kein neuronales Netz
* Kein Deep Learning
* Kein Reinforcement-Learning-Framework
* Keine Blackbox-Optimierung

Alle Effekte sind **mechanistisch erklärbar**.

---

## Nächste sinnvolle Experimente

* Ablationstests (Pheromon / Mycel / DNA aus)
* GPU-Mycel-Update als zusaetzlicher Kernel
* Species-Profile Sweeps (systematische Parameterstudien)

---

## Status

**Forschungs- und Experimentalsystem**
Stabil, deterministisch bei festem Seed, vollständig instrumentierbar.

Zusatz: Es existiert eine **C-API/DLL** (`micro_swarm.dll` / `libmicro_swarm.so`)
fuer externe Forschung (Python/Rust/Unity/Unreal).

---

## Warum C++ / warum kein Python
Micro-Swarm ist bewusst in C++ implementiert, um deterministische Laufzeiten, Speicherlokalität und vollständige Kontrolle über Diffusions- und Update-Ordnungen zu garantieren. Die Performance erlaubt Experimente im Bereich mehrerer Tausend Schritte in Sekunden und bildet die Grundlage für echte GPU-Skalierung über OpenCL.

---
**Autor:**
Ralf Krümmel
Artificial Life / Emergent Systems / Low-Level Simulation
