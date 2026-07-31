# FeatureExtractor — design della visualizzazione delle feature (revisione 2)

Documento di progettazione autonomo. Riguarda la rappresentazione grafica dei descrittori;
l'incastro con il piano di implementazione è rimandato.

---

## 1. I due scopi

### 1.1 Scopo principale: impostare i parametri

Tarare un parametro significa rispondere a una domanda di forma: *se muovo questo valore, che
cosa cambia?* Ne discendono due requisiti che una visualizzazione puramente descrittiva non ha:

| Requisito | Conseguenza |
|---|---|
| Il **materiale deve stare fermo** mentre il parametro si muove | si tara solo su segnale fisso (§7) |
| Il **ciclo modifica → riscontro** dev'essere immediato | ricalcolo incrementale, non ricostruzione (§6) |

A questi si aggiunge una terza esigenza, meno ovvia e più specifica di questo strumento:
**vedere il parametro, non solo il suo effetto** (§4). È ciò che collega un numero astratto a
qualcosa di riconoscibile sul grafico, ed è la funzione che più di ogni altra distingue uno
strumento di taratura da un visualizzatore.

### 1.2 Scopo secondario: figure per la pubblicazione

Dopo un'analisi su un corpus capita di individuare un file interessante e di volerne stampare il
grafico di una feature per un articolo. È un uso diverso dal primo — non interattivo, non
iterativo, ma con requisiti di qualità che il primo non ha — e va progettato esplicitamente
perché condiziona il modo in cui il disegno viene prodotto (§5).

I due scopi condividono l'intero impianto e divergono solo nell'ultimo stadio: la resa.

---

## 2. Il modello: substrato e sovrapposizione

### 2.1 L'osservazione di partenza

In una figura come quella di riferimento, ogni feature è disegnata **sopra una rappresentazione
del segnale**, che le fornisce contesto e scale:

| Pannello | Substrato | Sovrapposto |
|---|---|---|
| Waveform | segnale nel tempo | inviluppo AM |
| Power Spectrum | spettro medio della regione | picco spettrale, quartili, medie |
| Spectrogram | spettro nel tempo | F0 per frame + F0 media |

**Una vista = un substrato + una sovrapposizione.** È il modello di base. Ma non copre tutti i
casi, e le eccezioni sono importanti: alcune feature non si sovrappongono a niente perché sono
esse stesse l'immagine (§3.3), e alcune si spiegano meglio con le proprie quantità interne che
con il proprio risultato (§3.4).

### 2.2 Il substrato si sceglie dall'unità di USCITA

Sarebbe naturale far coincidere il substrato con l'ingresso della feature — «`F0` dipende dallo
spettro, quindi si disegna sullo spettro». **È sbagliato**, e il controesempio è netto:

> `Brightness` si calcola nel dominio del tempo, a partire da inviluppi RMS, ma **produce un
> valore in Hz**. Il posto dove ha senso guardarla è lo spettrogramma, che con il suo ingresso
> non ha niente a che vedere.

La regola corretta è quindi: **il substrato è la rappresentazione il cui asse è commensurabile
con ciò che la feature produce.** L'ingresso non ha alcun ruolo nella scelta.

```
uscita in Hz                → spettrogramma (andamento) | spettro medio (aggregato)
uscita in ampiezza/guadagno → forma d'onda
uscita adimensionale        → striscia collegata (§3.2)
uscita vettoriale per frame → campo 2D proprio (§3.3)
```

L'informazione necessaria è già dichiarata: è l'unità nel descrittore di canale.

### 2.3 Conseguenza: il substrato può non esistere nel grafo di analisi

Se l'utente sta tarando `Brightness` e non ha selezionato nessuna feature spettrale, nel grafo di
analisi **non c'è nessun nodo spettro** — eppure serve uno spettrogramma di sfondo.

La vista costruisce quindi un **grafo di visualizzazione** = sottografo di analisi + eventuali
nodi di substrato aggiunti per il solo disegno. Due regole per evitare confusione:

