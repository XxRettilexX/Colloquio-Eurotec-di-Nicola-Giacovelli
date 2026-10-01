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
dove=%UserProfile%\Downloads\         ; cartella controllata
file=scontr00mag.Xml                  ; file da cercare
sentinella=scontr00mag.ok             ; file che dice "il primo è pronto"
secondi=1                             ; ogni quanti secondi controllare
a=C:\SwInstallato\Olivetti\ElaExecute\EE_IN\   ; solo per "sposta": destinazione
rinomina=scontrino.Xml                ; solo per "sposta": nuovo nome (facoltativo)

[scontrino]
modo=stampa
dove=%UserProfile%\Downloads\
file=scontr00.001
sentinella=scontr00.on
secondi=3
computer=Eurotec-Master               ; solo per "stampa": nome del PC che ha la stampante
stampante=CassaCustom                 ; solo per "stampa": nome della condivisione
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

## Dove sta il codice (`sentinel.cpp`)

| Parte | Cosa fa |
|---|---|
| `loadConfig` | legge le regole da `sentinel.ini` |
| `scanJob` | il cuore: controlla file + sentinella, archivia, esegue, pulisce |
| `doAction` / `rawWrite` | sposta il file o lo scrive sulla stampante |
| `addHist` / `retry` | riga nella lista e tasto "Ripeti" |
| `proc` | finestra, timer di 1 secondo, icona nella tray |
| `selfTest` | prova automatica dei casi principali |

## Limiti noti

- La lista dello storico è solo in memoria: chiudendo il programma si svuota (le copie in `archivio\` restano).
- Non esiste la mappatura di una porta `LPT2` come nel `.bat`: si scrive direttamente sulla condivisione di rete della stampante.
- Se un'azione fallisce, la sentinella viene comunque cancellata (per non ritentare all'infinito); il file resta e si ripete a mano con il tasto.
- Il file `.ini` è letto in formato ANSI: evita lettere accentate nei percorsi.
- Non provato su una stampante di rete reale.
