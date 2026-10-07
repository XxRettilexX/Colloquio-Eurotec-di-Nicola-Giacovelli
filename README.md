# Colloquio — i due programmi

Due esercizi dal documento `2_set 26_Utility.docx`:

1. **Sentinel Utility** (`sentinel_utility/`) — utility Windows in C++ che sposta o stampa file. Dettagli tecnici e spiegazione del codice: [sentinel_utility/README.md](sentinel_utility/README.md).
2. **Scambio token tra form** (`token_demo/`) — quattro pagine HTML con JavaScript, senza server né database. Si apre `token_demo/index.html` nel browser.

Qui sotto c'è il discorso da fare al colloquio: prima la versione breve (1 minuto), poi quella completa, poi le domande probabili.

---

## Discorso breve (1 minuto)

> Mi avete chiesto due esercizi. Il primo è un'utility Windows che controlla la cartella Downloads ogni pochi secondi: quando trova un file scaricato lo sposta in un'altra cartella oppure lo manda a una stampante di rete. Il problema vero è che il browser mostra il file prima di aver finito di scriverlo, quindi uso un secondo file, la *sentinella*, che arriva dopo: se c'è la sentinella, il file è completo. L'ho scritta in C++ nativo, un solo `.exe` senza installazione, con icona vicino all'orologio, lista degli ultimi file e tasto "Ripeti".
>
> Il secondo è uno scambio di token tra pagine HTML senza database. La prima pagina genera un token casuale, offuscato e con scadenza; passa per una seconda pagina e torna alla prima, che confronta il token ricevuto con quello salvato nella sessione del browser e risponde OK, oppure FAIL se diverso, vuoto o scaduto. A fine procedura cancella tutto.

---

## Discorso completo

### Programma 1 — Sentinel Utility

**Il problema.** Un sito web fa scaricare un file (uno scontrino, un XML) nella cartella Downloads. Un altro software — la stampante di cassa, un gestionale — deve usarlo. Qualcuno deve quindi fare il "postino": guardare la cartella e portare il file nel posto giusto. Nel documento c'erano già due script `.bat` che lo facevano; io li ho presi come specifica, perché mostrano i controlli e i parametri da rispettare.

**Il punto delicato: la sentinella.** Quando il browser scarica, il file *esiste* ma è ancora in scrittura. Se lo copio subito, lo copio incompleto o trovo che è bloccato. Il sito allora scarica due file: prima quello vero e *dopo* un file piccolo, la sentinella. Il mio programma agisce solo se esistono **entrambi**: la sentinella è la prova che il primo file è finito. È un'idea semplice ma robusta, perché non devo indovinare tempi di attesa.

