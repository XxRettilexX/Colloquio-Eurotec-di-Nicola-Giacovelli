// Sentinel Utility - Win32 puro, C++17, nessuna dipendenza.
// Controlla cartelle: se esistono file + sentinella, li sposta (modo=sposta) o li invia
// RAW a una stampante di rete (modo=stampa). Tray icon, elenco storico, ripeti, impostazioni.
// Self-test: sentinel.exe --test
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <cwchar>
#include <ctime>
#include <string>
#include <vector>
using std::wstring;

struct Job { wstring name, dove, file, sentinella, modo, a, rinomina, computer, stampante; int secondi = 3; };
struct Entry { wstring when, name, esito, archive; Job job; bool ok; };

static wstring g_ini, g_archive;
static std::vector<Job> g_jobs;
static std::vector<Entry> g_hist;   // indice 0 = più recente (come la lista); ponytail: solo in memoria, non persistente
static HWND g_list;
static NOTIFYICONDATAW g_nid;

static wstring exeDir() { wchar_t b[MAX_PATH]; GetModuleFileNameW(nullptr, b, MAX_PATH); wstring s = b; return s.substr(0, s.find_last_of(L'\\')); }
static wstring env(const wstring& s) { wchar_t b[2048]; ExpandEnvironmentStringsW(s.c_str(), b, 2048); return b; }
static wstring dir(wstring p) { if (!p.empty() && p.back() != L'\\') p += L'\\'; return p; }
static wstring now(const wchar_t* fmt) { time_t t = time(nullptr); wchar_t b[64]; wcsftime(b, 64, fmt, localtime(&t)); return b; }
static wstring ini(const wchar_t* sec, const wchar_t* key) { wchar_t b[1024]; GetPrivateProfileStringW(sec, key, L"", b, 1024, g_ini.c_str()); return b; }
static bool exists(const wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

static void writeDefaults() {
    auto w = [](const wchar_t* s, const wchar_t* k, const wchar_t* v) { WritePrivateProfileStringW(s, k, v, g_ini.c_str()); };
    w(L"generale", L"info", L"un blocco [nome] per regola; modo=stampa|sposta; modifiche applicate al ciclo successivo");
    w(L"scontrino", L"modo", L"stampa");   w(L"scontrino", L"dove", L"%UserProfile%\\Downloads\\");
    w(L"scontrino", L"file", L"scontr00.001"); w(L"scontrino", L"sentinella", L"scontr00.on");
    w(L"scontrino", L"secondi", L"3");
    w(L"scontrino", L"computer", L"Eurotec-Master"); w(L"scontrino", L"stampante", L"CassaCustom");
    w(L"magazzino", L"modo", L"sposta");   w(L"magazzino", L"dove", L"%UserProfile%\\Downloads\\");
    w(L"magazzino", L"file", L"scontr00mag.Xml"); w(L"magazzino", L"sentinella", L"scontr00mag.ok");
    w(L"magazzino", L"secondi", L"1");
    w(L"magazzino", L"a", L"C:\\SwInstallato\\Olivetti\\ElaExecute\\EE_IN\\"); w(L"magazzino", L"rinomina", L"scontrino.Xml");
}

static void loadConfig() {
    g_jobs.clear();
    wchar_t names[4096];
    GetPrivateProfileSectionNamesW(names, 4096, g_ini.c_str());
    for (wchar_t* p = names; *p; p += wcslen(p) + 1) {
        if (!wcscmp(p, L"generale")) continue;
        Job j; j.name = p;
        j.dove = dir(env(ini(p, L"dove"))); j.file = ini(p, L"file"); j.sentinella = ini(p, L"sentinella");
        j.modo = ini(p, L"modo"); j.a = dir(env(ini(p, L"a"))); j.rinomina = ini(p, L"rinomina");
        j.computer = ini(p, L"computer"); j.stampante = ini(p, L"stampante");
        j.secondi = GetPrivateProfileIntW(p, L"secondi", 3, g_ini.c_str()); if (j.secondi < 1) j.secondi = 1;
        if (j.file.empty() || j.sentinella.empty()) continue;   // regola incompleta: ignorata
        g_jobs.push_back(j);
    }
}

// Copia RAW su un percorso già esistente (condivisione stampante: come COPY file \\pc\stampante).
static bool rawWrite(const wstring& src, const wstring& dst) {
    HANDLE in = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (in == INVALID_HANDLE_VALUE) return false;
    HANDLE out = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (out == INVALID_HANDLE_VALUE) { CloseHandle(in); return false; }
    char buf[65536]; DWORD n, w; bool ok = true;
    while (ok && ReadFile(in, buf, sizeof buf, &n, nullptr) && n) ok = WriteFile(out, buf, n, &w, nullptr) && w == n;
    CloseHandle(in); CloseHandle(out);
    return ok;
}

// Esegue l'azione della regola su src. Ritorna "" se ok, altrimenti il motivo.
static wstring doAction(const Job& j, const wstring& src) {
    bool ok;
    if (j.modo == L"stampa") ok = rawWrite(src, L"\\\\" + j.computer + L"\\" + j.stampante);
    else ok = CopyFileW(src.c_str(), (j.a + (j.rinomina.empty() ? j.file : j.rinomina)).c_str(), FALSE);
    return ok ? L"" : L"errore " + std::to_wstring(GetLastError());
}

static void addHist(const Job& j, const wstring& err, const wstring& archive) {
    Entry e{ now(L"%d/%m/%Y %H:%M:%S"), j.file, err.empty() ? L"OK" : err, archive, j, err.empty() };
    g_hist.insert(g_hist.begin(), e);
    if (!g_list) return;
    LVITEMW it{}; it.mask = LVIF_TEXT; it.iItem = 0; it.pszText = (LPWSTR)e.when.c_str();
    ListView_InsertItem(g_list, &it);
    ListView_SetItemText(g_list, 0, 1, (LPWSTR)e.name.c_str());
    ListView_SetItemText(g_list, 0, 2, (LPWSTR)e.esito.c_str());
}

static void scanJob(const Job& j) {
    wstring src = j.dove + j.file, sen = j.dove + j.sentinella;
    if (!exists(sen) || !exists(src)) return;   // la sentinella garantisce che il download sia concluso
    CreateDirectoryW(g_archive.c_str(), nullptr);
    wstring arc = g_archive + now(L"%Y%m%d_%H%M%S_") + j.file;   // copia per poter ripetere
    CopyFileW(src.c_str(), arc.c_str(), FALSE);
    wstring err = doAction(j, src);
    if (err.empty()) DeleteFileW(src.c_str());
    DeleteFileW(sen.c_str());   // anche su errore: niente loop infinito, si ripete dalla lista
    addHist(j, err, arc);
}

static void retry(int i) {
    if (i < 0 || i >= (int)g_hist.size()) return;
    Entry e = g_hist[i];
    addHist(e.job, exists(e.archive) ? doAction(e.job, e.archive) : L"copia archivio mancante", e.archive);
}

enum { ID_SET = 1, ID_RETRY, ID_OPEN, ID_EXIT, ID_TIMER = 1, WM_TRAY = WM_APP + 1 };

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    static int t = 0;
    switch (m) {
    case WM_CREATE: {
        HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        g_list = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                               0, 0, 0, 0, h, nullptr, nullptr, nullptr);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        const wchar_t* cols[] = { L"Data e ora", L"File", L"Esito" }; int wd[] = { 140, 170, 150 };
        for (int i = 0; i < 3; i++) { LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH; c.pszText = (LPWSTR)cols[i]; c.cx = wd[i]; ListView_InsertColumn(g_list, i, &c); }
        SendMessageW(g_list, WM_SETFONT, (WPARAM)f, TRUE);
        HWND b1 = CreateWindowW(L"BUTTON", L"Impostazioni", WS_CHILD | WS_VISIBLE, 10, 0, 110, 28, h, (HMENU)ID_SET, nullptr, nullptr);
        HWND b2 = CreateWindowW(L"BUTTON", L"Ripeti selezionato", WS_CHILD | WS_VISIBLE, 130, 0, 140, 28, h, (HMENU)ID_RETRY, nullptr, nullptr);
        SendMessageW(b1, WM_SETFONT, (WPARAM)f, TRUE); SendMessageW(b2, WM_SETFONT, (WPARAM)f, TRUE);
        SetTimer(h, ID_TIMER, 1000, nullptr);
        return 0; }
    case WM_SIZE: {
        int W = LOWORD(l), H = HIWORD(l);
        MoveWindow(g_list, 0, 0, W, H - 40, TRUE);
        MoveWindow(GetDlgItem(h, ID_SET), 10, H - 34, 110, 28, TRUE);
        MoveWindow(GetDlgItem(h, ID_RETRY), 130, H - 34, 140, 28, TRUE);
        return 0; }
    case WM_TIMER:
        t++;
        if (t % 3 == 0) loadConfig();   // le modifiche al file ini valgono senza riavvio
        for (auto& j : g_jobs) if (t % j.secondi == 0) scanJob(j);
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == ID_SET) ShellExecuteW(h, L"open", L"notepad.exe", g_ini.c_str(), nullptr, SW_SHOW);
        else if (LOWORD(w) == ID_RETRY) retry(ListView_GetNextItem(g_list, -1, LVNI_SELECTED));
        else if (LOWORD(w) == ID_OPEN) { ShowWindow(h, SW_SHOW); SetForegroundWindow(h); }
        else if (LOWORD(w) == ID_EXIT) DestroyWindow(h);
        return 0;
    case WM_TRAY:
        if (l == WM_LBUTTONUP) { ShowWindow(h, SW_SHOW); SetForegroundWindow(h); }
        else if (l == WM_RBUTTONUP) {
            HMENU mn = CreatePopupMenu(); POINT p; GetCursorPos(&p);
            AppendMenuW(mn, MF_STRING, ID_OPEN, L"Apri"); AppendMenuW(mn, MF_STRING, ID_EXIT, L"Esci");
            SetForegroundWindow(h); TrackPopupMenu(mn, TPM_RIGHTBUTTON, p.x, p.y, 0, h, nullptr); DestroyMenu(mn);
        }
        return 0;
    case WM_CLOSE: ShowWindow(h, SW_HIDE); return 0;   // resta nella tray
    case WM_DESTROY: Shell_NotifyIconW(NIM_DELETE, &g_nid); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static int selfTest() {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    wstring base = dir(tmp) + L"sentinel_test_" + now(L"%H%M%S") + L"\\";
    CreateDirectoryW(base.c_str(), nullptr); CreateDirectoryW((base + L"a\\").c_str(), nullptr); CreateDirectoryW((base + L"b\\").c_str(), nullptr);
    g_archive = base + L"arch\\";
    Job j; j.dove = base + L"a\\"; j.a = base + L"b\\"; j.file = L"x.xml"; j.rinomina = L"y.xml"; j.sentinella = L"x.ok"; j.modo = L"sposta";
    auto touch = [](const wstring& p) { HANDLE f = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr); CloseHandle(f); };
    scanJob(j);                                                    // niente file: nessuna azione
    bool ok = g_hist.empty();
    touch(j.dove + j.file); scanJob(j);                            // file senza sentinella: nessuna azione
    ok = ok && g_hist.empty() && exists(j.dove + j.file);
    touch(j.dove + j.sentinella); scanJob(j);                      // entrambi: sposta + rinomina
    ok = ok && g_hist.size() == 1 && g_hist[0].ok && exists(j.a + L"y.xml") && !exists(j.dove + j.file) && !exists(j.dove + j.sentinella);
    DeleteFileW((j.a + L"y.xml").c_str()); retry(0);               // ripeti dall'archivio
    ok = ok && g_hist.size() == 2 && g_hist[0].ok && exists(j.a + L"y.xml");
    j.a = base + L"inesistente\\"; touch(j.dove + j.file); touch(j.dove + j.sentinella); scanJob(j);   // destinazione assente: fallisce, il file resta
    ok = ok && !g_hist[0].ok && exists(j.dove + j.file);
    if (!wcsstr(GetCommandLineW(), L"--quiet")) MessageBoxW(nullptr, ok ? L"self-test OK" : L"self-test FALLITO", L"Sentinel", MB_OK);
    return ok ? 0 : 1;
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int) {
    if (wcsstr(GetCommandLineW(), L"--test")) return selfTest();
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"SentinelUtility_single");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    InitCommonControls();
    g_ini = exeDir() + L"\\sentinel.ini"; g_archive = exeDir() + L"\\archivio\\";
    if (!exists(g_ini)) writeDefaults();
    loadConfig();
    WNDCLASSW wc{}; wc.lpfnWndProc = proc; wc.hInstance = hi; wc.lpszClassName = L"SentinelWnd";
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION); wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);
    HWND h = CreateWindowW(L"SentinelWnd", L"Sentinel Utility", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 360, nullptr, nullptr, hi, nullptr);
    g_nid.cbSize = sizeof g_nid; g_nid.hWnd = h; g_nid.uID = 1; g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY; g_nid.hIcon = wc.hIcon; wcscpy(g_nid.szTip, L"Sentinel Utility");
    Shell_NotifyIconW(NIM_ADD, &g_nid);   // finestra nascosta all'avvio: solo icona vicino all'orologio
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    CloseHandle(mtx);
    return 0;
}