- se un nodo adatto esiste già nel grafo di analisi, **lo si riusa**: è gratis e garantisce che
  sfondo e feature guardino la stessa cosa;
- altrimenti se ne crea uno **di sola visualizzazione**, i cui parametri (risoluzione dello
  spettrogramma, per esempio) sono impostazioni della vista. **Non entrano nel setup e non
  toccano le firme usate per la deduplicazione in analisi**: un cambiamento di risoluzione dello
  sfondo non deve invalidare nulla del calcolo.

### 2.4 Che cosa serve al motore: la presa

L'unica capacità realmente nuova è una **presa** (*tap*) che registri l'uscita di un nodo sulla
regione selezionata. Un substrato è esattamente questo: la registrazione dell'uscita di un nodo,
resa secondo il tipo di porta.

È un'aggiunta piccola e ben delimitata, e serve anche altrove: è lo stesso meccanismo che
vorrebbe uno strumento di diagnosi per ispezionare un punto intermedio del grafo.

---

## 3. Le tre modalità di resa

### 3.1 Sovrapposizione — un vocabolario chiuso

Quando l'unità della feature è commensurabile con un asse del substrato, si disegna dentro quegli
assi. Le geometrie necessarie sono poche, ed è questo che rende il sistema estendibile senza
scrivere un disegnatore nuovo per ogni descrittore:

| Geometria | Che cos'è | Esempio |
|---|---|---|
| **Traiettoria** | serie temporale su un asse del substrato | F0 per frame sullo spettrogramma |
| **Livello** | scalare sull'asse Y | F0 media, soglia di silenzio |
| **Posizione** | scalare sull'asse X | quartili spettrali |
| **Marcatore** | punto (X, Y) | picco spettrale |
| **Fascia** | coppia di valori su un asse | estensione AM, intervallo [Q25, Q75] |
| **Maschera** | regione di validità/esclusione | frame scartati dagli aggregatori |

### 3.2 Striscia collegata — quando le unità non sono commensurabili

Una feature adimensionale non ha un'altezza sensata su uno spettro in dB. In quel caso va sotto,
in una striscia con **asse Y proprio e asse X condiviso e sincronizzato** con il substrato.

La regola non va dichiarata a mano, si deriva dall'unità del canale:

```
unità commensurabile con un asse del substrato  →  sovrapposizione
altrimenti                                       →  striscia collegata
```

### 3.3 Campo proprio — quando la feature È l'immagine

Alcune feature non si sovrappongono a niente. Il **chromagram** produce dodici valori per frame:
la sua rappresentazione naturale è una matrice con dodici elementi sull'asse Y e i frame
temporali sull'asse X. Non è un'annotazione sopra qualcos'altro, è il grafico.

Vale per qualunque feature **vettoriale per frame** con canali che formano un insieme coerente.

Un'economia importante: **questo è strutturalmente lo stesso disegnatore dello spettrogramma** —
un campo bidimensionale su (tempo × indice). Cambia solo la natura dell'asse Y:

| | Spettrogramma | Chromagram |
|---|---|---|
| Asse Y | continuo, in Hz | **categoriale**, classi di altezza |
| Etichette | scala numerica, lineare o logaritmica | C, C♯, D, … |
| Origine | nodo di substrato | la feature stessa |

Le etichette categoriali arrivano gratis: sono i nomi dei canali che il descrittore già dichiara.
Serve quindi **un solo disegnatore di campo 2D con due tipi di asse**, non due disegnatori.

### 3.4 Canali diagnostici — quando il risultato non è la cosa da guardare

Questo è il caso che una prima analisi della figura di riferimento aveva frainteso, e la
correzione porta a un pezzo di design che mancava.

La **Wiener entropy** è il rapporto fra media geometrica e media aritmetica dello spettro. Ciò
che compare utilmente sul grafico non è quel rapporto — adimensionale, senza un'altezza sensata
in dB — ma **le due medie**, che sono in dB, sono perfettamente commensurabili con l'asse dello
spettro, e la cui vicinanza *è* l'informazione: quando coincidono lo spettro è piatto, quando
divergono è concentrato.