**Cosa fa, in ordine.** Ogni secondo, per ogni regola configurata:
1. controlla se esistono file e sentinella;
2. fa una **copia di sicurezza** nella cartella `archivio\`;
3. esegue l'azione: **sposta** (copia nella destinazione, eventualmente rinominando) oppure **stampa** (scrive i byte sulla stampante di rete `\\computer\stampante`);
4. cancella il file originale solo se è andato tutto bene, la sentinella sempre;
5. aggiunge una riga alla lista: data e ora, nome file, esito.

**Perché C++ nativo.** Il requisito era "standalone, senza installazione, su Windows". Un `.exe` C++ compilato con MinGW usa solo le librerie che Windows ha già: niente runtime da installare, niente DLL da portarsi dietro. Si copia e si avvia. Inoltre consuma pochissima memoria, che conta per un programma che resta acceso tutto il giorno.

**L'interfaccia.** La finestra parte nascosta e il programma vive come **icona vicino all'orologio** (tray). Clic sull'icona: si apre la finestra con la lista degli ultimi file elaborati. La X nasconde soltanto; per chiudere davvero c'è "Esci" nel menu dell'icona. Due tasti, come richiesto:
- **Impostazioni**: apre `sentinel.ini` nel Blocco Note; le modifiche valgono entro 3 secondi, senza riavviare;
- **Ripeti selezionato**: rifà l'azione di una riga, anche fallita, ripartendo dalla copia in `archivio\` (per questo la copia di sicurezza è la prima cosa che faccio).

**Le impostazioni.** Un file `.ini` con un blocco `[nome]` per ogni regola: modo, cartella, file, sentinella, secondi, destinazione o stampante. Le due regole di esempio sono i due `.bat` tradotti. Aggiungere un terzo flusso significa aggiungere un blocco, **senza toccare il codice**. È la parte che rende l'utility riusabile.

**Come è fatto il codice.** Un unico file, `sentinel.cpp`, ~400 righe, leggibile dall'alto in basso. Un programma Windows non esegue "una riga dopo l'altra": entra in un **ciclo di messaggi** e aspetta eventi (clic, timer). Io chiedo a Windows un timer da 1 secondo; a ogni scatto la funzione `ognisecondo()` guarda quali regole sono "di turno" (con il resto della divisione: `secondiPassati % r.secondi == 0`) e chiama `controllaRegola()`, che contiene tutta la logica dell'esercizio in una dozzina di righe. Il resto è contorno: finestra, icona, `.ini`.

**Scelte e dettagli di cura.**
- *Una sola istanza* alla volta, con un mutex: se lo avvii due volte, la seconda si chiude (altrimenti due copie si contenderebbero i file).
- *Autotest*: `sentinel.exe --test` crea cartelle temporanee e verifica 5 casi (niente file; file senza sentinella; file + sentinella; ripeti; destinazione sbagliata). Con `--quiet` risponde solo col codice di uscita.
- *Errori*: se l'azione fallisce, il file originale **resta** dov'è e nella lista compare `errore N` (il numero è il codice Windows, es. 3 = percorso non trovato, 5 = accesso negato).

**Limiti che dichiaro io, prima che me li chiedano.**
- Lo storico è solo in memoria: chiudendo il programma si svuota (le copie in `archivio\` restano).
- Non mappo la porta `LPT2` come il `.bat`: scrivo direttamente sulla condivisione di rete, che è più semplice e non lascia mappature persistenti.
- Se un'azione fallisce, la sentinella viene comunque cancellata per non ritentare all'infinito: si ripete a mano col tasto.
- I percorsi con lettere accentate non sono supportati (testo ANSI).
- **Non l'ho provato su una stampante di rete reale**: la parte "sposta" è coperta dall'autotest, la parte "stampa" no.

---

### Programma 2 — Scambio token tra `<form>`

**Il problema.** Quattro pagine: una home con un link; una pagina con un form che contiene un `<input type="hidden">` con un token; una seconda pagina che lo riceve e lo ripassa in un altro form con due tasti (uno va a una terza pagina, l'altro torna indietro); la pagina iniziale deve verificare che il token tornato sia quello inviato. Vincoli: **niente database**; sessione o cookie ammessi, ma a fine procedura non deve restare traccia.

**Il flusso.**
```
index.html ──link──▶ form.html ──(t)──▶ step2.html ──Indietro (t)──▶ form.html  → OK / FAIL
                     genera token        rilancia t    └─Avanti──▶ step3.html (pulizia)
