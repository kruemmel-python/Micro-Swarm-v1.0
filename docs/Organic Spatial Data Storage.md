Whitepaper: MycoDB – Organic Spatial Data Storage

Version: 1.0
Date: January 9, 2026
Author: Ralf Krümmel
Engine: Micro-Swarm v1.0

1. Executive Summary

Traditionelle relationale Datenbanken (RDBMS) basieren auf starren mathematischen Strukturen (B-Trees, Hash-Maps), um Datenbeziehungen abzubilden. Dies führt oft zu einer Diskrepanz zwischen logischer Beziehung und physischer Speicherlokalität: Verwandte Daten (z. B. ein Kunde und seine Bestellungen) liegen auf dem Datenträger oft weit auseinander, was rechenintensive JOIN-Operationen erfordert.

MycoDB (Mycelium Database) ist ein experimenteller Ansatz, der Data Clustering durch Emergenz löst. Anstatt Indizes manuell zu definieren, nutzt MycoDB eine Schwarm-Simulation, in der Datenpakete als "Nahrung" betrachtet werden. Agenten transportieren diese Daten physisch durch einen 2D-Raum, geleitet von Pheromon-Spuren, die durch Fremdschlüssel-Beziehungen (Foreign Keys) entstehen.

Das Ergebnis ist eine Datenbank, die sich selbst organisiert, physisch optimiert und über eine C-API nahtlos in Game-Engines (Unity, Unreal) und Anwendungen integriert werden kann.

2. Das Kernkonzept: Biologisches Indexing

MycoDB übersetzt Datenbank-Konzepte in biologische Imperative:

SQL Konzept	Biologisches Äquivalent	Funktion
Row (Datensatz)	Payload (Last)	Ein physisches Objekt im 2D-Raum.
Table	Pheromon-Signatur	Jede Tabelle emittiert eine einzigartige Duftnote.
Foreign Key	Gravitation / Attraktion	Agenten werden von den Pheromonen der Eltern-Datensätze angezogen.
Query (Select)	Lokale Suche	Abfragen basieren auf räumlicher Nähe (Radius), nicht auf Full-Table-Scans.
Index	Cluster & Mycel	Das Mycel-Netzwerk stabilisiert die Pfade zwischen Clustern.
2.1 Der Ingestion-Prozess (Der "Schwarm-Sort")

Beim Importieren eines SQL-Dumps (db_ingest Modus) spawnt die Engine tausende Carrier Agents.

Ein Agent nimmt einen Datensatz auf.

Besitzt der Datensatz einen Foreign Key (z.B. ArtistId), sucht der Agent die Position des referenzierten Datensatzes.

Der Agent transportiert die Daten physisch zu diesem Ziel.

Emergenz: Nach wenigen tausend Simulationsschritten bilden sich organische Cluster. Alle Alben eines Künstlers liegen physisch um den Künstler herum.

3. Spatial SQL: Die Abfragesprache

MycoDB führt eine neue Art der Abfrage ein, die klassisches SQL mit räumlichen Komponenten verbindet. Da die Daten durch den Schwarm bereits vorsortiert ("geclustert") sind, werden JOIN-Operationen zu trivialen Nachbarschaftssuchen.

3.1 Das Kontext-Modell

Anstatt global zu suchen, nutzt MycoDB das Konzept von Fokus und Radius:

GOTO <PayloadID>: Setzt den Lesekopf der Datenbank physisch auf einen Datensatz.

RADIUS <N>: Definiert den Suchradius um den Fokuspunkt.

3.2 SQL-Light Parser

Die Engine enthält einen vollständig in C++ implementierten SQL-Parser, der folgende Features unterstützt:

Projektion: SELECT col1, col2...

Filterung: WHERE col=val AND col2 LIKE 'A%'

Aggregation: GROUP BY, HAVING, COUNT, SUM, AVG, MIN, MAX

Komplexe Relationen: JOIN (Inner, Left, Right, Cross), UNION, UNION ALL

Advanced: Common Table Expressions (WITH), Subqueries (IN, EXISTS).

Beispiel einer räumlichen Abfrage:

code
SQL
download
content_copy
expand_less
-- 1. Finde den Künstler (Globaler Scan oder Index)
Artist Name="AC/DC"
-- > Output: ID=5, x=120, y=45

-- 2. Setze Fokus (Physischer Sprung)
GOTO 5
RADIUS 10

-- 3. Lokale Abfrage (Extrem schnell, da nur im Radius gesucht wird)
SELECT Name FROM Album WHERE ArtistId=5

Da die Agenten die Alben bereits zum Künstler getragen haben, findet diese Query alle Ergebnisse sofort, ohne die gesamte Album-Tabelle zu durchsuchen.

4. Integration & Architektur

MycoDB ist nicht als Standalone-Server konzipiert, sondern als Embedded Engine (Middleware).

4.1 High-Performance Core

Sprache: C++17

Beschleunigung: OpenCL (GPU) für Pheromon-Diffusion und Feld-Updates.

Speicher: Columnar-Stores im RAM, serialisiert als .myco Datei (Spatial Snapshot).

4.2 Die C-API (DLL / Shared Object)

Die gesamte Datenbank-Funktionalität wird über eine stabile C-Schnittstelle (micro_swarm.dll) exportiert. Dies ermöglicht die Nutzung in fast jeder modernen Umgebung.

Wichtige API-Funktionen:

ms_db_create() / ms_db_destroy(): Kontext-Management.

ms_db_load_sql(...): Importiert klassische SQL-Dateien.

ms_db_run_ingest(...): Startet die Schwarm-Sortierung (Headless).

ms_db_query_sql(...): Führt SQL-Light Queries auf dem organischen Cluster aus.

ms_db_save_cluster_ppm(...): Exportiert die Daten-Topologie als Bild.

4.3 Integration in Game Engines
Unity (C#) & Unreal Engine (C++)

Da MycoDB teil der Micro-Swarm Engine ist, kann sie direkt in Spiele integriert werden.

Use Case: Ein Inventar-System, das sich organisch sortiert.

Use Case: NPC-Beziehungsgeflechte, die räumlich abgefragt werden ("Wer sind meine Freunde im Umkreis?").

Implementation: Wrapper-Klassen (MicroSwarmRunner.cs / C++ Loader) sind im SDK enthalten.

Python & Rust

Über ctypes (Python) oder FFI (Rust) kann MycoDB für Data-Science-Experimente genutzt werden, um hochdimensionale Daten organisch zu clustern und zu visualisieren.

5. Fazit

MycoDB beweist, dass Datenbanken nicht starr sein müssen. Durch die Anwendung von Schwarmintelligenz auf Datensätze erreichen wir eine automatische, kontext-sensitive Defragmentierung von Informationen.

Es ist die erste Datenbank, die man nicht nur abfragen, sondern der man beim "Denken" (Sortieren) zusehen kann.

Verfügbarkeit:
Enthalten in Micro-Swarm v1.0.
Lizenz: MIT.

© 2026 Ralf Krümmel - Artificial Life Research