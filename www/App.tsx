
import React, { useState, useEffect } from 'react';

/**
 * Component for a Copyable Code Block
 */
const CodeBlock: React.FC<{ code: string; language?: string }> = ({ code, language }) => {
  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    navigator.clipboard.writeText(code);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="code-block group">
      <button onClick={handleCopy} className="copy-btn">
        {copied ? 'Kopiert!' : 'Kopieren'}
      </button>
      <pre className="whitespace-pre-wrap break-all">
        <code>{code}</code>
      </pre>
    </div>
  );
};

/**
 * Component for "Allgemeines Datenbankwissen" Info Box
 */
const TheoryBox: React.FC<{ title: string; children: React.ReactNode }> = ({ title, children }) => (
  <div className="theory-box shadow-sm">
    <h4 className="text-green-800 font-bold mb-2 flex items-center">
      <svg className="w-5 h-5 mr-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
      </svg>
      Allgemeines Datenbankwissen: {title}
    </h4>
    <div className="text-green-900 text-sm leading-relaxed">
      {children}
    </div>
  </div>
);

/**
 * Component for MycoDB Note Boxes
 */
const MycoNote: React.FC<{ children: React.ReactNode }> = ({ children }) => (
  <div className="mycodb-box border-l-4 border-blue-500 bg-blue-50">
    <div className="text-blue-900 italic font-medium">
      {children}
    </div>
  </div>
);

const benchmarkPoints = [
  [252, 108], [265, 114], [272, 121], [258, 132], [249, 125],
  [266, 132], [275, 126], [262, 141], [255, 116], [269, 138],
  [282, 122], [246, 118], [260, 128], [277, 136], [256, 138]
];

/**
 * Visualisierung des 356ms Benchmarks (Island-Effect)
 */
const BenchmarkVisual: React.FC = () => (
  <div className="my-8 p-4 bg-gray-900 rounded-xl border border-gray-700 shadow-2xl relative overflow-hidden">
    <div className="flex justify-between items-center mb-4">
      <h4 className="text-green-400 font-mono text-sm">EMERGENT CLUSTER MAP (Size 2048)</h4>
      <span className="text-xs text-gray-400 font-mono">Status: 356ms Scan Complete</span>
    </div>

    <div className="relative aspect-video bg-black rounded border border-gray-800 flex items-center justify-center">
      <div
        className="absolute inset-0 opacity-10"
        style={{
          backgroundImage:
            "linear-gradient(#333 1px, transparent 1px), linear-gradient(90deg, #333 1px, transparent 1px)",
          backgroundSize: "20px 20px"
        }}
      ></div>

      <svg viewBox="0 0 400 200" className="w-full h-full relative z-10">
        <defs>
          <radialGradient id="scentGlow" cx="50%" cy="50%" r="50%">
            <stop offset="0%" stopColor="#22c1a7" stopOpacity="0.4" />
            <stop offset="100%" stopColor="#22c1a7" stopOpacity="0" />
          </radialGradient>
        </defs>

        <circle cx="260" cy="120" r="40" fill="url(#scentGlow)" className="animate-pulse" />

        {benchmarkPoints.map(([x, y], i) => (
          <circle key={i} cx={x} cy={y} r="1.5" fill="#fff" className="opacity-80" />
        ))}

        <circle cx="260" cy="120" r="30" fill="none" stroke="#0f6b88" strokeWidth="1" strokeDasharray="4 2" />

        <line x1="260" y1="120" x2="310" y2="70" stroke="#aaa" strokeWidth="0.5" />
        <text x="315" y="65" fill="#aaa" fontSize="8" className="font-mono">
          FOKUS: PL_496190 (Wilms)
        </text>
        <text x="315" y="75" fill="#22c1a7" fontSize="8" className="font-mono">
          97% Match Density
        </text>

        <rect x="10" y="170" width="80" height="20" rx="4" fill="rgba(34, 193, 167, 0.1)" stroke="#22c1a7" strokeWidth="0.5" />
        <text x="18" y="183" fill="#22c1a7" fontSize="10" fontWeight="bold" className="font-mono">
          356 ms
        </text>
      </svg>

      <div className="absolute inset-0 bg-gradient-to-t from-black via-transparent to-transparent opacity-60"></div>
    </div>

    <div className="mt-4 flex flex-wrap gap-4 text-[11px] text-gray-300">
      <span className="inline-flex items-center gap-2">
        <span className="inline-block w-2 h-2 rounded-full bg-white"></span> Payloads
      </span>
      <span className="inline-flex items-center gap-2">
        <span className="inline-block w-2 h-2 rounded-full" style={{ backgroundColor: "#22c1a7" }}></span> Pheromon-Glow
      </span>
      <span className="inline-flex items-center gap-2">
        <span className="inline-block w-3 h-[2px]" style={{ backgroundColor: "#0f6b88" }}></span> Radius-Scan
      </span>
      <span className="inline-flex items-center gap-2">
        <span className="inline-block w-3 h-[2px]" style={{ backgroundColor: "#aaa" }}></span> Fokus-Markierung
      </span>
    </div>

    <p className="mt-3 text-[11px] text-gray-400 italic leading-snug">
      Darstellung der Informations-Insel bei Size 2048. 97% der relevanten Daten liegen auf ~0,06% der Flaeche.
    </p>
  </div>
);

const faqQuizQuestions = [
  "Was ist das grundlegende Prinzip von Micro-Swarm und wie unterscheidet es sich von traditionellen KI-Ansätzen wie neuronalen Netzen?",
  "Nennen und beschreiben Sie kurz die vier Gedächtnisschichten in Micro-Swarm und ihre jeweiligen Zeithorizonte.",
  "Welche Rolle spielt das Mycel-Netzwerk im System und wie wird sein Wachstum gesteuert, um eine Sättigung zu verhindern?",
  "Was ist der Hauptunterschied zwischen dem „Baseline-Run“ und dem „Stress-Test“, und was soll der Stresstest nachweisen?",
  "Erklären Sie den Prozess, wie ein einzelner Agent in Micro-Swarm seine Bewegungsentscheidungen trifft. Welche Informationen nutzt er dafür?",
  "Beschreiben Sie die vier Agenten-Spezies und ihre spezifischen Rollen im Schwarm, um eine Arbeitsteilung zu erreichen.",
  "Welche Versionierungsstrategie wird für die Micro-Swarm C-API verwendet und was bedeuten die einzelnen Komponenten der Versionsnummer?",
  "Welche Garantien gibt die Micro-Swarm DLL bezüglich der Thread-Sicherheit und wie sollte ein Kontext in einer Multi-Threaded-Anwendung verwendet werden?",
  "Was ist das Kernkonzept von MycoDB und wie löst es das Problem des Data Clustering im Vergleich zu traditionellen Datenbanken?",
  "Erklären Sie das „Fokus und Radius“-Modell, das bei Abfragen in MycoDB verwendet wird, und warum es bei bereits geclusterten Daten effizient ist."
];

