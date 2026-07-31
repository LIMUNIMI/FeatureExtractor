# FeatureExtractor — piano di implementazione per fasi (revisione 5)

> [!TIP]
> Questa analisi emerge dall'incrocio di miei appunti con una lettura del codice fatta da Claude. Commento alcune cose con tips come questo.

> [!TIP]
> Il documento è diviso in 3 parti: **Panoramica**, **Le Fasi**, **Note Finali**. La parte operativa è la seconda (Fasi) che consiste nel piano di implementazione, diviso in 16 fasi (0-14 + 1), ognuno con degli step atomici abbastanza semplici. Le Fasi 13 e 14 si possono fare in parallelo alle altre (ma dopo la 4 o la 5), e possono essere tesi a sé (per la 14 vedi [design-visualizzazione-v2.md](Design/design-visualizzazione-v2.md)). Anche la 6-bis si può fare un po' quando si vuole dopo la 6, ma non è abbastanza per una tesi.
>
> Non tutto deve essere svolto in una sola tesi (4 fasi delle 16 proposte possono bastare, con una quinta come potenziale estensione se c'è tempo).
>
> Intanto che ci lavora una persona per volta suggerisco di creare un tag per ogni fase (con un tag `0.0.0` ora). La semantica dei tag sarà `0.FASE.STEP` (non è un metodo standard, dovremmo usare le Issues, ma per ora ce lo facciamo andare bene, restiamo agili) poi ultimato il piano possiamo passare a MAJOR.FEATURE.IMPROVEMENT. Oppure potremmo anche fare che a inizio di ogni fase (milestone) gli step diventano Issues. Vedete voi in base a quanto siete Git-fluent.

> [!TIP]
Non sono contro il vibe coding, ma è requisito FONDAMENTALE che tutto il codice generato venga letto e capito e ci si sappia mettere mano. Piccole richieste puntuali, non "fai questo step del piano".

> [!TIP]
Vedere il file [Conventions.md](Design/Conventions.md) per le convenzioni di programmazione da seguire.

Documento di lavoro per l'evoluzione di `LIMUNIMI/FeatureExtractor`.
Analisi condotta sul commit `2496b32`.

Il piano è pensato per essere eseguito in ordine. **Alla fine di ogni fase il software è
compilabile, eseguibile e utilizzabile**: nessuna fase lascia il progetto in uno stato
intermedio. Ogni fase produce anche almeno una miglioria percepibile da chi usa il programma,
non solo dal codice.

---

# Parte I — Panoramica

## 1. Che cos'è il software oggi

Applicazione **JUCE standalone** (non plugin) per l'estrazione di descrittori audio, ~2.800
righe in 24 file sorgente. Due modalità operative:

- **Live** — l'audio arriva dalla scheda o da un file in riproduzione, le feature vengono
  calcolate sul thread audio e inviate come messaggi **MIDI** (control change, note) e **OSC**,
  con un throttling a frequenza impostabile.
- **Batch** — una lista di file audio viene processata su un thread di lavoro; per ogni file le
  feature vengono aggregate da statistiche di sintesi e scritte come **una riga di CSV**.

### Componenti

| Concetto | Classe | Ruolo |
|---|---|---|
| Descrittore | `Feature` (interfaccia) | calcola valori dal segnale, blocco per blocco |
| Aggregatore | `Functional` (interfaccia) | riduce una sequenza di valori a uno solo, per file |
| Motore | `MyFeatureExtractor` | possiede entrambe le modalità di esecuzione |
| Trasporto | `AudioPlayer` | riproduzione, lista file, transport |
| Uscita | `MidiMapper`, `OscMapper` | conversione dei valori in messaggi |
| Dato | `FeatureResult` | `StringArray names` + `vector<float> values` |

Feature implementate: **RRMS**, **PAN**, **Brightness** (dominio del tempo); **SpectralMoments**
(centroide, spread, skewness, kurtosis), **Chromagram**, **F0** (dominio della frequenza, ognuna
con la propria STFT interna).
Functional implementati: **Average**, **Median**, **StdDev**, **IQR**.

### Architettura attuale

```
MainComponent (UI)
├── FeatList  ── OwnedArray<Feature>       ← LA UI POSSIEDE IL MODELLO DI DOMINIO
├── FuncList  ── OwnedArray<Functional>    ←
└── MyFeatureExtractor
    │   vector<Feature*>  activeFeatures   ← puntatori grezzi, prestati dalla UI
    ├── LIVE  getNextAudioBlock()  →  processBlock → getResult → MIDI/OSC
    └── BATCH run()                →  cicla file, cicla blocchi, aggrega, scrive CSV
```

Il design corrisponde a una "Fase 1" deliberatamente semplice: elenco statico di feature,
nessun registro dinamico, nessun singleton, nessun mutex. Questa scelta era corretta e non
viene rimessa in discussione: il piano che segue la fa evolvere.

## 2. Cosa non funziona

### 2.1 Difetti bloccanti

> [!TIP]
> **Su B1 e B2:** Questo lo dice Claude, a me sembra che il software funzionasse, quini è da prendere con le pinze. Vale comunque la pena controllare (magari non viene usato `f->getResult(featPackage)` ma un altro meccanismo). Alice hai commenti a riguardo?

**B1 — La modalità batch non produce dati.** In `ExtractionEngine.cpp:156-163` il pacchetto dei
risultati viene creato vuoto e mai popolato: manca la chiamata `f->getResult(featPackage)`
dentro il ciclo sulle feature. I `Functional` ricevono quindi sempre un pacchetto vuoto, e il
CSV esce con l'intestazione corretta e le righe contenenti il solo nome del file.
Sul percorso live la chiamata è presente (riga 77): è un'omissione, non una scelta.

**B2 — Comportamento indefinito in `Average::store`.** `Functionals.cpp:5` esegue
`res.values[0]` su un `std::vector` che (per via di B1) è vuoto. Accesso fuori dai limiti.

**B3 — Comportamento indefinito in `IQR::getResult`.** Con meno di 4 campioni accumulati, i
vettori dei quartili restano vuoti e vengono comunque indicizzati.

**B4 — Membri non inizializzati.** `SpectralFeatures.h:26`:
`float centroid, spread, skewness, kurtosis = 0.0f;` inizializza solo l'ultimo. I primi tre
restano indeterminati finché non arriva il primo frame STFT valido — cosa che su file molto
corti può non accadere mai.

**B5 — Due `AudioDeviceManager` distinti.** `MainComponent` eredita il proprio da
`AudioAppComponent`; `MyFeatureExtractor` ne dichiara un secondo per il MIDI e lo inizializza
aprendo un ulteriore device audio. `prepareLiveFeatures()` legge sample rate e block size da
**quello sbagliato**: le feature vengono preparate con parametri che possono non corrispondere
al device realmente in uso.

**B6 — Chiusura non sicura durante il batch.** `~MyFeatureExtractor()` non chiama
`stopThread()`, e in `MainComponent.h` l'extractor è dichiarato *prima* di `featList`: alla
distruzione le `Feature` muoiono per prime, mentre il motore ne detiene puntatori grezzi e
potenzialmente un thread ancora attivo.

**B7 — I/O di rete e di sistema sul thread audio.** `getNextAudioBlock` chiama direttamente
`oscSender.send(msg)`, che scrive su un socket, e `sendBlockOfMessagesNow`, che scrive su un
device MIDI di sistema. Sono violazioni real-time della stessa famiglia dell'allocazione di
memoria: possono bloccare per un tempo arbitrario e produrre interruzioni dell'audio.

**B8 — Guardia mancante in `RRMS::processBlock` per `windowSize == 0`.** Senza `prepareToPlay()`,
`windowSize` resta a zero e il calcolo dell'inviluppo va in divisione per zero e in un `% 0`
sull'indice circolare della history. Oggi il rischio è basso perché un solo chiamante costruisce
e avvia le feature in ordine fisso, ma quella garanzia sparisce non appena il motore ha più di un
chiamante (Fase 3) e più di un ciclo di vita di avvio (Fase 9, dove è l'host del plugin a decidere
quando chiamare `prepare`/`process`).

### 2.2 Difetti di validità dei risultati

**V1 — I segnali delle feature sono sottocampionati e con alias.** `RRMS` calcola il valore per
ogni campione ma ne conserva **solo l'ultimo del blocco**
(`rmsValue = aux.getSample(0, numSamples-1)`). È campionamento puntuale, quindi aliasing. In
batch il blocco è di 4096 campioni: a 44,1 kHz si campiona l'inviluppo a **10,8 Hz**, mentre una
finestra RMS da 35 ms lascia passare fluttuazioni fino a 15-30 Hz. In live il blocco è di 512
campioni e si campiona a 86 Hz, dove il problema non si pone.

Due conseguenze: **lo stesso file analizzato in live e in batch dà numeri diversi**, e le
statistiche di `Median`, `IQR` e `StdDev` in batch sono calcolate su un segnale con aliasing.

**V2 — Il gate di silenzio esiste solo in `Average`.** `Functionals.cpp:5` scarta i frame sotto
soglia, ma solo per la media. `Median`, `StdDev` e `IQR` accumulano anche il silenzio: media e
deviazione standard si riferiscono a popolazioni diverse e non sono coerenti fra loro. In più
il gate assume che il primo valore del pacchetto sia un livello, cosa vera solo se l'utente ha
selezionato RRMS come prima feature.

**V3 — La latenza non è compensata né dichiarata.** `STFT::getLatencyInSamples()` esiste ed è
corretto, ma non viene mai chiamato. Le feature temporali sono istantanee, quelle spettrali sono
in ritardo di circa `fftSize` campioni, e finiscono nella stessa riga come se fossero simultanee.

### 2.3 Debito strutturale

**D1 — `run()` fa tre lavori.** Novanta righe che contengono, annidati, l'orchestratore (ciclo
sui file, progresso, annullamento), il worker (lettura, ciclo sui blocchi) e il sink (apertura
dello stream, formattazione dell'intestazione e delle righe). Finché restano fusi, la
parallelizzazione è impossibile.

**D2 — La UI possiede il modello di dominio.** Le `Feature` sono costruite e possedute da
`FeatList`, un `juce::Component`. Il motore non può esistere né essere testato senza
l'interfaccia grafica, e ogni feature è di fatto un'istanza unica condivisa fra tutte le
sessioni.

**D3 — Nessun oggetto `Settings`, nessuna persistenza.** I parametri sono cablati nei
costruttori (`RRMS(0.035f, 1.0f)`, `SpectralFeature(10)`), incluso un
`mStrengthThreshold = 0.15` marcato `// DEBUG !!!! 0.5` con il valore di produzione lasciato in
commento. Non essendoci parametri, non esiste il concetto di "due istanze diverse della stessa
feature", né alcun modo di salvare e ricaricare una configurazione di analisi. Un
`//TODO: usare valuetreestate` nel codice segnala già il problema.

**D4 — Calcoli duplicati.** `F0` e `Chromagram` usano entrambe `fftOrder = 12` e calcolano **la
stessa identica FFT** due volte per hop. `Brightness` possiede internamente un `RRMS` con i
parametri di default, ricalcolando ciò che l'istanza in lista già calcola. Il downmix
stereo→mono è copiato in tre file e rieseguito una volta per feature attiva. Con tutte le
feature selezionate si paga circa il **40-50% di calcolo in più** del necessario.

**D5 — `FeatureResult` fa quattro lavori.** Trasporta insieme lo schema (i nomi delle colonne,
che sono invarianti) e i valori (che cambiano ogni frame), non distingue i generi di dato, non
porta semantica di dominio, e usa `isEmpty()` come canale d'errore per tre condizioni diverse.
Il sintomo visibile: `Chromagram::createResultPackage` costruisce l'array dei nomi delle note
**a ogni chiamata**, cioè dodici allocazioni di stringa per frame sul thread audio.

**D6 — I mapper sono chiusi alle estensioni.** `FeatureMapper.cpp` è una catena di `if/else` sui
nomi delle feature, con dentro conversioni logaritmiche e soglie cablate — incluso un commento
che ammette che il mapping di skewness e kurtosis è «basato su osservazione perché non c'è un
range ben definito». Aggiungere una feature richiede di modificare il mapper, perché il tipo di
dato non porta l'informazione che servirebbe (unità di misura, range atteso, scala).

**D7 — `createResultPackage` e `getResult` sono lo stesso metodo.** In tutte e sei le feature il
secondo si limita a chiamare il primo. Due metodi virtuali puri per una sola operazione.

**D8 — Un solo modo di usare il software.** Esiste un unico target che mescola motore e
interfaccia grafica. Non è possibile usarlo da terminale (quindi non è possibile un'analisi
riproducibile e versionabile), né inserirlo in un DAW. Il motore chiama direttamente
`AlertWindow` e `NativeMessageBox`, quindi non è nemmeno separabile allo stato attuale.

**D9 — Aggiungere una feature costa quattro interventi in tre file** (classe, lista nella UI,
ramo nel mapper MIDI, eventuale mapper OSC), senza documentazione né casi di prova di
riferimento.

**D10 — L'analisi non è descrivibile senza trascrizione manuale.** Il software conosce
esattamente che cosa ha calcolato e con quali parametri, ma niente di tutto ciò è esportabile.
Chi deve scrivere la sezione metodologica di un articolo ricopia a mano, con il risultato che le
informazioni meno appariscenti — hop, funzione finestra, trattamento del silenzio, variante
esatta di un algoritmo — vengono sistematicamente omesse, e che nessun lettore può verificare
la corrispondenza fra la descrizione e ciò che è stato eseguito.

**D11 — Stato nascosto in `MidiMapper`.** `lastNote` e `lastChord` sono tenuti per generare
eventi note-on/note-off da `F0` e `Chromagram`: logica di generazione di eventi infilata dentro
quello che dovrebbe essere un semplice convertitore di valori. Non è un difetto da correggere con
un refactoring locale: è una domanda di design rimasta aperta — *dove vive lo stato di una
mappatura event-like?* — che resta senza risposta anche dopo l'introduzione del grafo (Fase 4) e
si fa più costosa non appena il mapping diventa una tabella dati modificabile a runtime (Fase 8).

## 3. Dove si vuole arrivare

### 3.1 Il motore

La forma finale è un **grafo di elaborazione risolto in fase di preparazione ed eseguito senza
allocazioni**. I concetti che compaiono nelle fasi seguenti:

**Nodo (`FeatureNode`).** Unità di calcolo con porte d'ingresso e d'uscita tipizzate. Ogni porta
dichiara `{rate, canali, semantica}`: un blocco audio multicanale, uno spettro di magnitudini,
un inviluppo a sample rate e un vettore a frame rate sono cose diverse e il sistema lo sa. Un
nodo possiede il proprio buffer di uscita, allocato in fase di preparazione.

**Grafo (`ProcessingGraph`).** Elenco piatto di nodi in ordine topologico. L'esecuzione è un
ciclo `for` su puntatori: nessuna allocazione, nessun lock, nessuna ricorsione, tempo di
esecuzione prevedibile — quindi utilizzabile sul thread audio e dentro un plugin.

**Builder (`GraphBuilder`).** In fase di preparazione risolve le dipendenze, **deduplica** i nodi
identici (stesso nome, stessi parametri, stesso segnale), rileva i cicli e produce l'ordine
topologico, tutto in un'unica visita in post-ordine. È qui che le due FFT identiche diventano
una sola.

**Registro (`FeatureRegistry`).** Dizionario `nome → descrittore`, dove il descrittore dice come
costruire il nodo, quali dipendenze richiede, quali sono i parametri di default e quali i
riferimenti bibliografici.

**Sink (`IResultSink`).** Consumatore dei risultati. Il CSV aggregato, il CSV per frame, MIDI e
OSC sono tutti sink. È l'astrazione che unifica i due percorsi oggi duplicati.

**Preprocessing come nodi.** Ricampionamento, downmix, filtraggio e trimming non sono una fase a
parte: sono nodi come gli altri, posti a monte nel grafo. Ne segue che il sample rate deve
propagarsi lungo gli archi e non può essere una variabile globale del grafo.

**Banda come ramo del grafo, non come parametro.** Per analizzare le feature su una banda si
crea un ramo che parte da un nodo di banda e ci si appendono le feature volute. Il broadband è
semplicemente il ramo senza filtro. Il vantaggio non è solo la condivisione del filtro: è che la
banda è definita **una volta sola** e tutte le feature appese a quel ramo si riferiscono per
costruzione alla stessa cosa.

**Griglia temporale di analisi.** Il frame rate dell'analisi è disaccoppiato dal block size del
callback audio: il motore accumula l'ingresso in una FIFO ed emette frame ogni `H` campioni,
indipendentemente da come l'host spezzetta l'audio. Le feature temporali calcolano per campione
e vengono decimate **con una media sull'hop** (che è un filtro anti-aliasing) invece che
prendendo l'ultimo valore.

### 3.2 Tre modi di usare lo stesso motore

```
feature_core   (libreria statica)   ← nodi, grafo, registro, sink, setup. Zero GUI.
    ├── FeatureExtractor     (applicazione con interfaccia grafica)
    ├── featextract          (applicazione a riga di comando)
    └── FeatureExtractorFX   (plugin VST3 / AU, sola modalità live)
```

La riga di comando serve a rendere le analisi **riproducibili e versionabili**: un file di setup
più uno script descrivono esattamente l'analisi condotta, si mettono sotto controllo di versione
accanto ai dati e si rieseguono a distanza di mesi. Il plugin serve a inserire l'analizzatore
nella catena di un DAW, scrivendo il MIDI nel buffer fornito dall'host anziché su un device
esterno.

### 3.3 Il setup

Un **setup** è la descrizione completa e serializzabile di un'analisi: quali feature con quali
parametri, quali aggregatori, con quale preprocessing, su quali bande, con quale mapping verso
MIDI e OSC. È un unico artefatto, salvato in formato `ValueTree`, condiviso dai tre target:
l'interfaccia grafica lo salva e lo ricarica, la riga di comando lo prende come argomento, il
plugin lo memorizza nello stato dell'host.

Il setup **cresce fase per fase**. Ogni fase che aggiunge una possibilità di configurazione deve
estendere anche la sua serializzazione:

| Fase | Cosa entra nel setup |
|---|---|
| 2 | feature selezionate e parametri, aggregatori, opzioni CSV |
| 3 | diventa il formato di scambio con la riga di comando |
| 7 | frame rate di analisi, modalità di allineamento, tipo di decimazione |
| 8 | tabella di mapping MIDI/OSC |
| 9 | è lo stato che il plugin salva nell'host |
| 10 | catena di preprocessing |
| 11-12 | definizione delle bande e assegnazione feature → banda |
| 13 | preferenze di esportazione metodologica |

Da qui una regola da adottare fin dalla Fase 2: **il setup porta un numero di versione**, e ogni
fase che ne cambia la struttura fornisce la migrazione dal formato precedente. Senza questo,
ogni fase invalida i setup salvati dall'utente.

> [!TIP]
Per semplificare la retrocompatibilità basta settare dei default per i nodi del value tree che non esistono in un file salvato.

Da notare che **la bibliografia non entra nel setup**: i riferimenti sono proprietà dei
descrittori, non della configurazione, e vivono nel registro insieme a loro. Nel setup finisce
solo *come* si vuole che il documento metodologico venga generato.

### 3.4 La chiave di volta: i settings autodescrittivi

Un solo meccanismo, introdotto in Fase 2, alimenta sette cose diverse. Ogni oggetto di
configurazione descrive i propri campi — nome, tipo, intervallo, valore di default, unità di
misura, scala lineare o logaritmica — e da quella descrizione derivano gratuitamente:

1. la **firma di deduplicazione** usata dal grafo (Fase 4);
2. la **serializzazione** del setup in `ValueTree` (Fase 2);
3. il **pannello dei parametri** nell'interfaccia, costruito da sé (Fase 2);
4. l'output di **`--describe`** della riga di comando (Fase 3);
5. l'esposizione dei **parametri all'host** nel plugin (Fase 9);
6. metà del **riferimento dei descrittori** nella documentazione (Fase 5);
7. la **tabella dei parametri** del documento metodologico (Fase 13).

È l'intervento con il miglior rapporto fra costo e resa dell'intero piano, e la ragione per cui
la Fase 2 va progettata con cura anziché sbrigata.

### 3.5 Estensibilità come obiettivo esplicito

**Una feature semplice deve stare in un file solo, di circa quaranta righe, senza toccare
nient'altro tranne una riga nel file di registrazione.** Se il codice resterà nelle mani di una
sola persona questo è un lusso; se è previsto che altri aggiungano descrittori, è la differenza
fra un progetto che cresce e uno che si ferma. La Fase 5 è dedicata a raggiungere quell'obiettivo
e a documentarlo.

### 3.6 Provenienza e riproducibilità

Quattro elementi introdotti in fasi diverse, per ragioni diverse, compongono alla fine un'unica
catena verificabile:

- il **setup versionato e serializzabile** (Fase 2) è l'unico artefatto che descrive un'analisi;
- l'**intestazione di provenienza** nei CSV (Fase 3) lega ogni file di dati alla versione del
  software e al setup che l'ha prodotto;
- la **bibliografia strutturata** nei descrittori (Fase 5) rende ogni scelta algoritmica
  attribuibile;
- il **documento metodologico generato** (Fase 13) rende tutto questo leggibile in un articolo,
  con un'impronta che lega il testo al setup pubblicato come materiale supplementare.

Il risultato è che la catena `setup → analisi → dati → descrizione` è interamente derivata da un
solo artefatto e interamente ricontrollabile: chi legge l'articolo può recuperare il setup,
rieseguire `featextract --setup ...` e ottenere gli stessi numeri.

## 4. Metodo di lavoro

**File di riferimento (golden file).** Al termine della Fase 0, congelare un piccolo corpus di
prova (5-10 file audio brevi ma vari: silenzio, sinusoide, rumore, uno strumento, uno stereo
con pan marcato) e salvare il CSV prodotto. Da lì in poi, ogni fase deve o **riprodurre
esattamente gli stessi numeri**, o dichiarare in anticipo quali cambiano e perché.

Le fasi che cambiano legittimamente i numeri sono **solo due**: la Fase 7 (griglia temporale, che
corregge l'aliasing) e la Fase 10 (preprocessing, se attivato). Tutte le altre sono refactoring a
comportamento invariato, e un diff non vuoto è un bug introdotto.

**Stime.** Le durate indicate sono ordini di grandezza per una persona che conosce il codice.

### Riepilogo delle fasi

| Fase | Titolo | Durata | Novità per l'utente |
|---|---|---|---|
| 0 | Correttezza | 1-2 gg | il batch funziona |
| 1 | Stratificazione dell'esecuzione | 4-6 gg | CSV configurabile, export per frame, uscita live senza interruzioni |
| 2 | Il setup: parametri e persistenza | 6-8 gg | parametri modificabili, salva e carica setup |
| 3 | Nucleo separato e riga di comando | 5-7 gg | uso da terminale, analisi riproducibili |
| 4 | Il grafo | 8-12 gg | analisi più veloce |
| 5 | Estensibilità, documentazione, bibliografia | 8-12 gg | manuale, nuovi descrittori, bibliografia, software citabile |
| 6 | Parallelizzazione | 2-3 gg | batch N volte più rapido |
| 6bis | Resume del batch | 1-2 gg | un batch interrotto riprende senza rielaborare tutto |
| 7 | Griglia temporale unificata | 5-7 gg | live e batch coincidono |
| 8 | Mapping personalizzabile | 5-7 gg | MIDI e OSC configurabili |
| 9 | Plugin | 6-9 gg | uso dentro un DAW |
| 10 | Preprocessing | 5-7 gg | resampling, filtri, trimming |
| 11 | Bande fisse | 5-7 gg | feature per banda |
| 12 | Bande adattive | 4-6 gg | bande statistiche (Q75) |
| 13 | Esportazione del paragrafo metodologico | 3-5 gg | descrizione pronta per la pubblicazione |
| 14 | Visualizzazione e taratura dei parametri | 10-15 gg | vista interattiva per tarare i parametri, esportazione di figure in qualità editoriale |

> [!TIP]
Non c'è indicata una vera e propria fase di design della GUI generale (liste, pannelli di configurazione, gestione file), per ora facciamo che a fine lavoro, ogni studente si accolla un piccolo miglioramento della GUI (almeno fino a che non abbiamo fissato un design, in tal caso fare riferimento al relativo documento di specifiche [UX-and-GUI-design.md](Design/UX-and-GUI-design.md)). La visualizzazione dei descrittori fa eccezione: ha un design dedicato, la Fase 14.
>
> Ad ogni modo, il punto da cui conviene iniziare a pensare al design generale della GUI (non necessariamente implementarlo) è la Fase 2: è la prima a introdurre superfici di interazione realmente nuove rispetto a oggi — pannello dei parametri generato dai descrittori, salvataggio e caricamento del setup, elenco dei setup recenti, indicazione di modifiche non salvate. Tutto ciò che segue eredita lo stesso linguaggio: la tabella di mapping (Fase 8), l'assegnazione feature→banda (Fase 11), il pannello di esportazione del paragrafo metodologico (Fase 13). Deciso bene una volta in Fase 2, si eredita; deciso fase per fase, il rischio è ritrovarsi con convenzioni diverse per lo stesso tipo di widget. Due cose da tenere a mente per il [documento di design](Design/UX-and-GUI-design.md): la Fase 8 (tabella di mapping) è complessa abbastanza da poter meritare un mini-documento dedicato sullo stesso modello della Fase 14; la Fase 9 (plugin) impone di fattorizzare il pannello live in un componente condiviso fra applicazione e plugin, un vincolo strutturale utile da conoscere già in Fase 2 anche se si applica solo dopo.

---

# Parte II — Le fasi

---

## Fase 0 — Correttezza

### Stato di partenza
Applicazione completa nell'interfaccia e nelle funzioni annunciate. La modalità live funziona.
La modalità batch produce un CSV con intestazione corretta e righe vuote.

### Cosa è perfettibile
Tutto ciò che è elencato in §2.1 tranne B7 (che richiede infrastruttura e viene affrontato in
Fase 1), più la mitigazione immediata di V1. Nessun problema di design viene affrontato qui.

### Design scelto e perché
**Nessun cambiamento di design.** È deliberato: introdurre astrazioni su un codice che ha un bug
bloccante significa non sapere più, dopo, se un numero sbagliato viene dal refactoring o
dall'errore preesistente. Prima si stabilisce un comportamento corretto e riproducibile, poi lo
si preserva.

L'unica eccezione è la decimazione con media (punto 8): due righe che correggono un difetto di
validità dei risultati senza toccare la struttura, e che rendono il golden file degno di essere
congelato.

### Piano

> [!TIP]
Vedi nota sulla fondatezza di B1 (le guardie però restano comunque una buona idea)

1. **Aggiungere `f->getResult(featPackage);`** in `ExtractionEngine.cpp`, dentro il ciclo sulle
   feature alla riga 158, subito dopo `f->processBlock(buffer)`. È una riga e sblocca l'intera
   modalità batch. *(risolve B1)*
2. **Guardia all'ingresso di tutti e quattro gli `store()`**: `if (res.values.empty()) return;`.
   In `Average::store` va prima del test sulla soglia. *(risolve B2)*
3. **Guardia in `IQR::getResult`**: saltare la colonna se `v.size() < 4`. Rinominare la variabile
   di ciclo interna, che oggi fa shadowing di quella esterna. *(risolve B3)*
4. **Inizializzatori di default su tutti i membri POD** delle feature. In `SpectralFeatures.h:26`
   scrivere `float centroid = 0.0f, spread = 0.0f, skewness = 0.0f, kurtosis = 0.0f;`.
   Verificare l'intera gerarchia con `-Wuninitialized`. *(risolve B4)*
5. **Guardia in `RRMS::processBlock`** per `windowSize == 0`: `jassert` in debug, `return`
   immediato (nessun risultato finché non è preparato) in release.
   ```cpp
   jassert(windowSize > 0 && "RRMS::processBlock chiamato senza prepareToPlay()");
   if (windowSize == 0) return;
   ```
   *(risolve B8)*
6. **Un solo `AudioDeviceManager`.** Rimuovere quello interno a `MyFeatureExtractor`; passargli
   per riferimento quello di `MainComponent`. Far usare a `prepareLiveFeatures()` i parametri
   già ricevuti da `prepareToPlay()` invece di interrogare il device manager. *(risolve B5)*
7. **Chiusura sicura.** Aggiungere `stopThread(2000)` nel distruttore di `MyFeatureExtractor` e
   spostare la dichiarazione di `extractor` **dopo** `featList` e `funcList` in
   `MainComponent.h`. *(risolve B6)*
8. **Decimazione con media invece che con l'ultimo campione.** In `RRMS::processBlock`
   sostituire `rmsValue = aux.getSample(0, numSamples-1)` con la media di `aux` sul blocco.
   Stessa correzione in `Brightness`. Ridurre `BATCH_BLOCK_SIZE` da 4096 a 1024.
   *(mitiga V1; la soluzione completa arriva in Fase 7)*
9. **Pulizia minima**: rimuovere la variabile morta `nomeCartella`; gestire il fallimento di
   `createOutputStream()` con un messaggio all'utente invece di eseguire l'intera elaborazione
   e non scrivere nulla; unificare le tre soglie di silenzio (`1e-3f`, `1e-3f`, `0.0001f`) in
   una costante sola.
10. **Congelare il golden file** come descritto in §4.

### Risultato
Il batch produce un CSV corretto. I risultati di live e batch restano diversi (V1 è mitigato, non
risolto) ma entrambi sono ora statisticamente utilizzabili.

---

## Fase 1 — Stratificazione dell'esecuzione

### Stato di partenza
Software corretto. `run()` contiene ancora orchestratore, worker e scrittura CSV; il percorso
live ha una sua orchestrazione separata e duplicata, che invia MIDI e OSC direttamente dal thread
audio. `FeatureResult` trasporta insieme nomi e valori.

### Cosa è perfettibile
D1 (`run()` fa tre lavori), D5 limitatamente alla separazione schema/valori, D7 (i due metodi
identici), B7 (I/O sul thread audio). E, come conseguenza gratuita, la duplicazione fra i due
percorsi di uscita.

### Design scelto e perché
Si introducono **due sole astrazioni**, entrambe piccole e a comportamento invariato:

- **`IResultSink`**, che separa *cosa* si calcola da *dove finisce*. È il primo passo perché è
  quello che sblocca tutti gli altri: una volta che l'uscita è un oggetto, il worker diventa una
  funzione che non conosce il CSV, e l'orchestratore diventa un ciclo che non conosce l'audio.
- **La separazione schema/valori.** I nomi delle colonne sono invarianti e vanno calcolati una
  volta in preparazione; un frame deve trasportare solo numeri. Questo elimina le allocazioni
  per frame (D5, il caso di `Chromagram`) e collassa `createResultPackage`/`getResult` in un
  solo metodo (D7).

La creazione del sink MIDI/OSC è anche il momento naturale per risolvere B7: invece di inviare
dal thread audio, il sink scrive in una **coda lock-free** che un thread di servizio svuota e
consegna. Farlo ora costa poco perché il codice viene comunque spostato; farlo dopo significherà
toccarlo due volte. Ed è un prerequisito non negoziabile per il plugin (Fase 9), dove l'I/O sul
thread audio è inaccettabile.

Non si tocca ancora l'ownership né si introducono i settings: sono cambiamenti più invasivi e
conviene farli quando la struttura di esecuzione è già stratificata.

### Piano

1. **Definire l'interfaccia del sink.**
   ```cpp
   class IResultSink {
   public:
       virtual ~IResultSink() = default;
       virtual void begin(const juce::StringArray& schema) = 0;
       virtual void onFileBegin(const juce::String& fileName) {}
       virtual void onFrame(const float* values, int numValues) = 0;
       virtual void onFileEnd(const juce::String& fileName) {}
       virtual void end() {}
   };
   ```
2. **Separare schema e valori nell'interfaccia `Feature`.** Sostituire i due metodi identici con:
   ```cpp
   virtual juce::StringArray outputNames() const = 0;    // invariante, chiamato in preparazione
   virtual const std::vector<float>& values() const = 0;  // memoria posseduta dalla feature
   ```
   Ogni feature prealloca il proprio `std::vector<float>` in `prepareToPlay` e in `processBlock`
   ne scrive gli elementi. `Chromagram` costruisce l'array dei nomi delle note una volta sola,
   come membro statico.
3. **Implementare `CsvAggregateSink`**: incapsula i `Functional`, accumula in `onFrame`, riduce e
   scrive la riga in `onFileEnd`. Ci si spostano dentro il separatore di colonna, il separatore
   decimale e la precisione, che diventano **opzioni configurabili** invece di essere cablati
   (oggi sono `;` e la virgola, ostili a pandas/R con impostazioni di default).
4. **Implementare `CsvFrameSink`**: scrive una riga per frame invece che una per file. Costa
   poche righe una volta esistente l'interfaccia, ed è una funzione nuova genuinamente utile —
   oggi i valori per frame non sono esportabili in nessun modo.
5. **Implementare `MidiOscSink` con coda lock-free**: il thread audio scrive nella coda, un
   thread di servizio invia. Ci si sposta anche il throttling a `updateRate`. *(risolve B7)*
6. **Spezzare `run()` in tre.**
   ```cpp
   juce::StringArray buildSchema() const;                  // intestazione, una volta
   void processOneFile(const juce::File&, IResultSink&);    // worker: un file, tutti i blocchi
   void run() override;                                     // orchestratore: cicla, progresso, annulla
   ```
   `processOneFile` non deve leggere nessuno stato che non riceva come argomento: è il vincolo
   che rende possibile la Fase 6.
7. **Far convergere il percorso live sullo stesso schema**: `getNextAudioBlock` chiama le stesse
   feature e passa i valori al `MidiOscSink`. I due percorsi restano due metodi, ma condividono
   il modello di uscita.
8. **Aggiungere alla UI la scelta del tipo di export** (aggregato / per frame / entrambi) e le
   opzioni di formato CSV.

### Verifica
Il golden file deve essere riprodotto **byte per byte** con le opzioni impostate ai valori
precedenti (separatore `;`, decimale `,`, 4 cifre).

### Risultato
Stessi numeri, ma il motore è stratificato. L'utente guadagna l'export per frame, un CSV in
formato interoperabile e un'uscita live che non produce più interruzioni dell'audio.

---

## Fase 2 — Il setup: parametri e persistenza

### Stato di partenza
Motore stratificato. Le `Feature` sono ancora costruite e possedute da `FeatList`, con parametri
cablati nei costruttori e nessun modo di configurarli né di salvare una configurazione.

### Cosa è perfettibile
D2 (la UI possiede il modello) e D3 (nessun `Settings`, nessuna persistenza). Sono legati:
finché non esiste un oggetto che descrive *come* configurare una feature, non ha senso spostare
la proprietà delle istanze né salvare alcunché.

### Design scelto e perché
Si introduce la distinzione fra **ricetta** e **istanza**:

- un **setup** è dichiarativo: elenca *cosa* analizzare e *con quali parametri*. Non contiene
  oggetti `Feature`, è serializzabile, e può essere costruito dall'interfaccia, letto da un file
  o scritto in un test;
- le **istanze** sono create dal worker a partire dal setup, e vivono quanto l'analisi.

È il passaggio decisivo: da qui in poi l'interfaccia produce una descrizione, e il motore la
esegue. Ne segue che due istanze della stessa feature con parametri diversi diventano possibili,
che i test non hanno più bisogno dell'interfaccia grafica, che la Fase 3 può separare il nucleo
e la Fase 6 può parallelizzare.

**I settings sono autodescrittivi fin da subito**, non in un secondo momento. È una scelta
deliberata: descrivere i campi costa poco se fatto contestualmente alla loro creazione, e da
quella descrizione derivano immediatamente la serializzazione e il pannello dei parametri. Se
invece si scrivessero prima settings "muti" e poi si aggiungesse la descrizione, ogni feature
andrebbe toccata due volte e nel frattempo qualcuno avrebbe scritto a mano gli slider.

Senza descrizione automatica, «aggiungere una feature» continuerebbe a significare «scrivere il
pannello dei suoi parametri»: è l'attrito nascosto più costoso, e va tolto adesso.

`SettingsBase::getSignature()` merita attenzione: sarà la chiave di deduplicazione in Fase 4, e
deve essere **canonica** (ordine dei campi fisso, float con precisione esplicita, tutti e soli i
campi che influenzano il calcolo). Se due configurazioni identiche producono stringhe diverse, in
Fase 4 la deduplicazione non scatta e i calcoli si duplicano silenziosamente: il programma
funziona, solo più lento. Derivandola dalla descrizione dei campi, la canonicità è garantita per
costruzione.

### Piano

1. **Definire il descrittore di parametro**: nome, tipo, intervallo, default, unità di misura,
   scala lineare o logaritmica, testo di aiuto, **e l'asse su cui il parametro va letto quando
   viene visualizzato** (tempo, frequenza, livello, ampiezza — derivabile dall'unità nella
   maggior parte dei casi, esplicito quando l'unità da sola non basta). Nessun consumatore userà
   quest'ultimo campo prima della Fase 14, ma costa una riga per parametro scritta ora, contro il
   riaprire ogni feature già scritta se aggiunto dopo.
2. **Definire `SettingsBase`** con `describeParameters()` come unico metodo che le
   sottoclassi devono implementare; `getSignature()`, la serializzazione e la
   deserializzazione sono **derivate** e implementate una volta sola nella base.
3. **Scrivere i settings concreti** per ciascuna feature, portandoci dentro i parametri oggi
   cablati: finestra RMS di `RRMS`, ordine FFT delle spettrali, soglie di `F0`. Sistemare
   `mStrengthThreshold`, oggi al valore di debug `0.15` con quello di produzione in commento.
4. **Definire il setup.**
   ```cpp
   struct FeatureRequest { juce::String name; std::unique_ptr<SettingsBase> settings; };

   struct Setup {
       int                          version;
       std::vector<FeatureRequest>  features;
       std::vector<juce::String>    functionals;
       CsvOptions                   csv;
       // cresce nelle fasi successive
   };
   ```
5. **Introdurre una factory provvisoria** — un semplice `if/else` in un unico file — che dato
   nome e settings costruisce l'istanza. Non è ancora il registro: quello arriva in Fase 4, e
   questa factory è il punto che verrà sostituito.
6. **Spostare la proprietà**: `FeatList` diventa una pura vista che elenca i nomi disponibili e
   raccoglie la selezione; `processOneFile` istanzia le feature dal setup e le distrugge a fine
   file. Le istanze non attraversano più il confine fra interfaccia e motore.
7. **Serializzazione `ValueTree`** del setup, con **numero di versione** e infrastruttura di
   migrazione fin da ora, anche se la prima versione non ha nulla da migrare.
8. **Salva e carica setup nell'interfaccia**: comandi espliciti, elenco dei setup recenti,
   indicazione di modifiche non salvate. Salvataggio automatico dell'ultimo setup usato al
   riavvio.
9. **Pannello dei parametri generato** dai descrittori: nessun codice di interfaccia specifico
   per feature.
10. **Primi test senza interfaccia grafica**: costruire un `Setup` a mano, eseguire
    `processOneFile` su un file di prova con un sink in memoria, confrontare con i valori attesi.
    Includere un caso che chiama `processBlock` prima di `prepareToPlay` su `RRMS`, a verifica
    della guardia B8: è il primo punto in cui l'ordine di chiamata smette di essere implicito e
    diventa un caso di prova scrivibile.
	
> [!TIP]
Lo Step 10 lo svolge la stessa persona che svolge la Fase successiva (3)

### Verifica
Golden file identico, con un setup impostato ai valori che prima erano cablati. Un setup salvato
e ricaricato deve produrre un CSV identico.

### Risultato
I parametri di analisi sono modificabili, e **le configurazioni si salvano e si ricaricano**. Il
motore è testabile senza interfaccia.

---

## Fase 3 — Nucleo separato e riga di comando

### Stato di partenza
Setup dichiarativo e serializzabile, istanze create dal worker, motore testabile senza
interfaccia. Ma esiste ancora un solo target che mescola motore e GUI, e il motore chiama
direttamente `AlertWindow` e `NativeMessageBox`.

### Cosa è perfettibile
D8 (un solo modo di usare il software).

### Design scelto e perché
Si estrae il motore in una **libreria statica senza alcuna dipendenza dai moduli grafici di
JUCE**, e si aggiunge un secondo target che la usa da terminale.

Questa fase viene qui, e non più avanti, per tre ragioni. La prima è che il prerequisito — il
modello di dominio fuori dall'interfaccia — è appena stato soddisfatto dalla Fase 2, e la
separazione è quindi al suo costo minimo. La seconda è che la riga di comando dà subito valore
concreto per l'uso in ricerca: un setup più uno script di shell descrivono un'analisi
riproducibile, versionabile accanto ai dati e rieseguibile a mesi di distanza, cosa che con la
sola interfaccia grafica è documentabile solo in prosa. La terza è che la Fase 4 introdurrà il
registro, e conviene che nasca già dentro la libreria, con la strategia di registrazione giusta
(vedi sotto).

**Una trappola da conoscere in anticipo.** Un'idea diffusa e comoda è far sì che ogni feature si
iscriva da sé al registro tramite una variabile statica nel proprio `.cpp`. Questo funziona
quando tutti i sorgenti sono compilati direttamente nel target dell'eseguibile, come avviene
oggi. **Smette di funzionare quando finiscono in una libreria statica**: il linker include solo
gli object file che contengono simboli effettivamente referenziati, e un file che contiene *solo*
una registrazione statica non è referenziato da nessuno. Verrà scartato, e la feature sparirà dal
registro senza alcun errore di compilazione o di link.

Si potrebbe forzare l'inclusione dell'intero archivio (`--whole-archive`, `-force_load`,
`/WHOLEARCHIVE`), ma è specifico per piattaforma e va mantenuto in tre punti del build. La
scelta consigliata è la **registrazione esplicita**: una funzione `registerBuiltinFeatures()` in
un unico file che elenca tutte le feature, una riga ciascuna. Si perde l'automatismo, si guadagna
determinismo e portabilità, e soprattutto non esisterà mai il caso in cui una feature compila e
non appare. Va deciso ora perché il registro nasce in Fase 4.

### Piano

> [!TIP]
> Del nome `featextract` ne possiamo parlare... 

1. **Migrare da Projucer a CMake.** JUCE ha supporto CMake di prima classe (`juce_add_gui_app`,
   `juce_add_console_app`, `juce_add_plugin`) e gestire più target con un nucleo comune in
   Projucer è possibile ma scomodo. È anche l'occasione per avere build riproducibili in
   integrazione continua.
2. **Estrarre `feature_core`** come libreria statica: feature, functional, settings, setup,
   sink, worker, orchestratore. Regola non negoziabile: **nessuna dipendenza da moduli GUI**.
3. **Rimuovere dal nucleo ogni chiamata all'interfaccia.** `AlertWindow::showMessageBoxAsync` e
   `NativeMessageBox` vanno sostituiti con un'interfaccia `IProgressReporter`
   (`onProgress`, `onWarning`, `onError`) di cui interfaccia grafica, riga di comando e plugin
   forniscono implementazioni diverse.
4. **Verificare l'assenza di dipendenze grafiche** con un target di prova che linka solo
   `feature_core` e un `main()` minimo. Se compila e linka, il nucleo è pulito.
5. **Target `featextract`** come console app.
6. **Argomenti**: `--setup`, `--in` (file, cartella con ricorsione opzionale, o lista da stdin),
   `--out` (percorso oppure `-` per stdout), `--sink` (aggregato / per frame), `--threads`.
   In alternativa al setup, opzioni inline `--features` e `--functionals` per l'esplorazione
   rapida.
7. **Regola d'uscita**: su stdout solo i dati, progresso e messaggi su stderr, così che
   `featextract ... --out - | python analisi.py` funzioni senza filtri. Progresso disattivato
   automaticamente se stderr non è un terminale.
8. **Modalità introspettive**: `--list-features`, `--list-functionals`, `--describe <nome>` che
   stampa parametri, default, unità e canali prodotti. Sono quasi gratuite perché i descrittori
   della Fase 2 contengono già tutto, e sono il modo più economico di documentare il software a
   sé stesso.
9. **`--validate`**: carica il setup e ne verifica la coerenza senza eseguire nulla. Utile in
   script e in CI.
10. **Codici di uscita** convenzionali: 0 successo, 1 errore di setup, 2 errore di I/O, 3
    annullato. Errori sul singolo file che non interrompono il batch, con riepilogo finale.
11. **Intestazione di provenienza**: ogni CSV riporta in commento la versione del software, la
    data e il setup completo usato. È la differenza fra un file di risultati e un file di
    risultati citabile, ed è il primo anello della catena descritta in §3.6.
12. **Integrazione continua** che compila entrambi i target su due piattaforme ed esegue la
    verifica sul golden file.

### Verifica
`featextract` con un setup salvato dall'interfaccia deve produrre un CSV identico a quello
prodotto dall'interfaccia con lo stesso setup.

### Risultato
Il software è utilizzabile da terminale e le analisi diventano riproducibili. Il nucleo è pronto
a ospitare il plugin.

---

## Fase 4 — Il grafo

### Stato di partenza
Nucleo separato, setup dichiarativo, due target funzionanti. Le feature sono però ancora
indipendenti: ognuna riceve il blocco audio e fa tutto da sola, comprese le parti che altre
stanno già calcolando.

### Cosa è perfettibile
D4 (calcoli duplicati: due FFT identiche, un RRMS ridondante, tre downmix), D5 nella parte sulla
tipizzazione dei dati, D6 (i mapper chiusi alle estensioni). È la fase più corposa.

### Design scelto e perché
Si passa da un elenco di feature indipendenti a un **grafo risolto in fase di preparazione**.

> [!TIP]
> Qui c'è la differenza più grossa con il piano ideato in partenza: non più una pool di risultati ma un grafo di processing. Più tosto da costruire, ma poi più snello da gestire. Qui Claude cita anche altri motivi ("non adatto al realtime") che in realtà reggono fino a un certo punto (essendo immutabile una volta a regime, il pool si può preallocare e le ricorsioni si possono prevedere), tuttavia, più lo si ingegnerizza, più assomiglia a un "grafo di risultati", quindi tanto vale pensare il processing stesso come grafo.

L'alternativa considerata e scartata era la **valutazione pigra con memoization**: ogni feature
chiede i risultati a quelle da cui dipende, e una cache indicizzata su un identificativo di
buffer evita i ricalcoli. È elegante e non richiede ordinamento topologico, ma **non è
utilizzabile sul thread audio**: per risolvere una dipendenza bisognerebbe costruire una chiave
stringa (allocazione), cercarla in una mappa e potenzialmente allocare l'oggetto risultato, il
tutto in una discesa ricorsiva di profondità non limitata a priori. Su un buffer da 512 campioni
a 48 kHz il budget è di ~10,6 ms, e una singola allocazione che prende un lock nell'allocatore
produce un'interruzione udibile. Sarebbe inoltre incompatibile con la Fase 9.

Poiché **il grafo delle dipendenze è statico** — `Brightness` ha sempre bisogno dei suoi due
inviluppi, `F0` ha sempre bisogno del suo spettro — conviene risolverlo una volta sola in
preparazione, dove si può allocare e ordinare senza vincoli, e ottenere una lista piatta da
eseguire. Questo *elimina* componenti invece di aggiungerne: spariscono l'identificativo di
buffer, la cache, il rilevamento dei cicli a runtime e l'interfaccia di contesto iniettata nelle
feature. Sparisce anche un problema che con la valutazione pigra sarebbe stato insidioso: una
feature con memoria (un filtro, una media mobile) che salta dei blocchi perché nessuno gliela ha
chiesta, corrompendo silenziosamente il proprio stato. Nel grafo ogni nodo viene eseguito una
volta per blocco, sempre, in ordine.

Le **porte tipizzate** sono la seconda metà della fase. Con l'arrivo dei nodi, gli archi
trasportano cose strutturalmente diverse — un blocco audio multicanale, uno spettro di
magnitudini con la sua mappatura bin→Hz, un inviluppo a sample rate, un vettore a frame rate — e
chiamarle tutte `vector<float>` è una bugia che prima o poi perde. Basta che ogni porta dichiari
il proprio genere e che il builder validi le connessioni una volta sola, in preparazione: una
feature spettrale attaccata a una porta audio dev'essere un errore di costruzione, non una
sorpresa a runtime.

I **descrittori di canale** risolvono D6: se ogni valore esportato dichiara la propria unità
(Hz, guadagno lineare, rapporto adimensionale), il range atteso e la scala, i mapper MIDI e OSC
smettono di essere catene di `if/else` sui nomi e diventano generici. Sono l'analogo, sul lato
uscita, di ciò che i descrittori di parametro della Fase 2 sono sul lato configurazione.

### Piano

1. **Definire il nodo e le porte.**
   ```cpp
   enum class PortRate { PerSample, PerFrame };
   enum class PortKind { Audio, Spectrum, Scalar, Vector };
   struct PortSpec { PortRate rate; PortKind kind; int channels; };

   class FeatureNode {
   public:
       virtual ~FeatureNode() = default;
       virtual void prepare(double sampleRate, int maxBlockSize,
                            bool wantDiagnostics = false) = 0;          // può allocare
       virtual void process(const juce::AudioBuffer<float>& in) = 0;   // non può allocare
       virtual PortSpec outputSpec() const = 0;
       virtual std::vector<ChannelDescriptor> outputChannels() const = 0;
       const std::vector<float>& output() const noexcept { return m_output; }
   protected:
       std::vector<float> m_output;
   };
   ```
   Il parametro `wantDiagnostics` resta ignorato da tutte le feature esistenti: è
   un'anticipazione a costo zero per i canali diagnostici della Fase 14 (es. le due medie della
   Wiener entropy), che altrimenti richiederebbe di riaprire ogni nodo già scritto.
2. **Definire il descrittore di canale**: nome, unità (`Hz`, `Gain`, `Ratio`, `Ratio_pm1`,
   `Adimensional`), intervallo atteso, scala lineare o logaritmica, **e un flag `isDiagnostic`
   (default `false`)** — stessa ragione del punto precedente, consumato solo in Fase 14.
3. **Definire il descrittore di feature e il registro**, con `registerBuiltinFeatures()`
   esplicita come deciso in Fase 3.
   ```cpp
   struct FeatureDescriptor {
       juce::String name, category;
       std::function<std::vector<Dependency>(const SettingsBase&)> dependencies;
       std::function<std::unique_ptr<FeatureNode>(const SettingsBase&,
                                                  const std::vector<FeatureNode*>&)> create;
       std::function<std::unique_ptr<SettingsBase>()> defaultSettings;
   };
   ```
4. **Estrarre `MagnitudeSpectrum` come nodo autonomo.** È il passo che ripaga: `SpectralFeature`
   smette di essere "una feature che possiede una STFT" e diventa "una feature che dipende da un
   nodo spettro". `F0` e `Chromagram`, avendo lo stesso ordine, si risolvono sullo stesso nodo.
5. **Estrarre `Downmix` come nodo**, eliminando le tre copie del codice di somma stereo→mono.
6. **Convertire `Brightness`** perché dichiari due dipendenze da `RRMS` invece di possederle. Se
   l'utente ha selezionato anche `RRMS` con gli stessi parametri, la deduplicazione lo condivide.
7. **Scrivere il builder**: una visita ricorsiva in post-ordine che risolve le dipendenze,
   deduplica tramite una tabella `nome|firma → nodo`, rileva i cicli (se un nome è già in corso
   di risoluzione quando lo si reincontra) e produce l'ordine topologico come effetto
   collaterale — in post-ordine un nodo viene aggiunto **dopo** tutti quelli da cui dipende. Una
   quarantina di righe.
8. **Scrivere `ProcessingGraph`**, distinguendo `m_execution` (tutti i nodi, comprese le
   dipendenze interne) da `m_requested` (solo quelli chiesti dall'utente, che finiscono
   nell'export). È questa distinzione a permettere che gli inviluppi interni di `Brightness`
   siano condivisi senza comparire nel CSV.
9. **Riscrivere i mapper in forma generica**, guidati dai descrittori di canale. Conservare come
   casi dichiarati le mappature che non sono di valore ma di evento: le note MIDI di `F0` e gli
   accordi del `Chromagram`. Questo punto dichiara *che cosa* è un'eccezione, non *dove vive lo
   stato* di quell'eccezione (`lastNote`, `lastChord`, oggi in `MidiMapper`, D11): quella
   collocazione è rimandata deliberatamente alla Fase 8, dove il mapping diventa una tabella dati
   e lo stato ha finalmente un posto esplicito in cui stare — non è un buco lasciato aperto per
   caso.
10. **Far convergere live e batch sullo stesso nucleo**: entrambi costruiscono un grafo e
    chiamano `graph.process(buffer)`; differiscono solo per dimensione del blocco e sink.
11. **Sostituire la factory provvisoria della Fase 2** con il registro.

### Verifica
Golden file identico. Verificare inoltre, con un contatore temporaneo, che con tutte le feature
selezionate le FFT eseguite per hop siano **due** e non tre.

### Risultato
Analisi sensibilmente più rapida, con la stessa configurazione e gli stessi risultati.

---

## Fase 5 — Estensibilità, documentazione, bibliografia

### Stato di partenza
Motore a grafo completo, tre target, setup persistente, mapper generici. Aggiungere una feature
richiede ormai un file più una riga di registrazione — molto meno dei quattro interventi in tre
file di partenza, ma con ancora attrito residuo e nessuna documentazione.

### Cosa è perfettibile
D9. E, più in generale, tutto ciò che determina se qualcun altro riuscirà a contribuire. Si
prepara inoltre il materiale che la Fase 13 userà per D10.

### Design scelto e perché
Questa fase viene **subito dopo il grafo e prima delle estensioni analitiche** per una ragione
precisa: le fasi 7-12 aggiungeranno molti descrittori e molte varianti, e ogni descrittore
scritto prima che l'API sia semplice e documentata è un descrittore che andrà riscritto.

Documentazione ed estensibilità vanno fatte **insieme**, non in sequenza: la guida su come
aggiungere una feature è inseparabile dal lavoro che rende semplice aggiungerne una. Scritta
prima, documenta un'API che sta per cambiare; rimandata, non viene scritta mai.

Il grosso dell'attrito è già stato tolto altrove: i settings autodescrittivi (Fase 2) hanno
eliminato il lavoro di interfaccia, i descrittori di canale (Fase 4) hanno eliminato il lavoro
sui mapper. Restano il boilerplate del nodo e l'assenza di un modo semplice per verificare che
una feature nuova calcoli il valore giusto.

Per un software di ricerca c'è poi una seconda categoria di documentazione, altrettanto
importante e più facile da trascurare: **la definizione precisa dei descrittori**. Chi pubblica
risultati ottenuti con questo strumento deve poter dire esattamente che cosa ha misurato. Per
fare un esempio dal codice attuale, `Brightness` calcola `(sr/π)·asin(b/2)` dove `b` è il
rapporto fra l'inviluppo del segnale differenziato e quello del segnale: è uno stimatore
specifico, con assunzioni proprie, e senza documentazione il numero non è interpretabile né
citabile da nessuno tranne chi l'ha scritto.

Scrivere quel riferimento richiede già di raccogliere, per ciascun descrittore, definizione
formale e lavori di provenienza. È quindi il momento in cui la bibliografia va raccolta **e
registrata in forma strutturata**, non solo scritta in prosa: raccoglierla due volte, una per la
documentazione e una per l'esportazione della Fase 13, sarebbe uno spreco e le due copie
divergerebbero.

Su un punto vale la pena essere espliciti, perché la domanda si pone naturalmente: descrittori
come il **low energy ratio** o il conteggio di eventi, che sembrano richiedere una passata
sull'intero file, **non richiedono alcun meccanismo nuovo**. Non serve rileggere l'audio: serve
una seconda passata sulla *serie di valori già calcolata*, ed è esattamente ciò che
`Median` e `IQR` già fanno accumulando tutto e riducendo alla fine. Sono functional, non feature.
Implementarne uno serve da dimostrazione che il meccanismo basta così com'è.

### Piano

1. **Classe base per il caso comune** (un ingresso, N valori scalari per frame), che gestisce
   preparazione, buffer di uscita e dichiarazione delle porte, lasciando da implementare
   `compute()` e i nomi dei canali.
2. **Helper di registrazione template**, così che la riga di registrazione sia una sola e il
   downcast dei settings sia fattorizzato:
   `registerFeature<SpectralCentroid, CentroidSettings>(reg, "SpectralCentroid");`
3. **Due classi base per i functional**: accumulatore in streaming (media, deviazione standard:
   stato costante) e riduttore con buffer (mediana, IQR: stato proporzionale alla durata).
   Dichiarare la categoria nell'interfaccia, così da poter stimare la memoria e avvisare
   l'utente sul costo di un setup applicato a file lunghi.
4. **Implementare `LowEnergyRatio`** come functional, con la classe base con buffer. È al tempo
   stesso un descrittore utile e l'esempio che dimostra che i functional coprono già i casi "a
   due passate".
5. **File modello `ExampleFeature.h/.cpp` compilato nel progetto**, non un frammento nella
   documentazione: se l'API cambia, il modello smette di compilare e nessuno lavora su istruzioni
   obsolete. Stesso trattamento per `ExampleFunctional`.
6. **Banco di prova per singola feature**: eseguire un descrittore su un segnale sintetico e
   confrontare con un valore atteso. Fornire una decina di casi analitici come modello — l'RMS di
   una sinusoide a fondo scala è −3,01 dBFS, il centroide spettrale di una sinusoide a *f* è *f*,
   il centroide del rumore bianco tende a un quarto della frequenza di campionamento, il PAN di
   un segnale identico sui due canali è 0.
7. **Checklist del contratto**, verificata dove possibile con asserzioni in debug: non allocare
   in `process`, azzerare ogni stato in `prepare`, dichiarare la latenza, dichiarare unità e
   intervallo di ogni canale, essere deterministico.
8. **README**: cos'è, a cosa serve, come si compila, un esempio d'uso per ciascun target.
9. **«Come aggiungere una feature audio»** — la guida principale. Un tutorial completo che porta
   da zero a una feature funzionante e testata: il file modello per intero, la riga di
   registrazione, la dichiarazione dei parametri, un caso di prova, la checklist. Il codice
   mostrato è quello del modello compilato al punto 5.
10. **«Come aggiungere un functional»**, con la stessa struttura e la spiegazione della scelta
    fra le due classi base.
11. **Riferimento dei descrittori**: per ciascuno, definizione formale, unità, intervallo tipico,
    parametri, assunzioni. È il documento che rende i risultati citabili.
12. **Struttura bibliografica.** Definire una tabella di citazioni a livello di registro, con
    chiavi referenziate dai descrittori:
    ```cpp
    struct Citation {
        juce::String key;      // "camacho2008swipe"
        juce::String bibtex;   // voce BibTeX completa
        juce::String note;     // opzionale: a cosa si riferisce esattamente
    };
    ```
    I descrittori di feature e i functional guadagnano un campo
    `std::vector<juce::String> citationKeys`. La tabella sta nel registro, non duplicata nei
    descrittori, così che una fonte citata da più descrittori compaia una volta sola nel `.bib`
    generato. Anche l'applicazione ha le proprie citazioni: il software, le librerie di terze
    parti che influenzano i risultati numerici, e gli eventuali algoritmi implementati nel
    nucleo.
13. **Popolare la bibliografia**, una voce per descrittore. Per `F0` il riferimento è evidente dal
    codice, che implementa SWIPE; per gli altri va stabilito caso per caso.
    > **Effetto collaterale utile.** Questo lavoro funziona da revisione: obbliga a stabilire, per
    > ogni descrittore, se implementa un algoritmo pubblicato — e in quale variante — oppure se è
    > una formulazione originale o adattata. Nel secondo caso non c'è nulla da citare, ma va
    > documentato esplicitamente come tale. `Brightness` è probabilmente in questa categoria:
    > senza una nota esplicita, un lettore non ha modo di sapere se sta guardando un descrittore
    > standard o una scelta locale.
14. **Rendere citabile il software**: un file `CITATION.cff` alla radice del repository (GitHub
    lo interpreta e genera la citazione automaticamente) e un DOI tramite Zenodo, agganciato ai
    tag di versione. Costa poche ore e rende il software citabile in modo stabile.
15. **Panoramica dell'architettura** per chi manutiene: grafo, porte, sink, separazione fra
    costruzione ed esecuzione, e il motivo delle scelte — in particolare perché il grafo è
    risolto in preparazione e non valutato pigramente.
16. **Riferimento della riga di comando**, generato dallo stesso codice che implementa `--help`.
17. **Changelog e versionamento semantico**, coerenti con la versione scritta nell'intestazione
    dei CSV (Fase 3, punto 11).

### Verifica
Una persona esterna al progetto, seguendo la sola guida, deve riuscire ad aggiungere un
descrittore semplice e a vederlo comparire nell'interfaccia, nella riga di comando e nel CSV
senza altre istruzioni. È il criterio di accettazione della fase.

### Risultato
Il progetto è documentato e apribile a contributi. Un descrittore in più è un file da quaranta
righe. Il software è citabile e ogni descrittore è attribuito.

---

## Fase 6 — Parallelizzazione

### Stato di partenza
Ogni file viene analizzato da un grafo costruito per quel file, istanziato dal worker a partire
da un setup dichiarativo. Il worker non legge stato condiviso. Il batch è sequenziale.

### Cosa è perfettibile
Il tempo di elaborazione di un corpus, che oggi scala linearmente con il numero di file su un
solo core.

### Design scelto e perché
Nessuna nuova astrazione: **la parallelizzazione è già stata pagata** dalle scelte delle fasi
2 e 4. Ogni file ha il proprio grafo, i propri nodi e i propri buffer; l'unica cosa condivisa fra
i worker è il registro, immutabile dopo l'inizializzazione, e il sink, che è l'unico punto da
rendere thread-safe.

Questa fase è breve e va inserita qui, prima delle fasi funzionali, per due ragioni: rende
rapido il lavoro di verifica di tutto ciò che segue (i golden file si rigenerano in una frazione
del tempo), e **verifica sperimentalmente** che l'isolamento dello stato introdotto in Fase 4 sia
reale.

### Piano

1. **Rendere thread-safe i sink.** Il modo più semplice e prevedibile: ogni worker accumula il
   risultato del proprio file e lo consegna al sink sotto un solo lock a fine file. Il lock è
   preso una volta per file, non per frame.
2. **Introdurre un thread pool** dimensionato su `std::thread::hardware_concurrency()`, evitando
   un thread per file.
3. **Rendere deterministico l'ordine di uscita**: i file arrivano al sink in ordine di
   completamento, non di lista. Riordinare in scrittura perché il CSV sia riproducibile.
4. **Adattare progresso e annullamento** al modello parallelo: contatore atomico per il
   progresso, flag di annullamento controllato dentro il ciclo sui blocchi di ogni worker.
5. **Esporre `--threads`** nella riga di comando e la scelta corrispondente nell'interfaccia.
6. **Verifica di assenza di condivisione**: eseguire il batch con un solo thread e con N, e
   confrontare i CSV. Devono essere identici.

### Verifica
Golden file identico con qualunque numero di thread. Un diff non vuoto indica stato condiviso
sfuggito alla Fase 4.

### Risultato
Batch più rapido di un fattore prossimo al numero di core.

---

## Fase 6bis — Resume del batch

### Stato di partenza
Batch parallelo, con worker isolati per file e un solo lock nel sink a fine file. Setup
versionato e dotato di firma canonica (Fase 2).

### Cosa è perfettibile
L'impossibilità di riprendere un batch interrotto — crash, chiusura dell'applicazione,
cancellazione volontaria a metà — senza rielaborare l'intero corpus da capo, anche quando si
tratta esattamente della stessa analisi.

### Design scelto e perché
Il resume non introduce meccanismi nuovi: si appoggia su due garanzie già acquisite. La **firma
canonica del setup** (Fase 2) permette di riconoscere "stessi settings" senza ambiguità. Il
**lock preso una volta per file** nel sink (Fase 6) è già la prova che un file è "tutto scritto"
o "per niente scritto", mai a metà — è esattamente la proprietà di cui il resume ha bisogno per
essere sicuro: senza, riprendere rischierebbe righe duplicate o troncate nel CSV.

Per questo la fase va qui, subito dopo la 6: prima non esisterebbe nulla da riconoscere come
"stessi settings" (dipende dalla firma di Fase 2), né una marcatura di completamento thread-safe
a cui appoggiarsi (dipende dal lock per-file di Fase 6).

> [!TIP]
> Se il resume segnala che qualcosa è cambiato deve essere possibile continuare comunque, a meno che sia cambiato il numero di "colonne in output"

### Piano
1. **File di stato laterale** (es. `<output>.resume`, stesso formato `ValueTree` del setup):
   contiene la firma del setup e, per il corpus, l'elenco dei file con un identificativo leggero
   (percorso + dimensione + data di modifica — un hash del contenuto è più robusto ma costa una
   lettura completa di ogni file solo per verificarlo).
2. **Marcatura di completamento a grana di file**, scritta nello stesso punto in cui il sink
   riceve il lock a fine file: un campo in più nello stesso momento, non un meccanismo nuovo.
3. **All'avvio di un batch**: se esiste un file di stato con la stessa firma di setup e lo stesso
   corpus (o un suo sovrainsieme), filtrare dalla lista i file già completati e aprire il sink in
   una modalità che preserva l'esistente — append per il CSV aggregato, verifica di non
   duplicazione per il CSV per-frame.
4. **Pulizia automatica**: il file di stato si cancella al completamento con successo dell'intero
   corpus; resta solo se l'esecuzione è stata annullata o interrotta.
5. **Segnalazione esplicita del mismatch**: cambiare anche un solo parametro fra due esecuzioni
   invalida giustamente il resume (la firma cambia), ma va comunicato all'utente invece di
   fallire silenziosamente o ripartire da capo senza dirlo.
6. **Opzione `--resume`/`--no-resume`** nella riga di comando (Fase 3) e comando corrispondente
   nell'interfaccia.

### Verifica
Interrompere un batch a metà corpus (kill del processo), rilanciarlo con lo stesso setup e la
stessa lista file: il CSV finale dev'essere identico a quello di un'esecuzione ininterrotta.
Cambiare un parametro fra le due esecuzioni: il resume non deve applicarsi, e va segnalato.

### Risultato
Un batch interrotto su un corpus grande si riprende dal punto in cui si era fermato, invece di
essere rieseguito per intero.

---

## Fase 7 — Griglia temporale unificata

### Stato di partenza
Grafo funzionante, parallelo e documentato. Le feature temporali producono un valore per blocco
audio, quelle spettrali uno per hop STFT mediato sui frame caduti nel blocco. Live e batch usano
blocchi di dimensione diversa e quindi producono numeri diversi.

### Cosa è perfettibile
V1 (sottocampionamento e aliasing, mitigato in Fase 0 ma non risolto) e V3 (latenza non
compensata né dichiarata).

### Design scelto e perché
Si **disaccoppia il frame rate dell'analisi dal block size del callback audio**. Il motore
accumula l'ingresso in una FIFO ed emette frame di analisi ogni `H` campioni, indipendentemente
da come host o reader spezzettano l'audio. Il frame rate diventa così una proprietà
dell'analisi, dichiarata dall'utente, e non un effetto collaterale del driver audio.

Le feature temporali calcolano **per campione** — `RRMS` lo fa già internamente, e oggi butta via
tutto tranne un valore — e vengono decimate al frame con una **media sull'hop**, non prendendo un
campione isolato. La media è un filtro passa-basso a media mobile applicato prima del
decimatore: è anti-aliasing sostanzialmente gratuito.

Non si forza però tutto al sample rate: un chromagram che produce 12 valori per hop replicati su
4096 campioni sarebbe puro spreco. Ogni nodo **dichiara** il proprio rate (è il `PortRate`
introdotto in Fase 4) e la conversione avviene attraverso un nodo `Decimate` esplicito, con la
riduzione scelta dall'utente (media, ultimo, massimo, RMS).

Sulla latenza va scelta una semantica e dichiarata:

- **causale** — il valore al frame `k` descrive la finestra che finisce al campione `k·H`.
  Semplice, nessuna compensazione, ma un transiente compare nell'RMS (35 ms) prima che nello
  spettro (4096 campioni);
- **centrata** — ogni nodo dichiara il proprio ritardo di gruppo e il grafo compensa, così tutti
  i valori al frame `k` descrivono lo stesso istante centrale.

Per uno strumento di analisi la seconda è più difendibile, ma in live la compensazione è latenza
aggiunta sull'uscita. Va quindi resa **un'opzione**, attiva di default in offline e disattivata
in live. La dichiarazione di latenza per nodo introdotta qui sarà anche ciò che il plugin
comunicherà all'host in Fase 9.

**Questa è la prima fase che cambia legittimamente i numeri.** Il golden file va rigenerato e le
differenze documentate: sono la correzione dell'aliasing, non una regressione.

### Piano

1. **Introdurre `FrameClock`**: FIFO d'ingresso, hop `H` e dimensione di finestra `N`
   configurabili, che emette frame a cadenza fissa. Sostituisce l'accumulo interno di `STFT` e
   la media dei frame per blocco.
2. **Portare le feature temporali al calcolo per campione**, esponendo una porta `PerSample`.
   `RRMS` è già pronto e si limita a non scartare più il buffer. `PAN` va riscritto come rapporto
   di energie su finestra scorrevole invece che su blocco: oggi la sua finestra è la dimensione
   del blocco, cioè un parametro arbitrario dettato dal driver.
3. **Introdurre il nodo `Decimate`** con riduzione selezionabile, inserito automaticamente dal
   builder quando un consumatore `PerFrame` è collegato a un produttore `PerSample`.
4. **Aggiungere `latencyInSamples()` al nodo**, implementandolo per STFT (già scritto e mai
   usato: restituisce `fftSize`), per le finestre RMS (metà finestra) e per i filtri.
5. **Implementare la compensazione**: il builder calcola la latenza cumulativa lungo ogni
   cammino e inserisce linee di ritardo sui rami più veloci. Opzione attiva in offline,
   disattivata in live, e in ogni caso **riportata** all'utente.
6. **Esporre il frame rate di analisi** nell'interfaccia e nella riga di comando, con il vincolo
   di coerenza con la dimensione FFT delle feature spettrali.
7. **Estendere il setup** (versione +1, con migrazione) per contenere frame rate, modalità di
   allineamento e tipo di decimazione.
8. **Rigenerare il golden file** e produrre un breve documento che confronta prima/dopo su ogni
   feature, spiegando l'entità e la ragione delle differenze. Confluisce nel riferimento dei
   descrittori scritto in Fase 5.

### Verifica
Lo stesso file analizzato in live e in batch, con lo stesso frame rate, deve dare gli stessi
numeri entro la tolleranza numerica. È il criterio di accettazione della fase.

### Risultato
Risultati coerenti fra le due modalità e statisticamente validi. Frequenza di analisi
configurabile e indipendente dall'hardware.

---

## Fase 8 — Mapping personalizzabile

### Stato di partenza
Mapper generici, guidati dai descrittori di canale, che producono un mapping predefinito
ragionevole senza configurazione. Frame di analisi con posizione nota nel blocco.

### Cosa è perfettibile
La rigidità residua del mapping: numeri di control change fissi, intervalli non modificabili,
nessuna possibilità di adattare l'uscita allo strumento che la riceve.

### Design scelto e perché

> [!TIP]
> Qui il design va pensato da noi Informatici Musicali alla luce di cosa esisterà quando ci arriviamo.

Dopo la Fase 4 il mapping predefinito è già sensato, quindi la personalizzazione è un livello
sopra qualcosa che funziona, non una necessità per far funzionare le cose. Questo permette di
progettarla come **tabella di mapping** salvata nel setup, in cui ogni riga associa un canale di
feature a una destinazione, con intervalli, curva e smoothing.

Tre aggiunte meritano attenzione particolare.

**Calibrazione automatica.** Una modalità «apprendi intervallo» che osserva per N secondi e
imposta minimo e massimo. Risolve il problema dei descrittori senza intervallo naturale, che oggi
è affrontato con costanti trovate a mano — il codice attuale lo ammette in un commento a
proposito di skewness e kurtosis.

**Risoluzione e continuità sul MIDI.** I control change a 7 bit sono grossolani per descrittori
continui: vale la pena supportare i CC a 14 bit. E per `F0`, la conversione attuale quantizza al
semitono più vicino: un'uscita in pitch bend o MPE preserverebbe l'informazione continua che
l'algoritmo ha effettivamente estratto e che oggi viene buttata via.

**Offset campione-accurato.** Il clock della Fase 7 sa in quale posizione del blocco cade ogni
frame: gli eventi MIDI possono essere collocati esattamente invece che tutti all'inizio del
blocco. Nello standalone è un miglioramento; in Fase 9 diventerà la modalità naturale.

Questa fase precede il plugin perché è opportuno che il plugin nasca già con il mapping buono.

**Dove vive lo stato delle righe evento (D11).** `lastNote` e `lastChord`, oggi membri della
classe monolitica `MidiMapper`, diventano proprietà della **singola riga di tabella** con
destinazione nota o accordo — non della tabella nel suo insieme e non di una classe globale.
Questo è ciò che rende sensato aggiungere e rimuovere righe a runtime: ogni riga porta con sé
tutto ciò che le serve, comprese le eventuali quattro righe che oggi condividerebbero lo stato
per errore. Quello stato **non entra nel setup** — è transitorio quanto la cache di
`FeatureBase`, non un parametro — e si azzera esplicitamente all'avvio della riproduzione live
(dove oggi vive `prepareLiveFeatures()`); il gancio gemello nel plugin arriva in Fase 9.

### Piano

1. **Modello dati della tabella di mapping**: sorgente (feature, canale), destinazione (CC con
   numero e canale MIDI, nota, pitch bend, indirizzo OSC), intervallo d'ingresso e d'uscita,
   curva, smoothing, frequenza d'invio. Le righe con destinazione nota o accordo portano in più
   uno stato di runtime opzionale:
   ```cpp
   struct MappingRow {
       // ... sorgente, destinazione, intervalli, curva, smoothing ...
       struct EventState { int lastNote = -1; int lastChord = -1; };
       std::optional<EventState> eventState;   // presente solo se la riga è event-like
   };
   ```
2. **Valori di default derivati dai descrittori di canale**, così che una tabella vuota si
   comporti come oggi.
3. **Ciclo di vita dello stato di riga**: azzerare `eventState` di ogni riga event-like all'avvio
   della riproduzione live, non solo alla costruzione della tabella — altrimenti una nota rimasta
   accesa da una riproduzione precedente non si spegne mai. *(risolve D11)*
4. **Estendere il setup** (versione +1, con migrazione) per contenere la tabella — **senza**
   `eventState`, che è runtime-only e non va serializzato.
5. **Editor a tabella** nell'interfaccia, con anteprima del valore in ingresso e in uscita.
6. **Modalità di apprendimento dell'intervallo**, con indicazione dei valori osservati.
7. **CC a 14 bit** e uscita in pitch bend / MPE per i descrittori di altezza.
8. **Indirizzi OSC configurabili** con modello (`/feature/{nome}/{canale}`), scelta fra un
   messaggio per feature e uno per valore, e supporto ai bundle con timetag.
9. **Smoothing per riga**, con costante di tempo indipendente dal frame rate di analisi.
10. **Offset campione-accurato** per gli eventi MIDI.
11. **Monitor in tempo reale** dei messaggi inviati: è lo strumento di diagnosi che serve ogni
    volta che il mapping non fa quello che ci si aspetta.

### Verifica
Con la tabella vuota, i messaggi prodotti devono coincidere con quelli della Fase 7.

### Risultato
Uscita MIDI e OSC completamente configurabile e salvata nel setup.

---

## Fase 9 — Plugin

### Stato di partenza
Nucleo separato senza dipendenze grafiche, grafo eseguibile senza allocazioni, latenza dichiarata
per nodo, uscita MIDI/OSC con coda lock-free e mapping configurabile, setup serializzabile.

### Cosa è perfettibile
L'impossibilità di usare l'analizzatore dentro un DAW.

### Design scelto e perché
Tutti i prerequisiti sono già stati soddisfatti da fasi precedenti, ciascuna per ragioni proprie:
il grafo senza allocazioni (Fase 4) rende l'esecuzione ammissibile sul thread audio, la coda
lock-free (Fase 1) rimuove l'I/O, la latenza dichiarata (Fase 7) è ciò che si comunica all'host,
il setup serializzabile (Fase 2) è lo stato che l'host salva, i settings autodescrittivi (Fase 2)
diventano i parametri automatizzabili.

**Solo modalità live**: il batch presuppone accesso al filesystem e lavoro bloccante, cose che in
un plugin non hanno senso.

Il MIDI scritto nel buffer fornito dall'host è **migliore** dell'approccio dello standalone, non
solo diverso: non passa da un device esterno e la sincronizzazione la gestisce l'host.
`MidiMapper::toMidi` riceve già un `MidiBuffer&` e vi aggiunge eventi, quindi la firma è pronta.

Un dettaglio elegante: durante un rendering offline l'host segnala `isNonRealtime()`, e in quel
caso i nodi dichiarati non real-time — la dichiarazione verrà introdotta in Fase 10 per il
preprocessing — **possono** essere ammessi. Se le fasi 10-12 vengono realizzate, il plugin ne
beneficia automaticamente in bounce.

### Piano

1. **Target plugin** (VST3 e AU; il target standalone del plugin arriva in omaggio), con un
   `AudioProcessor` che possiede un `ProcessingGraph` costruito in `prepareToPlay`.
2. **Fattorizzare l'interfaccia**: estrarre il pannello live (selezione feature, parametri,
   mapping, metering) in un componente riusabile fra applicazione e plugin; escludere lista file,
   trasporto, opzioni CSV e finestra di progresso.
3. **Parametri esposti all'host** tramite `AudioProcessorValueTreeState`, alimentato dai
   descrittori di parametro della Fase 2.
4. **Stato del plugin** = il setup della Fase 2, con `getStateInformation` e
   `setStateInformation` che ne fanno serializzazione e deserializzazione. Un setup salvato
   dall'applicazione dev'essere caricabile nel plugin e viceversa.
5. **MIDI nel buffer dell'host**, con gli offset campione-accurati introdotti in Fase 8.
6. **Dichiarazione della latenza** con `setLatencySamples()`, alimentata dalla Fase 7. Se la
   compensazione di allineamento è attiva, l'host deve saperlo.
7. **OSC opzionale** nel plugin, attraverso la stessa coda lock-free della Fase 1.
8. **Gestione corretta di `reset()`** e del cambio di sample rate da parte dell'host, che nel
   grafo significa ripreparare o ricostruire i nodi. `reset()` deve azzerare anche lo stato di
   riga del mapping (`eventState`, Fase 8, D11): non è solo un problema dei nodi del grafo, e
   un `lastNote` sopravvissuto a un `reset()` produce una nota bloccata alla riproduzione
   successiva.
9. **Verifica con `pluginval`** e prova nei principali host, includendo esplicitamente lo
   scenario "l'host chiama `processBlock` prima che `prepareToPlay` sia stato completato" —
   uno scenario che `pluginval` testa di routine e che è la ragione concreta, non solo teorica,
   della guardia B8 introdotta in Fase 0.

### Verifica
Lo stesso setup, sullo stesso segnale, deve produrre gli stessi valori nell'applicazione e nel
plugin.

### Risultato
L'analizzatore è utilizzabile dentro un DAW, con automazione, salvataggio di stato e MIDI
sincronizzato dall'host.

---

## Fase 10 — Preprocessing

### Stato di partenza
Grafo con griglia temporale coerente e latenza dichiarata, disponibile in tre modalità di
distribuzione. Il segnale entra nell'analisi esattamente com'è nel file: nessun
ricampionamento, nessun filtraggio, nessuna gestione del silenzio se non il gate incoerente
dentro `Average`.

### Cosa è perfettibile
V2 (il gate di silenzio in un solo functional) e l'impossibilità di analizzare in modo
confrontabile un corpus con sample rate eterogenei.

### Design scelto e perché
Il preprocessing **non è una fase separata: sono altri nodi**, posti a monte nel grafo.
Trattarlo come uno stadio a parte significherebbe duplicare preparazione, gestione della latenza
e conversione dei rate — tutte cose che il grafo già fa.

Due conseguenze che il grafo deve imparare a gestire:

- **il sample rate deve propagarsi lungo gli archi.** Un nodo di ricampionamento cambia il rate
  di tutto ciò che sta a valle, quindi `prepare(sampleRate, ...)` non può essere una
  distribuzione globale: ogni nodo deve interrogare il proprio ingresso;
- **il trimming rompe la base dei tempi.** Il taglio di testa e coda è un semplice scostamento e
  si gestisce. La rimozione di silenzio interno no: dopo, il frame `k` non corrisponde più a
  nessun istante noto del file originale. La scelta è quindi **non rimuovere ma marcare**, con
  una maschera di validità per frame.

> [!TIP]
> Confermo che il trimming è solo di testa e coda (con fade). L'event detection e auto-cut in campioni separati è materiale per un secondo software.

Quella maschera risolve anche V2: il gate di silenzio smette di essere una riga dentro
`Average::store` e diventa un segnale prodotto una volta a monte, con soglia configurabile, che
tutti i functional onorano allo stesso modo. Media, mediana, deviazione standard e IQR tornano a
riferirsi alla stessa popolazione.

La catena di preprocessing **non è lineare**: `PAN` ha bisogno del segnale stereo, quindi deve
attingere a monte del downmix. Anche la sorgente è quindi un piccolo grafo:

```
File → Resample → ┬→ [PAN: stereo]
                  └→ Downmix → Filter → [spettrali e temporali]
```

Il ricampionamento merita una nota per l'uso in ricerca: analizzare un corpus a sample rate misti
verso un target comune è ciò che rende le feature confrontabili fra file, cosa che oggi non sono.

Infine, alcuni nodi non sono utilizzabili in tempo reale. Devono **dichiararlo**, e il builder
deve rifiutare un setup non eseguibile in live al momento della costruzione, invece di farlo
scoprire con un'interruzione dell'audio. La stessa dichiarazione permette al plugin di ammetterli
durante un rendering offline.

> [!TIP]
Questa storia di feature disponibili solo offline nel plugin non è fondamentale. O forse non ho capito cosa intende.

### Piano

1. **Propagare il sample rate lungo gli archi**: `prepare()` interroga la porta d'ingresso
   invece di ricevere un valore globale.
2. **Aggiungere al nodo la dichiarazione `isRealtimeCapable()`** e la validazione del setup
   contro la modalità nel builder.
3. **Implementare `Resample`** con filtro di qualità e latenza dichiarata; esporre il sample rate
   di destinazione per il batch.
4. **Promuovere `Downmix` a nodo di preprocessing** collocabile esplicitamente nella catena, e
   riorganizzare il grafo di sorgente perché `PAN` attinga a monte.
5. **Implementare `Filter`** (passa-alto, passa-basso, passa-banda) con latenza dichiarata.
   Preferire FIR a fase lineare: il ritardo costante è compensabile esattamente, mentre il
   ritardo di gruppo di un IIR dipende dalla frequenza e non lo è.
6. **Implementare `Trim`** per testa e coda, come scostamento sulla sorgente che preserva la
   corrispondenza con i tempi del file originale.
7. **Implementare `ValidityMask`**: nodo che produce un flag di validità per frame in base a una
   soglia di livello configurabile.
8. **Far onorare la maschera a tutti i functional**, rimuovendo il gate cablato da
   `Average::store`. Aggiungere al CSV una colonna con il numero di frame validi, perché
   l'informazione oggi è invisibile nell'output.
9. **Estendere il setup** (versione +1, con migrazione) per contenere la catena di preprocessing,
   con l'ordine dei nodi esplicito e modificabile: l'ordine conta, perché ricampionare prima o
   dopo un filtro dà risultati diversi.
10. **Interfaccia della catena** e opzioni corrispondenti nella riga di comando.

### Verifica
Con il preprocessing disattivato, il golden file della Fase 7 deve essere riprodotto identico.

### Risultato
Corpus con sample rate eterogenei analizzabili in modo confrontabile. Gestione del silenzio
coerente fra tutti gli aggregatori. Filtraggio e trimming disponibili.

---

## Fase 11 — Bande fisse

### Stato di partenza
Grafo completo di preprocessing, con rate e latenza gestiti lungo gli archi. Ogni feature si
calcola sull'intero spettro del segnale.

### Cosa è perfettibile
L'impossibilità di analizzare una feature su una porzione dello spettro — che per molti
descrittori è tanto sensato quanto l'analisi a banda intera.

### Design scelto e perché
La **banda è un ramo del grafo, non un parametro della feature.** Si crea un ramo che parte da
un nodo di banda e ci si appendono le feature volute; il broadband è semplicemente il ramo senza
filtro.

L'alternativa — dare a ogni feature un proprio filtro passa-banda — è stata scartata per la
stessa ragione per cui in Fase 4 è stata scartata la duplicazione delle FFT: se tre feature
vogliono la banda [200, 2000], con il filtro interno si filtra tre volte, mentre con un nodo di
banda la deduplicazione del builder lo condivide automaticamente. Ma il vantaggio maggiore è un
altro: **la banda è definita una volta sola**, e tutte le feature appese a quel ramo si
riferiscono per costruzione alla stessa cosa. Con la banda come parametro per-feature, prima o
poi due feature "sulla stessa banda" avranno definizioni leggermente diverse e non sarà evidente.

Il builder materializza **pigramente**: se sul ramo ci sono solo feature spettrali non costruisce
alcun filtro e si limita a passare l'intervallo di bin; se ce n'è almeno una nel dominio del
tempo, istanzia il filtro. Chi richiede l'analisi non deve saperlo.

Una distinzione da mantenere ferma: i filtri **intrinseci all'algoritmo** — il differenziatore
dentro `Brightness`, un'eventuale pesatura A — restano dentro la feature. Non sono bande scelte
dall'utente, sono parte della definizione del descrittore.

**Un'avvertenza da documentare**: l'RMS calcolato su un segnale filtrato e l'RMS ricavato dai bin
di quella banda **non danno lo stesso numero**. L'identità di Parseval regge solo
approssimativamente, per via della finestra, della banda di transizione del filtro e della
normalizzazione dell'overlap. Sono due definizioni diverse di "energia in banda", entrambe
legittime, e il nome della colonna deve dire quale è stata usata.

Un banco di FIR a fase lineare **tutti della stessa lunghezza** ha una proprietà preziosa: tutte
le bande hanno lo stesso ritardo e sono quindi mutuamente allineate senza compensazione.

### Piano

1. **Definire `BandSpec`** come specifica polimorfa che, dato un contesto, si risolve in
   `[fLow, fHigh]`. Implementare le specifiche fisse: Hz espliciti, ottava, terzo d'ottava, e
   `FullBand` come caso degenere.
2. **Includere la banda nella firma dei settings** — è parte della chiave di deduplicazione — e
   **nel nome delle colonne** esportate, es. `RMS_[200-2000]`.
3. **Implementare `BandNode`**, che espone due porte: un segnale filtrato nel tempo e un
   intervallo di bin per i consumatori spettrali.
4. **Materializzazione pigra nel builder**: il filtro viene istanziato solo se esiste almeno un
   consumatore nel dominio del tempo.
5. **Adattare le feature spettrali** perché accettino un intervallo di bin invece di sommare
   sempre su tutto lo spettro.
6. **Banco di FIR a fase lineare di lunghezza uniforme**, così che le bande siano mutuamente
   allineate per costruzione.
7. **Estendere il setup** (versione +1, con migrazione) con la struttura a gruppi
   `(banda, [feature])` invece di un elenco piatto: rispecchia la struttura a rami ed evita
   l'esplosione combinatoria nell'interfaccia.
8. **Interfaccia** per definire le bande e assegnare le feature a ciascuna; opzioni
   corrispondenti nella riga di comando.
9. **Documentare la differenza fra le due definizioni di energia in banda**, nell'interfaccia,
   nei nomi delle colonne e nel riferimento dei descrittori.

### Verifica
Con la sola banda intera selezionata, il golden file della Fase 10 deve essere riprodotto
identico.

### Risultato
Qualunque feature analizzabile su una o più bande, con condivisione automatica dei filtri.

---

## Fase 12 — Bande adattive

### Stato di partenza
Bande fisse funzionanti, definite dall'utente in Hz o per suddivisione dello spettro.

### Cosa è perfettibile
Le bande fisse non si adattano al contenuto: la stessa banda [0, 5000] è larga per uno strumento
grave e stretta per uno acuto. Servono bande definite **statisticamente sul segnale**, per
esempio "fino al terzo quartile dell'energia spettrale" (Q75, il rolloff spettrale al 75%).

### Design scelto e perché
Qui esiste un'asimmetria forte fra i due domini, e va sfruttata come regola invece di essere
subita.

Nel **dominio della frequenza** una banda adattiva è esatta e gratuita: si cambia l'indice
superiore della sommatoria. Nel **dominio del tempo** è patologica: una banda che si muove frame
per frame è un filtro modulato, con interpolazione dei coefficienti, artefatti, stabilità da
verificare se è IIR, e un transitorio di assestamento durante il quale la banda non è nemmeno
definita.

Da qui la regola: **bande adattive per frame solo per le feature spettrali; bande fisse per
entrambi i domini.**

Ma c'è una possibilità migliore per l'uso offline, ed è quella che risolve davvero il problema:
**calcolare Q75 sull'intero file** invece che per frame. Si ottiene una banda **costante per quel
file** — più interpretabile statisticamente, perché i functional aggregano una feature su una
banda ferma anziché su un bersaglio mobile, e soprattutto **compatibile con il dominio del
tempo**, dato che una banda fissa è un filtro fisso. Costa una passata preliminare leggera sul
file, che accumula il solo spettro medio.

> [!TIP]
> Qui Claude si è fattə un sacco di pippe, ma la banda fissa è quello che intendevo. Tuttavia se costa poco, anche il valore per frame può avere senso.

> [!TIP]
> Qui si parla solo di frequenza Q75, ma anche altre caratterisriche potrebbero essere esplorabili, ad esempio nel dominio del tempo l'integrated lufs, oltre alla soglia fissa ne ha una che dipende dal momentary.

Sono quindi **due descrittori distinti**, con nomi distinti:

| | `Q75_frame` | `Q75_file` |
|---|---|---|
| Calcolo | per frame | sull'intero file |
| Modalità | live e offline | solo offline |
| Domini | solo spettrale | spettrale e temporale |
| Costo | nullo | una passata aggiuntiva |
| Interpretazione | bersaglio mobile | banda ferma per file |

`Q75_file` si incastra con una decisione già presa per altre ragioni: **un grafo per file**. Una
banda costante per file è semplicemente un parametro noto al momento in cui quel grafo viene
costruito. Il meccanismo della passata preliminare è generale: serve a qualunque nodo i cui
parametri dipendano da statistiche globali del file.

Due cautele di interpretazione, entrambe da tradurre in requisiti:

- **esporre i bordi della banda come colonne di output.** Senza, una variazione del centroide in
  [0, Q75] può venire dallo spettro o dalla banda che si è spostata, e a valle degli aggregatori
  non è più distinguibile;
- **dichiarare rispetto a quale spettro Q75 è calcolato.** Appena si annida una banda dentro
  un'altra la cosa diventa ambigua: il riferimento dev'essere sempre il nodo genitore,
  esplicitamente.

### Piano

1. **Estendere `BandSpec` alle specifiche adattive**, con il quantile come parametro (Q50, Q75,
   Q90) e il riferimento al nodo genitore esplicito.
2. **Implementare `Q75_frame`** come nodo che dipende dallo spettro e produce una frequenza. Il
   builder deve **rifiutare** un ramo temporale appeso a una banda adattiva per frame, con un
   messaggio d'errore comprensibile.
3. **Introdurre nel worker la passata di analisi preliminare** come meccanismo generale,
   opzionale e attivata solo se il setup lo richiede: legge il file una volta accumulando le
   statistiche necessarie, e solo dopo costruisce il grafo definitivo.
4. **Implementare `Q75_file`**, utilizzabile da entrambi i domini poiché produce una banda fissa
   per il file.
5. **Esporre i bordi delle bande adattive come colonne** dell'export, sia per frame sia
   aggregate.
6. **Smoothing opzionale su `Q75_frame`**: il rolloff calcolato frame per frame è rumoroso.
7. **Estendere il setup** (versione +1, con migrazione) e l'interfaccia, con indicazione chiara
   di quali feature sono compatibili con quale tipo di banda e avviso del costo della passata
   aggiuntiva.
8. **Documentare i due descrittori** nel riferimento, con definizione precisa e avvertenze di
   interpretazione. È materiale che serve a chi userà i dati, non solo a chi scrive il codice.

### Verifica
Con le sole bande fisse, il golden file della Fase 11 deve essere riprodotto identico. Con
`Q75_file` su un segnale stazionario, la banda deve coincidere con quella calcolata a mano sullo
spettro medio.

### Risultato
Bande definite statisticamente sul contenuto, in due varianti dalla semantica esplicita.

---

## Fase 13 — Esportazione del paragrafo metodologico

### Stato di partenza
Motore completo. Il software conosce, in forma strutturata, tutto ciò che riguarda un'analisi: i
descrittori usati con i loro parametri, unità di misura e definizioni (Fasi 2 e 4); la catena di
preprocessing (Fase 10); la griglia temporale, la finestra, l'hop e la modalità di allineamento
(Fase 7); le bande (Fasi 11-12); gli aggregatori e il trattamento del silenzio (Fase 10); la
versione del software e il setup completo (Fasi 2 e 3); la bibliografia (Fase 5).

### Cosa è perfettibile
D10. Tutta quell'informazione, al momento di scrivere un articolo, viene trascritta a mano: lento,
sistematicamente incompleto e non verificabile.

### Design scelto e perché

**Generazione deterministica per riempimento di modelli, non generazione di testo.** Lo stesso
setup deve produrre byte identici, sempre. Questo rende l'esportatore verificabile con un test su
file di riferimento come qualunque altra parte del sistema, e soprattutto rende il documento
prodotto un artefatto *derivato* dal setup, non un commento su di esso. Nessun modello
linguistico, nessuna variabilità.

**Fatti, non giustificazioni.** Il documento riporta che cosa è stato calcolato e con quali
parametri. Non tenta di spiegare *perché* quei descrittori sono stati scelti o che cosa
significano i risultati: sono decisioni dell'autore, e un testo generato che provasse a
formularle produrrebbe esattamente il riempitivo che i revisori riconoscono a prima vista. Il
paragrafo è un punto di partenza da rileggere e adattare, e va presentato come tale.

**Un fascicolo, non un paragrafo solo.** Una prosa scorrevole non può contenere trenta parametri
senza diventare illeggibile. L'esportazione produce quindi quattro elementi complementari:

| Elemento | Contenuto | Destinazione tipica |
|---|---|---|
| Paragrafo | prosa continua, i parametri essenziali | sezione *Methods* |
| Tabella dei parametri | ogni descrittore, ogni parametro, valore e unità | appendice o materiale supplementare |
| File `.bib` | solo le voci effettivamente citate | bibliografia |
| Setup | il file di configurazione stesso | materiale supplementare |

> [!TIP]
> Però anche l'opzione "un paragrafo solo" può essere utile per tutte le analisi standard.

**Chiusura del ciclo di riproducibilità.** Il paragrafo riporta un'impronta breve del setup, la
stessa scritta nell'intestazione dei CSV prodotti (Fase 3, punto 11). Un lettore che disponga del
setup pubblicato come materiale supplementare può quindi verificare che paragrafo, dati e
configurazione corrispondano, e rieseguire l'analisi con `featextract --setup ...` ottenendo gli
stessi numeri. È l'anello che chiude la catena descritta in §3.6.

**Lingua inglese**, indipendentemente dalla lingua dell'interfaccia.

Questa fase viene per ultima perché consuma metadati prodotti da quasi tutte le altre. È però
anche la fase che dà valore retroattivo a quelle scelte: le unità di misura dichiarate in Fase 4,
la dichiarazione della latenza in Fase 7, la maschera di validità in Fase 10 erano state
introdotte per ragioni tecniche, e qui diventano contenuto descrittivo.

### Piano

1. **Definire `RunSummary`**, la controparte del setup sul lato esecuzione: numero di file
   analizzati, durata complessiva dell'audio, frame totali e frame validi, sample rate incontrati
   nel corpus, eventuali file scartati con la ragione. Sono informazioni che oggi il worker ha e
   getta.
2. **Definire `MethodsReporter`**, componente del nucleo che prende `(Setup, RunSummary,
   Bibliography)` e produce il fascicolo. Non è un `IResultSink`: non consuma frame, consuma
   metadati.
3. **Scrivere il modello del paragrafo**, con sezioni attivate solo se pertinenti — nessuna frase
   sul preprocessing se il preprocessing non è stato usato:
   - software, versione, DOI;
   - corpus: numero di file, durata complessiva, sample rate;
   - preprocessing: ricampionamento con frequenza di destinazione, downmix, filtri con tipo e
     frequenze di taglio, trimming;
   - griglia di analisi: dimensione della finestra, hop, funzione finestra, frame rate
     risultante, modalità di allineamento e compensazione della latenza;
   - descrittori: nome, definizione sintetica, parametri non predefiniti, citazione;
   - bande, se presenti, con la definizione e — nel caso adattivo — la specificazione della
     variante e del riferimento;
   - aggregazione: quali statistiche, e **come è stato trattato il silenzio**, con la soglia e la
     percentuale di frame esclusi;
   - impronta del setup.
4. **Tabella dei parametri**, che elenca *tutti* i parametri, anche quelli lasciati al valore
   predefinito; la prosa cita invece solo quelli modificati, per restare leggibile.
5. **Generazione del `.bib`** contenente le sole voci citate, deduplicate per chiave.
6. **Tre formati di uscita**: testo semplice, Markdown, LaTeX. Quest'ultimo con `\cite{}` per i
   riferimenti e una tabella `tabular` già formattata.
7. **Requisiti di determinismo**, verificati da un test dedicato: ordine di enumerazione fisso e
   dichiarato, numeri formattati con precisione esplicita e indipendente dal locale, nessuna
   data né percorso di sistema nel corpo del documento. La data di generazione, se presente, va
   in una riga di provenienza chiaramente separata ed esclusa dal confronto.
8. **Comando nell'interfaccia** («Export methods…»), attivo sia dopo un batch sia sul solo setup
   senza aver eseguito nulla — in quel caso senza le informazioni di `RunSummary`.
9. **Opzione `--export-methods`** nella riga di comando, con scelta del formato; utilizzabile
   anche insieme a `--validate` per produrre la descrizione senza eseguire l'analisi.
10. **Preferenze di esportazione nel setup** (versione +1, con migrazione): formato, verbosità,
    inclusione o meno della tabella completa.
11. **Test su file di riferimento** per l'esportatore stesso: un setup di prova, un documento
    atteso, confronto byte per byte.
12. **Un esempio completo nella documentazione**: setup, paragrafo generato, `.bib`, e il
    documento LaTeX compilato. Serve a mostrare il risultato reale a chi deve decidere se
    fidarsene.

### Cosa questa funzione non deve fare

Vale la pena metterlo per iscritto, perché la tentazione di allargare è forte e sarebbe un errore.

- **Non deve scrivere la motivazione delle scelte.** «I descrittori spettrali sono stati scelti
  per la loro rilevanza percettiva» è una frase che l'autore deve decidere se sostenere, e in
  quale forma. Un testo generato che la produce automaticamente non aggiunge informazione e
  toglie credibilità al resto.
- **Non deve commentare i risultati.** L'esportatore descrive il metodo, non l'esito.
- **Non deve essere incollato senza rilettura**, e l'interfaccia farà bene a dirlo. È un punto di
  partenza completo e corretto, non una sezione finita: l'ordine di esposizione, il livello di
  dettaglio e l'integrazione con il resto dell'articolo restano lavoro dell'autore.
- **Non deve nascondere le lacune.** Se un descrittore non ha un riferimento bibliografico perché
  è una formulazione originale, il documento deve dirlo esplicitamente anziché ometterlo. Una
  descrizione che tace su un punto è peggiore di una che lo segnala.

### Verifica
Lo stesso setup esportato due volte, su macchine e con impostazioni locali diverse, deve produrre
file identici. Il `.bib` generato deve compilare senza voci mancanti né inutilizzate. Ogni valore
numerico presente nel paragrafo dev'essere rintracciabile nel setup.

### Risultato
Un'analisi diventa descrivibile in una pubblicazione senza trascrizione manuale, in modo completo
e verificabile. Il ciclo si chiude: setup → analisi → dati → descrizione, tutto derivato dallo
stesso artefatto e tutto ricontrollabile.

---

## Fase 14 — Visualizzazione e taratura dei parametri

> [!TIP]
> Questa fase nasce da un design separato (design-visualizzazione-v2.md), non è sequenziale come le altre: il prerequisito stretto è solo la Fase 4. La maschera di validità sovrapposta richiede anche la Fase 10; senza, la vista mostra il solo gate di silenzio attuale. Può quindi iniziare subito dopo la Fase 4 e procedere in parallelo alle fasi 5-13.

### Stato di partenza
Motore a grafo (Fase 4), descrittori di canale e di parametro autodescrittivi (con il campo
"asse di lettura" anticipato in Fase 2 e il flag `isDiagnostic` anticipato in Fase 4), nessuna
vista oltre alla forma d'onda grezza.

### Cosa è perfettibile
Non esiste modo di vedere l'effetto di un parametro prima di lanciare un'analisi completa, né di
produrre una figura per una pubblicazione dallo stesso motore che produce i CSV. Oggi l'unica via
sarebbe riesportare i dati e ridisegnarli altrove — con il rischio concreto che la figura mostri
numeri diversi da quelli pubblicati.

### Design scelto e perché
Ogni feature si disegna su un **substrato** scelto dall'unità di *uscita* — mai dall'ingresso: il
controesempio guida è `Brightness`, calcolata nel dominio del tempo ma letta in Hz sullo
spettrogramma — più un vocabolario chiuso di **sei geometrie di sovrapposizione** (traiettoria,
livello, posizione, marcatore, fascia, maschera). Le feature adimensionali vanno in una striscia
con asse proprio, sincronizzato ma non commensurabile col substrato. Le feature vettoriali per
frame (il chromagram) sono un campo 2D che condivide il disegnatore con lo spettrogramma,
cambiando solo il tipo di asse Y (continuo contro categoriale). Il ricalcolo a ogni modifica di
parametro sfrutta le firme dei nodi già presenti dal builder di Fase 4: nessun meccanismo nuovo,
solo invalidazione mirata a valle del nodo cambiato.

Il vincolo non negoziabile, lo stesso che regge l'intero piano fin dalla Fase 0: **la
visualizzazione usa lo stesso grafo dell'analisi, mai un motore di disegno alternativo.** Una
figura pubblicata che mostra numeri diversi da quelli della tabella è un errore che non si
recupera.

### Piano

1. **Implementare la presa (`tap`)**: si registra su un nodo qualunque di `m_execution` (Fase 4,
   punto 8) — inclusi nodi interni mai richiesti dall'utente, come gli inviluppi di `Brightness`
   — e ne accumula l'output su una regione selezionata, leggendo semplicemente `output()`.
2. **Costruire il grafo di visualizzazione** = sottografo di analisi (nodi riusati per firma) +
   eventuali nodi di sola visualizzazione. Regola non negoziabile: questi ultimi non entrano mai
   nel setup né nelle firme di deduplicazione dell'analisi — cambiare la risoluzione dello
   spettrogramma di sfondo non deve invalidare nulla del calcolo.
3. **Implementare il vocabolario delle sei geometrie** come primitive riusabili da qualunque
   feature tramite il proprio descrittore di canale (unità + asse) — nessun codice di disegno
   specifico per feature.
4. **Implementare la scelta automatica sovrapposizione/striscia collegata**, derivata dalla
   commensurabilità dell'unità con gli assi del substrato.
5. **Implementare un solo disegnatore di campo 2D**, parametrizzato per asse continuo (Hz, per lo
   spettrogramma) o categoriale (classi di altezza, per il chromagram): stesso disegnatore,
   cambia solo il tipo di asse Y e la provenienza delle etichette.
6. **Attivare i canali diagnostici** anticipati in Fase 4 (`isDiagnostic`, `wantDiagnostics`): qui
   si accende il calcolo condizionale nei nodi che li dichiarano — l'esempio guida è la Wiener
   entropy, dove le due medie che la compongono sono commensurabili con l'asse dello spettro, il
   loro rapporto no.
7. **Rendere visibile il parametro sul grafico**: usare il campo "asse di lettura" anticipato in
   Fase 2 per disegnare l'estensione del parametro (segmento temporale, fascia in frequenza,
   linea di livello) senza scrivere codice di disegno per-feature.
8. **Le due sovrapposizioni sempre presenti**: l'aggregato come *livello* sullo stesso asse
   dell'andamento; la maschera di validità (richiede Fase 10 — prima di allora mostrare il solo
   gate attuale, dichiarandone esplicitamente i limiti in interfaccia).
