# Conventions.md — LIMUNIMI/FeatureExtractor

> [!IMPORTANT]
> Riferimento unico per le convenzioni di programmazione del progetto, per codice scritto a
> mano e per codice generato con l'aiuto di un assistente AI (vedi §5).

---

## 1. Architettura

- **CMake** unica fonte di verità per build e dipendenze, al posto degli exporter Projucer per
  singolo SO. Un solo `CMakeLists.txt` genera i project file nativi (`cmake -G Xcode` su
  macOS, `-G "Visual Studio 17 2022"` su Windows, `-G Ninja` per build headless/CI su Linux),
  senza configurazioni duplicate da tenere allineate a mano. Niente percorsi locali cablati.
- **`feature_core`** è una libreria statica senza dipendenze da moduli GUI: il motore deve
  restare compilabile e testabile senza `juce_gui_basics`.
- **Setup e Run separati**, strutturalmente: nessuna feature/functional si aggiunge o si
  rimuove a motore in esecuzione.
- **`processBlock()` è real-time-safe**: niente `new`/allocazioni, niente `setSize()`, niente
  calcoli pesanti sui parametri lì dentro. Tutto ciò che può essere preparato va preparato in
  `prepareToPlay()`.

---

## 2. Naming

| Elemento | Convenzione | Esempio |
|---|---|---|
| Classi, struct, interfacce | `PascalCase`, nessun prefisso | `Feature`, `SignalSource` |
| Metodi, funzioni, variabili (anche membri) | `camelCase` | `processBlock()`, `blockSize` |
| Costanti globali, `constexpr` | `UPPER_SNAKE_CASE` | `BATCH_BLOCK_SIZE` |
| File | `PascalCase.h` / `.cpp`, un file = una classe principale | `Spectrogram.cpp` |

**Niente Hungarian notation**: né `m_` sui membri né `I` sulle interfacce — ci pensa l'IDE
(syntax highlighting) a distinguere un membro da una variabile locale, e un'interfaccia si
riconosce dal contesto (metodi virtuali puri) più che dal nome.

Simboli di codice e messaggi a runtime: **inglese**. I documenti di design restano in italiano.

---

## 3. Principi di design

Non vanno ripetuti a memoria, ma tenuti presenti quando si disegna una classe o un'interfaccia:

- **Aperto/chiuso** — il design si estende, non si riscrive.
- **Singola responsabilità** — una classe ha un solo motivo per cambiare.
- **Basso accoppiamento** — disaccoppiare gli oggetti il più possibile.
- **Composizione più che ereditarietà.**
- **Legge di Demetra** — un oggetto interagisce solo con ciò che gli è "vicino" di un livello.
- **Inversione delle dipendenze** — dipendere da astrazioni, non da implementazioni; le classi
  di alto livello non dipendono da quelle di basso livello (entrambe dipendono da un'astrazione).
- **Programmare verso l'interfaccia**, non verso l'implementazione.

In una parola: **SOLID**, oltre a **DRY**, **KISS**, **YAGNI** e i
[JUCE Coding Standards](https://juce.com/).
Per allenare l'occhio (esercizi, non regole):
[Object Calisthenics](https://williamdurand.fr/2013/06/03/object-calisthenics/).

---

## 4. Commenti

- Il codice deve essere leggibile senza commenti: se un nome richiede una spiegazione, si
  rinomina, non si commenta.
- Commenti solo per il *perché*, mai per il *cosa* (che si legge già dal codice).
- Ogni metodo pubblico ha un'intestazione Doxygen (`/** ... */`): descrizione in una riga,
  `@param`/`@return` solo se non ovvi dal nome/tipo. Metodi privati banali (getter, `reset()`)
  ne fanno a meno.

  ```cpp
  /** Calcola la posizione stereo istantanea del blocco corrente.
      @param buffer  blocco stereo interleaved, normalizzato in [-1, 1]
      @return        valore in [-1, 1], 0 = centro
  */
  float computePan(const juce::AudioBuffer<float>& buffer);
  ```

---

## 5. Codice generato con AI

> [!TIP]
> Vibe coding sì, ma tutto il codice generato va letto e capito. Richieste piccole e puntuali, non "fai questo step del piano".

Ogni blocco generato da un assistente AI più lungo di poche righe va marcato:

**Nel codice**, subito sopra:
```cpp
// AI: generato con Claude — "guardia su vector vuoto in IQR::getResult"
// rivisto e capito da <iniziali>, 2026-07-31
```

**Nel commit**, come trailer:
```
AI-assisted: Claude (rivisto da <iniziali>)
```

Sotto soglia (una riga, un rename) non serve marcare nulla.

---

## 6. Dimensione di metodi e file

Un metodo fa una cosa sola — se per descriverlo serve una "e" o un "poi", va spezzato (è il
motivo per cui `run()` va scorporato in orchestratore + `processOneFile()`, P1 del piano).
Come segnale, non come regola rigida: oltre ~40-50 righe per metodo o ~300-400 per file, vale
la pena chiedersi se non convenga scomporre — per responsabilità, non per stare sotto un numero.

---

## 7. Configurazione e persistenza

- Ogni gruppo di parametri vive in una struct dedicata (`XxxSettings`) con default espliciti su
  ogni campo, mai in un `ValueTree` letto al volo senza un livello tipizzato sopra.
- Pattern uniforme `load(const juce::ValueTree&)` / `save() const -> juce::ValueTree`.
- Il setup porta un **numero di versione** fin da subito, con percorso di migrazione previsto
  anche quando non c'è ancora nulla da migrare.

---

## 8. Modelli dati

Niente coppie parallele non tipizzate (`vector<string> names` + `vector<float> values`
scorrelati) come struttura primaria — solo come formato di export finale. All'interno del
motore i risultati sono oggetti che impongono l'invariante nomi/valori.

---

## 9. Gestione degli errori

| Livello | Dove | Strategia |
|---|---|---|
| Core | feature, functional, settings | fail fast: `jassert`/eccezioni su invarianti violati |
| I/O e runtime | file, device audio, OSC | degrado controllato: log + skip, mai crash del batch |
| Confine applicativo | entry point, UI | cattura tutto, nessuna eccezione risale fino alla GUI |

---

## 10. Logging

Un unico punto per l'output diagnostico (niente `DBG`/`std::cout` sparsi), vocabolario di
livelli coerente. Log diagnostico e dati (CSV) restano canali separati.

---

## 11. Git: commit e versionamento

Formato commit (vedi [GIT-Cheat-Sheet.md](GIT-Cheat-Sheet.html)):
```
<emoji> <tipo>: <descrizione breve, imperativo>
```

| Emoji | Tipo | Quando |
|---|---|---|
| ✨ | `feat` | nuova funzionalità |
| 🐛 | `fix` | bug fix |
| 📝 | `docs` | documentazione |
| ♻️ | `refactor` | refactoring senza cambio di comportamento |
| 🚀🔧➕ | `chore` | versione / configurazione / dipendenza |

Un commit, un cambiamento logico. Il messaggio dice *cosa* e, se non ovvio, *perché*.

> [!TIP]
> Versionamento (da [piano-implementazione-v5](piano-implementazione-v5.md)): finché si
> procede fase per fase, tag `0.FASE.STEP` (si suppone che uno step abbia tanti commit, 
> si tagga solo il commit che lo risolve definitivamente, passado i test).
> Da `1.0`, `MAJOR.FEATURE.IMPROVEMENT` — MAJOR per rotture di compatibilità setup/CSV, 
> FEATURE per nuove funzionalità retrocompatibili, IMPROVEMENT per fix. 
> La versione taggata finisce nell'intestazione di provenienza dei CSV.

---

## 12. Dipendenze esterne

- Tutte le librerie di terze parti, **JUCE compresa**, vivono in `Libraries/` come submodule
  git, pinnate a un tag/commit — mai "ultima disponibile". Il codice resta fisicamente nel
  repo e ispezionabile dall'IDE, non scaricato invisibilmente in una cartella di build.
- Il `CMakeLists.txt` root si auto-inizializza se il submodule manca, così non serve ricordarsi
  `git submodule update --init --recursive` a ogni nuova persona che clona il repo:
  ```cmake
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/Libraries/JUCE/CMakeLists.txt")
      execute_process(COMMAND git submodule update --init --recursive
                       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  endif()
  ```
- `FetchContent` con `GIT_TAG` pinnato resta un'alternativa accettabile per dipendenze
  leggere aggiunte in seguito — le due tecniche non sono mutuamente esclusive, si sceglie per
  singola libreria.