Generalizzando: **per molte feature la quantità da guardare per tarare un parametro non è
l'uscita, ma una grandezza interna da cui l'uscita deriva.** Vale in particolare per ogni feature
con una soglia: ciò che serve vedere è la grandezza a cui la soglia si applica, con la soglia
disegnata sopra. Il valore di uscita, di per sé, non dice se la soglia è messa bene.

Il design lo prevede così: una feature può dichiarare **canali diagnostici**, cioè uscite
aggiuntive destinate al solo disegno.

- Sono esclusi dallo schema e non compaiono mai nel CSV né nel mapping.
- Hanno il proprio descrittore di canale, quindi la scelta fra sovrapposizione e striscia segue
  le stesse regole delle uscite normali. Le due medie dell'entropia sono in dB e si sovrappongono
  allo spettro medio; il rapporto va nella striscia o come lettura numerica.
- Hanno un costo, quindi **si calcolano solo su richiesta**: il nodo riceve in preparazione
  l'indicazione che le diagnostiche servono, e in batch e in live non le produce.

### 3.5 Due sovrapposizioni sempre presenti

Indipendentemente dalla feature, due elementi vanno mostrati sempre, perché riguardano
direttamente parametri da tarare.

**L'aggregato accanto all'andamento.** Se sono selezionati dei functional, disegnarne il
risultato come *livello* sullo stesso asse dell'andamento. Mostra a colpo d'occhio se l'aggregato
è rappresentativo: se la mediana cade dove la traiettoria non passa quasi mai, il problema è nei
parametri o nella soglia di silenzio — cioè esattamente ciò che si sta cercando di capire.

**La maschera di validità.** Ombreggiare le regioni dell'asse temporale che gli aggregatori
scarteranno. La soglia di silenzio è essa stessa un parametro da tarare ed è la fonte di sorprese
meno visibile: senza questa ombreggiatura si guarda un aggregato senza sapere su quanti frame è
calcolato.

---

## 4. Rendere visibile il parametro

Un parametro numerico è astratto; la stessa quantità disegnata sul grafico è immediata. Essendo
stato messo da parte il confronto fra impostazioni (§9), questa è **la** funzione che rende lo
strumento utile alla taratura, e va curata di conseguenza.

- finestra RMS di 35 ms → un **segmento orizzontale lungo 35 ms**
- dimensione FFT → un segmento della durata corrispondente, con accanto la risoluzione in
  frequenza che ne deriva
- intervallo di ricerca di `F0` fra 80 e 400 Hz → una **fascia orizzontale** sullo spettrogramma
- soglia di silenzio → una **linea di livello** sull'inviluppo
- banda [200, 2000] Hz → una fascia sull'asse delle frequenze

La parte interessante è che è **in buona parte generico**: i descrittori di parametro portano già
l'unità di misura, quindi un parametro in secondi si disegna come estensione temporale, uno in
hertz come fascia di frequenza, uno in guadagno come livello. Serve solo che il descrittore
dichiari, oltre all'unità, **su quale asse** il parametro va letto — un campo in più, e la
maggior parte delle feature non deve scrivere codice di disegno.

Combinato con il ricalcolo immediato (§6), questo dà il ciclo di taratura completo: si muove lo
slider, si vede l'estensione del parametro cambiare sul grafico, e si vede contemporaneamente
come cambia la feature.

---

## 5. Esportazione in qualità editoriale

### 5.1 Non è uno screenshot

Il requisito non è salvare ciò che si vede, ma **ridisegnare** alle dimensioni e alla risoluzione
di destinazione. La differenza è sostanziale: la decimazione della forma d'onda calcolata per la
larghezza dello schermo produrrebbe, ingrandita, una figura sbagliata — non solo brutta, ma con
picchi nel posto sbagliato.

