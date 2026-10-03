// Sentinel Utility - versione semplice e commentata.
// Controlla cartelle: se trova file + sentinella, sposta il file o lo manda a una stampante di rete.
// Prova automatica: sentinel.exe --test
#include <windows.h>    // finestre, file, timer di Windows
#include <commctrl.h>   // la lista con le colonne (ListView)
#include <shellapi.h>   // icona vicino all'orologio, apertura del Blocco Note
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
using namespace std;

// ---------- Dati ----------

// Una regola = "cosa controllare e cosa farne". Una per ogni blocco [nome] del file .ini.
struct Regola {
    string nome, cartella, file, sentinella;
    string modo;                        // "sposta" oppure "stampa"
    string destinazione, nuovoNome;     // solo per "sposta"
    string computer, stampante;         // solo per "stampa"
    int secondi;                        // ogni quanti secondi controllare
};

// Una riga della lista a video = un file elaborato.
struct Riga {
    string quando, file, esito;
    string copia;       // dove ho salvato la copia di sicurezza (serve per "Ripeti")
    Regola regola;      // la regola usata (serve per "Ripeti")
};

string g_ini;                   // percorso di sentinel.ini
string g_archivio;              // cartella con le copie di sicurezza
vector<Regola> g_regole;        // le regole lette dal .ini
vector<Riga> g_storico;         // righe elaborate: la posizione 0 e' la piu' recente
HWND g_lista = NULL;            // la lista nella finestra (NULL = nessuna finestra, usato dal test)
NOTIFYICONDATA g_icona;         // l'icona vicino all'orologio

// ---------- Funzioni di aiuto ----------

string cartellaExe() {
    char percorso[MAX_PATH];
    GetModuleFileName(NULL, percorso, MAX_PATH);    // percorso completo di sentinel.exe
    string s = percorso;
    return s.substr(0, s.rfind('\\'));              // tolgo il nome del file, tengo la cartella
}

string espandi(string s) {              // "%UserProfile%\Downloads" -> "C:\Users\nome\Downloads"
    char buffer[1024];
    ExpandEnvironmentStrings(s.c_str(), buffer, 1024);
    return buffer;
}

string conBackslash(string p) {         // mi assicuro che il percorso finisca con '\'
    if (!p.empty() && p.back() != '\\') p += '\\';
    return p;
}

bool esiste(string percorso) {
    return GetFileAttributes(percorso.c_str()) != INVALID_FILE_ATTRIBUTES;
}

string adesso(const char* formato) {    // data/ora corrente, es. formato "%d/%m/%Y %H:%M:%S"
    time_t t = time(NULL);
    char buffer[64];
    strftime(buffer, 64, formato, localtime(&t));
    return buffer;
}

// ---------- File di impostazioni (.ini) ----------

string leggi(string sezione, string chiave) {
    char buffer[1024];
    GetPrivateProfileString(sezione.c_str(), chiave.c_str(), "", buffer, 1024, g_ini.c_str());
    return buffer;
}

void scrivi(const char* sezione, const char* chiave, const char* valore) {
    WritePrivateProfileString(sezione, chiave, valore, g_ini.c_str());
}

void creaIniDiEsempio() {
    scrivi("scontrino", "modo", "stampa");
    scrivi("scontrino", "cartella", "%UserProfile%\\Downloads\\");
    scrivi("scontrino", "file", "scontr00.001");
    scrivi("scontrino", "sentinella", "scontr00.on");
    scrivi("scontrino", "secondi", "3");
    scrivi("scontrino", "computer", "Eurotec-Master");
    scrivi("scontrino", "stampante", "CassaCustom");

    scrivi("magazzino", "modo", "sposta");
    scrivi("magazzino", "cartella", "%UserProfile%\\Downloads\\");
    scrivi("magazzino", "file", "scontr00mag.Xml");
    scrivi("magazzino", "sentinella", "scontr00mag.ok");
    scrivi("magazzino", "secondi", "1");
    scrivi("magazzino", "destinazione", "C:\\SwInstallato\\Olivetti\\ElaExecute\\EE_IN\\");
    scrivi("magazzino", "nuovoNome", "scontrino.Xml");
}

