/*
 * Memory Periodic Table  —  native Windows version
 * Shows RAM usage as a periodic table: every running program is an "element".
 * Hover a tile for a details card. Refreshes every 2 seconds.
 *
 * Build (MinGW):
 *   x86_64-w64-mingw32-gcc -O2 -s -municode -mwindows memtable.c memtable.res.o \
 *       -o MemoryPeriodicTable.exe -lpsapi -ladvapi32 -lgdi32 -luser32 -static
 */
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <math.h>

#define REFRESH_MS 2000
#define COLS 18
#define MAX_TILES 90
#define MAX_GROUPS 2048
#define MAX_PROCS 8192
#define NAME_LEN 64
#define MAX_ROWS 14

#define C_BG     RGB(0x0f, 0x11, 0x17)
#define C_PANEL  RGB(0x17, 0x1a, 0x23)
#define C_TEXT   RGB(0xe8, 0xea, 0xf0)
#define C_MUTED  RGB(0x8b, 0x90, 0xa0)
#define C_LINE   RGB(0x26, 0x2a, 0x36)
#define C_TRACK  RGB(0x22, 0x26, 0x33)

enum { G_BROWSER, G_SYSTEM, G_DEV, G_COMMS, G_OFFICE, G_MEDIA, G_SECURITY, G_OTHER, G_COUNT };

static const WCHAR *GROUP_LABEL[G_COUNT] = {
    L"Browsers", L"System", L"Developer", L"Communication",
    L"Office & Docs", L"Media & Games", L"Security", L"Other apps"};
static const COLORREF GROUP_COLOR[G_COUNT] = {
    RGB(0x4f, 0x8c, 0xff), RGB(0x8a, 0x94, 0xa8), RGB(0x2f, 0xbf, 0x71), RGB(0xb4, 0x6c, 0xff),
    RGB(0xff, 0x9f, 0x43), RGB(0xff, 0x5d, 0x8f), RGB(0xf5, 0xc5, 0x42), RGB(0x26, 0xc6, 0xda)};

static const WCHAR *KW_BROWSER[] = {L"chrome", L"msedge", L"edge", L"firefox", L"opera", L"brave",
    L"vivaldi", L"arc", L"iexplore", NULL};
static const WCHAR *KW_DEV[] = {L"code", L"python", L"node", L"java", L"idea", L"pycharm", L"webstorm",
    L"docker", L"git", L"postgres", L"mysql", L"sqlservr", L"devenv", L"studio", L"cursor", L"postman",
    L"wsl", L"vmmem", L"php", L"ruby", L"dotnet", L"electron", L"n8n", L"windowsterminal",
    L"powershell", L"pwsh", L"cmd", L"conhost", L"openconsole", NULL};
static const WCHAR *KW_COMMS[] = {L"teams", L"ms-teams", L"slack", L"discord", L"zoom", L"whatsapp",
    L"telegram", L"skype", L"outlook", L"olk", L"thunderbird", L"signal", L"hxoutlook", NULL};
static const WCHAR *KW_OFFICE[] = {L"winword", L"excel", L"powerpnt", L"onenote", L"acrobat", L"acrord",
    L"notion", L"obsidian", L"pbidesktop", L"powerbi", L"figma", L"photoshop", L"illustrator",
    L"notepad", L"wordpad", NULL};
static const WCHAR *KW_MEDIA[] = {L"spotify", L"vlc", L"music", L"photos", L"steam", L"epicgames",
    L"obs", L"netflix", L"mpv", L"itunes", L"premiere", L"capcut", L"gamebar", NULL};
static const WCHAR *KW_SECURITY[] = {L"msmpeng", L"defender", L"antimalware", L"avast", L"avg",
    L"kaspersky", L"mcafee", L"norton", L"eset", L"securityhealth", L"nissrv", L"mpdefender", NULL};
static const WCHAR *KW_SYSTEM[] = {L"system", L"svchost", L"explorer", L"dwm", L"csrss", L"lsass",
    L"services", L"winlogon", L"searchhost", L"searchindexer", L"searchapp", L"runtimebroker", L"smss",
    L"wininit", L"fontdrvhost", L"audiodg", L"spoolsv", L"ctfmon", L"sihost", L"taskhostw",
    L"startmenuexperiencehost", L"shellexperiencehost", L"textinputhost", L"registry",
    L"memory compression", L"widgets", L"taskmgr", L"applicationframehost", L"lockapp",
    L"phoneexperiencehost", L"onedrive", L"igfx", L"nvcontainer", L"nvidia", L"amdrs", L"radeon",
    L"realtek", L"rtkaud", L"intel", L"wmiprvse", L"dllhost", L"secure system", L"lsaiso",
    L"wudfhost", L"dashost", L"msedgewebview2", L"winedevice", L"wineserver", L"plugplay",
    L"rpcss", L"tabtip", L"useroobebroker", L"backgroundtaskhost", L"smartscreen", L"crashpad", NULL};

static const WCHAR **KW[G_COUNT] = {KW_BROWSER, KW_SYSTEM, KW_DEV, KW_COMMS, KW_OFFICE, KW_MEDIA,
                                    KW_SECURITY, NULL};

static const WCHAR *DESC[][2] = {
    {L"chrome", L"Google Chrome web browser"}, {L"msedge", L"Microsoft Edge web browser"},
    {L"firefox", L"Mozilla Firefox web browser"}, {L"opera", L"Opera web browser"},
    {L"brave", L"Brave web browser"}, {L"code", L"Visual Studio Code editor"},
    {L"explorer", L"Windows Explorer (taskbar, desktop & file windows)"},
    {L"svchost", L"Windows service host (runs background services)"},
    {L"dwm", L"Desktop Window Manager (draws windows & effects)"},
    {L"msmpeng", L"Microsoft Defender antivirus engine"},
    {L"memory compression", L"Windows compressed-RAM store"},
    {L"searchhost", L"Windows Search"}, {L"searchindexer", L"Windows Search indexer"},
    {L"runtimebroker", L"Permission broker for Store apps"},
    {L"csrss", L"Windows client/server runtime"}, {L"lsass", L"Windows login & security service"},
    {L"ms-teams", L"Microsoft Teams"}, {L"teams", L"Microsoft Teams"}, {L"slack", L"Slack"},
    {L"discord", L"Discord"}, {L"zoom", L"Zoom"}, {L"whatsapp", L"WhatsApp Desktop"},
    {L"telegram", L"Telegram Desktop"}, {L"outlook", L"Microsoft Outlook"},
    {L"olk", L"Microsoft Outlook (new)"}, {L"winword", L"Microsoft Word"},
    {L"excel", L"Microsoft Excel"}, {L"powerpnt", L"Microsoft PowerPoint"},
    {L"onenote", L"Microsoft OneNote"}, {L"pbidesktop", L"Power BI Desktop"},
    {L"figma", L"Figma"}, {L"spotify", L"Spotify"}, {L"onedrive", L"Microsoft OneDrive sync"},
    {L"python", L"Python"}, {L"pythonw", L"Python (no console)"}, {L"node", L"Node.js"},
    {L"vmmem", L"WSL / virtual machine memory"}, {L"docker desktop", L"Docker Desktop"},
    {L"postman", L"Postman API client"}, {L"obs64", L"OBS Studio"}, {L"steam", L"Steam"},
    {L"widgets", L"Windows Widgets board"}, {L"audiodg", L"Windows audio engine"},
    {L"taskmgr", L"Task Manager"}, {L"notepad", L"Notepad"}, {L"postgres", L"PostgreSQL database"},
    {L"msedgewebview2", L"Edge WebView2 (web content inside other apps)"},
    {L"memoryperiodictable", L"This app"}, {L"wmiprvse", L"Windows Management Instrumentation"},
    {L"dllhost", L"COM surrogate (hosts system components)"},
    {L"startmenuexperiencehost", L"Windows Start menu"},
    {L"shellexperiencehost", L"Windows shell UI (notifications, clock)"},
    {L"textinputhost", L"Windows touch keyboard & emoji panel"},
    {L"ctfmon", L"Windows text input / language bar"},
    {L"spoolsv", L"Windows print spooler"}, {L"services", L"Windows Services controller"},
    {NULL, NULL}};