```

**Generazione del token (`form.html`).** Ogni volta che la pagina si apre *senza* token nell'indirizzo, ne genera uno nuovo:
- **Univoco**: 16 byte casuali presi da `crypto.getRandomValues` (il generatore crittografico del browser, non `Math.random`), più il timestamp. Ogni apertura produce un token diverso.
- **Poco leggibile**: il contenuto (`timestamp.casuale`) viene mescolato con uno **XOR** con una chiave casuale di 1 byte, che viaggia in testa al token, e poi codificato in **base64url**. Non è crittografia vera — con la chiave in chiaro chiunque può decodificarlo — ma risponde alla richiesta: non si legge a colpo d'occhio e non contiene parole comuni. Il testo del documento dice proprio "anche semplice o banale".
- **Con scadenza**: il timestamp di creazione viene salvato con il token; la validità è 60 secondi (`TTL = 60000`, una sola costante da cambiare).

**Dove si conserva.** Il token originale sta in `sessionStorage` (un'area che il browser svuota alla chiusura della scheda). Non uso database né cookie, e non serve alcun server: l'unica persistenza è quella sessione.

**Il viaggio.** Il token è nell'`input hidden` e viaggia nell'indirizzo (`?t=...`) con `method="get"`. `step2.html` lo legge dall'indirizzo e lo rimette in un nuovo form hidden con due tasti: `formaction` diverso per ciascuno (`step3.html` per "Avanti", `form.html` per "Indietro"). Un solo form, due destinazioni.

**La verifica.** Quando `form.html` si riapre *con* `?t=` nell'indirizzo, capisce che è un ritorno e confronta:
- token mancante, vuoto o diverso da quello in sessione → `FAIL: token diverso o vuoto`;
- uguale ma più vecchio di 60 s → `FAIL: token scaduto`;
- uguale e in tempo → `OK`.

**Nessuna traccia.** Subito dopo la lettura la pagina cancella il token dalla sessione (`removeItem`) e ripulisce l'indirizzo con `history.replaceState`, così il token non resta nemmeno nella barra o nella cronologia. Anche `step3.html` ripulisce, nel caso si esca dal percorso con "Avanti". Quindi il token vale **una sola volta**: ricaricare la pagina di ritorno non lo fa passare di nuovo.

**Limiti che dichiaro io.**
- Il token viaggia nell'indirizzo (GET): scelta per semplicità e per poter usare un form nativo; con POST servirebbe un server per leggerlo.
- La scadenza è controllata dal browser stesso, quindi è una dimostrazione di logica, non una sicurezza vera: per un caso reale token e scadenza andrebbero verificati lato server.
- XOR a un byte è offuscamento, non cifratura.

---

## Domande probabili e risposte pronte

**Perché C++ e non Python/C#/.NET?** Per rispettare "standalone, senza installazione": un `.exe` nativo parte ovunque su Windows senza runtime. Python o .NET avrebbero richiesto di installarli o di impacchettare tutto in un file molto più grande.

**Perché non usare un watcher di eventi (`ReadDirectoryChangesW`) invece di controllare ogni secondo?** Il documento chiede esplicitamente di controllare "ogni pochi secondi", e il polling è più semplice e prevedibile; con una sola cartella e due `IF EXIST` il costo è irrilevante. Con molte cartelle o file grossi passerei agli eventi.

**E se il sito scarica la sentinella ma il file è ancora bloccato?** La sentinella arriva per costruzione dopo il file. Se comunque la copia fallisse, l'errore finisce in lista, il file originale resta e si può usare Ripeti.

**Perché la copia di sicurezza?** Perché l'originale viene cancellato dopo l'azione: con la copia posso ripetere un'operazione fallita (stampante spenta, cartella assente) anche dopo che l'originale non c'è più.

**Come aggiungo un nuovo flusso?** Un nuovo blocco `[nome]` nel `.ini`. Nessuna ricompilazione.

**Come proveresti la parte stampa?** Condividendo una stampante "Generic / Text Only" su un PC e puntando `computer` e `stampante` lì; oggi non l'ho potuto provare su hardware reale e lo dico apertamente.

**Il token è sicuro?** No, non nel senso forte: è offuscato e con scadenza, come chiesto. Per un caso reale: token firmato (HMAC) o cifrato con chiave segreta, generato e verificato dal server, in cookie `HttpOnly`.

**Cosa miglioreresti?** Programma 1: storico su file, log, notifica a video in caso di errore, avvio automatico con Windows, test sulla stampante reale. Programma 2: verifica lato server, POST al posto di GET, scadenza mostrata all'utente.
