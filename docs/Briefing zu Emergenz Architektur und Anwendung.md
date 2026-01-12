Micro-Swarm: Briefing zu Emergenz, Architektur und Anwendung

Executive Summary

Dieses Briefing-Dokument fasst die Kernkonzepte, experimentellen Ergebnisse und die technische Architektur des Micro-Swarm-Systems zusammen, eines biologisch inspirierten Artificial-Life-Frameworks. Das System demonstriert, wie komplexe, adaptive und evolvierende Verhaltensweisen aus dem Zusammenspiel einfacher lokaler Regeln und eines mehrschichtigen Gedächtnissystems entstehen können, ohne auf neuronale Netze, Backpropagation oder Reinforcement Learning zurückzugreifen.

Die zentralen Thesen sind:

1. Emergenz aus lokalen Regeln: Globale, intelligente Koordination entsteht ohne zentrale Steuerung. Tausende von Agenten agieren ausschließlich auf Basis ihrer direkten, lokalen Umgebung, was zur Selbstorganisation von effizienten Versorgungsnetzwerken führt.
2. Mehrschichtiges Gedächtnis: Intelligenz ist eine Funktion von interagierenden Gedächtnisschichten, die auf unterschiedlichen Zeitskalen operieren – von flüchtigen Molekülen (Sekunden) über Pheromone (Minuten) und Mycel-Strukturen (Stunden) bis hin zu persistenter DNA (Generationen).
3. Nachvollziehbare Kausalität („Glass Box“): Im Gegensatz zu Black-Box-KI-Modellen sind in Micro-Swarm alle Zustände und Dynamiken messbar, nachvollziehbar und mechanistisch erklärbar. Es gibt keine versteckten Schichten.
4. Robuste technische Implementierung: Das System basiert auf einem performanten C++17-Kern und ist über eine stabile, portable C-API (DLL/Shared Object) in diverse Umgebungen wie Python, Rust, Unity und Unreal Engine einbettbar. Dies ermöglicht wiederholbare wissenschaftliche Experimente und die Integration in externe Anwendungen.
5. MycoDB – Organische Datenbanken: Als innovative Anwendung nutzt MycoDB die Schwarmintelligenz, um relationale Daten räumlich zu clustern. Anstatt starrer B-Tree-Indizes werden Daten-Payloads von Agenten physisch sortiert, was JOIN-Operationen zu effizienten Nachbarschaftssuchen macht.

Experimente belegen die Fähigkeit des Systems zur (1) Ordnung durch Selbstorganisation, (2) Anpassung an plötzliche Störungen und (3) Optimierung durch einen evolutionären Selektionsprozess, der die Effizienz des Schwarms dramatisch steigert.

1. Kernkonzepte des Micro-Swarm-Systems

Micro-Swarm ist ein Experiment über Emergenz, das von Autor Ralf Krümmel im Forschungsfeld Artificial Life und Emergente Systeme entwickelt wurde. Der Fokus liegt auf der Untersuchung, wie globale Strukturen aus lokalen Interaktionen entstehen, anstatt ein globales Ziel zu optimieren.

1.1 Grundprinzipien: Jenseits von Backpropagation

Das System verzichtet bewusst auf konventionelle KI-Werkzeuge, um die fundamentalen Mechanismen der Selbstorganisation zu isolieren:

* Keine Neuronalen Netze: Die Agentenlogik basiert nicht auf neuronalen Architekturen.
* Keine Gradientenabstiege: Es findet keine Backpropagation oder ähnliche Optimierung statt.
* Keine expliziten Optimierungsziele: Das System hat kein vordefiniertes Ziel (z. B. via Reinforcement Learning), das es maximieren soll.
* Keine zentrale Steuerung: Es gibt keine globale Sicht oder Koordinationseinheit. Jeder Agent agiert autonom.