L'esportazione è quindi un secondo percorso di resa che parte dagli stessi dati e li disegna con
parametri diversi. La sequenza `substrato → geometrie → disegnatore` è la stessa; cambia solo il
disegnatore.

### 5.2 Requisiti

**Formato vettoriale**: PDF, che è ciò che serve a LaTeX, e SVG per l'editing successivo. Il
raster ad alta risoluzione come ripiego.

**Dimensioni fisiche**, non in pixel: larghezza in millimetri, con i valori tipici di colonna
singola (~85 mm) e doppia (~170 mm) come preimpostazioni. È l'unità in cui il problema è posto.

**Tipografia esplicita**: famiglia e corpo del carattere, per accordarsi al documento; spessori
delle linee.

**Modalità monocromatica.** Molte riviste fanno pagare il colore, e una figura deve restare
leggibile in scala di grigi. Le geometrie sovrapposte vanno quindi distinte anche da tratteggio e
spessore, non dal solo colore — un vincolo che ricade sul vocabolario del §3.1 e va rispettato
fin da subito, non aggiunto dopo.

**Controllo esplicito degli assi**: intervalli, passo delle tacche, etichette con unità, scala
lineare o logaritmica. L'auto-scaling va bene per lo schermo, non per una figura che deve
affiancarsene un'altra.

**Elementi opzionali**: titolo e legenda disattivabili, perché nelle pubblicazioni quel contenuto
sta di norma nella didascalia.

### 5.3 Provenienza

L'esportazione emette accanto alla figura un file di corredo con i parametri usati, il file e la
regione, e l'**impronta del setup** — la stessa scritta nell'intestazione dei CSV. Serve a due
cose: scrivere una didascalia verificabile, e ritrovare a distanza di tempo con quali
impostazioni quella figura era stata prodotta.

Ne segue anche un requisito di **determinismo**: stessa regione, stesso setup, stesso stile →
file identico.

### 5.4 Il vincolo che l'esportazione rende non negoziabile

La visualizzazione deve usare **lo stesso grafo dell'analisi**, mai una reimplementazione
semplificata per il disegno. Vale già per la taratura — altrimenti si tarano i parametri contro
qualcosa che non è ciò che il batch calcolerà — ma con l'esportazione diventa più grave: una
figura pubblicata che mostra numeri diversi da quelli della tabella è un errore che non si
recupera.

---

## 6. Ricalcolo incrementale

Perché il ciclo sia immediato, muovere un parametro non deve ricalcolare tutto.

Il grafo dà la risposta senza meccanismi aggiuntivi. Cambiare un parametro cambia la firma di
quel nodo, e **solo di quello**: ciò che sta a monte conserva la propria firma e resta valido,
ciò che sta a valle va ricostruito.

```
Source → Downmix → MagnitudeSpectrum → SpectralCentroid
         └──── invariati, riusati ────┘   └─ solo questo si ricostruisce ─┘
```

Il **substrato si calcola una volta sola** — ed è la parte cara, lo spettrogramma soprattutto —
mentre a ogni movimento di slider si ricalcola solo il ramo a valle, sulla sola regione
selezionata.

Ne consegue che selezionare una porzione non è solo comodità di lettura: è ciò che rende il ciclo
abbastanza rapido da sembrare continuo. Su una regione di dieci o venti secondi si sta ampiamente
sotto la soglia di percezione per la maggior parte dei descrittori.

---

## 7. Offline o realtime

Per lo scopo principale la risposta è **offline**, e non per comodità implementativa.

Tarare significa iterare sullo stesso materiale: si guarda, si corregge, si riguarda. Se
l'ingresso cambia fra un'occhiata e l'altra, la differenza che si osserva non è attribuibile al
parametro — può venire dal segnale. Non è che il realtime tari peggio: non consente di tarare.
Lo scopo secondario, la figura per la pubblicazione, è per sua natura offline: riguarda un file
e una regione scelti.