const faqAnswerKey = [
  "Grundprinzip: Micro-Swarm ist ein biologisch inspiriertes Artificial-Life-System, das auf Emergenz aus einfachen lokalen Regeln und einem mehrschichtigen Gedächtnis basiert. Im Gegensatz zu traditionellen KI-Ansätzen verzichtet es bewusst auf neuronale Netze, Backpropagation und Reinforcement Learning und fungiert als „Glass Box“, bei der jeder Effekt mechanistisch nachvollziehbar ist.",
  "Gedächtnisschichten: Moleküle (Kurzzeitgedächtnis, Sekunden), die als flüchtige Reaktionssignale dienen; Pheromone (Mittelzeitgedächtnis, Minuten), die als Kommunikationskanäle für die Stigmergie fungieren; Mycel (Langzeitgedächtnis, Stunden), das als strukturelles Gedächtnis stabile Pfade bildet; und DNA (persistentes Gedächtnis, Generationen), das erfolgreiche Verhaltensstrategien speichert.",
  "Mycel-Netzwerk: Das Mycel-Netzwerk ist das strukturelle Langzeitgedächtnis, das Pfade mit anhaltend hoher Agenten-Aktivität stabilisiert und verstärkt. Sein Wachstum wird durch logistische Mechanismen und einen diffusionsartigen Transport (Laplacian) gesteuert, was explosive Rückkopplungsschleifen und eine globale Sättigung des Feldes verhindert.",
  "Baseline vs. Stress-Test: Der „Baseline-Run“ ist ein Referenzlauf ohne externe Störungen, der die Selbstorganisation des Systems zeigt. Der „Stress-Test“ führt ab einem bestimmten Schritt (z. B. Schritt 120) eine Störung ein, wie das Blockieren von Ressourcen und das Hinzufügen von Pheromon-Rauschen, um die Resilienz und Anpassungsfähigkeit des Schwarms zu testen.",
  "Bewegungsentscheidung: Ein Agent trifft seine Entscheidungen ausschließlich auf Basis seiner lokalen Umgebung. Er nutzt Sensoren in drei Richtungen (links, vorne, rechts), um verschiedene Felder (Ressourcen, Nahrungs-Pheromon, Gefahren-Pheromon, Moleküle) abzutasten und wählt seine nächste Bewegungsrichtung stochastisch basierend auf einer gewichteten Summe dieser lokalen Gradienten.",
  "Spezies-Rollen: Explorator (Neugier), der neue Gradienten sucht; Integrator (Structure), der bestehende Pfade stabilisiert und verstärkt; Regulator (Stabilität), der auf Gefahren-Pheromone reagiert und Überdichtung vermeidet; und Innovator (Variation), der mit erhöhter Mutationsrate neue Strategien testet.",
  "Versionierung: Die API verwendet die Semantische Versionierung im Format MAJOR.MINOR.PATCH. Eine MAJOR-Änderung signalisiert inkompatible ABI- oder Verhaltensänderungen. Eine MINOR-Änderung steht für abwärtskompatible Ergänzungen (z. B. neue Funktionen oder angehängte Struct-Felder). Eine PATCH-Änderung beinhaltet nur Fehlerbehebungen ohne API-Änderungen.",
  "Thread-Sicherheit: Ein Micro-Swarm-Kontext (Handle) ist nicht thread-sicher, und parallele Aufrufe auf denselben Handle führen zu undefiniertem Verhalten. Die API ist synchron und blockierend ohne internes Threading. Ein Kontext darf jedoch zwischen Threads übergeben werden, solange zu jedem Zeitpunkt nur ein einziger Thread darauf zugreift.",
  "MycoDB Konzept: MycoDB ist ein experimenteller Ansatz für „biologisches Indexing“, der Schwarmintelligenz nutzt, um Daten räumlich zu clustern. Anstatt starre Indizes (z.B. B-Trees) zu verwenden, transportieren Agenten zusammengehörige Datensätze (Payloads), die durch Fremdschlüssel verbunden sind, physisch an denselben Ort im 2D-Raum, wodurch sich organische Datencluster bilden.",
  "Fokus und Radius: Anstatt globale Suchen durchzuführen, verwendet MycoDB das „Fokus und Radius“-Modell. Eine Abfrage setzt mit GOTO zunächst einen Fokuspunkt auf einen bestimmten Datensatz und durchsucht dann mit RADIUS nur die unmittelbare räumliche Nachbarschaft. Da verwandte Daten bereits durch den Schwarm physisch nahe beieinander platziert wurden, werden JOIN-Operationen zu extrem schnellen lokalen Suchen."
];

const faqEssayQuestions = [
  "Erläutern Sie die „Glass Box“-Philosophie von Micro-Swarm. Wie tragen das mehrschichtige Gedächtnissystem und die rein lokalen Agentenregeln zu dieser Transparenz bei, und worin besteht der grundlegende Gegensatz zu „Black Box“-Modellen wie tiefen neuronalen Netzen?",
  "Beschreiben Sie den „Kreislauf der Emergenz“ in Micro-Swarm. Detaillieren Sie, wie lokale Agentenaktionen zu globalen Strukturen wie Mycel-Netzwerken führen und wie diese Strukturen wiederum das zukünftige Verhalten der Agenten beeinflussen und lenken.",
  "Vergleichen Sie die drei primären experimentellen Szenarien: Baseline, Stress-Test und Turbo-Evolution. Diskutieren Sie, was jedes Szenario demonstrieren soll, und analysieren Sie die Schlüsselmetriken (z. B. mycel mean, resources mean), die zur Validierung der jeweiligen Ergebnisse herangezogen werden.",
  "Analysieren Sie das Design und die Stabilitätsgarantien der Micro-Swarm C-API. Warum wurde eine reine C-API gewählt, und welche spezifischen Regeln (Aufrufkonvention, Struct-Evolution, Versionierung) gewährleisten ihre Portabilität und langfristige Nutzbarkeit in verschiedenen Umgebungen wie Python, Unity und Unreal Engine?",
  "Erarbeiten Sie das Konzept des „biologischen Indexing“ in MycoDB. Wie übersetzt es traditionelle Datenbankkonzepte (Zeilen, Tabellen, Fremdschlüssel, Indizes) in biologische Äquivalente, und was sind die theoretischen Vorteile dieses emergenten Clustering-Ansatzes für bestimmte Arten von Abfragen und Datenstrukturen?"
];