9. **Ricalcolo incrementale**: nessun meccanismo nuovo — le firme di nodo del builder (Fase 4) già
   distinguono ciò che resta valido da ciò che va ricostruito quando cambia un parametro.
10. **Percorso di esportazione in qualità editoriale**: secondo disegnatore sugli stessi dati,
    dimensioni fisiche in millimetri, formati vettoriali (PDF/SVG), tipografia esplicita, modalità
    monocromatica (tratteggio e spessore oltre al colore), assi controllati esplicitamente (mai
    auto-scaling), titolo e legenda disattivabili. Il file di corredo con la provenienza riusa
    l'**intestazione di provenienza** già introdotta per i CSV (Fase 3, punto 11) e la stessa
    **impronta del setup** usata nel documento metodologico (Fase 13): stesso meccanismo, terzo
    utilizzo, non una terza implementazione.
11. **Comando "congela"** per il ponte fra live e offline: cattura N secondi di ingresso live in
    un buffer e apre la vista offline su quella cattura, per tarare su materiale reale quando il
    file non esiste ancora.
12. **Test di determinismo**: stessa regione, stesso setup, stesso stile di esportazione → file
    identico byte per byte. Stesso principio già richiesto in Fase 13 per il documento
    metodologico, qui applicato alle figure.

### Cosa questa fase non deve fare
Nessun cruscotto multi-feature (una feature alla volta). Nessuna interazione oltre selezione,
zoom e scrub. Nessuna rappresentazione tridimensionale. Nessun percorso di calcolo alternativo
per il disegno (il vincolo del paragrafo precedente). Nessun confronto fra impostazioni in questa
versione — rimandato, con la traccia fantasma singola come sviluppo futuro più economico se sarà
ripreso.