Le due visualizzazioni sono quindi **strumenti distinti con scopi distinti**, e conviene
trattarli come tali invece di cercare un compromesso che serve male entrambi:

| | Offline (taratura, figure) | Realtime (monitoraggio) |
|---|---|---|
| Scopo | scegliere i parametri, stampare | verificare che l'uscita risponda |
| Substrato | regione fissa, ricalcolabile | finestra scorrevole |
| Interazione | selezione, zoom, scrub | nessuna |
| Esportazione | sì | no |
| Complessità | dove va investita | minima |

Per il realtime basta il segnale nel dominio della feature con la feature sovraimpressa, in una
finestra scorrevole. Il suo valore vero è un altro: verificare l'intervallo dinamico effettivo
sul materiale reale, cioè alimentare la calibrazione automatica del mapping.

**Il ponte fra i due.** Un comando che **congela** gli ultimi N secondi di ingresso live in un
buffer e apre la vista offline su quella cattura. Materiale reale, proveniente dalla catena
d'ingresso effettiva, ma su un substrato fermo su cui si può tarare ed esportare. È poco codice e
copre il caso in cui il file non esiste ancora perché la sorgente è dal vivo.

---

## 8. Cosa non costruire

- **Nessun cruscotto multi-feature.** Una feature alla volta.
- **Nessuna interazione oltre selezione, zoom e scrub.** Niente annotazioni, misurazioni,
  strumenti di editing.
- **Nessuna rappresentazione tridimensionale.**
- **Nessun percorso di calcolo alternativo** per il disegno (§5.4).
- **Nessun confronto fra impostazioni** in questa versione (§9).

---

## 9. Rimandato: confronto fra impostazioni

Mostrare più valori di un parametro contemporaneamente — traccia fantasma dell'impostazione
precedente, o ventaglio di tre-cinque valori sullo stesso segnale — è il modo più diretto di
capire che cosa fa un parametro, ma ha due costi: complica l'implementazione, e complica
l'interpretazione per chi non ha dimestichezza con il dominio, che si trova davanti più curve
senza sapere quale guardare.

Resta annotato come sviluppo futuro. Se verrà ripreso, la forma più contenuta è la **traccia
fantasma singola** con attivazione esplicita: costa poco e non affolla il grafico più di una
curva in più.

Nel frattempo la funzione di taratura poggia su ricalcolo immediato (§6) e parametro visibile
sul grafico (§4).

---

## 10. Riepilogo: che cosa serve al motore

| Serve | Stato |
|---|---|
| Scegliere il substrato dall'unità di uscita | **già disponibile**: descrittore di canale |
| Ricostruire solo il ramo modificato | **già disponibile**: firme dei nodi e deduplicazione |
| Le regioni scartate dagli aggregatori | **già disponibile**: maschera di validità |
| I nomi dei canali per gli assi categoriali | **già disponibile**: descrittore di canale |
| Registrare l'uscita di un nodo su una regione | **da aggiungere**: la presa (§2.4), piccola |
| Nodi di substrato di sola visualizzazione | **da aggiungere**: piccola, ma con la regola di non contaminare il setup (§2.3) |
| Canali diagnostici, attivabili su richiesta | **da aggiungere**: un flag sul canale, più il calcolo condizionale nel nodo (§3.4) |
| Su quale asse si legge un parametro | **da aggiungere**: un campo nel descrittore di parametro (§4) |
| Le sei geometrie e i disegnatori | **da aggiungere**: il grosso del lavoro |
| Disegnatore di campo 2D con asse continuo o categoriale | **da aggiungere**: uno solo, serve sia lo spettrogramma sia il chromagram (§3.3) |
| Secondo percorso di resa per l'esportazione | **da aggiungere**: stessi dati, disegnatore diverso (§5) |

Le aggiunte al motore restano contenute e nessuna è invasiva. Il lavoro vero è nella resa: il
vocabolario delle geometrie, il disegnatore di campo 2D, e il percorso di esportazione.