const faqGlossary = [
  {
    term: "__cdecl",
    definition: "Die Standard-Aufrufkonvention für C-Programme auf Windows. Sie muss von Aufrufern der Micro-Swarm-DLL (z. B. in Python mit ctypes.CDLL) beachtet werden, um eine korrekte Stapelverwaltung sicherzustellen."
  },
  {
    term: "ABI (Application Binary Interface)",
    definition: "Die binäre Schnittstelle zwischen zwei Programm-Modulen. Die Micro-Swarm-API legt strenge ABI-Regeln fest (z. B. __cdecl), um die Kompatibilität der DLL über verschiedene Compiler und Sprachen hinweg zu gewährleisten."
  },
  {
    term: "Agent",
    definition: "Eine mobile Einheit im Micro-Swarm-System mit einem internen Zustand (Position, Energie, Genom), die ausschließlich auf Basis lokaler Sensorik ohne globale Sicht oder zentrales Ziel agiert."
  },
  {
    term: "API (Application Programming Interface)",
    definition: "Die definierte Schnittstelle, über die Softwarekomponenten miteinander interagieren. Micro-Swarm stellt eine stabile C-API in der Header-Datei micro_swarm_api.h bereit."
  },
  {
    term: "Baseline-Run",
    definition: "Ein Referenz-Simulationslauf ohne externe Störungen oder evolutionäre Selektion. Er dient als „Ground Truth“, um die grundlegende Fähigkeit des Systems zur Selbstorganisation und Strukturbildung zu demonstrieren."
  },
  {
    term: "Carrier Agents",
    definition: "Spezielle Agenten im MycoDB db_ingest-Modus, die jeweils einen Datensatz (Payload) aufnehmen und ihn basierend auf Fremdschlüssel-Beziehungen physisch zu einem Zielort im 2D-Raum transportieren."
  },
  {
    term: "Delta-Store",
    definition: "Ein temporärer Speicher in MycoDB, in dem Änderungen (INSERT, UPDATE, DELETE) gesammelt werden, bevor sie durch einen merge-Befehl dauerhaft in die Datenbank geschrieben werden. Dies ermöglicht Undo-Operationen und hypothetische Szenarien."
  },
  {
    term: "DNA",
    definition: "Die persistenteste Gedächtnisschicht, die Strategien in Form von erfolgreichen Genomen (Parametervektoren) über Generationen von Agenten hinweg speichert. Ein globaler Pool ermöglicht die Evolution von Verhalten."
  },
  {
    term: "Emergenz",
    definition: "Der Prozess, bei dem komplexe Muster und globales Verhalten aus einfachen, lokalen Interaktionen vieler Einzelkomponenten (Agenten) entstehen, ohne dass eine zentrale Steuerung existiert."
  },
  {
    term: "Explorator (Species 0)",
    definition: "Eine Agenten-Spezies mit hohem Erkundungsdrang und geringer Pheromon-Bindung, deren Hauptfunktion die Entdeckung neuer Ressourcen-Gradienten ist."
  },
  {
    term: "FFI (Foreign Function Interface)",
    definition: "Eine Schnittstelle, die es ermöglicht, in einer Programmiersprache geschriebene Funktionen aus einer anderen Sprache aufzurufen. Die C-API von Micro-Swarm ist FFI-sicher konzipiert."
  },
  {
    term: "Fokus (MycoDB)",
    definition: "Ein Konzept in MycoDB, bei dem der „Lesekopf“ der Datenbank physisch auf einen bestimmten Datensatz (Payload) gesetzt wird (GOTO). Nachfolgende Abfragen werden dann lokal um diesen Punkt herum ausgeführt."
  },
  {
    term: "Glass Box",
    definition: "Ein System, dessen interne Mechanismen vollständig transparent, messbar und kausal nachvollziehbar sind. Micro-Swarm wird als Glass Box konzipiert, im Gegensatz zu Black-Box-Modellen wie neuronalen Netzen."
  },
  {
    term: "Handle",
    definition: "Ein abstrakter Verweis auf eine vom System verwaltete Ressource. Funktionen wie ms_create() geben ein Handle auf einen Simulationskontext zurück, der mit ms_destroy() wieder freigegeben werden muss."
  },
  {
    term: "Headless Pipeline",
    definition: "Eine Kommandozeilenschnittstelle (--mode), die es ermöglicht, Simulationen ohne grafische Oberfläche durchzuführen und automatisch Berichte (z.B. Offline-HTML oder CSV-Dumps) zu generieren."
  },
  {
    term: "Ingestion-Prozess",
    definition: "Der Prozess in MycoDB (db_ingest-Modus), bei dem Daten aus einer Quelle (z.B. SQL-Dump) importiert und von Carrier Agents in einem 2D-Raum räumlich sortiert und geclustert werden."
  },
  {
    term: "Integrator (Species 1)",
    definition: "Eine Agenten-Spezies, die bestehende Pheromon- und Mycel-Pfade stark gewichtet, um Routen zu stabilisieren und zu verstärken. Sie ist entscheidend für den Aufbau des strukturellen Gedächtnisses."
  },
  {
    term: "Innovator (Species 3)",
    definition: "Eine Agenten-Spezies mit erhöhter Mutationsrate und schwacher DNA-Bindung, die gezielt Regionen hoher Entropie aufsucht, um neue Strategien zu testen und Variation zu erzeugen."
  },
  {
    term: "Moleküle",
    definition: "Das Kurzzeitgedächtnis des Systems. Moleküle sind stark verdampfende, lokale Signale, die von Agenten emittiert werden, um eine unmittelbare Reaktion auf Ereignisse zu kodieren und zu verhindern, dass Agenten sofort umkehren."
  },
  {
    term: "Mycel (Mycelium)",
    definition: "Das strukturelle Langzeitgedächtnis. Es ist ein langsam wachsendes Dichtefeld, das an Orten mit anhaltend hoher Agenten-Aktivität entsteht und so erfolgreiche Pfade verfestigt und als bleibendes Gedächtnis der kollektiven Erfahrung dient."
  },
  {
    term: "MycoDB",
    definition: "Eine experimentelle Datenbank-Engine, die auf Micro-Swarm aufbaut und Schwarmintelligenz nutzt, um Daten durch einen emergenten Prozess räumlich zu clustern („biologisches Indexing“)."
  },
  {
    term: "OpenCL",
    definition: "Ein Framework zur Ausführung von parallelen Programmen auf heterogenen Plattformen wie GPUs. Micro-Swarm kann optional OpenCL nutzen, um die Diffusion von Pheromonen und Molekülen auf der GPU zu beschleunigen."
  },
  {
    term: "P/Invoke",
    definition: "Eine Technologie in .NET (z.B. in Unity/C#), die es ermöglicht, Funktionen aus nativen Bibliotheken wie der micro_swarm.dll aufzurufen."
  },
  {
    term: "Payload",
    definition: "Ein einzelner Datensatz in MycoDB, der als physisches Objekt im 2D-Raum existiert und von Agenten transportiert wird. In der Shell wird er als Zeile/Datensatz dargestellt."
  },
  {
    term: "Pheromone",
    definition: "Das Mittelzeitgedächtnis, das als Kommunikationsmedium für Stigmergie dient. Es gibt zwei Kanäle: phero_food (anziehend) und phero_danger (abstoßend)."
  },
  {
    term: "POD (Plain Old Data)",
    definition: "Ein Datentyp, der nur aus passiven Datenfeldern besteht (wie eine C-struct). Die API von Micro-Swarm verwendet ausschließlich POD-Structs, um die Kompatibilität über verschiedene Sprachen hinweg zu maximieren."
  },
  {
    term: "Radius (MycoDB)",
    definition: "Definiert den Suchradius um einen gesetzten Fokuspunkt in MycoDB. Abfragen werden auf diesen lokalen Bereich beschränkt, was eine hohe Effizienz ermöglicht."
  },
  {
    term: "Regulator (Species 2)",
    definition: "Eine Agenten-Spezies mit hoher Sensitivität für Gefahren-Pheromone, die für negatives Feedback sorgt, Überdichtung vermeidet und zur Stabilität des Schwarms beiträgt."
  },
  {
    term: "repr(C)",
    definition: "Eine Anweisung (z.B. in Rust), die sicherstellt, dass die Speicheranordnung einer Datenstruktur mit der von C kompatibel ist. Alle Structs der Micro-Swarm-API sind repr(C)-kompatibel."
  },
  {
    term: "Semantische Versionierung",
    definition: "Eine formale Konvention zur Vergabe von Versionsnummern (MAJOR.MINOR.PATCH), die die Art der Änderungen in einer neuen Version klar kommuniziert. Sie ist ein Kernprinzip der API-Stabilität von Micro-Swarm."
  },
  {
    term: "Stigmergie",
    definition: "Eine Form der indirekten Kommunikation, bei der Individuen ihre Umgebung modifizieren (z. B. durch Pheromonspuren) und andere Individuen auf diese Veränderungen reagieren. Dies ist ein zentraler Koordinationsmechanismus in Micro-Swarm."
  },
  {
    term: "Stress-Test",
    definition: "Ein Simulationsszenario, das die Resilienz und Anpassungsfähigkeit des Schwarms durch die Einführung einer plötzlichen Störung (z. B. blockierte Ressourcen) testet."
  },
  {
    term: "Tombstones",
    definition: "Markierungen im Delta-Store von MycoDB, die anzeigen, welche Datensätze gelöscht wurden. Sie bleiben bis zum nächsten merge-Vorgang bestehen."
  },
  {
    term: "Turbo-Evolution",
    definition: "Ein Simulationslauf, bei dem eine gezielte Elite-Selektion aktiviert ist. Nur Genome, die einen nachweislichen Fitness-Zuwachs erzielen, werden in den DNA-Pool aufgenommen, was die Evolution effizienter Strategien dramatisch beschleunigt."
  }
];

const mycodmConceptTable = [
  {
    sql: "Zeile (Datensatz)",
    bio: "Payload (Last)",
    detail: "Ein physisches Datenpaket, das sich in einem 2D-Raum befindet."
  },
  {
    sql: "Tabelle",
    bio: "Pheromon-Signatur",
    detail: "Jede Tabelle emittiert eine einzigartige „Duftnote“, die als Signal dient."
  },
  {
    sql: "Fremdschlüssel",
    bio: "Anziehungskraft",
    detail: "Agenten werden von den Pheromonen der Elterndatensätze angezogen."
  },
  {
    sql: "Index",
    bio: "Cluster & Mycel",
    detail: "Daten bilden Cluster. Ein Mycel-Netzwerk stabilisiert die Pfade dazwischen."
  },
  {
    sql: "Abfrage (Query)",
    bio: "Lokale Suche",
    detail: "Suche wird auf die unmittelbare räumliche Nähe beschränkt (Hocheffizient)."
  }
];

