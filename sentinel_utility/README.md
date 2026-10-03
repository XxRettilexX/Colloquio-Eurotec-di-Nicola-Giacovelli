# Sentinel Utility — esercizio 1

## Che problema risolve

Un sito web fa scaricare dei file (uno scontrino, un XML) nella cartella **Downloads**. Un altro programma (la stampante di cassa, un gestionale) deve poi usarli. Serve quindi un programma che resti acceso e:

1. controlla la cartella ogni pochi secondi;
2. quando arriva il file, lo **sposta** in un'altra cartella oppure lo **stampa** su una stampante di rete;
3. mostra cosa ha fatto e permette di rifarlo se è andato male.

Nel documento c'erano già due script `.bat` che fanno questo lavoro. Qui sono riscritti in C++ come programma Windows con un'icona vicino all'orologio.

## Il trucco della "sentinella"

Quando il browser scarica un file, il file **compare subito** nella cartella ma è ancora in scrittura: se lo si legge in quel momento è incompleto o bloccato.

Soluzione: il sito scarica **due file**, prima quello vero e *dopo* un secondo file piccolo, la **sentinella**. Se la sentinella esiste, vuol dire che il primo file è completo e si può usarlo.

```
Downloads\scontr00.001   <- il file vero (può essere ancora in scrittura)
Downloads\scontr00.on    <- la sentinella (arriva dopo: ora il file è pronto)
```

Il programma agisce solo quando esistono **entrambi**. Alla fine cancella il file (se tutto è andato bene) e la sentinella, così non rielabora lo stesso file.

## Cosa fa il programma, passo per passo

Ogni secondo, per ogni regola:

1. controlla se esistono il file e la sentinella nella cartella indicata;
2. se sì, fa una **copia di sicurezza** in `archivio\` accanto all'exe;
3. esegue l'azione della regola:
   - **sposta**: copia il file nella cartella di destinazione, eventualmente con un altro nome;
   - **stampa**: scrive i byte del file direttamente sulla stampante di rete `\\computer\stampante`;
4. se è andata bene cancella il file originale; cancella sempre la sentinella;
5. aggiunge una riga alla lista: data/ora, nome file, esito (`OK` oppure `errore NNN`).

## Le due regole di esempio (corrispondono ai due `.bat`)

| | `scontrino` | `magazzino` |
|---|---|---|
| azione | stampa | sposta |
| file | `scontr00.001` | `scontr00mag.Xml` |
| sentinella | `scontr00.on` | `scontr00mag.ok` |
| controllo ogni | 3 secondi | 1 secondo |
| destinazione | stampante `CassaCustom` sul PC `Eurotec-Master` | `C:\SwInstallato\Olivetti\ElaExecute\EE_IN\`, rinominato `scontrino.Xml` |

## Come si usa

- **Avvio:** doppio clic su `sentinel.exe`. Non c'è installazione e non serve altro. La finestra parte nascosta: cerca l'icona vicino all'orologio (se non la vedi, clicca la freccia "icone nascoste").
- **Clic sull'icona:** apre la finestra con la lista dei file elaborati. La X della finestra la nasconde soltanto. Clic destro sull'icona → **Esci** per chiudere davvero.
- **Impostazioni:** apre `sentinel.ini` nel Blocco Note. Salva e chiudi: le modifiche valgono entro 3 secondi, senza riavviare.
- **Ripeti selezionato:** seleziona una riga (anche una fallita) e premi il tasto. Il programma riesegue l'azione usando la copia in `archivio\`.

## Il file `sentinel.ini`

Viene creato al primo avvio accanto all'exe. Ogni regola è un blocco `[nome]`. Per aggiungere una regola basta aggiungere un blocco.

```ini
[magazzino]
modo=sposta                           ; sposta oppure stampa
cartella=%UserProfile%\Downloads\     ; cartella controllata
file=scontr00mag.Xml                  ; file da cercare
sentinella=scontr00mag.ok             ; file che dice "il primo è pronto"
secondi=1                             ; ogni quanti secondi controllare
destinazione=C:\SwInstallato\Olivetti\ElaExecute\EE_IN\   ; solo "sposta": dove metterlo
nuovoNome=scontrino.Xml               ; solo "sposta": nuovo nome (facoltativo)