Das Ergebnis ist ein „Glass Box“-System, in dem jeder Effekt kausal und mechanistisch erklärbar ist. Jeder Pfad im Mycel ist die integrierte, messbare Geschichte tausender lokaler Entscheidungen.

1.2 Die Bausteine des Systems

Das System besteht aus zwei fundamentalen Komponenten: Agenten und einer dynamischen Umwelt.

Komponente	Beschreibung
Agenten	Mobile Einheiten mit einem internen Zustand (Position, Energie, Genom). Sie handeln ausschließlich auf Basis lokaler Sensorik (links, vorne, rechts) und besitzen weder globale Sicht noch ein zentrales Ziel.
Felder (GridFields)	Eine 2D-Gitter-Umgebung, die verschiedene dynamische Schichten enthält. Ressourcen regenerieren langsam und werden von Agenten konsumiert. Weitere Umwelt-Layer (Pheromone, Moleküle) dienen als Kommunikationsmedium.

Kernprinzip: Globale Ordnung entsteht, obwohl jeder Agent nur seine unmittelbare Nachbarschaft "sieht".

1.3 Ein Gedächtnis mit vier Zeithorizonten

Emergenz erfordert die Überlagerung von Informationsspuren mit unterschiedlicher Lebensdauer. Micro-Swarm modelliert dies durch vier interagierende Gedächtnisschichten.

Schicht	Zeitskala	Funktion & Eigenschaften	Analogie
Moleküle	Sekunden (Kurzlebig)	Reaktion: Dienen als unmittelbares "Recency"-Signal ("ein Agent war gerade hier"). Sie sind stark verdampfend, sehr lokal und verhindern, dass Agenten sofort umkehren.	Flüchtige chemische Signale
Pheromone	Minuten (Mittel-lebig)	Kommunikation: Ermöglichen stigmergische Kommunikation und Pfadmarkierung. Sie sind diffusiv und verdampfen langsam. Es gibt zwei Kanäle: phero_food (attraktiv) und phero_danger (repulsiv).	Duftspuren von Ameisen
Mycel	Stunden (Langlebig)	Struktur: Stabilisiert und verstärkt Pfade, die anhaltende Agenten-Aktivität aufweisen. Es wächst langsam und verfällt nur sehr langsam, was es zum bleibenden Gedächtnis kollektiver Erfahrung macht.	Ein Pilzgeflecht, das Nährstoffautobahnen bildet
DNA	Generationen (Persistent)	Strategie: Speichert erfolgreiche Verhaltensstrategien (Parametervektoren) in einem globalen Gen-Pool. Agenten mit hoher Energie tragen ihre Genome in den Pool ein; Agenten mit niedriger Energie werden mit einem Genom aus dem Pool reinitialisiert.	Evolutionärer Speicher

1.4 Arbeitsteilung durch Spezialisierung (Spezies)

Der Schwarm ist nicht monolithisch, sondern besteht aus vier Spezies, die komplementäre Intelligenzfunktionen erfüllen.

Spezies	Rolle	Eigenschaften & Verhalten
Spezies 0	Explorator (Neugier)	Hoher Explorations-Bias, geringe Pheromon-Bindung. Sucht aktiv neue Gradienten und fördert die Informationsgewinnung.
Spezies 1	Integrator (Strukturaufbau)	Hohe Gewichtung bestehender Pfade (Pheromon/Mycel). Stabilisiert und verstärkt bewährte Routen, fördert die Gedächtnisbildung.
Spezies 2	Regulator (Stabilität)	Hohe Sensitivität für "Gefahr"-Pheromone, meidet Überdichte. Sorgt für negatives Feedback und verhindert Überlastung.
Spezies 3	Innovator (Variation)	Erhöhte Mutationsrate, sucht gezielt Regionen hoher Entropie auf. Testet neue Strategien und bricht bestehende Muster auf.

1.5 Der Kreislauf der Emergenz

Lokales Handeln und globale Strukturbildung sind in einem kontinuierlichen Feedback-Kreislauf verbunden:

1. Agenten nehmen Felder wahr: Sie lesen lokale Gradienten aus den Gedächtnisschichten.
2. Sie handeln lokal: Basierend auf den Sinneseindrücken treffen sie eine stochastische Entscheidung.
3. Ihre Handlungen verändern die Felder: Sie emittieren Pheromone und Moleküle.
4. Anhaltende Aktivität formt das Mycel: Erfolgreiche Pfade werden im strukturellen Gedächtnis verankert.
5. Erfolgreiche Strategien werden in der DNA gespeichert: Die besten Verhaltensparameter überleben.
6. Die veränderte Umwelt und neue DNA beeinflussen zukünftige Agenten: Der Kreislauf beginnt von vorn.

2. Experimentelle Validierung und Ergebnisse

Drei Schlüsselexperimente demonstrieren die Fähigkeiten des Systems: der Baseline-Run, der Stress-Test und der Evolutionslauf.

2.1 Baseline: Vom Chaos zur Struktur

Dieser Referenzlauf zeigt die Selbstorganisation des Systems ohne äußere Eingriffe. Aus einer zufälligen Verteilung formt der Schwarm ein effizientes Versorgungsnetzwerk.

* Start (Step 0): Alle Gedächtnisfelder sind leer (mycel mean = 0.0000).
* Entwicklung: Agenten hinterlassen Pheromonspuren, die sich zu Pfaden verdichten. Diese Pfade werden langsam vom Mycel "fossilisiert".
* Ende (Step 450): Ein stabiles Netzwerk aus "Autobahnen" hat sich gebildet. Der mycel mean Wert erreicht 0.0831 und der nonzero_ratio steigt auf 1.0. Der resources mean Wert wächst auf 0.1992.

Detaillierte Metriken des Baseline-Runs

Die folgende Tabelle zeigt die Entwicklung zentraler Metriken für die Felder mycel und resources an ausgewählten Schritten des Baseline-Runs.

Schritt	Feld	Mean	Non-Zero Ratio	Norm. Entropy
0	resources	0.0148	0.0197	0.0395
0	mycel	0.0000	0.0000	0.0000
50	resources	0.0568	1.0000	0.4051
50	mycel	0.0043	0.5417	0.4060
200	resources	0.1206	1.0000	0.6727
200	mycel	0.0167	0.9958	0.7276
450	resources	0.1992	1.0000	0.7964
450	mycel	0.0831	1.0000	0.8772

2.2 Stress-Test: Resilienz durch Umlernen

Dieses Experiment prüft die Anpassungsfähigkeit des Systems. Ab Schritt 120 wird eine massive Störung eingeführt: ein zentrales 30x30 Ressourcenfeld wird blockiert und konstantes Pheromon-Rauschen (pheromone_noise=0.004) wird injiziert.

* Reaktion: Etablierte Pfade werden nutzlos. Der phero_danger Mittelwert steigt signifikant an, von 0.0002 (t=100) auf 0.0457 (t=150).
* Anpassung: Das System kollabiert nicht. Stattdessen gibt der Schwarm bestehende Pfade auf und findet neue, effiziente Routen um das Hindernis herum.
* Ergebnis: Das strukturelle Gedächtnis (Mycel) beweist seine Plastizität. Trotz der Störung wächst der mycel mean Wert weiter und erreicht bei t=150 einen Wert von 0.0134, verglichen mit 0.0114 im Baseline-Run zur gleichen Zeit. Das System ist nicht nur stabil, sondern adaptiv.

2.3 Evolution: Bessere Strategien durch Selektion

Dieser "Turbo-Evolution"-Lauf aktiviert eine gezielte Elite-Selektion (--evo-enable), bei der nur Genome, die den Fitness-Gewinn (Energiezuwachs über Zeit) nachweislich steigern, in den DNA-Pool aufgenommen werden.