typedef struct {
    WCHAR name[NAME_LEN];
    WCHAR key[NAME_LEN];
    ULONGLONG rss;
    int count;
    int threads;
    double cpu;
    DWORD topPid;
    ULONGLONG topRss;
    int other;
} Group;

typedef struct { DWORD pid; ULONGLONG create; ULONGLONG cpu; } CpuRec;
typedef struct { WCHAR key[NAME_LEN]; WCHAR sym[4]; } SymRec;

typedef struct {
    WCHAR title[160];
    WCHAR subtitle[160];
    COLORREF color;
    int nrows;
    WCHAR label[MAX_ROWS][32];
    WCHAR value[MAX_ROWS][MAX_PATH + 64];
} TipContent;

static Group g_groups[MAX_GROUPS];
static int g_ngroups;
static Group g_tiles[MAX_TILES];
static int g_ntiles;
static RECT g_tileRect[MAX_TILES];
static CpuRec g_prev[MAX_PROCS], g_cur[MAX_PROCS];
static int g_nprev;
static ULONGLONG g_lastTick;
static int g_ncpu = 1;
static SymRec g_syms[4096];
static int g_nsyms;
static MEMORYSTATUSEX g_ms;

static HWND g_main, g_tipWnd;
static TipContent g_tip;
static int g_tipW, g_tipH, g_tipVisible;
static int g_hover = -1;
#ifndef FREE_ID
#define FREE_ID 1000
#endif
static int g_tracking;
static double g_scale = 1.0;
static WCHAR g_status[256] = L"Hover over an element to see details   \x2022   Press M to switch views";

/* ---------------------------------------------------------------- helpers */

static COLORREF Blend(COLORREF a, COLORREF b, double t) {
    int r = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t + 0.5);
    int g = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t + 0.5);
    int bl = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t + 0.5);
    return RGB(r, g, bl);
}

static void Fill(HDC dc, int x1, int y1, int x2, int y2, COLORREF c) {
    RECT r = {x1, y1, x2, y2};
    SetDCBrushColor(dc, c);
    FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
}

static void Frame(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int w) {
    Fill(dc, x1, y1, x2, y1 + w, c);
    Fill(dc, x1, y2 - w, x2, y2, c);
    Fill(dc, x1, y1, x1 + w, y2, c);
    Fill(dc, x2 - w, y1, x2, y2, c);
}

static HFONT MakeFont(int px, int weight) {
    if (px < 7) px = 7;
    return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

static void Text(HDC dc, const WCHAR *s, int x1, int y1, int x2, int y2, UINT fmt, HFONT f,
                 COLORREF c) {
    RECT r = {x1, y1, x2, y2};
    SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, s, -1, &r, fmt | DT_NOPREFIX);
}

static int TextWidth(HDC dc, const WCHAR *s, HFONT f) {
    SIZE sz;
    SelectObject(dc, f);
    GetTextExtentPoint32W(dc, s, (int)wcslen(s), &sz);
    return sz.cx;
}

static void FmtBytes(double b, WCHAR *out, int n) {
    if (b >= 1073741824.0) _snwprintf(out, n, L"%.1f GB", b / 1073741824.0);
    else if (b >= 1048576.0) _snwprintf(out, n, L"%.0f MB", b / 1048576.0);
    else _snwprintf(out, n, L"%.0f KB", b / 1024.0);
    out[n - 1] = 0;
}

static ULONGLONG FtToU64(FILETIME f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }

static int Classify(const WCHAR *k) {
    static const int order[] = {G_SECURITY, G_BROWSER, G_COMMS, G_OFFICE, G_MEDIA, G_DEV, G_SYSTEM};
    for (int oi = 0; oi < 7; oi++) {
        int g = order[oi];
        for (const WCHAR **kw = KW[g]; *kw; kw++) {
            size_t L = wcslen(*kw);
            if (!wcscmp(k, *kw) || !wcsncmp(k, *kw, L) || (L > 3 && wcsstr(k, *kw))) return g;
        }
    }
    return G_OTHER;
}

static const WCHAR *Describe(const WCHAR *key) {
    for (int i = 0; DESC[i][0]; i++)
        if (!wcscmp(DESC[i][0], key)) return DESC[i][1];
    return NULL;
}

static int SymUsed(const WCHAR *s) {
    for (int i = 0; i < g_nsyms; i++)
        if (!wcscmp(g_syms[i].sym, s)) return 1;
    return 0;
}

/* Stable, unique 1-3 character chemistry-style symbol per program. */
static const WCHAR *GetSymbol(const Group *g) {
    static WCHAR tmp[4];
    if (g->other) return L"\x03A3";
    for (int i = 0; i < g_nsyms; i++)
        if (!wcscmp(g_syms[i].key, g->key)) return g_syms[i].sym;

    WCHAR letters[NAME_LEN];
    int n = 0;
    for (const WCHAR *p = g->name; *p && n < NAME_LEN - 1; p++)
        if (iswalpha(*p) && *p < 0x2000) letters[n++] = *p;
    if (n == 0) { letters[0] = L'X'; letters[1] = L'x'; n = 2; }
    WCHAR first = towupper(letters[0]);
    WCHAR cand[4] = {0};
    int found = 0;

    if (n == 1) { cand[0] = first; cand[1] = 0; found = !SymUsed(cand); }
    for (int pass = 0; pass < 2 && !found; pass++)
        for (int i = 1; i < n && !found; i++) {
            WCHAR c = towlower(letters[i]);
            if (pass == 0 && wcschr(L"aeiou", c)) continue;
            cand[0] = first; cand[1] = c; cand[2] = 0;
            found = !SymUsed(cand);
        }
    for (int i = 1; i < n && !found; i++)
        for (int j = i + 1; j < n && !found; j++) {
            cand[0] = first; cand[1] = towlower(letters[i]); cand[2] = towlower(letters[j]); cand[3] = 0;
            found = !SymUsed(cand);
        }
    for (int k = 2; !found && k < 100; k++) {
        _snwprintf(cand, 4, L"%lc%d", first, k);
        cand[3] = 0;
        found = !SymUsed(cand);
    }
    if (g_nsyms < 4096) {
        wcscpy(g_syms[g_nsyms].key, g->key);
        wcscpy(g_syms[g_nsyms].sym, cand);
        return g_syms[g_nsyms++].sym;
    }
    wcscpy(tmp, cand);
    return tmp;
}

/* ---------------------------------------------------------------- data */

static int CmpGroup(const void *a, const void *b) {
    ULONGLONG x = ((const Group *)a)->rss, y = ((const Group *)b)->rss;
    return x < y ? 1 : x > y ? -1 : 0;
}