[scontrino]
modo=stampa
cartella=%UserProfile%\Downloads\
file=scontr00.001
sentinella=scontr00.on
secondi=3
computer=Eurotec-Master               ; solo "stampa": nome del PC che ha la stampante
stampante=CassaCustom                 ; solo "stampa": nome della condivisione
```

`%UserProfile%` viene sostituito col tuo profilo utente (`C:\Users\nome`).

## Come provarlo senza stampante

1. Nel `.ini` imposta `dove` a una cartella di prova e `a` a un'altra cartella che esiste.
2. Crea prima il file (`scontr00mag.Xml`), poi la sentinella (`scontr00mag.ok`), in quest'ordine.
3. Entro un secondo il file deve comparire nella destinazione con il nuovo nome e nella lista deve apparire `OK`.
4. Prova l'errore: metti una destinazione che non esiste. Nella lista compare `errore NNN`, il file originale resta dov'è e puoi usare **Ripeti** dopo aver corretto la destinazione.

Test automatico: `sentinel.exe --test` (con `--quiet` risponde solo col codice di uscita 0 = ok).

## Compilare

Serve MinGW-w64 (g++). Da questa cartella: `build.bat`. Produce `sentinel.exe` standalone. Se `sentinel.exe` è già in esecuzione, chiudilo prima (Esci dall'icona), altrimenti Windows non permette di sovrascriverlo.

## Il codice spiegato (`sentinel.cpp`)

Il file è un unico `.cpp`, letto dall'alto in basso. Le funzioni sono scritte **prima** di chi le usa, quindi si può leggere in ordine.

### Concetti C++ usati (pochi e di base)

| Cosa | Significato |
|---|---|
| `#include <...>` | "porta dentro" un insieme di funzioni già pronte (`<string>` per i testi, `<vector>` per le liste, `<windows.h>` per Windows) |
| `string` | un testo. Si possono unire con `+`: `cartella + file` |
| `vector<Regola>` | una lista che cresce: `push_back(x)` aggiunge in fondo, `insert(begin(), x)` in cima, `.size()` dice quanti elementi ci sono |
| `struct` | un "contenitore" che raggruppa più dati. `Regola` raggruppa cartella, file, sentinella... così si passano insieme |
| `const Regola& r` | passo la regola **senza copiarla** (`&`) e prometto di non modificarla (`const`) |
| `for (const Regola& r : g_regole)` | "per ogni regola nella lista" |
| `g_qualcosa` | convenzione: variabile **globale**, visibile da tutte le funzioni |
| `""` e `.empty()` | testo vuoto e controllo se è vuoto |
| `NULL` | "niente / non c'è" |

### Concetti di Windows usati

| Cosa | Significato |
|---|---|
| `HANDLE`, `HWND` | un "numero maniglia" che Windows ti dà per indicare un file aperto (`HANDLE`) o una finestra (`HWND`). Lo si passa alle funzioni e alla fine si chiude con `CloseHandle` |
| `GetLastError()` | dopo una funzione fallita, dà il numero dell'errore (es. 3 = percorso non trovato, 5 = accesso negato). Compare nella lista come `errore 3` |
| `WinMain` | il "main" dei programmi con finestre |
| **Messaggi** | un programma Windows non esegue una riga dopo l'altra e finisce: **aspetta eventi** (clic, timer, ridimensionamento). `gestisciEventi` viene chiamata da Windows per ognuno |
| `SetTimer` | chiede a Windows di mandare l'evento `WM_TIMER` ogni 1000 ms: è il nostro "ogni secondo" |
| Tray icon | `Shell_NotifyIcon` mette l'icona vicino all'orologio; quando la clicchi Windows manda a `gestisciEventi` il nostro messaggio `MSG_ICONA` |

### Funzione per funzione

