# 🍄 MycoDB: Die sich selbst organisierende Datenbank

### Einleitung: Wenn Daten laufen lernen
Was wäre, wenn eine Datenbank ihre eigenen Daten so organisieren könnte, dass Abfragen intuitiver und schneller werden? Traditionelle RDBMS sind durch ein ungelöstes Kernproblem gefesselt: die **Kluft zwischen logischer Verwandtschaft und physischer Speicherlokalität**. 

Logisch zusammengehörige Daten, wie zum Beispiel ein Kunde und seine Bestellungen, sind auf der Festplatte oft weit voneinander entfernt gespeichert. Um diese Verbindungen wiederherzustellen, sind rechenintensive **JOIN-Operationen** notwendig, die das System verlangsamen. 

**MycoDB (Mycelium Database)** ist ein experimenteller, biologisch inspirierter Ansatz, der dieses Problem nicht durch starre mathematische Indexstrukturen wie B-Trees, sondern durch **Emergenz und Selbstorganisation** löst. Es ist mehr als nur eine Datenbank; es ist eine innovative Anwendung und der primäre Proof-of-Concept für das **Micro-Swarm Framework**, eine hochperformante C++ Artificial-Life-Engine, die darauf ausgelegt ist, komplexe Probleme durch das Zusammenspiel einfacher, lokaler Regeln zu lösen. Als eingebettete Engine, ausgeliefert als Teil der Micro-Swarm DLL, wendet MycoDB diese Prinzipien an, um eine der klassischsten Herausforderungen der Datenspeicherung zu meistern.

---

## 1. Das Kernkonzept: Biologisches Indexing statt starrer Strukturen
MycoDB übersetzt traditionelle Datenbankkonzepte in biologische Äquivalente. Anstatt Daten in starren Strukturen abzulegen, werden sie zu physischen Objekten in einer dynamischen 2D-Welt, die von einem Agentenschwarm intelligent geordnet werden.

| Traditionelles SQL-Konzept | Biologisches Äquivalent | Funktion & Erklärung |
| :--- | :--- | :--- |
| **Zeile (Datensatz)** | **Payload (Last)** | Ein physisches Datenpaket, das sich in einem 2D-Raum befindet. |
| **Tabelle** | **Pheromon-Signatur** | Jede Tabelle emittiert eine einzigartige „Duftnote“, die als Signal dient. |
| **Fremdschlüssel** | **Anziehungskraft** | Agenten werden von den Pheromonen der Elterndatensätze angezogen. |
| **Index** | **Cluster & Mycel** | Daten bilden Cluster. Ein Mycel-Netzwerk stabilisiert die Pfade dazwischen. |
| **Abfrage (Query)** | **Lokale Suche** | Suche wird auf die unmittelbare räumliche Nähe beschränkt (Hocheffizient). |

---

## 2. Der Mechanismus: Wie der „Schwarm-Sort“ Daten organisiert
Die Datenorganisation in MycoDB ist kein einmaliger, statischer Vorgang, sondern ein aktiver Prozess, der als **„Schwarm-Sort“** bezeichnet wird. Dabei wird ein Agentenschwarm genutzt, um die Daten physisch zu sortieren. Der Prozess der Datenaufnahme (**Ingestion**) läuft wie folgt ab:

1.  Ein klassischer SQL-Dump wird über die API-Funktion `ms_db_load_sql()` importiert, wobei die Datensätze zunächst an zufälligen Positionen existieren.
2.  Tausende von **„Carrier Agents“** (Träger-Agenten) werden in einem 2D-Raum erzeugt.
3.  Jeder Agent nimmt einen einzelnen Datensatz (eine **„Payload“**) auf und wird zu dessen Träger.
4.  Besitzt der Datensatz einen Fremdschlüssel (z.B. `ArtistId`), sucht der Agent die Position des zugehörigen Elterndatensatzes, indem er dessen einzigartiger Pheromon-Signatur folgt.
5.  Der Agent transportiert seine Payload physisch durch den 2D-Raum zu seinem Ziel.

Das Ergebnis dieses Prozesses, der über `ms_db_run_ingest()` gestartet wird, ist faszinierend: Nach tausenden von Simulationsschritten bilden sich organische Cluster. 

> *„Alle Alben eines Künstlers liegen nun physisch um den Künstler herum.“*

Der optimierte Zustand wird anschließend als `.myco`-Datei persistiert. Diese physische Vorsortierung ist der Schlüssel zur außergewöhnlichen Abfragegeschwindigkeit von MycoDB.

---

## 3. Der Nutzen: Wie komplexe JOINs zu schnellen Nachbarschaftssuchen werden
Da logisch zusammengehörige Daten bereits physisch nahe beieinander liegen, werden komplexe und langsame JOIN-Operationen zu extrem schnellen, lokalen Suchen. MycoDB führt dazu ein neues, kontextbasiertes Abfragemodell ein, das auf **„Fokus und Radius“** basiert. 

*   **GOTO:** Setzt den "Lesekopf" der Datenbank physisch auf die Position eines bestimmten Datensatzes.
*   **RADIUS:** Definiert einen Suchradius (z.B. 10 Einheiten) um diesen Fokuspunkt. 

Alle folgenden Abfragen werden ausschließlich in diesem kleinen, lokalen Bereich ausgeführt. Es ist entscheidend zu verstehen, dass dieses Fokus-Modell keine proprietäre Abfragesprache ersetzt. Im Gegenteil: Es fungiert als eine kontextsetzende Schicht über einer leistungsfähigen, in C++ implementierten **SQL-Light-Engine**. Diese unterstützt `SELECT`, `WHERE`, `JOINs`, `Subqueries` und `CTEs (WITH)`.

### Praxis-Beispiel:
1.  **Fokus finden (Globaler Scan):** 
    `Artist Name="AC/DC" --> Output: ID=5, x=120, y=45`
2.  **Fokus setzen (Physischer Sprung):** 
    `GOTO ID=5`
3.  **Lokal abfragen (Extrem schnell):** 
    `SELECT * FROM Album RADIUS 10`

Dieser Ansatz findet alle Ergebnisse sofort, da die Agenten die Alben bereits physisch zum Künstler getragen haben. Ein teurer Scan der gesamten Tabelle ist nicht mehr nötig.

---

## 4. Fazit: Eine Datenbank, der man beim Denken zusehen kann
Die zentrale Innovation von MycoDB beweist, dass Datenbanksysteme nicht starr sein müssen. Es dient als greifbarer Beweis für die zentrale These des Micro-Swarm-Projekts: dass komplexe, adaptive Systeme aus einfachen, lokalen Regeln entstehen können.

Durch die Anwendung von Prinzipien der Schwarmintelligenz erreicht MycoDB eine emergente, selbstorganisierende Defragmentierung von Information. Anstatt dass ein Entwickler manuelle Indizes definieren muss, organisiert sich die Datenbank selbst.

> **„Es ist die erste Datenbank, die man nicht nur abfragen, sondern der man beim 'Denken' (Sortieren) zusehen kann.“**

***