static void Refresh(void) {
    g_ms.dwLength = sizeof(g_ms);
    GlobalMemoryStatusEx(&g_ms);

    ULONGLONG now = GetTickCount64();
    double elapsed = g_lastTick ? (now - g_lastTick) / 1000.0 : 0.0;
    g_lastTick = now;

    int ncur = 0;
    g_ngroups = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == 0) continue;
                ULONGLONG rss = 0;
                double cpu = 0;
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (h) {
                    PROCESS_MEMORY_COUNTERS pmc;
                    if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) rss = pmc.WorkingSetSize;
                    FILETIME c, e, k, u;
                    if (GetProcessTimes(h, &c, &e, &k, &u)) {
                        ULONGLONG t = FtToU64(k) + FtToU64(u), cr = FtToU64(c);
                        if (ncur < MAX_PROCS) {
                            g_cur[ncur].pid = pe.th32ProcessID;
                            g_cur[ncur].create = cr;
                            g_cur[ncur].cpu = t;
                            ncur++;
                        }
                        if (elapsed > 0)
                            for (int i = 0; i < g_nprev; i++)
                                if (g_prev[i].pid == pe.th32ProcessID && g_prev[i].create == cr) {
                                    if (t >= g_prev[i].cpu)
                                        cpu = (t - g_prev[i].cpu) / 1e7 / elapsed / g_ncpu * 100.0;
                                    break;
                                }
                    }
                    CloseHandle(h);
                }
                if (rss == 0) continue;

                WCHAR name[NAME_LEN], key[NAME_LEN];
                wcsncpy(name, pe.szExeFile, NAME_LEN - 1);
                name[NAME_LEN - 1] = 0;
                size_t L = wcslen(name);
                if (L > 4 && !_wcsicmp(name + L - 4, L".exe")) name[L - 4] = 0;
                wcscpy(key, name);
                _wcslwr(key);

                Group *g = NULL;
                for (int i = 0; i < g_ngroups; i++)
                    if (!wcscmp(g_groups[i].key, key)) { g = &g_groups[i]; break; }
                if (!g) {
                    if (g_ngroups >= MAX_GROUPS) continue;
                    g = &g_groups[g_ngroups++];
                    memset(g, 0, sizeof(*g));
                    wcscpy(g->name, name);
                    wcscpy(g->key, key);
                }
                g->rss += rss;
                g->count++;
                g->threads += (int)pe.cntThreads;
                g->cpu += cpu;
                if (rss > g->topRss) { g->topRss = rss; g->topPid = pe.th32ProcessID; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }
    memcpy(g_prev, g_cur, sizeof(CpuRec) * ncur);
    g_nprev = ncur;

    qsort(g_groups, g_ngroups, sizeof(Group), CmpGroup);
    if (g_ngroups <= MAX_TILES) {
        g_ntiles = g_ngroups;
        memcpy(g_tiles, g_groups, sizeof(Group) * g_ntiles);
    } else {
        g_ntiles = MAX_TILES;
        memcpy(g_tiles, g_groups, sizeof(Group) * (MAX_TILES - 1));
        Group *o = &g_tiles[MAX_TILES - 1];
        memset(o, 0, sizeof(*o));
        wcscpy(o->name, L"Everything else");
        wcscpy(o->key, L"everything else");
        o->other = 1;
        for (int i = MAX_TILES - 1; i < g_ngroups; i++) {
            o->rss += g_groups[i].rss;
            o->count += g_groups[i].count;
            o->threads += g_groups[i].threads;
            o->cpu += g_groups[i].cpu;
        }
    }
}

/* ---------------------------------------------------------------- tooltip */

static void AddRow(const WCHAR *label, const WCHAR *fmt, ...) {
    if (g_tip.nrows >= MAX_ROWS) return;
    va_list ap;
    va_start(ap, fmt);
    wcsncpy(g_tip.label[g_tip.nrows], label, 31);
    g_tip.label[g_tip.nrows][31] = 0;
    _vsnwprintf(g_tip.value[g_tip.nrows], MAX_PATH + 63, fmt, ap);
    g_tip.value[g_tip.nrows][MAX_PATH + 63] = 0;
    va_end(ap);
    g_tip.nrows++;
}

static void BuildTip(int i) {
    static const WCHAR *MON[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
                                 L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
    if (i == FREE_ID) {
        WCHAR av[32], tot[32], usedS[32];
        double total = (double)g_ms.ullTotalPhys, avail = (double)g_ms.ullAvailPhys;
        FmtBytes(avail, av, 32);
        FmtBytes(total, tot, 32);
        FmtBytes(total - avail, usedS, 32);
        memset(&g_tip, 0, sizeof(g_tip));
        g_tip.color = C_MUTED;
        wcscpy(g_tip.title, L"Available memory");
        wcscpy(g_tip.subtitle, L"RAM that programs can use right now (free + standby cache)");
        AddRow(L"Available", L"%ls  (%.0f%% of RAM)", av, total > 0 ? avail * 100 / total : 0);
        AddRow(L"In use", L"%ls", usedS);
        AddRow(L"Total RAM", L"%ls", tot);
        _snwprintf(g_status, 255, L"Available memory   \x2022   %ls of %ls", av, tot);
        return;
    }
    Group *g = &g_tiles[i];
    double share = g_ms.ullTotalPhys ? g->rss * 100.0 / g_ms.ullTotalPhys : 0;
    WCHAR mem[32];
    FmtBytes((double)g->rss, mem, 32);
    memset(&g_tip, 0, sizeof(g_tip));

    if (g->other) {
        g_tip.color = GROUP_COLOR[G_OTHER];
        _snwprintf(g_tip.title, 159, L"#%d  \x03A3  Everything else", i + 1);
        wcscpy(g_tip.subtitle, L"Smaller programs that didn't fit in the table");
        AddRow(L"Memory", L"%ls  (%.1f%% of RAM)", mem, share);
        AddRow(L"Programs", L"%d", g_ngroups - (MAX_TILES - 1));
        AddRow(L"Processes", L"%d", g->count);
        AddRow(L"CPU", L"%.1f%%", g->cpu);
        _snwprintf(g_status, 255, L"#%d  Everything else   \x2022   %ls", i + 1, mem);
        return;
    }

    int grp = Classify(g->key);
    g_tip.color = GROUP_COLOR[grp];
    _snwprintf(g_tip.title, 159, L"#%d  %ls  \x00B7  %ls", i + 1, GetSymbol(g), g->name);
    const WCHAR *d = Describe(g->key);
    if (d) wcscpy(g_tip.subtitle, d);

    AddRow(L"Memory", L"%ls  (%.1f%% of RAM)", mem, share);
    AddRow(L"Rank", L"#%d of %d programs", i + 1, g_ngroups);
    AddRow(L"CPU", L"%.1f%%", g->cpu);
    AddRow(L"Processes", g->count == 1 ? L"%d process" : L"%d processes", g->count);
    AddRow(L"Threads", L"%d", g->threads);
    AddRow(L"Main PID", L"%lu", (unsigned long)g->topPid);
    AddRow(L"Category", L"%ls", GROUP_LABEL[grp]);

    WCHAR path[MAX_PATH] = L"", user[160] = L"";
    FILETIME created = {0, 0};
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g->topPid);
    if (h) {
        DWORD n = MAX_PATH;
        if (!QueryFullProcessImageNameW(h, 0, path, &n)) path[0] = 0;
        FILETIME e, k, u;
        if (!GetProcessTimes(h, &created, &e, &k, &u)) created.dwLowDateTime = created.dwHighDateTime = 0;
        HANDLE tok;
        if (OpenProcessToken(h, TOKEN_QUERY, &tok)) {
            BYTE buf[512];
            DWORD len;
            if (GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &len)) {
                TOKEN_USER *tu = (TOKEN_USER *)buf;
                WCHAR nm[128], dm[128];
                DWORD nl = 128, dl = 128;
                SID_NAME_USE use;
                if (LookupAccountSidW(NULL, tu->User.Sid, nm, &nl, dm, &dl, &use))
                    _snwprintf(user, 159, dl > 0 ? L"%ls\\%ls" : L"%ls%ls", dm, nm);
            }
            CloseHandle(tok);
        }
        CloseHandle(h);
    }
    if (created.dwLowDateTime || created.dwHighDateTime) {
        FILETIME nowFt, local;
        SYSTEMTIME st;
        GetSystemTimeAsFileTime(&nowFt);
        FileTimeToLocalFileTime(&created, &local);
        FileTimeToSystemTime(&local, &st);
        ULONGLONG mins = (FtToU64(nowFt) - FtToU64(created)) / 600000000ULL;
        int hr12 = st.wHour % 12 == 0 ? 12 : st.wHour % 12;
        WCHAR ago[48];
        if (mins >= 1440) _snwprintf(ago, 47, L"%ud %uh ago", (unsigned)(mins / 1440), (unsigned)(mins % 1440 / 60));
        else if (mins >= 60) _snwprintf(ago, 47, L"%uh %um ago", (unsigned)(mins / 60), (unsigned)(mins % 60));
        else _snwprintf(ago, 47, L"%um ago", (unsigned)mins);
        AddRow(L"Started", L"%d %ls, %d:%02d %ls  (%ls)", st.wDay, MON[(st.wMonth + 11) % 12], hr12,
               st.wMinute, st.wHour < 12 ? L"AM" : L"PM", ago);
    }
    if (user[0]) AddRow(L"User", L"%ls", user);
    AddRow(L"Location", L"%ls", path[0] ? path : L"Protected (run as administrator to see)");

    _snwprintf(g_status, 255, L"#%d  %ls   \x2022   %ls  (%.1f%% of RAM)   \x2022   %d process%ls   \x2022   %ls",
               i + 1, g->name, mem, share, g->count, g->count == 1 ? L"" : L"es", GROUP_LABEL[grp]);
}