**Dati (in alto)**
- `Regola`: cosa controllare e cosa farne (una per ogni blocco del `.ini`).
- `Riga`: un file elaborato, con data, esito, e *quale regola e quale copia di sicurezza* servono per ripeterlo.
- `g_regole`, `g_storico`: le due liste in memoria.

**Funzioni di aiuto**
- `cartellaExe()`: dove si trova `sentinel.exe`, per salvare lì `.ini` e archivio.
- `espandi()`: trasforma `%UserProfile%` nel percorso vero.
- `conBackslash()`: aggiunge `\` in fondo ai percorsi se manca, così `cartella + file` funziona sempre.
- `esiste()`: dice se un file/cartella esiste.
- `adesso()`: data e ora come testo.

**Impostazioni (`.ini`)**
- `leggi()` / `scrivi()`: leggono e scrivono un valore nel file `.ini` (funzioni già fatte da Windows).
- `creaIniDiEsempio()`: al primo avvio scrive le due regole di partenza.
- `caricaRegole()`: legge tutti i blocchi `[nome]` del `.ini` e riempie `g_regole`. Salta le regole senza `file` o `sentinella`.

**Azioni**
- `scriviSuStampante()`: apre il file e la stampante e passa i dati a blocchi da 4096 byte, come fa `COPY file \pc\stampante`. Ritorna 0 se tutto bene, altrimenti il numero dell'errore.
- `eseguiAzione()`: guarda `modo` e fa "stampa" oppure "sposta" (`CopyFile`, con il nuovo nome se c'è). Ritorna testo vuoto se ok, altrimenti `errore N`.
- `aggiungiRiga()`: registra il risultato nello storico e in cima alla lista a video.
- **`controllaRegola()`: il cuore.** Se esistono sentinella **e** file → copia di sicurezza → `eseguiAzione` → cancella il file (solo se ok) e la sentinella → `aggiungiRiga`. Se manca uno dei due, esce senza fare nulla.
- `ripeti()`: rifà l'azione di una riga della lista, partendo dalla copia in `archivio\`.

**Finestra**
- `creaContenuto()`: crea la lista a colonne, i due tasti e avvia il timer da 1 secondo.
- `ridimensiona()`: posiziona lista e tasti quando cambia la dimensione della finestra.
- `ognisecondo()`: chiamata dal timer. Conta i secondi; ogni 3 rilegge il `.ini`; per ogni regola, se è il suo turno (`secondiPassati % r.secondi == 0`) chiama `controllaRegola`. Il `%` è il resto della divisione: con `secondi=3` scatta a 3, 6, 9...
- `menuIcona()`: il menu "Apri / Esci" col tasto destro sull'icona.
- `gestisciEventi()`: riceve ogni evento e decide cosa fare: tasto premuto, clic sull'icona, timer, X della finestra (nasconde e basta, il programma resta attivo)...

**Prova e avvio**
- `provaAutomatica()`: crea cartelle temporanee e verifica 5 casi: niente file; file senza sentinella; file + sentinella; ripeti; destinazione sbagliata. Si lancia con `sentinel.exe --test`.
- `WinMain()`: esce se c'è già un'altra copia, crea il `.ini` se manca, carica le regole, crea la finestra (nascosta), mette l'icona e poi entra nel **ciclo dei messaggi**, dove resta finché non si preme Esci.

### Se vuoi capire partendo da una sola funzione
Leggi `controllaRegola()` (circa 12 righe): contiene tutta la logica dell'esercizio. Il resto del file è "contorno" per avere finestra, icona e impostazioni.

## Limiti noti

- La lista dello storico è solo in memoria: chiudendo il programma si svuota (le copie in `archivio\` restano).
- Non esiste la mappatura di una porta `LPT2` come nel `.bat`: si scrive direttamente sulla condivisione di rete della stampante.
- Se un'azione fallisce, la sentinella viene comunque cancellata (per non ritentare all'infinito); il file resta e si ripete a mano con il tasto.
- Il file `.ini` e il programma usano testo ANSI: evita lettere accentate nei percorsi.
- Non provato su una stampante di rete reale.