### Verifica
La vista in fase di taratura e il CSV emesso da un batch sullo stesso file e sullo stesso setup
devono mostrare esattamente gli stessi numeri. Un'esportazione ripetuta con lo stesso setup e lo
stesso stile deve produrre un file identico.

### Risultato
Uno strumento di taratura interattivo che usa lo stesso motore dell'analisi, e un percorso di
esportazione di figure in qualità editoriale che non può divergere strutturalmente dai dati
pubblicati.

---

# Parte III — Note finali

## Ordine e flessibilità

Le fasi **0-5 sono sequenziali e non riordinabili**: ognuna rimuove un ostacolo alla successiva.
In particolare la Fase 3 dipende dalla 2 (il modello dev'essere fuori dall'interfaccia prima di
poter estrarre un nucleo senza GUI) e la Fase 4 dipende dalla 3 (la strategia di registrazione
cambia a seconda che il codice stia in un eseguibile o in una libreria).

Dalla Fase 6 in poi c'è margine:

- la **Fase 6** (parallelizzazione) può essere rinviata se il tempo di elaborazione non è un
  problema, ma conviene farla presto perché velocizza tutte le verifiche successive;
- la **Fase 6bis** (resume del batch) può essere rinviata a piacere o saltata del tutto se i
  corpus restano piccoli — il solo vincolo è che non preceda la Fase 6, da cui eredita la
  marcatura di completamento thread-safe;