static int Block(HDC dc, const WCHAR *s, int x1, int y, int x2, UINT fmt, HFONT f, COLORREF c, int draw) {
    RECT r = {x1, y, x2, y + 4000};
    SelectObject(dc, f);
    DrawTextW(dc, s, -1, &r, fmt | DT_CALCRECT | DT_NOPREFIX);
    int h = r.bottom - r.top;
    if (draw) {
        RECT d = {x1, y, x2, y + h};
        SetTextColor(dc, c);
        DrawTextW(dc, s, -1, &d, fmt | DT_NOPREFIX);
    }
    return h;
}

static int TipLayout(HDC dc, int draw) {
    double s = g_scale;
    int W = g_tipW, pad = (int)(16 * s), y = (int)(14 * s);
    HFONT fT = MakeFont((int)(19 * s), FW_BOLD), fS = MakeFont((int)(12 * s), FW_NORMAL);
    HFONT fL = MakeFont((int)(12 * s), FW_NORMAL), fV = MakeFont((int)(13 * s), FW_SEMIBOLD);
    SetBkMode(dc, TRANSPARENT);
    if (draw) {
        Fill(dc, 0, 0, W, g_tipH, C_PANEL);
        Frame(dc, 0, 0, W, g_tipH, g_tip.color, (int)(2 * s) < 1 ? 1 : (int)(2 * s));
    }
    y += Block(dc, g_tip.title, pad, y, W - pad, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS, fT, g_tip.color, draw);
    if (g_tip.subtitle[0])
        y += Block(dc, g_tip.subtitle, pad, y + (int)(2 * s), W - pad, DT_LEFT | DT_WORDBREAK, fS, C_MUTED, draw) + (int)(2 * s);
    y += (int)(8 * s);
    if (draw) Fill(dc, pad, y, W - pad, y + 1, RGB(0x2a, 0x2e, 0x3b));
    y += (int)(9 * s);
    int labelW = (int)(86 * s);
    for (int r = 0; r < g_tip.nrows; r++) {
        int hl = Block(dc, g_tip.label[r], pad, y + (int)(1 * s), pad + labelW, DT_LEFT | DT_SINGLELINE, fL, C_MUTED, draw);
        int hv = Block(dc, g_tip.value[r], pad + labelW, y, W - pad, DT_LEFT | DT_WORDBREAK | DT_EDITCONTROL, fV, C_TEXT, draw);
        y += (hl > hv ? hl : hv) + (int)(5 * s);
    }
    y += (int)(10 * s);
    DeleteObject(fT); DeleteObject(fS); DeleteObject(fL); DeleteObject(fV);
    return y;
}

static void MoveTip(void) {
    if (!g_tipVisible) return;
    POINT pt;
    GetCursorPos(&pt);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork;
    int off = (int)(18 * g_scale);
    int x = pt.x + off, y = pt.y + off;
    if (x + g_tipW > wa.right) x = pt.x - g_tipW - off;
    if (y + g_tipH > wa.bottom) y = pt.y - g_tipH - off;
    if (x < wa.left) x = wa.left;
    if (y < wa.top) y = wa.top;
    SetWindowPos(g_tipWnd, HWND_TOPMOST, x, y, g_tipW, g_tipH, SWP_NOACTIVATE);
}

static void ShowTip(void) {
    g_tipW = (int)(390 * g_scale);
    HDC dc = GetDC(g_tipWnd);
    g_tipH = TipLayout(dc, 0);
    ReleaseDC(g_tipWnd, dc);
    if (!g_tipVisible) {
        g_tipVisible = 1;
        MoveTip();
        ShowWindow(g_tipWnd, SW_SHOWNOACTIVATE);
    } else {
        MoveTip();
    }
    InvalidateRect(g_tipWnd, NULL, FALSE);
}

static void HideTip(void) {
    if (g_tipVisible) {
        ShowWindow(g_tipWnd, SW_HIDE);
        g_tipVisible = 0;
    }
}

static LRESULT CALLBACK TipProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(mem, bmp);
        TipLayout(mem, 1);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---------------------------------------------------------------- main paint */

static int g_view = 0; /* 0 = periodic table, 1 = memory map */
static int g_showFree = 1; /* memory map: show the Available block (toggle with A) */
static RECT g_btnRect[2];
static RECT g_freeRect;
#define FREE_ID 1000
#define HINT_TEXT L"Hover over an element to see details   \x2022   Press M to switch views"

static int TileGroup(const Group *g) { return g->other ? G_OTHER : Classify(g->key); }

/* Bigger memory users glow brighter; small ones fade into the background. */
static COLORREF Heat(COLORREF col, double r, int hov) {
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    double t = 0.10 + 0.52 * sqrt(r);
    if (hov) t += 0.14;
    if (t > 0.85) t = 0.85;
    return Blend(C_BG, col, t);
}

static double UsedBytes(void) { return (double)(g_ms.ullTotalPhys - g_ms.ullAvailPhys); }

static double TopShareOfUsed(int n) {
    double sum = 0, used = UsedBytes();
    for (int i = 0; i < g_ntiles && i < n; i++)
        if (!g_tiles[i].other) sum += (double)g_tiles[i].rss;
    double p = used > 0 ? sum * 100.0 / used : 0;
    return p > 100 ? 100 : p;
}

static COLORREF GaugeColor(double pct) {
    return pct < 60 ? RGB(0x2f, 0xbf, 0x71) : pct < 85 ? RGB(0xf5, 0xc5, 0x42) : RGB(0xff, 0x5d, 0x5d);
}

static void DrawGroupBar(HDC dc, int bx1, int by1, int bx2, int by2) {
    double total = (double)g_ms.ullTotalPhys, sums[G_COUNT] = {0};
    for (int i = 0; i < g_ntiles; i++) sums[TileGroup(&g_tiles[i])] += (double)g_tiles[i].rss;
    Fill(dc, bx1, by1, bx2, by2, C_TRACK);
    double x = bx1;
    for (int k = 0; k < G_COUNT; k++) {
        double seg = total > 0 ? (bx2 - bx1) * sums[k] / total : 0;
        if (seg > bx2 - x) seg = bx2 - x;
        if (seg > 0.5) {
            Fill(dc, (int)x, by1, (int)(x + seg), by2, GROUP_COLOR[k]);
            x += seg;
        }
    }
}