const App: React.FC = () => {
  const [scrollProgress, setScrollProgress] = useState(0);

  useEffect(() => {
    const handleScroll = () => {
      const totalScroll = document.documentElement.scrollHeight - window.innerHeight;
      const currentProgress = (window.scrollY / totalScroll) * 100;
      setScrollProgress(currentProgress);
    };
    window.addEventListener('scroll', handleScroll);
    return () => window.removeEventListener('scroll', handleScroll);
  }, []);

  return (
    <div className="flex">
      {/* Scroll Progress Bar */}
      <div className="progress-container">
        <div id="progress-bar" style={{ width: `${scrollProgress}%` }}></div>
      </div>

      {/* Sidebar Navigation */}
      <nav className="sidebar p-6 hidden md:block">
        <div className="mb-8">
          <h1 className="text-xl font-bold text-gray-800">MycoDB Portal</h1>
          <p className="text-xs text-gray-500">Schulungs-Lehrbuch</p>
        </div>
        <ul className="space-y-3 text-sm">
          <li><a href="#manifest" className="hover:text-blue-600 font-medium">Manifest & Mechanik</a></li>
          <li><a href="#quickstart" className="hover:text-blue-600 font-medium">Erste 60 Sekunden</a></li>
          <li><a href="#kap1" className="hover:text-blue-600 font-medium">Kapitel 1: Grundlagen</a></li>
          <li><a href="#kap2" className="hover:text-blue-600 font-medium">Kapitel 2: Daten finden</a></li>
          <li><a href="#kap3" className="hover:text-blue-600 font-medium">Kapitel 3: SQL</a></li>
          <li><a href="#kap4" className="hover:text-blue-600 font-medium">Kapitel 4: Delta-Prinzip</a></li>
          <li><a href="#kap5" className="hover:text-blue-600 font-medium">Kapitel 5: Graph-Konzepte</a></li>
          <li><a href="#mycodm" className="hover:text-blue-600 font-medium">MycoDM</a></li>
          <li><a href="#benchmark" className="hover:text-blue-600 font-medium">Benchmark & Emergenz</a></li>
          <li><a href="#cmdref" className="hover:text-blue-600 font-medium">Kommando-Referenz</a></li>
          <li><a href="#kap6" className="hover:text-blue-600 font-medium">Kapitel 6: Produktivität</a></li>
          <li><a href="#troubleshooting" className="hover:text-blue-600 font-medium">Troubleshooting</a></li>
          <li><a href="#faq" className="hover:text-blue-600 font-medium">FAQ</a></li>
          <li><a href="#cheatsheet" className="hover:text-blue-600 font-medium">Cheat Sheet</a></li>
        </ul>
      </nav>

      {/* Main Content Area */}
      <main className="main-content">
        <header className="mb-12">
          <h1 className="text-4xl font-extrabold text-gray-900 mb-2">Micro-Swarm & MycoDB</h1>
          <p className="text-xl text-blue-600 font-medium mb-4">Wenn Daten lebendig werden.</p>
          <div className="text-sm text-gray-500 mb-6">
            Forschungsfeld: Artificial Life & Emergent Systems | Autor: Ralf Krümmel
          </div>
        </header>

        {/* Das Manifest Section */}
        <section id="manifest" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Das Manifest: Emergenz statt Blackbox</h2>
          
          <TheoryBox title="Künstliche Intelligenz vs. Künstliches Leben">
            Traditionelle KI (Deep Learning) basiert auf zentraler Steuerung und undurchsichtigen "Hidden Layers". Micro-Swarm (Artificial Life) hingegen nutzt dezentrale Selbstorganisation. Hier entsteht Intelligenz durch Interaktion, was zu einer vollständig erklärbaren Kausalität ("Glass Box") führt.
          </TheoryBox>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-8 my-8">
            <div className="p-6 bg-gray-50 rounded-lg border border-gray-200">
              <h4 className="font-bold text-gray-800 mb-2">Die Mechanik der Emergenz</h4>
              <ul className="text-sm space-y-3">
                <li className="flex items-start"><span className="bg-blue-500 text-white rounded-full w-5 h-5 flex items-center justify-center text-[10px] mr-2 mt-0.5">1</span> <strong>Agenten:</strong> Blinde Akteure mit lokalen Sensoren handeln dezentral.</li>
                <li className="flex items-start"><span className="bg-blue-500 text-white rounded-full w-5 h-5 flex items-center justify-center text-[10px] mr-2 mt-0.5">2</span> <strong>Pheromone:</strong> Dienen als Mittelzeit-Gedächtnis für die Kommunikation.</li>
                <li className="flex items-start"><span className="bg-blue-500 text-white rounded-full w-5 h-5 flex items-center justify-center text-[10px] mr-2 mt-0.5">3</span> <strong>Mycel:</strong> Das Langzeit-Gedächtnis bildet die physische Infrastruktur.</li>
                <li className="flex items-start"><span className="bg-blue-500 text-white rounded-full w-5 h-5 flex items-center justify-center text-[10px] mr-2 mt-0.5">4</span> <strong>DNA:</strong> Strategie-Speicher für die evolutionäre Optimierung.</li>
              </ul>
            </div>
            <div className="p-6 bg-blue-900 text-white rounded-lg shadow-xl">
              <h4 className="font-bold mb-2">Warum dieser Kurs?</h4>
              <p className="text-sm leading-relaxed opacity-90">
                SQL speichert verwandte Daten oft getrennt, was teure JOINs erfordert. MycoDB löst dies durch <strong>Organic Spatial Storage</strong>. Daten werden zu "Nahrung", und der Schwarm sortiert die Datenbank organisch in Clustern im 2D-Raum. 
                <br/><br/>
                Dieser Kurs ist essenziell, um das neue Paradigma <strong>"Vom Suchen zum Finden durch lokale Nähe"</strong> zu verstehen.
              </p>
            </div>
          </div>

          <TheoryBox title="Biologisches Indexing">
            In MycoDB wandern Tabellen. Eine <strong>Row</strong> wird zur <strong>Payload</strong> (physisches Paket), ein <strong>Foreign Key</strong> zur <strong>Pheromon-Attraktion</strong> (Gravitation) und eine <strong>Table</strong> zur <strong>Pheromon-Signatur</strong> (Duftnote). Carrier-Agenten transportieren Daten physisch zur Duftspur verwandter Datensätze.
          </TheoryBox>
        </section>

        {/* Schnellstart */}
        <section id="quickstart" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Erste 60 Sekunden (Schnellstart)</h2>
          <p className="mb-4 text-gray-700 italic">Bereit für den ersten Kontakt mit dem Schwarm? Starten wir die Shell.</p>
          
          <TheoryBox title="Shell und Kommandozeile">
            Datenbanken werden oft über eine Kommandozeile (Shell/Terminal) verwaltet. Ein Programmaufruf besteht meist aus dem Programmnamen und verschiedenen "Flags" (Optionen), die das Verhalten steuern.
          </TheoryBox>

          <h3 className="text-lg font-semibold mt-6">Startkommando</h3>
          <CodeBlock code={`cd build\\Release\n.\\micro_swarm.exe --mode db_shell --db chinook_optimized.myco --db-radius 5500`} />
          
          <MycoNote>
            TECH SPEC: MycoDB basiert auf einem C++17 High-Performance Core und nutzt OpenCL GPU Parallelisierung für die Simulation der Diffusions-Felder.
          </MycoNote>

          <h3 className="text-lg font-semibold mt-8">Zustand zurücksetzen (Reset)</h3>
          <CodeBlock code={`unfocus\nradius 5500\nshow off\nlimit off\nsort reset\nformat table`} />
        </section>

        {/* Kapitel 1 */}
        <section id="kap1" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Kapitel 1: Hallo MycoDB (Grundlagen)</h2>
          
          <TheoryBox title="Tabellen, Zeilen und Spalten">
            In relationalen Datenbanken werden Daten in <strong>Tabellen</strong> organisiert. Jede <strong>Zeile</strong> repräsentiert einen Datensatz, und jede <strong>Spalte</strong> ein Attribut (z.B. Name, Preis).
          </TheoryBox>
          
          <div className="space-y-6">
            <div>
              <p className="font-medium">Tabellen anzeigen:</p>
              <CodeBlock code="tables" />
            </div>
            <div>
              <p className="font-medium">Struktur erkunden:</p>
              <CodeBlock code="describe Album" />
            </div>
            <div>
              <p className="font-medium">Datenmenge zählen:</p>
              <CodeBlock code="stats" />
            </div>
          </div>
        </section>

        {/* Kapitel 2 */}
        <section id="kap2" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Kapitel 2: Daten finden</h2>
          
          <TheoryBox title="Primärschlüssel (Primary Key)">
            Ein Primärschlüssel ist ein eindeutiger Identifikator. Er stellt sicher, dass man genau einen Datensatz (z.B. Album 1) adressieren kann.
          </TheoryBox>
          <CodeBlock code="Album 1" />
          
          <TheoryBox title="Fremdschlüssel (Foreign Key)">
            Fremdschlüssel verknüpfen Tabellen. In MycoDB erzeugt dies eine <strong>physische Nähe</strong>: Alle Alben eines Künstlers liegen räumlich um den Künstler herum.
          </TheoryBox>
          <CodeBlock code="Track AlbumId=1" />

          <h3 className="text-xl font-semibold mt-8 mb-4">Ausgabe filtern</h3>
          <CodeBlock code="Track AlbumId=1 show TrackId,Name,Milliseconds" />
          
          <div className="mt-8 p-6 bg-blue-50 border border-blue-200 rounded-lg shadow-inner">
            <h4 className="font-bold text-blue-800 mb-4">Mini-Übungen (Kapitel 2)</h4>
            <ol className="list-decimal pl-5 space-y-4">
              <li>
                <p>Finde das Album mit der ID 1.</p>
                <details>
                  <summary>Lösung anzeigen</summary>
                  <code className="block mt-2 bg-gray-100 p-2 rounded">Album 1</code>
                </details>
              </li>
              <li>
                <p>Liste alle Tracks von Album 1.</p>
                <details>
                  <summary>Lösung anzeigen</summary>
                  <code className="block mt-2 bg-gray-100 p-2 rounded">Track AlbumId=1</code>
                </details>
              </li>
            </ol>
          </div>
        </section>

        {/* Kapitel 3 */}
        <section id="kap3" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Kapitel 3: SQL in MycoDB</h2>
          
          <TheoryBox title="Standard SQL">
            SQL ist die Standardsprache für Datenbanken. MycoDB unterstützt Standard-SELECTs, ergänzt diese aber um räumliche Konzepte.
          </TheoryBox>
          <CodeBlock code="sql SELECT TrackId, Name FROM Track WHERE AlbumId=1" />
          
          <MycoNote>
            HINWEIS: MycoDB erreicht <strong>O(1) Zugriffszeit</strong> im lokalen Kontext, da man "nicht suchen muss, wenn man schon dort ist".
          </MycoNote>
        </section>

        {/* Kapitel 4 */}
        <section id="kap4" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Kapitel 4: Das Delta-Prinzip</h2>
          
          <TheoryBox title="Resilienz & Wundheilung">
            In der Natur reparieren sich Systeme selbst. MycoDB nutzt das Delta-Prinzip für Änderungen. Änderungen landen erst im Staging (Delta), bevor sie "gemerged" werden. Das System zeigt dabei "Wundheilung": Neue Strukturen bilden sich stabil um Blockaden herum.
          </TheoryBox>
          
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
              <p className="font-bold mb-2">Delta anzeigen:</p>
              <CodeBlock code="delta show" />
            </div>
            <div>
              <p className="font-bold mb-2">Änderungen fixieren:</p>
              <CodeBlock code="merge" />
            </div>
          </div>
        </section>

        {/* Kapitel 5 */}
        <section id="kap5" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Kapitel 5: Navigation & Fokus</h2>
          
          <TheoryBox title="Spatial SQL">
            In MycoDB springt man zu einem Cluster (z.B. Artist) und setzt einen Suchradius. Dies ersetzt komplexe Index-Scans durch räumliche Eingrenzung.
          </TheoryBox>
          
          <CodeBlock code="goto 1\nradius 15.0\nfocus" />
          <div className="mt-8 p-6 bg-blue-50 border border-blue-200 rounded-lg shadow-inner">
            <h4 className="font-bold text-blue-800 mb-3">Lehrbeispiel: Lokale Abfrage mit neuem Radius</h4>
            <p className="text-sm text-blue-900 mb-4">
              Fokus und Radius bleiben aktiv, bis du sie aenderst oder entfernst.
            </p>
            <CodeBlock
              code={`goto 496190\nradius 50\nsql SELECT vorname, nachname FROM users WHERE nachname = 'Wilms' LIMIT 20\nexplain\nradius 120\nsql SELECT vorname, nachname FROM users WHERE nachname = 'Wilms' LIMIT 5\nexplain\nunfocus\nradius 5500`}
            />
          </div>
        </section>

        {/* MycoDM */}
        <section id="mycodm" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">MycoDM: The Architecture of Emergence</h2>
          <h3 className="text-xl font-semibold mb-3">MycoDB: Die sich selbst organisierende Datenbank</h3>

          <h4 className="text-lg font-semibold mt-6 mb-2">Einleitung: Wenn Daten laufen lernen</h4>
          <p className="text-gray-700 mb-4">
            Was wäre, wenn eine Datenbank ihre eigenen Daten so organisieren könnte, dass Abfragen intuitiver und
            schneller werden? Traditionelle RDBMS sind durch ein ungelöstes Kernproblem gefesselt: die Kluft zwischen
            logischer Verwandtschaft und physischer Speicherlokalität.
          </p>
          <p className="text-gray-700 mb-4">
            Logisch zusammengehörige Daten, wie zum Beispiel ein Kunde und seine Bestellungen, sind auf der Festplatte
            oft weit voneinander entfernt gespeichert. Um diese Verbindungen wiederherzustellen, sind rechenintensive
            JOIN-Operationen notwendig, die das System verlangsamen.
          </p>
          <p className="text-gray-700 mb-6">
            MycoDB (Mycelium Database) ist ein experimenteller, biologisch inspirierter Ansatz, der dieses Problem nicht
            durch starre mathematische Indexstrukturen wie B-Trees, sondern durch Emergenz und Selbstorganisation löst.
            Es ist mehr als nur eine Datenbank; es ist eine innovative Anwendung und der primäre Proof-of-Concept für
            das Micro-Swarm Framework, eine hochperformante C++ Artificial-Life-Engine, die darauf ausgelegt ist, komplexe
            Probleme durch das Zusammenspiel einfacher, lokaler Regeln zu lösen. Als eingebettete Engine, ausgeliefert als
            Teil der Micro-Swarm DLL, wendet MycoDB diese Prinzipien an, um eine der klassischsten Herausforderungen der
            Datenspeicherung zu meistern.
          </p>

          <h4 className="text-lg font-semibold mt-6 mb-3">1. Das Kernkonzept: Biologisches Indexing statt starrer Strukturen</h4>
          <p className="text-gray-700 mb-4">
            MycoDB übersetzt traditionelle Datenbankkonzepte in biologische Äquivalente. Anstatt Daten in starren
            Strukturen abzulegen, werden sie zu physischen Objekten in einer dynamischen 2D-Welt, die von einem
            Agentenschwarm intelligent geordnet werden.
          </p>
          <div className="overflow-x-auto bg-white border border-gray-200 rounded-lg shadow-sm mb-6">
            <table className="min-w-full text-sm text-left">
              <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                <tr>
                  <th className="px-4 py-3">Traditionelles SQL-Konzept</th>
                  <th className="px-4 py-3">Biologisches Äquivalent</th>
                  <th className="px-4 py-3">Funktion & Erklärung</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                {mycodmConceptTable.map((row) => (
                  <tr key={row.sql}>
                    <td className="px-4 py-3 font-semibold text-gray-900">{row.sql}</td>
                    <td className="px-4 py-3 text-gray-700">{row.bio}</td>
                    <td className="px-4 py-3 text-gray-700">{row.detail}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <h4 className="text-lg font-semibold mt-6 mb-3">2. Der Mechanismus: Wie der „Schwarm-Sort“ Daten organisiert</h4>
          <p className="text-gray-700 mb-4">
            Die Datenorganisation in MycoDB ist kein einmaliger, statischer Vorgang, sondern ein aktiver Prozess, der als
            „Schwarm-Sort“ bezeichnet wird. Dabei wird ein Agentenschwarm genutzt, um die Daten physisch zu sortieren.
            Der Prozess der Datenaufnahme (Ingestion) läuft wie folgt ab:
          </p>
          <ol className="list-decimal pl-6 space-y-2 text-gray-800 mb-4">
            <li>Ein klassischer SQL-Dump wird über die API-Funktion ms_db_load_sql() importiert, wobei die Datensätze zunächst an zufälligen Positionen existieren.</li>
            <li>Tausende von „Carrier Agents“ (Träger-Agenten) werden in einem 2D-Raum erzeugt.</li>
            <li>Jeder Agent nimmt einen einzelnen Datensatz (eine „Payload“) auf und wird zu dessen Träger.</li>
            <li>Besitzt der Datensatz einen Fremdschlüssel (z.B. ArtistId), sucht der Agent die Position des zugehörigen Elterndatensatzes, indem er dessen einzigartiger Pheromon-Signatur folgt.</li>
            <li>Der Agent transportiert seine Payload physisch durch den 2D-Raum zu seinem Ziel.</li>
          </ol>
          <p className="text-gray-700 mb-4">
            Das Ergebnis dieses Prozesses, der über ms_db_run_ingest() gestartet wird, ist faszinierend: Nach tausenden von
            Simulationsschritten bilden sich organische Cluster.
          </p>
          <blockquote className="border-l-4 border-blue-400 bg-blue-50 text-blue-900 px-4 py-3 italic mb-6">
            „Alle Alben eines Künstlers liegen nun physisch um den Künstler herum.“
          </blockquote>
          <p className="text-gray-700 mb-6">
            Der optimierte Zustand wird anschließend als .myco-Datei persistiert. Diese physische Vorsortierung ist der
            Schlüssel zur außergewöhnlichen Abfragegeschwindigkeit von MycoDB.
          </p>

          <h4 className="text-lg font-semibold mt-6 mb-3">3. Der Nutzen: Wie komplexe JOINs zu schnellen Nachbarschaftssuchen werden</h4>
          <p className="text-gray-700 mb-4">
            Da logisch zusammengehörige Daten bereits physisch nahe beieinander liegen, werden komplexe und langsame
            JOIN-Operationen zu extrem schnellen, lokalen Suchen. MycoDB führt dazu ein neues, kontextbasiertes
            Abfragemodell ein, das auf „Fokus und Radius“ basiert.
          </p>
          <ul className="list-disc pl-6 space-y-2 text-gray-800 mb-4">
            <li><strong>GOTO:</strong> Setzt den "Lesekopf" der Datenbank physisch auf die Position eines bestimmten Datensatzes.</li>
            <li><strong>RADIUS:</strong> Definiert einen Suchradius (z.B. 10 Einheiten) um diesen Fokuspunkt.</li>
          </ul>
          <p className="text-gray-700 mb-4">
            Alle folgenden Abfragen werden ausschließlich in diesem kleinen, lokalen Bereich ausgeführt. Es ist entscheidend
            zu verstehen, dass dieses Fokus-Modell keine proprietäre Abfragesprache ersetzt. Im Gegenteil: Es fungiert als
            eine kontextsetzende Schicht über einer leistungsfähigen, in C++ implementierten SQL-Light-Engine. Diese
            unterstützt SELECT, WHERE, JOINs, Subqueries und CTEs (WITH).
          </p>

          <h5 className="text-base font-semibold mt-4 mb-2">Praxis-Beispiel:</h5>
          <ol className="list-decimal pl-6 space-y-2 text-gray-800 mb-6">
            <li><strong>Fokus finden (Globaler Scan):</strong> Artist Name="AC/DC" --&gt; Output: ID=5, x=120, y=45</li>
            <li><strong>Fokus setzen (Physischer Sprung):</strong> GOTO ID=5</li>
            <li><strong>Lokal abfragen (Extrem schnell):</strong> SELECT * FROM Album RADIUS 10</li>
          </ol>
          <p className="text-gray-700 mb-6">
            Dieser Ansatz findet alle Ergebnisse sofort, da die Agenten die Alben bereits physisch zum Künstler getragen haben.
            Ein teurer Scan der gesamten Tabelle ist nicht mehr nötig.
          </p>

          <h4 className="text-lg font-semibold mt-6 mb-3">4. Fazit: Eine Datenbank, der man beim Denken zusehen kann</h4>
          <p className="text-gray-700 mb-4">
            Die zentrale Innovation von MycoDB beweist, dass Datenbanksysteme nicht starr sein müssen. Es dient als greifbarer
            Beweis für die zentrale These des Micro-Swarm-Projekts: dass komplexe, adaptive Systeme aus einfachen, lokalen
            Regeln entstehen können.
          </p>
          <p className="text-gray-700 mb-4">
            Durch die Anwendung von Prinzipien der Schwarmintelligenz erreicht MycoDB eine emergente, selbstorganisierende
            Defragmentierung von Information. Anstatt dass ein Entwickler manuelle Indizes definieren muss, organisiert sich
            die Datenbank selbst.
          </p>
          <blockquote className="border-l-4 border-blue-400 bg-blue-50 text-blue-900 px-4 py-3 italic">
            „Es ist die erste Datenbank, die man nicht nur abfragen, sondern der man beim 'Denken' (Sortieren) zusehen kann.“
          </blockquote>
        </section>

        {/* Benchmark & Emergenz */}
        <section id="benchmark" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Benchmark & Emergenz: Der Morningstar-Report</h2>
          <div className="text-gray-700 mb-6">
            <p><strong>Datum:</strong> 11. Januar 2026</p>
            <p><strong>System:</strong> Morningstar (Ryzen 5 5600H, APU mit GPU-Offloading)</p>
            <p><strong>Ziel:</strong> Vergleich von Daten-Dichte (Kompaktheit) vs. Daten-Freiraum (Emergenz) zur Optimierung von SQL-Abfragen.</p>
          </div>

          <BenchmarkVisual />

          <h3 className="text-xl font-semibold mb-4">1. Das Duell der Welten (Side-by-Side)</h3>
          <p className="text-gray-700 mb-4">
            Um die 500.000 Payloads zu organisieren, haben wir zwei gegensaetzliche Philosophien getestet.
          </p>
          <div className="overflow-x-auto mb-6">
            <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
              <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                <tr>
                  <th className="px-4 py-3">Parameter</th>
                  <th className="px-4 py-3">Welt A: "Der Massanzug"</th>
                  <th className="px-4 py-3">Welt B: "Die weite Welt"</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-200">
                <tr>
                  <td className="px-4 py-3 font-semibold">Gitter-Groesse (--size)</td>
                  <td className="px-4 py-3">900</td>
                  <td className="px-4 py-3 font-semibold">2048</td>
                </tr>
                <tr>
                  <td className="px-4 py-3 font-semibold">Gesamtflaeche</td>
                  <td className="px-4 py-3">0,81 Mio. Zellen</td>
                  <td className="px-4 py-3 font-semibold">4,19 Mio. Zellen</td>
                </tr>
                <tr>
                  <td className="px-4 py-3 font-semibold">Daten-Dichte</td>
                  <td className="px-4 py-3">~62 % (Kompakt)</td>
                  <td className="px-4 py-3 font-semibold">~12 % (Sparse/Leer)</td>
                </tr>
                <tr>
                  <td className="px-4 py-3 font-semibold">Zeit (Global Scan)</td>
                  <td className="px-4 py-3">~46 Sek.</td>
                  <td className="px-4 py-3 font-semibold">~27 Sek.</td>
                </tr>
                <tr>
                  <td className="px-4 py-3 font-semibold">Zeit (Lokal r=100)</td>
                  <td className="px-4 py-3">1.544 ms</td>
                  <td className="px-4 py-3 font-semibold text-green-700">356 ms</td>
                </tr>
                <tr>
                  <td className="px-4 py-3 font-semibold">Effizienz-Gewinn</td>
                  <td className="px-4 py-3">Referenz</td>
                  <td className="px-4 py-3 font-semibold">4,3x schneller als die kompakte Welt</td>
                </tr>
              </tbody>
            </table>
          </div>

          <TheoryBox title="Warum ist groesser schneller?">
            In der kompakten Welt (900) sind die Daten gequetscht und der Radius-Scan trifft viel Rauschen. In der weiten
            Welt (2048) entstehen saubere Cluster-Inseln. Der lokale Scan trifft fast nur relevante Daten: "Fleisch" statt
            "Fett".
          </TheoryBox>

          <h3 className="text-xl font-semibold mt-8 mb-4">2. Skalierung des Radius (Geometrie-Falle)</h3>
          <ul className="list-disc pl-6 text-gray-700 mb-6">
            <li><strong>Radius 1000:</strong> ~27,5 s (fast global)</li>
            <li><strong>Radius 500:</strong> ~7,5 s</li>
            <li><strong>Radius 100:</strong> 356 ms (Cluster-Kern)</li>
          </ul>

          <h3 className="text-xl font-semibold mt-8 mb-4">3. Morningstar-Fazit</h3>
          <p className="text-gray-700 mb-4">
            Waehle ein Gitter, das deutlich groesser ist als die Datenmenge (Dichte ~10-15%). Nutze den Ingest-Prozess
            (100-200 Steps), um Informations-Inseln entstehen zu lassen. Eine Abfrage mit <code>goto</code> und kleinem
            <code>radius</code> (&lt; 100) transformiert Minuten in Millisekunden.
          </p>
          <MycoNote>
            <strong>Fazit:</strong> 356 ms fuer 500.000 Datensaetze ohne klassischen Index ist ein Triumph der C++ Architektur.
          </MycoNote>
        </section>

        <section id="cmdref" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Kommando-Referenzhandbuch</h2>
          <p className="text-gray-700 mb-6">
            Dieses Dokument beschreibt die Interaktionsmoeglichkeiten mit der <strong>micro_swarm Engine</strong> im
            <code>db_shell</code>-Modus.
          </p>

          <div className="space-y-8">
            <div>
              <h3 className="text-xl font-semibold mb-3">1. Raeumliche Navigation (Spatial Control)</h3>
              <p className="text-gray-700 mb-4">Diese Befehle steuern den physischen Fokus innerhalb des Daten-Gitters.</p>
              <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Befehl</th>
                    <th className="px-4 py-3">Beschreibung</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  <tr>
                    <td className="px-4 py-3 font-semibold">goto &lt;id&gt;</td>
                    <td className="px-4 py-3">Spricht den Lesekopf direkt auf die (x,y) Position der angegebenen Payload-ID an.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">radius &lt;n&gt;</td>
                    <td className="px-4 py-3">Definiert den Wirkungskreis fuer nachfolgende Abfragen. Ein kleinerer Radius bedeutet extreme Geschwindigkeit.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">focus</td>
                    <td className="px-4 py-3">Zeigt die aktuellen Koordinaten und den gesetzten Radius an.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">unfocus</td>
                    <td className="px-4 py-3">Entfernt den raeumlichen Filter und kehrt zum globalen Scan zurueck.</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div>
              <h3 className="text-xl font-semibold mb-3">2. Inspektion &amp; Metadaten</h3>
              <p className="text-gray-700 mb-4">Schnelle Einblicke in die Struktur der geladenen Welt.</p>
              <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Befehl</th>
                    <th className="px-4 py-3">Beschreibung</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  <tr>
                    <td className="px-4 py-3 font-semibold">tables</td>
                    <td className="px-4 py-3">Listet alle Tabellen auf, die im .myco Abbild vorhanden sind.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">stats</td>
                    <td className="px-4 py-3">Zeigt die Anzahl der Datensaetze (Payloads) pro Tabelle an (Sofort-Abruf).</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">schema &lt;table&gt;</td>
                    <td className="px-4 py-3">Listet alle Spaltennamen der angegebenen Tabelle auf.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">describe &lt;table&gt;</td>
                    <td className="px-4 py-3">Kombiniert das Schema mit einem physischen Beispiel-Datensatz aus dem Gitter.</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div>
              <h3 className="text-xl font-semibold mb-3">3. Abfragen &amp; Suche</h3>
              <p className="text-gray-700 mb-4">Vom intuitiven Schnellzugriff bis hin zum komplexen Standard-SQL.</p>
              <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Befehl</th>
                    <th className="px-4 py-3">Beschreibung</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  <tr>
                    <td className="px-4 py-3 font-semibold">&lt;Table&gt; &lt;ID&gt;</td>
                    <td className="px-4 py-3">PK-Shortcut: Findet den Datensatz in Tabelle X mit der Primaerschluessel-ID Y.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">&lt;Table&gt; &lt;Col&gt;=&lt;Val&gt;</td>
                    <td className="px-4 py-3">Column-Shortcut: Findet alle Datensaetze, die in einer bestimmten Spalte den Wert haben.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">&lt;Col&gt;=&lt;Val&gt;</td>
                    <td className="px-4 py-3">Global-Shortcut: Durchsucht alle Tabellen nach diesem Spaltenwert.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">sql &lt;statement&gt;</td>
                    <td className="px-4 py-3">Fuehrt echtes SQL aus (SELECT, INSERT, UPDATE, DELETE).</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">explain</td>
                    <td className="px-4 py-3">Analysiert die letzte Query: War sie lokal oder global? Wie viele Hits gab es?</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div>
              <h3 className="text-xl font-semibold mb-3">4. Delta-Store &amp; Transaktionen</h3>
              <p className="text-gray-700 mb-4">Aenderungen sicher verwalten, ohne den Hauptspeicher sofort zu korrumpieren.</p>
              <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Befehl</th>
                    <th className="px-4 py-3">Beschreibung</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  <tr>
                    <td className="px-4 py-3 font-semibold">delta</td>
                    <td className="px-4 py-3">Statusbericht: Wie viele Aenderungen (Upserts/Tombstones) liegen im Wartebereich?</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">delta show</td>
                    <td className="px-4 py-3">Listet alle ausstehenden Schreiboperationen im Detail auf.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">merge</td>
                    <td className="px-4 py-3">Commit: Schreibt das Delta permanent in das Gitter und loest ein Re-Clustering aus.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">merge auto &lt;n&gt;</td>
                    <td className="px-4 py-3">Aktiviert das automatische Mergen, sobald n Aenderungen erreicht sind.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">undo</td>
                    <td className="px-4 py-3">Macht die letzte Aenderung im Delta-Store rueckgaengig.</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div>
              <h3 className="text-xl font-semibold mb-3">5. Konfiguration &amp; Ausgabe</h3>
              <p className="text-gray-700 mb-4">Steuerung der Darstellung und Verarbeitungsmenge.</p>
              <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Befehl</th>
                    <th className="px-4 py-3">Beschreibung</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  <tr>
                    <td className="px-4 py-3 font-semibold">limit &lt;n|off&gt;</td>
                    <td className="px-4 py-3">Begrenzt die Anzahl der angezeigten Zeilen (schont das Terminal).</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">show &lt;cols|off&gt;</td>
                    <td className="px-4 py-3">Setzt einen dauerhaften Spaltenfilter fuer alle kommenden Abfragen.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">format &lt;table|csv|json&gt;</td>
                    <td className="px-4 py-3">Wechselt den Ausgabestil (Tabelle fuer Menschen, CSV/JSON fuer Maschinen).</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">export &lt;csv|json&gt; &lt;path&gt;</td>
                    <td className="px-4 py-3">Speichert das letzte Abfrageergebnis direkt in eine Datei.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">cls | clear</td>
                    <td className="px-4 py-3">Leert das Konsolenfenster.</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div>
              <h3 className="text-xl font-semibold mb-3">6. Ingest &amp; Import</h3>
              <p className="text-gray-700 mb-4">Laedt einen SQL-Dump und ersetzt die aktuelle Session.</p>
              <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Befehl</th>
                    <th className="px-4 py-3">Beschreibung</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  <tr>
                    <td className="px-4 py-3 font-semibold">ingest &lt;sql_path&gt; [rules_path]</td>
                    <td className="px-4 py-3">Fuehrt einen Ingest aus einer SQL-Datei aus. Optional mit ingest_rules.json.</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div>
              <h3 className="text-xl font-semibold mb-3">7. Produktivitaet &amp; Makros</h3>
              <p className="text-gray-700 mb-4">Automatisierung wiederkehrender Aufgaben.</p>
              <table className="min-w-full text-sm text-left border border-gray-200 rounded-lg overflow-hidden">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Befehl</th>
                    <th className="px-4 py-3">Beschreibung</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  <tr>
                    <td className="px-4 py-3 font-semibold">history</td>
                    <td className="px-4 py-3">Zeigt die Liste der zuletzt eingegebenen Befehle.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">last | redo | !n</td>
                    <td className="px-4 py-3">Wiederholt die letzte Query oder einen spezifischen Befehl aus der Historie.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">save &lt;name&gt; [cmd]</td>
                    <td className="px-4 py-3">Speichert den aktuellen oder einen angegebenen Befehl als Makro.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">run &lt;name&gt;</td>
                    <td className="px-4 py-3">Fuehrt ein gespeichertes Makro aus.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">macros save [path]</td>
                    <td className="px-4 py-3">Exportiert alle Makros in eine JSON-Datei.</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-3 font-semibold">macros load &lt;path&gt;</td>
                    <td className="px-4 py-3">Laedt Makros aus einer JSON-Datei nach.</td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>

          <div className="mt-10">
            <h3 className="text-xl font-semibold mb-3">8. Genetic Ingest Rules (nur db_ingest)</h3>
            <p className="text-gray-700 mb-4">
              Regelbasierte Cluster-Bildung beim Ingest. Die Regeln wirken nur waehrend des Ingest-Prozesses.
            </p>
            <CodeBlock code="--ingest-rules <path>" />
            <CodeBlock code=".\\micro_swarm.exe --mode db_ingest --input data.sql --output data.myco --ingest-rules docs\\ingest_rules.example.json" />
            <ul className="list-disc pl-6 text-gray-700 mt-4">
              <li><code>foreign_key</code> (Default: <code>.*_id$</code>)</li>
              <li><code>trait_cluster</code></li>
              <li><code>domain_cluster</code></li>
            </ul>
            <p className="text-gray-700 mt-3">Beispiel-JSON: <code>docs/ingest_rules.example.json</code></p>
          </div>
        </section>

        {/* FAQ */}
        <section id="faq" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">FAQ für Micro-Swarm</h2>
          <p className="text-gray-700 mb-6">
            Dieser Leitfaden dient zur Überprüfung des Verständnisses der Kernkonzepte, der technischen Implementierung
            und der Anwendungsfälle des Micro-Swarm-Systems. Er umfasst ein Quiz, einen Antwortschlüssel, Essay-Fragen
            und ein Glossar der Schlüsselbegriffe.
          </p>

          <div className="mb-8">
            <h3 className="text-xl font-semibold mb-3">Quiz: Überprüfung des Wissens</h3>
            <p className="text-sm text-gray-600 mb-4">
              Beantworten Sie die folgenden zehn Fragen in jeweils zwei bis drei Sätzen. Ihre Antworten sollten sich
              ausschließlich auf die bereitgestellten Quelldokumente stützen.
            </p>
            <ol className="list-decimal pl-6 space-y-3 text-gray-800">
              {faqQuizQuestions.map((question, index) => (
                <li key={`quiz-${index}`}>{question}</li>
              ))}
            </ol>
          </div>

          <div className="mb-8">
            <h3 className="text-xl font-semibold mb-3">Antwortschlüssel</h3>
            <ol className="list-decimal pl-6 space-y-3 text-gray-800">
              {faqAnswerKey.map((answer, index) => (
                <li key={`answer-${index}`}>{answer}</li>
              ))}
            </ol>
          </div>

          <div className="mb-8">
            <h3 className="text-xl font-semibold mb-3">Essay-Fragen</h3>
            <p className="text-sm text-gray-600 mb-4">
              Formulieren Sie umfassende Antworten auf die folgenden Fragen. Diese Fragen erfordern die Synthese von
              Informationen aus verschiedenen Abschnitten der Quelldokumente.
            </p>
            <ol className="list-decimal pl-6 space-y-3 text-gray-800">
              {faqEssayQuestions.map((question, index) => (
                <li key={`essay-${index}`}>{question}</li>
              ))}
            </ol>
          </div>

          <div className="mb-8">
            <h3 className="text-xl font-semibold mb-3">Praxis-Tipp: Makros speichern</h3>
            <p className="text-sm text-gray-600 mb-4">
              Makros gelten nur fuer die aktuelle Shell-Session. Speichere sie als JSON und lade sie spaeter wieder.
            </p>
            <CodeBlock
              code={`save toptracks sql SELECT TrackId,Name FROM Track WHERE AlbumId=1\nmacros save\nmacros load 2026-01-11_macros.json`}
            />
          </div>

          <div>
            <h3 className="text-xl font-semibold mb-3">Glossar</h3>
            <div className="overflow-x-auto bg-white border border-gray-200 rounded-lg shadow-sm">
              <table className="min-w-full text-sm text-left">
                <thead className="bg-gray-100 text-gray-700 uppercase tracking-wide">
                  <tr>
                    <th className="px-4 py-3">Begriff</th>
                    <th className="px-4 py-3">Definition</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-200">
                  {faqGlossary.map((entry) => (
                    <tr key={entry.term}>
                      <td className="px-4 py-3 font-semibold text-gray-900">{entry.term}</td>
                      <td className="px-4 py-3 text-gray-700">{entry.definition}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </section>

        {/* Cheat Sheet */}
        <section id="cheatsheet" className="mb-16">
          <h2 className="text-2xl font-bold mb-6 border-b pb-2">Cheat Sheet (Die 19 wichtigsten)</h2>
          <div className="bg-gray-900 text-green-400 p-6 rounded-lg font-mono text-sm overflow-x-auto shadow-2xl">
            <pre>
{`help             - Hilfe anzeigen
tables           - Tabellen auflisten
describe <t>     - Struktur & Beispiel
schema <t>       - Nur Spalten
stats            - Zeilen zählen
show <cols|off>  - Filter setzen
limit <n|off>   - Limit setzen
sql <stmt>       - SQL ausführen
macros save      - Makros speichern
macros load <p>  - Makros laden
sort <col>       - Sortieren
sort reset       - Sortierung zurück
focus / unfocus  - Fokus-Status
radius <n>       - Suchradius
delta            - Delta-Status
delta show       - Details
merge            - Festschreiben
undo             - Rückgängig
export <csv|j>   - Exportieren
explain          - Query-Analyse
exit             - Shell beenden`}
            </pre>
          </div>
        </section>

        <footer className="text-center text-gray-400 text-sm mt-20 border-t pt-8 pb-12">
          &copy; 2026 Ralf Krümmel - Artificial Life Research | MycoDB Lab
        </footer>
      </main>
    </div>
  );
};

export default App;