- le **fasi 8 e 9** (mapping e plugin) possono essere spostate più avanti se l'uso in tempo reale
  non è prioritario; l'unico vincolo è che la 8 preceda la 9, così che il plugin nasca con il
  mapping buono;
- le **fasi 10 e 11** potrebbero invertirsi se le bande fossero più urgenti del preprocessing, al
  prezzo di implementare i filtri di banda prima dell'infrastruttura che poi li ospiterà;
- la **Fase 13** è posta per ultima perché consuma i metadati di tutte le altre, ma **può essere
  anticipata in forma ridotta** se una scadenza di pubblicazione lo richiede: l'esportatore emette
  ciò che esiste al momento e ignora le sezioni non ancora implementate. Il prerequisito reale è
  solo la Fase 5, che fornisce la bibliografia;
- la **Fase 14** (visualizzazione) è la più libera di tutte: dipende solo dalla Fase 4 nella sua
  forma ridotta, dalla Fase 10 in più per la maschera di validità completa, e può procedere in
  parallelo alle fasi 5-13 senza alcun vincolo di ordine verso di esse — non consuma nulla che
  quelle fasi producano, a parte i due campi anticipati nelle Fasi 2 e 4.

## I due punti che non conviene rimandare

**La Fase 2.** Finché il modello di dominio vive dentro un componente dell'interfaccia grafica,
ogni fase successiva costa di più: non si può testare senza interfaccia, non si può estrarre un
nucleo, non si può parallelizzare, non si possono avere due istanze della stessa feature. Ed è la
fase che introduce i settings autodescrittivi, da cui dipendono sette cose diverse in altrettante
fasi successive.