/* Summary panel that sits in the empty space of the periodic table. */
static void DrawSummary(HDC dc, int x1, int y1, int x2, int y2) {
    double s = g_scale;
    int h = y2 - y1, pad = (int)(16 * s);
    Fill(dc, x1, y1, x2, y2, C_PANEL);
    Frame(dc, x1, y1, x2, y2, C_LINE, 1);
    double total = (double)g_ms.ullTotalPhys, used = UsedBytes();
    double pct = total > 0 ? used * 100.0 / total : 0;

    /* group bar along the bottom */
    int barH = (int)(12 * s);
    int by2 = y2 - pad, by1 = by2 - barH;
    HFONT fInfo = MakeFont((int)(12 * s), FW_NORMAL);
    DrawGroupBar(dc, x1 + pad, by1, x2 - pad, by2);
    Text(dc, L"Memory by group", x1 + pad, by1 - (int)(24 * s), x2 - pad, by1 - (int)(2 * s),
         DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOCLIP, fInfo, C_MUTED);
    WCHAR top[96];
    _snwprintf(top, 95, L"Top 5 programs hold %.0f%% of used memory", TopShareOfUsed(5));
    Text(dc, top, x1 + pad, by1 - (int)(24 * s), x2 - pad, by1 - (int)(2 * s),
         DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_NOCLIP, fInfo, C_TEXT);
    int contentBottom = by1 - (int)(26 * s);

    /* left column: big percentage + key numbers */
    int lw = (int)((x2 - x1) * 0.30);
    int bigPx = (int)(h * 0.27);
    if (bigPx > (int)(lw * 0.40)) bigPx = (int)(lw * 0.40);
    HFONT fBig = MakeFont(bigPx, FW_BOLD), fSub = MakeFont((int)(13 * s), FW_NORMAL);
    HFONT fLab = MakeFont((int)(12 * s), FW_NORMAL), fVal = MakeFont((int)(13 * s), FW_BOLD);
    WCHAR buf[64];
    _snwprintf(buf, 63, L"%.0f%%", pct);
    int y = y1 + pad - (int)(4 * s);
    Text(dc, buf, x1 + pad, y, x1 + lw, y2, DT_LEFT | DT_TOP | DT_SINGLELINE, fBig, GaugeColor(pct));
    y += (int)(bigPx * 1.12);
    Text(dc, L"of RAM in use", x1 + pad, y, x1 + lw, y2, DT_LEFT | DT_TOP | DT_SINGLELINE, fSub, C_MUTED);
    y += (int)(24 * s);
    WCHAR v[3][32];
    FmtBytes(used, v[0], 32);
    FmtBytes((double)g_ms.ullAvailPhys, v[1], 32);
    FmtBytes(total, v[2], 32);
    const WCHAR *labs[3] = {L"Used", L"Available", L"Total"};
    int lineH = (int)(19 * s);
    for (int k = 0; k < 3 && y + lineH <= contentBottom + (int)(4 * s); k++, y += lineH) {
        Text(dc, labs[k], x1 + pad, y, x1 + lw, y + lineH, DT_LEFT | DT_TOP | DT_SINGLELINE, fLab, C_MUTED);
        Text(dc, v[k], x1 + pad + (int)(72 * s), y, x1 + lw, y + lineH, DT_LEFT | DT_TOP | DT_SINGLELINE, fVal, C_TEXT);
    }

    /* right column: where your memory is (top 5) */
    int rx1 = x1 + lw + (int)(8 * s), rx2 = x2 - pad;
    Fill(dc, rx1 - (int)(12 * s), y1 + pad, rx1 - (int)(11 * s), contentBottom, C_LINE);
    HFONT fHead = MakeFont((int)(11 * s), FW_BOLD);
    Text(dc, L"WHERE YOUR MEMORY IS", rx1, y1 + pad - (int)(2 * s), rx2, y2, DT_LEFT | DT_TOP | DT_SINGLELINE, fHead, C_MUTED);
    int listTop = y1 + pad + (int)(18 * s);
    int n = 0;
    for (int i = 0; i < g_ntiles && n < 5; i++) if (!g_tiles[i].other) n++;
    if (n > 0) {
        int rowH = (contentBottom - listTop) / 5;
        if (rowH > (int)(32 * s)) rowH = (int)(32 * s);
        HFONT fName = MakeFont((int)(13 * s), FW_SEMIBOLD), fNum = MakeFont((int)(12 * s), FW_NORMAL);
        int nameW = (int)((rx2 - rx1) * 0.30), valW = (int)(118 * s);
        double biggest = (double)g_tiles[0].rss;
        for (int i = 0, r = 0; i < g_ntiles && r < 5; i++) {
            Group *g = &g_tiles[i];
            if (g->other) continue;
            COLORREF col = GROUP_COLOR[TileGroup(g)];
            int ry = listTop + r * rowH, cy = ry + rowH / 2;
            int sq = (int)(9 * s);
            Fill(dc, rx1, cy - sq / 2, rx1 + sq, cy - sq / 2 + sq, col);
            WCHAR nm[96];
            _snwprintf(nm, 95, L"%ls  %ls", GetSymbol(g), g->name);
            Text(dc, nm, rx1 + sq + (int)(8 * s), ry, rx1 + nameW, ry + rowH,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, fName, i == g_hover ? col : C_TEXT);
            int bx1 = rx1 + nameW + (int)(6 * s), bx2 = rx2 - valW;
            int bh = (int)(8 * s);
            if (bx2 > bx1) {
                Fill(dc, bx1, cy - bh / 2, bx2, cy - bh / 2 + bh, C_TRACK);
                Fill(dc, bx1, cy - bh / 2, bx1 + (int)((bx2 - bx1) * (g->rss / biggest)), cy - bh / 2 + bh, col);
            }
            WCHAR mem[32], val[64];
            FmtBytes((double)g->rss, mem, 32);
            _snwprintf(val, 63, L"%ls  \x00B7  %.0f%%", mem, total > 0 ? g->rss * 100.0 / total : 0);
            Text(dc, val, rx2 - valW, ry, rx2, ry + rowH, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, fNum, C_TEXT);
            SetRect(&g_tileRect[i], 0, 0, 0, 0); /* list rows are not hover targets */
            r++;
        }
        DeleteObject(fName); DeleteObject(fNum);
    }
    DeleteObject(fInfo); DeleteObject(fBig); DeleteObject(fSub);
    DeleteObject(fLab); DeleteObject(fVal); DeleteObject(fHead);
}