* Effekt: Durch die gezielte Selektion verfeinert der Schwarm seine Sammelstrategien und steigert seine Effizienz dramatisch. Der Pool erfolgreicher Genome wächst und stabilisiert sich.
* Vergleich der Endwerte: Die Evolution übertrifft die Baseline-Performance deutlich.

Metrik	Baseline (450 Steps)	Evolution (1800 Steps)
mycel mean	0.0831	0.5126
resources mean	0.1992	0.5473

Diese Ergebnisse zeigen, dass die DNA als Langzeitgedächtnis für Verhalten fungiert und das System befähigt, sich selbst zu optimieren.

3. Technische Architektur und Integration

Die gesamte Simulations-Engine ist in einer performanten, deterministischen C++17-Bibliothek gekapselt und über eine stabile C-API zugänglich.

3.1 Kern-Engine

* Sprache: C++17 für Performance, Speicherlokalität und deterministische Laufzeiten.
* Performance: Tausende von Schritten pro Sekunde.
* GPU-Beschleunigung: Optionale Beschleunigung der Diffusionsberechnungen (Pheromone, Moleküle) via OpenCL. Das System fällt bei fehlender OpenCL-Runtime automatisch auf die CPU zurück.
* Headless Pipeline: Eine Kommandozeilenschnittstelle ermöglicht wiederholbare Batch-Läufe für systematische Experimente und automatisierte Reporterstellung. Reports werden als Offline-HTML-Dateien generiert und enthalten detaillierte Statistiken, Heatmaps und Zeitreihen-Visualisierungen.

3.2 Stabile C-API (micro_swarm_api.h)

Die API ist das Herzstück für die Integration und wurde mit Fokus auf Stabilität, Portabilität und Einfachheit entworfen.

* Plattformen: Verfügbar als micro_swarm.dll (Windows) und libmicro_swarm.so (Linux).
* Versioning: Nutzt Semantische Versionierung (MAJOR.MINOR.PATCH).
  * MAJOR: Bei inkompatiblen ABI- oder Verhaltensänderungen.
  * MINOR: Für abwärtskompatible Erweiterungen (neue Funktionen, neue Struct-Felder).
  * PATCH: Für Bugfixes ohne API-Änderungen.
* ABI-Stabilität:
  * Calling Convention: __cdecl auf Windows.
  * Structs: Alle Structs sind POD (Plain Old Data) und repr(C)-kompatibel, ohne spezielle Packing-Pragmas (Default-Alignment).
  * Struct-Evolution: Neue Felder werden immer am Ende von Structs angehängt. Bestehende Felder werden niemals entfernt oder umsortiert.
* Lebenszyklus: Die API ist Handle-basiert (ms_create() / ms_destroy()). Der Aufrufer behält die volle Kontrolle.
* Synchronität: Alle API-Funktionen sind synchron und blockierend. Es gibt kein internes Threading. Ein API-Kontext ist nicht thread-sicher.
* Fehlerbehandlung: int-Rückgabewerte folgen der Konvention > 0 für Erfolg und 0 für Fehler.

3.3 Integrationsleitfaden

Die stabile C-API ermöglicht eine nahtlose Integration in praktisch jede moderne Entwicklungsumgebung.