void caricaRegole() {
    g_regole.clear();
    // Ottengo l'elenco dei blocchi [..] del file: nomi separati da '\0', finiti da un '\0' doppio.
    char nomi[4096];
    GetPrivateProfileSectionNames(nomi, 4096, g_ini.c_str());
    for (char* p = nomi; *p != '\0'; p += strlen(p) + 1) {
        Regola r;
        r.nome = p;
        r.cartella = conBackslash(espandi(leggi(p, "cartella")));
        r.file = leggi(p, "file");
        r.sentinella = leggi(p, "sentinella");
        r.modo = leggi(p, "modo");
        r.destinazione = conBackslash(espandi(leggi(p, "destinazione")));
        r.nuovoNome = leggi(p, "nuovoNome");
        r.computer = leggi(p, "computer");
        r.stampante = leggi(p, "stampante");
        r.secondi = GetPrivateProfileInt(p, "secondi", 3, g_ini.c_str());
        if (r.secondi < 1) r.secondi = 1;
        if (r.file == "" || r.sentinella == "") continue;   // regola incompleta: la salto
        g_regole.push_back(r);
    }
}

// ---------- Le azioni ----------

// Scrive i byte di un file sulla stampante di rete. Ritorna 0 se ok, altrimenti il codice errore di Windows.
DWORD scriviSuStampante(string origine, string stampante) {
    HANDLE in = CreateFile(origine.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (in == INVALID_HANDLE_VALUE) return GetLastError();
    HANDLE out = CreateFile(stampante.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (out == INVALID_HANDLE_VALUE) {
        DWORD errore = GetLastError();
        CloseHandle(in);
        return errore;
    }
    char blocco[4096];
    DWORD letti, scritti, errore = 0;
    while (ReadFile(in, blocco, sizeof(blocco), &letti, NULL) && letti > 0) {   // leggo a blocchi...
        if (!WriteFile(out, blocco, letti, &scritti, NULL)) {                   // ...e li scrivo
            errore = GetLastError();
            break;
        }
    }
    CloseHandle(in);
    CloseHandle(out);
    return errore;
}

// Esegue l'azione della regola sul file "origine". Ritorna "" se ok, altrimenti un messaggio.
string eseguiAzione(const Regola& r, string origine) {
    DWORD errore = 0;
    if (r.modo == "stampa") {
        errore = scriviSuStampante(origine, "\\\\" + r.computer + "\\" + r.stampante);
    } else {
        string nome = r.nuovoNome.empty() ? r.file : r.nuovoNome;
        if (!CopyFile(origine.c_str(), (r.destinazione + nome).c_str(), FALSE)) errore = GetLastError();
    }
    if (errore == 0) return "";
    return "errore " + to_string(errore);
}

// Aggiunge una riga in cima allo storico e alla lista a video.
void aggiungiRiga(const Regola& r, string errore, string copia) {
    Riga riga;
    riga.quando = adesso("%d/%m/%Y %H:%M:%S");
    riga.file = r.file;
    riga.esito = errore.empty() ? "OK" : errore;
    riga.copia = copia;
    riga.regola = r;
    g_storico.insert(g_storico.begin(), riga);

    if (g_lista == NULL) return;    // nessuna finestra (test): niente da disegnare
    LVITEM voce = {};
    voce.mask = LVIF_TEXT;
    voce.iItem = 0;                 // inserisco in cima
    voce.pszText = (char*)riga.quando.c_str();
    ListView_InsertItem(g_lista, &voce);
    ListView_SetItemText(g_lista, 0, 1, (char*)riga.file.c_str());
    ListView_SetItemText(g_lista, 0, 2, (char*)riga.esito.c_str());
}

// Il cuore del programma: controlla una regola.
void controllaRegola(const Regola& r) {
    string origine = r.cartella + r.file;
    string sentinella = r.cartella + r.sentinella;
    if (!esiste(sentinella) || !esiste(origine)) return;    // serve TUTTI E DUE: la sentinella dice che il file e' completo

    CreateDirectory(g_archivio.c_str(), NULL);
    string copia = g_archivio + adesso("%Y%m%d_%H%M%S_") + r.file;
    CopyFile(origine.c_str(), copia.c_str(), FALSE);        // copia di sicurezza, serve per "Ripeti"

    string errore = eseguiAzione(r, origine);
    if (errore.empty()) DeleteFile(origine.c_str());        // cancello il file solo se e' andato tutto bene
    DeleteFile(sentinella.c_str());                         // la sentinella la cancello sempre (niente ripetizioni infinite)
    aggiungiRiga(r, errore, copia);
}

// Tasto "Ripeti": rifa' l'azione della riga numero "posizione" usando la copia di sicurezza.
void ripeti(int posizione) {
    if (posizione < 0 || posizione >= (int)g_storico.size()) return;    // nessuna riga selezionata
    Riga vecchia = g_storico[posizione];    // ne faccio una copia: aggiungiRiga modifica lo storico
    string errore;
    if (esiste(vecchia.copia)) errore = eseguiAzione(vecchia.regola, vecchia.copia);
    else errore = "copia mancante";
    aggiungiRiga(vecchia.regola, errore, vecchia.copia);
}

// ---------- Finestra ----------

// Numeri che identificano i tasti, il timer e i comandi del menu dell'icona.
const int ID_IMPOSTAZIONI = 1, ID_RIPETI = 2, ID_APRI = 3, ID_ESCI = 4;
const int ID_TIMER = 1;
const int MSG_ICONA = WM_APP + 1;   // messaggio nostro: Windows lo manda quando si clicca l'icona

void mostraFinestra(HWND finestra) {
    ShowWindow(finestra, SW_SHOW);
    SetForegroundWindow(finestra);
}

void creaContenuto(HWND finestra) {
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    g_lista = CreateWindow(WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                           0, 0, 0, 0, finestra, NULL, NULL, NULL);
    ListView_SetExtendedListViewStyle(g_lista, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    SendMessage(g_lista, WM_SETFONT, (WPARAM)font, TRUE);

    const char* titoli[3] = { "Data e ora", "File", "Esito" };
    int larghezze[3] = { 140, 170, 150 };
    for (int i = 0; i < 3; i++) {
        LVCOLUMN colonna = {};
        colonna.mask = LVCF_TEXT | LVCF_WIDTH;
        colonna.pszText = (char*)titoli[i];
        colonna.cx = larghezze[i];
        ListView_InsertColumn(g_lista, i, &colonna);
    }

    // La posizione dei tasti la decide ridimensiona(), qui li creo soltanto.
    HWND t1 = CreateWindow("BUTTON", "Impostazioni", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, finestra, (HMENU)ID_IMPOSTAZIONI, NULL, NULL);
    HWND t2 = CreateWindow("BUTTON", "Ripeti selezionato", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, finestra, (HMENU)ID_RIPETI, NULL, NULL);
    SendMessage(t1, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(t2, WM_SETFONT, (WPARAM)font, TRUE);

    SetTimer(finestra, ID_TIMER, 1000, NULL);   // Windows ci manda WM_TIMER ogni 1000 ms
}

void ridimensiona(HWND finestra, int larghezza, int altezza) {
    MoveWindow(g_lista, 0, 0, larghezza, altezza - 40, TRUE);
    MoveWindow(GetDlgItem(finestra, ID_IMPOSTAZIONI), 10, altezza - 34, 110, 28, TRUE);
    MoveWindow(GetDlgItem(finestra, ID_RIPETI), 130, altezza - 34, 140, 28, TRUE);
}

void ognisecondo() {
    static int secondiPassati = 0;
    secondiPassati++;
    if (secondiPassati % 3 == 0) caricaRegole();    // rileggo il .ini: le modifiche valgono senza riavviare
    for (const Regola& r : g_regole) {
        if (secondiPassati % r.secondi == 0) controllaRegola(r);
    }
}

void menuIcona(HWND finestra) {
    POINT mouse;
    GetCursorPos(&mouse);
    HMENU menu = CreatePopupMenu();
    AppendMenu(menu, MF_STRING, ID_APRI, "Apri");
    AppendMenu(menu, MF_STRING, ID_ESCI, "Esci");
    SetForegroundWindow(finestra);      // serve perche' il menu si chiuda cliccando fuori
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, mouse.x, mouse.y, 0, finestra, NULL);
    DestroyMenu(menu);
}

// Windows chiama questa funzione per ogni "evento" della finestra (clic, timer, ridimensionamento...).
LRESULT CALLBACK gestisciEventi(HWND finestra, UINT evento, WPARAM w, LPARAM l) {
    if (evento == WM_CREATE) {
        creaContenuto(finestra);
    } else if (evento == WM_SIZE) {
        ridimensiona(finestra, LOWORD(l), HIWORD(l));
    } else if (evento == WM_TIMER) {
        ognisecondo();
    } else if (evento == WM_COMMAND) {              // clic su un tasto o su una voce di menu
        int id = LOWORD(w);
        if (id == ID_IMPOSTAZIONI) ShellExecute(finestra, "open", "notepad.exe", g_ini.c_str(), NULL, SW_SHOW);
        if (id == ID_RIPETI) ripeti(ListView_GetNextItem(g_lista, -1, LVNI_SELECTED));
        if (id == ID_APRI) mostraFinestra(finestra);
        if (id == ID_ESCI) DestroyWindow(finestra);
    } else if (evento == MSG_ICONA) {               // clic sull'icona vicino all'orologio
        if (l == WM_LBUTTONUP) mostraFinestra(finestra);
        if (l == WM_RBUTTONUP) menuIcona(finestra);
    } else if (evento == WM_CLOSE) {                // la X della finestra: la nascondo e basta
        ShowWindow(finestra, SW_HIDE);
    } else if (evento == WM_DESTROY) {              // chiusura vera
        Shell_NotifyIcon(NIM_DELETE, &g_icona);
        PostQuitMessage(0);
    } else {
        return DefWindowProc(finestra, evento, w, l);   // tutto il resto: comportamento standard
    }
    return 0;
}

// ---------- Prova automatica (sentinel.exe --test) ----------

void creaFile(string percorso) {
    HANDLE f = CreateFile(percorso.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(f);
}

int provaAutomatica() {
    char temp[MAX_PATH];
    GetTempPath(MAX_PATH, temp);
    string base = string(temp) + "sentinel_test_" + adesso("%H%M%S") + "\\";
    CreateDirectory(base.c_str(), NULL);
    CreateDirectory((base + "a\\").c_str(), NULL);
    CreateDirectory((base + "b\\").c_str(), NULL);
    g_archivio = base + "arch\\";

    Regola r;
    r.cartella = base + "a\\";
    r.destinazione = base + "b\\";
    r.file = "x.xml";
    r.nuovoNome = "y.xml";
    r.sentinella = "x.ok";
    r.modo = "sposta";

    controllaRegola(r);                                     // 1) niente file: non deve fare nulla
    bool ok = g_storico.empty();

    creaFile(r.cartella + r.file);
    controllaRegola(r);                                     // 2) file senza sentinella: non deve fare nulla
    ok = ok && g_storico.empty() && esiste(r.cartella + r.file);

    creaFile(r.cartella + r.sentinella);
    controllaRegola(r);                                     // 3) file + sentinella: sposta e rinomina
    ok = ok && g_storico.size() == 1 && g_storico[0].esito == "OK"
            && esiste(r.destinazione + "y.xml") && !esiste(r.cartella + r.file) && !esiste(r.cartella + r.sentinella);

    DeleteFile((r.destinazione + "y.xml").c_str());
    ripeti(0);                                              // 4) ripeti dalla copia di sicurezza
    ok = ok && g_storico.size() == 2 && g_storico[0].esito == "OK" && esiste(r.destinazione + "y.xml");

    r.destinazione = base + "inesistente\\";
    creaFile(r.cartella + r.file);
    creaFile(r.cartella + r.sentinella);
    controllaRegola(r);                                     // 5) destinazione sbagliata: errore, il file resta
    ok = ok && g_storico[0].esito != "OK" && esiste(r.cartella + r.file);

    if (strstr(GetCommandLine(), "--quiet") == NULL) {
        MessageBox(NULL, ok ? "self-test OK" : "self-test FALLITO", "Sentinel", MB_OK);
    }
    return ok ? 0 : 1;      // 0 = test superato
}

// ---------- Avvio ----------

// Punto di ingresso di un programma con finestre (al posto di main).
int WINAPI WinMain(HINSTANCE istanza, HINSTANCE, LPSTR, int) {
    if (strstr(GetCommandLine(), "--test") != NULL) return provaAutomatica();

    // Una sola copia alla volta: se il "mutex" con questo nome esiste gia', esco.
    HANDLE mutex = CreateMutex(NULL, TRUE, "SentinelUtility_unica_istanza");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    InitCommonControls();                           // serve per la lista con le colonne
    g_ini = cartellaExe() + "\\sentinel.ini";
    g_archivio = cartellaExe() + "\\archivio\\";
    if (!esiste(g_ini)) creaIniDiEsempio();
    caricaRegole();

    // Descrivo e registro il "tipo" di finestra, poi la creo (resta nascosta: non chiamo ShowWindow).
    WNDCLASS tipo = {};
    tipo.lpfnWndProc = gestisciEventi;              // chi gestisce gli eventi
    tipo.hInstance = istanza;
    tipo.lpszClassName = "SentinelFinestra";
    tipo.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    tipo.hCursor = LoadCursor(NULL, IDC_ARROW);
    tipo.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&tipo);
    HWND finestra = CreateWindow("SentinelFinestra", "Sentinel Utility", WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 500, 360, NULL, NULL, istanza, NULL);

    // Icona vicino all'orologio.
    g_icona.cbSize = sizeof(g_icona);
    g_icona.hWnd = finestra;
    g_icona.uID = 1;
    g_icona.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_icona.uCallbackMessage = MSG_ICONA;
    g_icona.hIcon = tipo.hIcon;
    strcpy(g_icona.szTip, "Sentinel Utility");
    Shell_NotifyIcon(NIM_ADD, &g_icona);

    // Ciclo dei messaggi: il programma resta qui finche' non si preme "Esci".
    MSG messaggio;
    while (GetMessage(&messaggio, NULL, 0, 0) > 0) {
        TranslateMessage(&messaggio);
        DispatchMessage(&messaggio);    // consegna l'evento a gestisciEventi
    }
    CloseHandle(mutex);
    return 0;
}