static void PaintPeriodic(HDC dc, int ax1, int ay1, int ax2, int ay2, int *legendX, int *legendY) {
    double s = g_scale;
    int gap = (int)(4 * s);
    if (gap < 2) gap = 2;
    double tileW = (ax2 - ax1 - gap * (COLS - 1)) / (double)COLS;
    double tileH = (ay2 - ay1 - gap * 6) / 7.0;
    if (tileW * 1.18 < tileH) tileH = tileW * 1.18;
    if (tileH * 0.95 < tileW && tileH < tileW) tileW = tileW; /* keep width */
    if (tileH < 12) tileH = 12;
    double gridW = COLS * tileW + (COLS - 1) * gap;
    double ox = ax1 + ((ax2 - ax1) - gridW) / 2, oy = ay1;
#define CX(c) (ox + (c) * (tileW + gap))
#define CY(r) (oy + (r) * (tileH + gap))

    DrawSummary(dc, (int)CX(2) + (int)(6 * s), (int)CY(0), (int)(CX(11) + tileW) - (int)(6 * s), (int)(CY(2) + tileH));

    static int slotR[MAX_TILES], slotC[MAX_TILES], slotsReady = 0;
    if (!slotsReady) {
        int n = 0;
        slotR[n] = 0; slotC[n++] = 0;
        slotR[n] = 0; slotC[n++] = 17;
        for (int r = 1; r <= 2; r++) {
            slotR[n] = r; slotC[n++] = 0;
            slotR[n] = r; slotC[n++] = 1;
            for (int c = 12; c < 18; c++) { slotR[n] = r; slotC[n++] = c; }
        }
        for (int r = 3; r < 7; r++)
            for (int c = 0; c < COLS; c++) { slotR[n] = r; slotC[n++] = c; }
        slotsReady = 1;
    }
    HFONT fSmall = MakeFont((int)(tileH * 0.13), FW_NORMAL);
    HFONT fSmallB = MakeFont((int)(tileH * 0.13), FW_BOLD);
    HFONT fSym = MakeFont((int)(tileH * 0.40), FW_BOLD);
    HFONT fName = MakeFont((int)(tileH * 0.135), FW_NORMAL);
    double biggest = g_ntiles ? (double)g_tiles[0].rss : 1;
    for (int i = 0; i < g_ntiles; i++) {
        Group *g = &g_tiles[i];
        COLORREF col = GROUP_COLOR[TileGroup(g)];
        int hov = (i == g_hover);
        int x1 = (int)CX(slotC[i]), y1 = (int)CY(slotR[i]);
        int x2 = (int)(CX(slotC[i]) + tileW), y2 = (int)(CY(slotR[i]) + tileH);
        int th = y2 - y1;
        double share = biggest > 0 ? g->rss / biggest : 0;
        if (share > 1) share = 1;
        Fill(dc, x1, y1, x2, y2, Heat(col, share, hov));
        int strong = hov || i < 5;
        Frame(dc, x1, y1, x2, y2, strong ? col : Blend(C_BG, col, 0.25 + 0.4 * sqrt(share)), strong ? 2 : 1);
        int barH = (int)(th * 0.07);
        if (barH < 3) barH = 3;
        Fill(dc, x1 + 1, y2 - barH, x1 + 1 + (int)((x2 - x1 - 2) * share), y2 - 1, col);

        COLORREF txt = Blend(C_BG, C_TEXT, 0.55 + 0.45 * sqrt(share));
        COLORREF sym = Blend(C_BG, col, 0.55 + 0.45 * sqrt(share));
        if (hov) txt = C_TEXT, sym = col;
        WCHAR num[8], mem[32];
        _snwprintf(num, 7, L"%d", i + 1);
        FmtBytes((double)g->rss, mem, 32);
        int ip = (int)(4 * s);
        Text(dc, num, x1 + ip, y1 + ip / 2, x2, y2, DT_LEFT | DT_TOP | DT_SINGLELINE, fSmallB, txt);
        Text(dc, mem, x1, y1 + ip / 2, x2 - ip, y2, DT_RIGHT | DT_TOP | DT_SINGLELINE, fSmall, i < 5 ? C_TEXT : C_MUTED);
        Text(dc, GetSymbol(g), x1, y1 + (int)(th * 0.16), x2, y1 + (int)(th * 0.70),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE, fSym, i < 5 ? (hov ? C_TEXT : C_TEXT) : sym);
        Text(dc, g->name, x1 + 2, y1 + (int)(th * 0.66), x2 - 2, y1 + (int)(th * 0.90),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, fName, txt);
        SetRect(&g_tileRect[i], x1, y1, x2, y2);
    }
    DeleteObject(fSmall); DeleteObject(fSmallB); DeleteObject(fSym); DeleteObject(fName);
    *legendX = (int)ox;
    *legendY = (int)CY(7) - gap;
#undef CX
#undef CY
}

/* Squarified treemap: block area = memory. */
typedef struct { double x1, y1, x2, y2; } DRect;

static double Worst(const double *v, int a, int b, double sum, double side) {
    double mx = v[a], mn = v[b - 1], s2 = sum * sum, w2 = side * side;
    double r1 = w2 * mx / s2, r2 = s2 / (w2 * mn);
    return r1 > r2 ? r1 : r2;
}

static void Squarify(const double *v, int n, double x, double y, double w, double h, DRect *out) {
    int start = 0;
    while (start < n) {
        if (w <= 0 || h <= 0) {
            for (int i = start; i < n; i++) out[i].x1 = out[i].y1 = out[i].x2 = out[i].y2 = 0;
            return;
        }
        double side = w < h ? w : h, rowSum = 0, best = 1e300;
        int end = start;
        while (end < n) {
            double s2 = rowSum + v[end];
            if (v[end] <= 0) { end++; continue; }
            double wr = Worst(v, start, end + 1, s2, side);
            if (end > start && wr > best) break;
            best = wr;
            rowSum = s2;
            end++;
        }
        if (rowSum <= 0) { start = end; continue; }
        if (w >= h) {
            double cw = rowSum / h, cy = y;
            for (int i = start; i < end; i++) {
                double hh = v[i] / cw;
                out[i].x1 = x; out[i].y1 = cy; out[i].x2 = x + cw; out[i].y2 = cy + hh;
                cy += hh;
            }
            x += cw; w -= cw;
        } else {
            double rh = rowSum / w, cx = x;
            for (int i = start; i < end; i++) {
                double ww = v[i] / rh;
                out[i].x1 = cx; out[i].y1 = y; out[i].x2 = cx + ww; out[i].y2 = y + rh;
                cx += ww;
            }
            y += rh; h -= rh;
        }
        start = end;
    }
}

