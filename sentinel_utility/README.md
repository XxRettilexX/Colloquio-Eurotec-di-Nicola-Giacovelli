# Sentinel Utility

Utility Windows per controllare file scaricati e avviarne lo spostamento o la stampa RAW. Richiede Python 3.11+ per lo sviluppo. La stampa usa `net use` solo quando è configurata una porta LPT; in alternativa scrive direttamente su un percorso UNC.

## Assunzioni
- Ogni regola abbina un singolo nome file e una sentinella esatti.
- L'archivio conserva una copia dopo l'operazione riuscita, prima della cancellazione dell'origine.
- Un mapping porta LPT è per utente Windows e viene rimosso alla chiusura del programma.
- Il retry si applica all'elemento presente; dopo il limite viene rimossa solo la sentinella.
- La finestra parte nascosta; l'icona tray richiede un desktop interattivo.

## Sviluppo
`py -3.11 -m venv .venv`
`.venv\Scripts\activate`
`pip install -r requirements.txt`
`python main.py`

## Build
Eseguire `build.bat` da Windows con Python 3.11. Produce `dist\SentinelUtility.exe` in modalità onefile e noconsole. Configurazione, storico e log vengono salvati accanto all'eseguibile (in sviluppo accanto a main.py).

## Configurazione
Le regole sono modificabili dalla finestra Impostazioni. MOVE richiede destinazione; PRINT richiede UNC stampante, e facoltativamente una porta LPT per creare il mapping. Il timeout stampa e i tentativi sono configurabili globalmente nel JSON. Una cartella archivio è facoltativa.

## Prova senza stampante
Crea la cartella sorgente configurata, poi crea prima il file indicato e infine la sentinella. Per provare PRINT senza hardware impostare destinazione UNC a una condivisione di test scrivibile. MOVE è verificabile con una cartella temporanea.

## Problemi comuni
SmartScreen o l'antivirus possono segnalare un exe PyInstaller non firmato; compilare da una macchina fidata e distribuire tramite canale aziendale approvato. Per stampare, l'utente deve avere accesso alla condivisione e la stampante deve accettare dati RAW. Verificare che la porta LPT scelta non sia già mappata.

## Verifiche manuali Windows
Verificare avvio tray e singola istanza, chiusura in tray, avvio automatico, permessi su cartelle, comportamento durante download in corso, mapping LPT, output binario e stampa effettiva sulla stampante condivisa.