**La Fase 5.** Se è previsto che altri contribuiscano descrittori, va fatta prima delle fasi
7-12, non dopo: ogni descrittore scritto prima che l'API sia semplice e documentata è un
descrittore da riscrivere. Se invece il progetto resterà nelle mani di una sola persona, può
scivolare — ma conviene comunque farla prima della Fase 11, perché le bande moltiplicano il
numero di combinazioni da documentare, e prima della Fase 13, che dalla sua bibliografia dipende
interamente.

## Cosa conservare

Il codice attuale contiene scelte che il piano **non** deve rimuovere e che vale la pena
riconoscere:

- nessun singleton e nessuna variabile globale;
- l'asse **Feature × Functional**, astrazione ortogonale e scalabile, che regge senza modifiche
  fino alla fine del piano e copre da sola i descrittori "a due passate" come il low energy
  ratio;
- `StdDev` in forma incrementale (somme e somme di quadrati) invece che a due passate, e
  `Median` con `nth_element` invece di `sort`: entrambe le scelte giuste per lo streaming;
- la comunicazione fra interfaccia e motore tramite `std::function` invece che per dipendenza
  diretta;
- l'annullamento del batch controllato a granularità di blocco e non di file;
- la separazione `prepareToPlay` / `processBlock` / `releaseResources`, che segue il contratto
  JUCE ed è la ragione per cui la Fase 9 è un'aggiunta e non una riscrittura.