static void PaintMap(HDC dc, int ax1, int ay1, int ax2, int ay2, int *legendX, int *legendY) {
    double s = g_scale;
    double total = (double)g_ms.ullTotalPhys, used = UsedBytes();
    double pct = total > 0 ? used * 100.0 / total : 0;

    /* header strip */
    int hh = (int)(34 * s);
    HFONT fHead = MakeFont((int)(16 * s), FW_BOLD), fHeadN = MakeFont((int)(13 * s), FW_NORMAL);
    WCHAR a[64], u[32], t[32], rest[200];
    _snwprintf(a, 63, L"%.0f%% of RAM in use", pct);
    Text(dc, a, ax1, ay1, ax2, ay1 + hh, DT_LEFT | DT_VCENTER | DT_SINGLELINE, fHead, GaugeColor(pct));
    int aw = TextWidth(dc, a, fHead);
    FmtBytes(used, u, 32);
    FmtBytes(total, t, 32);
    _snwprintf(rest, 199, L"   \x2022   %ls used of %ls   \x2022   Top 5 programs hold %.0f%% of used memory   \x2022   Block size = memory used",
               u, t, TopShareOfUsed(5));
    Text(dc, rest, ax1 + aw, ay1, ax2, ay1 + hh, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, fHeadN, C_MUTED);
    DeleteObject(fHead); DeleteObject(fHeadN);

    int mx1 = ax1, my1 = ay1 + hh + (int)(6 * s), mx2 = ax2, my2 = ay2;
    int n = g_ntiles + (g_showFree ? 1 : 0); /* + available RAM */
    static double vals[MAX_TILES + 1];
    static DRect rects[MAX_TILES + 1];
    static int order[MAX_TILES + 1];
    double avail = g_showFree ? (double)g_ms.ullAvailPhys : 0;
    /* order by size (available memory takes its natural place) */
    int k = 0, placed = 0;
    for (int i = 0; i < g_ntiles; i++) {
        if (g_showFree && !placed && avail >= (double)g_tiles[i].rss) { order[k++] = FREE_ID; placed = 1; }
        order[k++] = i;
    }
    if (g_showFree && !placed) order[k++] = FREE_ID;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        vals[i] = order[i] == FREE_ID ? avail : (double)g_tiles[order[i]].rss;
        sum += vals[i];
    }
    double area = (double)(mx2 - mx1) * (my2 - my1);
    for (int i = 0; i < n; i++) vals[i] = sum > 0 ? vals[i] * area / sum : 0;
    Squarify(vals, n, mx1, my1, mx2 - mx1, my2 - my1, rects);

    double biggest = g_ntiles ? (double)g_tiles[0].rss : 1;
    int inset = (int)(1.5 * s) < 1 ? 1 : (int)(1.5 * s);
    for (int j = 0; j < n; j++) {
        int x1 = (int)(rects[j].x1 + 0.5) + inset, y1 = (int)(rects[j].y1 + 0.5) + inset;
        int x2 = (int)(rects[j].x2 + 0.5) - inset, y2 = (int)(rects[j].y2 + 0.5) - inset;
        if (x2 - x1 < 2 || y2 - y1 < 2) continue;
        int w = x2 - x1, h = y2 - y1, m = w < h ? w : h, pad = (int)(7 * s);

        if (order[j] == FREE_ID) {
            int hov = g_hover == FREE_ID;
            Fill(dc, x1, y1, x2, y2, hov ? RGB(0x22, 0x26, 0x31) : RGB(0x16, 0x19, 0x21));
            Frame(dc, x1, y1, x2, y2, hov ? C_MUTED : C_LINE, hov ? 2 : 1);
            if (w > 60 * s && h > 40 * s) {
                HFONT f1 = MakeFont((int)(m * 0.16 < 26 * s ? m * 0.16 : 26 * s), FW_BOLD);
                HFONT f2 = MakeFont((int)(13 * s), FW_NORMAL);
                WCHAR fb[64], fp[64];
                FmtBytes(avail, fb, 64);
                _snwprintf(fp, 63, L"%.0f%% of RAM free to use", total > 0 ? avail * 100 / total : 0);
                int cy = y1 + h / 2;
                Text(dc, L"Available", x1, cy - (int)(m * 0.16) - (int)(10 * s), x2, cy, DT_CENTER | DT_BOTTOM | DT_SINGLELINE, f1, C_MUTED);
                Text(dc, fb, x1, cy, x2, cy + (int)(20 * s), DT_CENTER | DT_TOP | DT_SINGLELINE, f2, C_MUTED);
                Text(dc, fp, x1 + pad, cy + (int)(20 * s), x2 - pad, cy + (int)(40 * s), DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS, f2, C_MUTED);
                DeleteObject(f1); DeleteObject(f2);
            }
            SetRect(&g_freeRect, x1, y1, x2, y2);
            continue;
        }

        int i = order[j];
        Group *g = &g_tiles[i];
        COLORREF col = GROUP_COLOR[TileGroup(g)];
        int hov = (i == g_hover);
        double share = biggest > 0 ? g->rss / biggest : 0;
        if (share > 1) share = 1;
        Fill(dc, x1, y1, x2, y2, Heat(col, 0.25 + 0.75 * share, hov));
        Frame(dc, x1, y1, x2, y2, hov ? C_TEXT : col, hov ? 2 : 1);
        SetRect(&g_tileRect[i], x1, y1, x2, y2);

        WCHAR mem[32], pctS[48], num[8];
        FmtBytes((double)g->rss, mem, 32);
        _snwprintf(pctS, 47, L"%.1f%% of RAM", total > 0 ? g->rss * 100.0 / total : 0);
        _snwprintf(num, 7, L"#%d", i + 1);
        if (w >= 70 * s && h >= 58 * s) {
            int symPx = (int)(m * 0.36);
            if (symPx > (int)(78 * s)) symPx = (int)(78 * s);
            int smallPx = (int)(m * 0.11);
            if (smallPx < (int)(11 * s)) smallPx = (int)(11 * s);
            if (smallPx > (int)(16 * s)) smallPx = (int)(16 * s);
            HFONT fSym = MakeFont(symPx, FW_BOLD), fS = MakeFont(smallPx, FW_NORMAL), fSB = MakeFont(smallPx, FW_BOLD);
            Text(dc, num, x1 + pad, y1 + pad / 2, x2, y2, DT_LEFT | DT_TOP | DT_SINGLELINE, fSB, C_TEXT);
            Text(dc, mem, x1, y1 + pad / 2, x2 - pad, y2, DT_RIGHT | DT_TOP | DT_SINGLELINE, fSB, C_TEXT);
            int cy = y1 + h / 2;
            Text(dc, GetSymbol(g), x1, cy - symPx, x2, cy + symPx / 6, DT_CENTER | DT_BOTTOM | DT_SINGLELINE, fSym, C_TEXT);
            Text(dc, g->name, x1 + pad, cy + symPx / 6, x2 - pad, cy + symPx / 6 + smallPx + (int)(6 * s),
                 DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS, fSB, C_TEXT);
            if (h >= 110 * s)
                Text(dc, pctS, x1 + pad, cy + symPx / 6 + smallPx + (int)(6 * s), x2 - pad, y2,
                     DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS, fS, Blend(col, C_TEXT, 0.6));
            DeleteObject(fSym); DeleteObject(fS); DeleteObject(fSB);
        } else if (w >= 30 * s && h >= 18 * s) {
            HFONT fS = MakeFont((int)(11 * s), FW_SEMIBOLD);
            Text(dc, h >= 34 * s ? g->name : GetSymbol(g), x1 + 3, y1, x2 - 3, h >= 34 * s ? y1 + h / 2 + (int)(2 * s) : y2,
                 DT_CENTER | (h >= 34 * s ? DT_BOTTOM : DT_VCENTER) | DT_SINGLELINE | DT_END_ELLIPSIS, fS, C_TEXT);
            if (h >= 34 * s)
                Text(dc, mem, x1 + 3, y1 + h / 2 + (int)(2 * s), x2 - 3, y2, DT_CENTER | DT_TOP | DT_SINGLELINE, fS, Blend(col, C_TEXT, 0.6));
            DeleteObject(fS);
        }
    }
    *legendX = ax1;
    *legendY = ay2;
}