* Python (ctypes): Nutzung via ctypes.CDLL (wegen __cdecl auf Windows). Die DLL muss sich im Pfad oder neben dem Skript befinden.
* Rust (FFI): Einbindung über extern "C" Blöcke mit #[repr(C)] Structs.
* Unity (C#): Die DLL wird in Assets/Plugins/ platziert. Die Anbindung erfolgt über P/Invoke mit [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]. Handles werden als IntPtr gemarshallt.
* Unreal Engine (C++): Die DLL wird in Binaries/Win64/ platziert und zur Laufzeit mit FPlatformProcess::GetDllHandle und FPlatformProcess::GetDllExport geladen.

Anwendungsfälle in Game Engines:

* Visualisierung: Auslesen von Feldern mit ms_copy_field_out zur Steuerung von Texturen (Heatmaps), Niagara-Systemen oder Material-Overlays.
* Interaktion: Schreiben von lokalen Änderungen in die Simulation mit ms_copy_field_in, um auf Spieleraktionen (Mausklicks, Collider-Events) zu reagieren.
* Gameplay-Logik: Nutzung von Simulationszuständen (z.B. Mycel-Dichte) als Grundlage für Spielmechaniken.
* Editor-Tools: Integration von Headless-Batch-Läufen direkt in den Editor.

4. MycoDB: Organische räumliche Datenspeicherung

MycoDB ist ein experimenteller Ansatz, der die Prinzipien von Micro-Swarm anwendet, um ein sich selbst organisierendes Datenbanksystem zu schaffen. Es ist als eingebettete Engine konzipiert und Teil der Micro-Swarm DLL.

4.1 Konzept: Biologisches Indexing

Traditionelle Datenbanken nutzen starre Indexstrukturen (z.B. B-Trees). MycoDB ersetzt dies durch einen emergenten Clustering-Prozess, der von einem Agentenschwarm gesteuert wird.

SQL-Konzept	Biologisches Äquivalent	Funktion in MycoDB
Row (Datensatz)	Payload (Last)	Ein physisches Datenpaket in einem 2D-Raum.
Tabelle	Pheromon-Signatur	Jede Tabelle emittiert eine einzigartige "Duftnote".
Foreign Key	Anziehungskraft	Agenten werden von den Pheromonen der Elterndatensätze angezogen.
Index	Mycel & Cluster	Das Mycel-Netzwerk stabilisiert die Pfade zwischen logisch zusammengehörigen Datenclustern.

4.2 Ingestion-Prozess ("Schwarm-Sort")

1. Ein SQL-Dump wird mit ms_db_load_sql() importiert.
2. Im db_ingest-Modus werden tausende "Carrier Agents" erzeugt.
3. Jeder Agent nimmt einen Datensatz (Payload) auf.
4. Besitzt der Datensatz einen Fremdschlüssel (z.B. ArtistId), sucht der Agent die Position des referenzierten Elterndatensatzes.
5. Der Agent transportiert die Payload physisch zu diesem Ziel.
6. Nach tausenden Simulationsschritten (--steps) bilden sich organische Cluster. Alle Alben eines Künstlers liegen nun physisch um den Künstler herum.
7. Der resultierende Zustand wird als .myco-Datei gespeichert (ms_db_save_myco()).

4.3 Spatial SQL und Abfragen

Da die Daten bereits räumlich vorsortiert sind, werden komplexe JOINs zu schnellen Nachbarschaftssuchen. MycoDB führt ein kontextbasiertes Abfragemodell ein:

* Fokus & Radius: Anstatt global zu suchen, wird mit goto <payload_id> ein Fokuspunkt gesetzt und mit radius <N> ein Suchradius definiert. Alle folgenden Abfragen werden nur in diesem lokalen Bereich ausgeführt.
* SQL-Light Parser: Die Engine enthält einen C++-SQL-Parser, der eine Teilmenge von SQL unterstützt, darunter SELECT, WHERE, GROUP BY, JOIN (Inner, Left, Right, Cross), Subqueries und Common Table Expressions (WITH).
* Interaktive Shell: Der db_shell-Modus bietet eine interaktive Kommandozeile (myco>) zur Ausführung von einfachen und SQL-basierten Abfragen.

Wichtige API-Funktionen:

* ms_db_create() / ms_db_destroy(): Lebenszyklus des DB-Kontexts.
* ms_db_run_ingest(): Startet den Sortierprozess.
* ms_db_query_sql() / ms_db_query_simple_focus(): Führt Abfragen aus.
* ms_db_get_last_error(): Ruft Fehlertexte ab.