static void PaintMain(HDC dc, int W, int H) {
    double s = g_scale;
    SetBkMode(dc, TRANSPARENT);
    Fill(dc, 0, 0, W, H, C_BG);
    for (int i = 0; i < MAX_TILES; i++) SetRectEmpty(&g_tileRect[i]);
    SetRectEmpty(&g_freeRect);

    int statusH = (int)(38 * s), margin = (int)(18 * s), titleH = (int)(54 * s), legendH = (int)(62 * s);
    HFONT fTitle = MakeFont((int)(27 * s), FW_BOLD), fInfo = MakeFont((int)(13 * s), FW_NORMAL);
    HFONT fBtn = MakeFont((int)(13 * s), FW_SEMIBOLD);

    Text(dc, L"Memory Periodic Table", margin, margin, W - margin, margin + titleH,
         DT_LEFT | DT_TOP | DT_SINGLELINE, fTitle, C_TEXT);

    /* view switch buttons */
    static const WCHAR *BTN[2] = {L"Periodic table", L"Memory map"};
    COLORREF accent = RGB(0x4f, 0x8c, 0xff);
    int bh = (int)(30 * s), by = margin + (int)(2 * s), bx = W - margin;
    for (int k = 1; k >= 0; k--) {
        int bw = TextWidth(dc, BTN[k], fBtn) + (int)(30 * s);
        SetRect(&g_btnRect[k], bx - bw, by, bx, by + bh);
        int act = (k == g_view);
        Fill(dc, bx - bw, by, bx, by + bh, act ? Blend(C_BG, accent, 0.35) : C_PANEL);
        Frame(dc, bx - bw, by, bx, by + bh, act ? accent : C_LINE, 1);
        Text(dc, BTN[k], bx - bw, by, bx, by + bh, DT_CENTER | DT_VCENTER | DT_SINGLELINE, fBtn, act ? C_TEXT : C_MUTED);
        bx -= bw + (int)(6 * s);
    }
    WCHAR info[128];
    _snwprintf(info, 127, L"%d programs  \x2022  refreshes every %ds", g_ngroups, REFRESH_MS / 1000);
    Text(dc, info, margin, by, bx - (int)(10 * s), by + bh, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, fInfo, C_MUTED);

    int top = margin + titleH, bottom = H - statusH - legendH, lx = margin, ly = bottom;
    if (g_view == 0) PaintPeriodic(dc, margin, top, W - margin, bottom, &lx, &ly);
    else PaintMap(dc, margin, top, W - margin, bottom, &lx, &ly);

    /* legend with each group's share of RAM */
    {
        double total = (double)g_ms.ullTotalPhys, sums[G_COUNT] = {0};
        for (int i = 0; i < g_ntiles; i++) sums[TileGroup(&g_tiles[i])] += (double)g_tiles[i].rss;
        int y = ly + (int)(14 * s), x = lx, sq = (int)(15 * s);
        HFONT fPct = MakeFont((int)(13 * s), FW_BOLD);
        for (int k = 0; k < G_COUNT; k++) {
            Fill(dc, x, y, x + sq, y + sq, Blend(C_BG, GROUP_COLOR[k], 0.30));
            Frame(dc, x, y, x + sq, y + sq, GROUP_COLOR[k], 1);
            x += sq + (int)(7 * s);
            Text(dc, GROUP_LABEL[k], x, y - (int)(2 * s), W, y + sq + (int)(2 * s), DT_LEFT | DT_VCENTER | DT_SINGLELINE, fInfo, C_TEXT);
            x += TextWidth(dc, GROUP_LABEL[k], fInfo) + (int)(5 * s);
            WCHAR p[16];
            _snwprintf(p, 15, L"%.0f%%", total > 0 ? sums[k] * 100 / total : 0);
            Text(dc, p, x, y - (int)(2 * s), W, y + sq + (int)(2 * s), DT_LEFT | DT_VCENTER | DT_SINGLELINE, fPct, GROUP_COLOR[k]);
            x += TextWidth(dc, p, fPct) + (int)(20 * s);
        }
        HFONT fHint = MakeFont((int)(12 * s), FW_NORMAL);
        Text(dc, g_view == 0
                 ? L"Brighter tile = more memory   \x2022   Number = rank   \x2022   Top right = memory used   \x2022   % = share of all RAM"
                 : (g_showFree ? L"Each block's size is the memory that program uses   \x2022   Dark block = available RAM   \x2022   Press A to hide available RAM" : L"Each block's size is the memory that program uses   \x2022   Showing programs only   \x2022   Press A to show available RAM"),
             lx, y + sq + (int)(8 * s), W, y + sq + (int)(30 * s), DT_LEFT | DT_TOP | DT_SINGLELINE, fHint, C_MUTED);
        DeleteObject(fHint); DeleteObject(fPct);
    }

    /* status bar */
    HFONT fStatus = MakeFont((int)(14 * s), FW_NORMAL);
    Fill(dc, 0, H - statusH, W, H, C_PANEL);
    Text(dc, g_status, (int)(16 * s), H - statusH, W - (int)(16 * s), H,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, fStatus, C_TEXT);
    DeleteObject(fStatus); DeleteObject(fTitle); DeleteObject(fInfo); DeleteObject(fBtn);
}

static void SetView(int v) {
    g_view = v;
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MemoryPeriodicTable", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) == ERROR_SUCCESS) {
        DWORD d = (DWORD)v;
        RegSetValueExW(k, L"View", 0, REG_DWORD, (const BYTE *)&d, sizeof(d));
        RegCloseKey(k);
    }
}

static void LoadView(void) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MemoryPeriodicTable", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        DWORD d = 0, sz = sizeof(d), type = 0;
        if (RegQueryValueExW(k, L"View", NULL, &type, (BYTE *)&d, &sz) == ERROR_SUCCESS && type == REG_DWORD)
            g_view = d ? 1 : 0;
        RegCloseKey(k);
    }
}

/* ---------------------------------------------------------------- main window */

static void SetHover(int i) {
    if (i == g_hover) return;
    g_hover = i;
    if (i != FREE_ID && (i < 0 || i >= g_ntiles)) {
        g_hover = -1;
        wcscpy(g_status, HINT_TEXT);
        HideTip();
    } else {
        BuildTip(i);
        ShowTip();
    }
    InvalidateRect(g_main, NULL, FALSE);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        if (rc.right > 0 && rc.bottom > 0) {
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ old = SelectObject(mem, bmp);
            PaintMain(mem, rc.right, rc.bottom);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SIZE:
        if (w == SIZE_MINIMIZED) SetHover(-1);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = (int)(900 * g_scale);
        mm->ptMinTrackSize.y = (int)(600 * g_scale);
        return 0;
    }
    case WM_TIMER:
        Refresh();
        if (g_hover >= g_ntiles && g_hover != FREE_ID) SetHover(-1);
        if (g_hover >= 0) { BuildTip(g_hover); ShowTip(); }
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_MOUSEMOVE: {
        if (!g_tracking) {
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0};
            TrackMouseEvent(&t);
            g_tracking = 1;
        }
        POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        int hit = -1;
        for (int i = 0; i < g_ntiles; i++)
            if (PtInRect(&g_tileRect[i], pt)) { hit = i; break; }
        if (hit < 0 && PtInRect(&g_freeRect, pt)) hit = FREE_ID;
        if (hit != g_hover) SetHover(hit);
        else if (hit >= 0) MoveTip();
        return 0;
    }
    case WM_MOUSELEAVE: {
        g_tracking = 0;
        POINT pt;
        RECT rc;
        GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        GetClientRect(h, &rc);
        if (!PtInRect(&rc, pt)) SetHover(-1);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        for (int k = 0; k < 2; k++)
            if (PtInRect(&g_btnRect[k], pt) && k != g_view) {
                SetHover(-1);
                SetView(k);
                InvalidateRect(h, NULL, FALSE);
            }
        return 0;
    }
    case WM_KEYDOWN:
        if (w == 'A' && g_view == 1) {
            SetHover(-1);
            g_showFree = !g_showFree;
            InvalidateRect(h, NULL, FALSE);
        }
        if (w == 'M' || w == VK_TAB) {
            SetHover(-1);
            SetView(!g_view);
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(h, &pt);
            if (PtInRect(&g_btnRect[0], pt) || PtInRect(&g_btnRect[1], pt)) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_ACTIVATEAPP:
        if (!w) SetHover(-1);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show) {
    (void)prev; (void)cmd;
    SetProcessDPIAware();
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    g_ncpu = si.dwNumberOfProcessors ? (int)si.dwNumberOfProcessors : 1;
    HDC sdc = GetDC(NULL);
    g_scale = GetDeviceCaps(sdc, LOGPIXELSY) / 96.0;
    ReleaseDC(NULL, sdc);
    if (g_scale < 1.0) g_scale = 1.0;

    HICON icon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    if (!icon) icon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = icon;
    wc.hIconSm = icon;
    wc.lpszClassName = L"MemoryPeriodicTable";
    RegisterClassExW(&wc);

    WNDCLASSEXW tc = {sizeof(tc)};
    tc.lpfnWndProc = TipProc;
    tc.hInstance = inst;
    tc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    tc.lpszClassName = L"MemoryPeriodicTableTip";
    tc.style = CS_DROPSHADOW;
    RegisterClassExW(&tc);

    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int ww = (int)(1320 * g_scale), wh = (int)(860 * g_scale);
    int maxW = (wa.right - wa.left) * 92 / 100, maxH = (wa.bottom - wa.top) * 92 / 100;
    if (ww > maxW) ww = maxW;
    if (wh > maxH) wh = maxH;
    int wx = wa.left + ((wa.right - wa.left) - ww) / 2, wy = wa.top + ((wa.bottom - wa.top) - wh) / 2;

    LoadView();
    Refresh();
    g_main = CreateWindowExW(0, wc.lpszClassName, L"Memory Periodic Table", WS_OVERLAPPEDWINDOW,
                             wx, wy, ww, wh, NULL, NULL, inst, NULL);
    g_tipWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, tc.lpszClassName,
                               L"", WS_POPUP, 0, 0, 10, 10, g_main, NULL, inst, NULL);
    ShowWindow(g_main, show);
    UpdateWindow(g_main);
    SetTimer(g_main, 1, REFRESH_MS, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
