/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "SumatraPDF.h"
#include <malloc.h>
#include <wininet.h>

#include "AppPrefs.h"
#include "AppTools.h"
#include "CrashHandler.h"
#include "DebugLog.h"
#include "DirIter.h"
#include "Doc.h"
#include "EbookController.h"
#include "EbookWindow.h"
#include "ExternalPdfViewer.h"
#include "FileHistory.h"
#include "FileModifications.h"
#include "Favorites.h"
#include "FileUtil.h"
#include "FileWatcher.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "HttpUtil.h"
#include "HtmlWindow.h"
#include "IniParser.h"
#include "Menu.h"
#include "Mui.h"
#include "Notifications.h"
#include "ParseCommandLine.h"
#include "PdfEngine.h"
#include "PdfSync.h"
#include "Print.h"
#include "PsEngine.h"
#include "RenderCache.h"
#include "Resource.h"
#include "Search.h"
#include "Selection.h"
#include "Sidebar.h"
#include "SumatraAbout.h"
#include "SumatraAbout2.h"
#include "SumatraDialogs.h"
#include "SumatraProperties.h"
#include "SumatraWindow.h"
#include "StressTesting.h"
#include "Tab.h"
#include "TableOfContents.h"
#include "Timer.h"
#include "ThreadUtil.h"
#include "Toolbar.h"
#include "Touch.h"
#include "Translations.h"
#include "UITask.h"
#include "Version.h"
#include "WindowInfo.h"
#include "WinUtil.h"

/* if true, we're in debug mode where we show links as blue rectangle on
   the screen. Makes debugging code related to links easier. */
#ifdef DEBUG
bool             gDebugShowLinks = true;
#else
bool             gDebugShowLinks = false;
#endif

/* if true, we're rendering everything with the GDI+ back-end,
   otherwise Fitz/MuPDF is used at least for screen rendering.
   In Debug builds, you can switch between the two through the Debug menu */
bool             gUseGdiRenderer = false;

// in plugin mode, the window's frame isn't drawn and closing and
// fullscreen are disabled, so that SumatraPDF can be displayed
// embedded (e.g. in a web browser)
WCHAR *          gPluginURL = NULL; // owned by CommandLineInfo in WinMain

#define ABOUT_BG_LOGO_COLOR     RGB(0xFF, 0xFF, 0xD4)
#define ABOUT_BG_GRAY_COLOR     RGB(0xCC, 0xCC, 0xCC)

// Background color comparison:
// Adobe Reader X   0x565656 without any frame border
// Foxit Reader 5   0x9C9C9C with a pronounced frame shadow
// PDF-XChange      0xACA899 with a 1px frame and a gradient shadow
// Google Chrome    0xCCCCCC with a symmetric gradient shadow
// Evince           0xD7D1CB with a pronounced frame shadow
#ifdef DRAW_PAGE_SHADOWS
// SumatraPDF (old) 0xCCCCCC with a pronounced frame shadow
#define COL_WINDOW_BG           RGB(0xCC, 0xCC, 0xCC)
#define COL_PAGE_FRAME          RGB(0x88, 0x88, 0x88)
#define COL_PAGE_SHADOW         RGB(0x40, 0x40, 0x40)
#else
// SumatraPDF       0x999999 without any frame border
#define COL_WINDOW_BG           RGB(0x99, 0x99, 0x99)
#endif

#define PANEL_SPLITTER_BG_COLOR         RGB(0xF0, 0xF0, 0xF0)
#define PANEL_SPLITTER_EDGE_BG_COLOR    RGB(0x6B, 0x6B, 0x6B)
#define SIDEBAR_SPLITTER_BG_COLOR       RGB(0xF0, 0xF0, 0xF0)
#define SIDEBAR_SPLITTER_EDGE_BG_COLOR  RGB(0x6B, 0x6B, 0x6B)

#define SEPARATOR_BG_COLOR              RGB(0x6B, 0x6B, 0x6B)

#define CONTAINER_CLASS_NAME         L"SUMATRA_PDF_CONTAINER"
#define PANEL_CLASS_NAME             L"SUMATRA_PDF_PANEL"
#define CANVAS_CLASS_NAME            L"SUMATRA_PDF_CANVAS"
#define TOCBOX_CLASS_NAME            L"SUMATRA_PDF_TOCBOX"
#define FAVBOX_CLASS_NAME            L"SUMATRA_PDF_FAVBOX"
#define PANEL_SPLITTER_CLASS_NAME    L"PanelSplitter"
#define SIDEBAR_SPLITTER_CLASS_NAME  L"SidebarSplitter"
//#define FAV_SPLITTER_CLASS_NAME      L"FavSplitter"
#define RESTRICTIONS_FILE_NAME       L"sumatrapdfrestrict.ini"
#define CRASH_DUMP_FILE_NAME         L"sumatrapdfcrash.dmp"

#define DEFAULT_LINK_PROTOCOLS       L"http,https,mailto"
#define DEFAULT_FILE_PERCEIVED_TYPES L"audio,video"

#define SPLITTER_DX         7
#define SIDEBAR_MIN_WIDTH   150

// #define SPLITTER_DY         4
#define TOC_MIN_DY          100

// minimum size of the window
#define MIN_WIN_DX 480
#define MIN_WIN_DY 320

#define REPAINT_TIMER_ID            1
#define REPAINT_MESSAGE_DELAY_IN_MS 1000

#define HIDE_CURSOR_TIMER_ID        3
#define HIDE_CURSOR_DELAY_IN_MS     3000

#define AUTO_RELOAD_TIMER_ID        5
#define AUTO_RELOAD_DELAY_IN_MS     100

OSVERSIONINFOEX              gOS;

HINSTANCE                    ghinst = NULL;

HCURSOR                      gCursorArrow;
HCURSOR                      gCursorHand;
HCURSOR                      gCursorIBeam;
HBRUSH                       gBrushLogoBg;
HBRUSH                       gBrushAboutBg;
HBRUSH                       gBrushSepLineBg;
HBRUSH                       gBrushStaticBg;
HFONT                        gDefaultGuiFont;

// TODO: combine into Vec<SumatraWindow> (after 2.0) ?
Vec<TopWindowInfo*>          gWIN; // Record all top windows. (Ebook windows are not included.)
Vec<WindowInfo*>             gWindows;
Vec<EbookWindow*>            gEbookWindows;
FileHistory                  gFileHistory;
Favorites *                  gFavorites;

static HCURSOR                      gCursorDrag;
static HCURSOR                      gCursorScroll;
static HCURSOR                      gCursorSizeWE;
static HCURSOR                      gCursorSizeNS;
static HCURSOR                      gCursorNo;
static HBRUSH                       gBrushNoDocBg;

static HBRUSH                       gBrushPanelSplitterBg;
static HBRUSH                       gBrushPanelSplitterEdgeBg;
static HBRUSH                       gBrushSidebarSplitterBg;
static HBRUSH                       gBrushSidebarSplitterEdgeBg;

static HBITMAP                      gBitmapReloadingCue;
static RenderCache                  gRenderCache;
static bool                         gCrashOnOpen = false;

// in restricted mode, some features can be disabled (such as
// opening files, printing, following URLs), so that SumatraPDF
// can be used as a PDF reader on locked down systems
static int                          gPolicyRestrictions = Perm_All;
// only the listed protocols will be passed to the OS for
// opening in e.g. a browser or an email client (ignored,
// if gPolicyRestrictions doesn't contain Perm_DiskAccess)
static WStrVec                      gAllowedLinkProtocols;
// only files of the listed perceived types will be opened
// externally by LinkHandler::LaunchFile (i.e. when clicking
// on an in-document link); examples: "audio", "video", ...
static WStrVec                      gAllowedFileTypes;

// gFileExistenceChecker is initialized at startup and should
// terminate and delete itself asynchronously while the UI is
// being set up
class FileExistenceChecker;
static FileExistenceChecker *       gFileExistenceChecker = NULL;

static void UpdateUITextForLanguage();
static void UpdateToolbarAndScrollbarState(WindowInfo& win);
static void EnterFullscreen(WindowInfo& win, bool presentation=false);
static void ExitFullscreen(WindowInfo& win);
static LRESULT CALLBACK WndProcContainer(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WndProcPanel(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WndProcSidebar(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WndProcSidebarTop(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WndProcSidebarBottom(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); 

bool HasPermission(int permission)
{
    return (permission & gPolicyRestrictions) == permission;
}

static void SetCurrentLang(const char *langCode)
{
    if (langCode) {
        gGlobalPrefs.currLangCode = langCode;
        trans::SetCurrentLangByCode(langCode);
    }
}

#ifndef SUMATRA_UPDATE_INFO_URL
#ifdef SVN_PRE_RELEASE_VER
#define SUMATRA_UPDATE_INFO_URL L"http://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-prerelease-latest.txt"
#else
#define SUMATRA_UPDATE_INFO_URL L"http://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-latest.txt"
#endif
#endif

#ifndef SVN_UPDATE_LINK
#ifdef SVN_PRE_RELEASE_VER
#define SVN_UPDATE_LINK         L"http://blog.kowalczyk.info/software/sumatrapdf/prerelease.html"
#else
#define SVN_UPDATE_LINK         L"http://blog.kowalczyk.info/software/sumatrapdf/download-free-pdf-viewer.html"
#endif
#endif

#define SECS_IN_DAY 60*60*24

// lets the shell open a URI for any supported scheme in
// the appropriate application (web browser, mail client, etc.)
bool LaunchBrowser(const WCHAR *url)
{
    if (gPluginMode) {
        // pass the URI back to the browser
        CrashIf(gWindows.Count() == 0);
        if (gWindows.Count() == 0)
            return false;
        HWND plugin = gWindows.At(0)->hwndFrame;
        HWND parent = GetAncestor(plugin, GA_PARENT);
        ScopedMem<char> urlUtf8(str::conv::ToUtf8(url));
        if (!parent || !urlUtf8)
            return false;
        COPYDATASTRUCT cds = { 0x4C5255 /* URL */, str::Len(urlUtf8) + 1, urlUtf8.Get() };
        return SendMessage(parent, WM_COPYDATA, (WPARAM)plugin, (LPARAM)&cds);
    }

    if (!HasPermission(Perm_DiskAccess))
        return false;

    // check if this URL's protocol is allowed
    ScopedMem<WCHAR> protocol;
    if (!str::Parse(url, L"%S:", &protocol))
        return false;
    str::ToLower(protocol);
    if (!gAllowedLinkProtocols.Contains(protocol))
        return false;

    return LaunchFile(url, NULL, L"open");
}

// lets the shell open a file of any supported perceived type
// in the default application for opening such files
bool OpenFileExternally(const WCHAR *path)
{
    if (!HasPermission(Perm_DiskAccess) || gPluginMode)
        return false;

    // check if this file's perceived type is allowed
    const WCHAR *ext = path::GetExt(path);
    ScopedMem<WCHAR> perceivedType(ReadRegStr(HKEY_CLASSES_ROOT, ext, L"PerceivedType"));
    // since we allow following hyperlinks, also allow opening local webpages
    if (str::EndsWithI(path, L".htm") || str::EndsWithI(path, L".html") || str::EndsWithI(path, L".xhtml"))
        perceivedType.Set(str::Dup(L"webpage"));
    if (str::IsEmpty(perceivedType.Get()))
        return false;
    str::ToLower(perceivedType);
    if (!gAllowedFileTypes.Contains(perceivedType) && !gAllowedFileTypes.Contains(L"*"))
        return false;

    // TODO: only do this for trusted files (cf. IsUntrustedFile)?
    return LaunchFile(path);
}

#define DEFINE_GUID_STATIC(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
DEFINE_GUID_STATIC(CLSID_SendMail, 0x9E56BE60, 0xC50F, 0x11CF, 0x9A, 0x2C, 0x00, 0xA0, 0xC9, 0x0A, 0x90, 0xCE);

bool CanSendAsEmailAttachment(WindowInfo *win)
{
    // Requirements: a valid filename and access to SendMail's IDropTarget interface
    if (!CanViewExternally(win))
        return false;

    ScopedComPtr<IDropTarget> pDropTarget;
    return pDropTarget.Create(CLSID_SendMail);
}

static bool SendAsEmailAttachment(WindowInfo *win)
{
    if (!CanSendAsEmailAttachment(win))
        return false;

    // We use the SendTo drop target provided by SendMail.dll, which should ship with all
    // commonly used Windows versions, instead of MAPISendMail, which doesn't support
    // Unicode paths and might not be set up on systems not having Microsoft Outlook installed.
    ScopedComPtr<IDataObject> pDataObject(GetDataObjectForFile(win->loadedFilePath, win->hwndFrame));
    if (!pDataObject)
        return false;

    ScopedComPtr<IDropTarget> pDropTarget;
    if (!pDropTarget.Create(CLSID_SendMail))
        return false;

    POINTL pt = { 0, 0 };
    DWORD dwEffect = 0;
    pDropTarget->DragEnter(pDataObject, MK_LBUTTON, pt, &dwEffect);
    HRESULT hr = pDropTarget->Drop(pDataObject, MK_LBUTTON, pt, &dwEffect);
    return SUCCEEDED(hr);
}

inline void MoveWindow(HWND hwnd, RectI rect)
{
    MoveWindow(hwnd, rect.x, rect.y, rect.dx, rect.dy, TRUE);
}

void SwitchToDisplayMode(WindowInfo *win, DisplayMode displayMode, bool keepContinuous)
{
    if (!win->IsDocLoaded())
        return;

    if (keepContinuous && IsContinuous(win->dm->GetDisplayMode())) {
        switch (displayMode) {
            case DM_SINGLE_PAGE: displayMode = DM_CONTINUOUS; break;
            case DM_FACING: displayMode = DM_CONTINUOUS_FACING; break;
            case DM_BOOK_VIEW: displayMode = DM_CONTINUOUS_BOOK_VIEW; break;
        }
    }

    win->dm->ChangeDisplayMode(displayMode);
    UpdateToolbarState(win);
}

TopWindowInfo *FindTopWindowInfoByHwnd(HWND hwnd)
{
    HWND hwndFrame = GetAncestor(hwnd, GA_ROOTOWNER);

    for (size_t i = 0; i < gWIN.Count(); i++) {
        TopWindowInfo *WIN = gWIN.At(i);
        if (hwndFrame == WIN->hwndFrame)
            return WIN;
    }
    return NULL;
}

// In any case, only when hwnd is a child of a container can return non-NULL value.
ContainerInfo *FindContainerInfoByHwnd(HWND hwnd)
{
    TopWindowInfo *WIN = FindTopWindowInfoByHwnd(hwnd);

    if (WIN == NULL || WIN->gContainer.Count() == 0 || hwnd == WIN->hwndFrame)
        return NULL;

    HWND hwndParent = hwnd;
    WCHAR ClassName[MAX_PATH] = {0}; // Need to check if this is free later.
    GetClassName(hwndParent, ClassName, MAX_PATH);
    while(!str::Eq(ClassName, CONTAINER_CLASS_NAME)) { // Need to think about too long classname.
        hwndParent = GetParent(hwndParent);
        GetClassName(hwndParent, ClassName, MAX_PATH);
        if (str::Eq(ClassName, FRAME_CLASS_NAME))
            return NULL;
    }
    HWND hwndContainer = hwndParent;

    for (size_t i = 0; i < WIN->gContainer.Count(); i++) {
        ContainerInfo *Container = WIN->gContainer.At(i);
        if (hwndContainer == Container->hwndContainer)
            return Container;
    }
    return NULL;
}

PanelInfo *FindPanelInfoByHwnd(HWND hwnd)
{
    TopWindowInfo *WIN = FindTopWindowInfoByHwnd(hwnd);

    if (WIN == NULL)
        return NULL;

    if (WIN->gPanel.Count() == 0)
        return NULL;

    if (hwnd == WIN->hwndFrame)
        return WIN->panel;

    // Now WIN != NULL and it has panels;
    // If hwnd is a child of a container, container != NULL;
    ContainerInfo *container = FindContainerInfoByHwnd(hwnd); // NULL if hwnd == WIN->hwndFrame.

    // If container == NULL here, hwnd can only be a child in ReBar or Sidebar, and it is not in any container.
    if (container == NULL)
        return WIN->panel; // WIN != NULL implies that WIN->panel != NULL;

    // Now hwnd is in a container.

    // If hwnd is already one of the container, return panel;
    if (hwnd == container->hwndContainer)
        return WIN->panel;

    // Now hwnd is not any of hwndContainer, is in one of  the container.
    HWND hwndParent = hwnd;
    HWND hwndParentOld = NULL;

    while (hwndParent != container->hwndContainer) {
        hwndParentOld = hwndParent;
        hwndParent = GetParent(hwndParent);
    }
    HWND hwndPanel = hwndParentOld;

    for (size_t i = 0; i < WIN->gPanel.Count(); i++) {
        PanelInfo *panel = WIN->gPanel.At(i);
        if (hwndPanel == panel->hwndPanel)
            return panel;
    }
    return NULL;
}

WindowInfo *FindWindowInfoByHwnd(HWND hwnd)
{
    PanelInfo *panel = FindPanelInfoByHwnd(hwnd);

    if (panel == NULL)
        return NULL;

    if (panel->gWin.Count() == 0)
        return NULL;

    if (hwnd == panel->hwndPanel || hwnd == panel->win->hwndFrame)
        return panel->win;

    ContainerInfo *container = FindContainerInfoByHwnd(hwnd);

    if (container == NULL)
        return panel->win;

    if (hwnd == container->hwndContainer)
        return panel->win;

    HWND hwndParent = hwnd;
    HWND hwndParentOld = NULL;

    while (hwndParent != panel->hwndPanel) {
        hwndParentOld = hwndParent;
        hwndParent = GetParent(hwndParent);
    }
    HWND hwndAncestor = hwndParentOld;

    for ( size_t i = 0; i < panel->gWin.Count(); i++) {
        WindowInfo *win = panel->gWin.At(i);
        if (hwnd == win->hwndCanvas)
            return win;
    }

    for ( size_t i = 0; i < panel->gWin.Count(); i++) {
        WindowInfo *win = panel->gWin.At(i);
        if ((win->toolBar() && hwndAncestor == win->toolBar()->hwndReBar) ||
            (win->sideBar() && (hwndAncestor == win->sideBar()->hwndSidebar || hwndAncestor == win->sideBar()->hwndSidebarSplitter))
           )
            return panel->win;
    }
    return NULL;
}

bool WindowInfoStillValid(WindowInfo *win)
{
    return gWindows.Contains(win);
}

// Find the first window showing a given PDF file
WindowInfo* FindWindowInfoByFile(const WCHAR *file)
{
    ScopedMem<WCHAR> normFile(path::Normalize(file));

    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        if (!win->IsAboutWindow() && path::IsSame(win->loadedFilePath, normFile))
            return win;
    }

    return NULL;
}

// Find the first window that has been produced from <file>
WindowInfo* FindWindowInfoBySyncFile(const WCHAR *file)
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        Vec<RectI> rects;
        UINT page;
        if (win->pdfsync && win->pdfsync->SourceToDoc(file, 0, 0, &page, rects) != PDFSYNCERR_UNKNOWN_SOURCEFILE)
            return win;
    }
    return NULL;
}

class HwndPasswordUI : public PasswordUI
{
    HWND hwnd;

public:
    HwndPasswordUI(HWND hwnd) : hwnd(hwnd)
    {}

    virtual WCHAR * GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey);
};

/* Get password for a given 'fileName', can be NULL if user cancelled the
   dialog box or if the encryption key has been filled in instead.
   Caller needs to free() the result. */
WCHAR *HwndPasswordUI::GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                               unsigned char decryptionKeyOut[32], bool *saveKey)
{
    DisplayState *fileFromHistory = gFileHistory.Find(fileName);
    if (fileFromHistory && fileFromHistory->decryptionKey) {
        ScopedMem<char> fingerprint(str::MemToHex(fileDigest, 16));
        *saveKey = str::StartsWith(fileFromHistory->decryptionKey.Get(), fingerprint.Get());
        if (*saveKey && str::HexToMem(fileFromHistory->decryptionKey.Get() + 32, decryptionKeyOut, 32))
            return NULL;
    }

    *saveKey = false;

    if (IsStressTesting())
        return NULL;

    // extract the filename from the URL in plugin mode instead
    // of using the more confusing temporary filename
    ScopedMem<WCHAR> urlName;
    if (gPluginMode) {
        urlName.Set(ExtractFilenameFromURL(gPluginURL));
        if (urlName)
            fileName = urlName;
    }

    fileName = path::GetBaseName(fileName);
    // check if window is still validity as it might have been
    // closed by now
    if (!IsWindow(hwnd))
        hwnd = GetForegroundWindow();
    bool *rememberPwd =  gGlobalPrefs.rememberOpenedFiles ? saveKey : NULL;
    return Dialog_GetPassword(hwnd, fileName, rememberPwd);
}

// update global windowState for next default launch when either
// no pdf is opened or a document without window dimension information
static void RememberDefaultWindowPosition(WindowInfo& win)
{
    if (win.presentation)
        gGlobalPrefs.windowState = win.windowStateBeforePresentation;
    else if (win.fullScreen)
        gGlobalPrefs.windowState = WIN_STATE_FULLSCREEN;
    else if (IsZoomed(win.hwndFrame))
        gGlobalPrefs.windowState = WIN_STATE_MAXIMIZED;
    else if (!IsIconic(win.hwndFrame))
        gGlobalPrefs.windowState = WIN_STATE_NORMAL;

    gGlobalPrefs.sidebarDx = WindowRect(win.sideBar()->hwndTocBox).dx;

    /* don't update the window's dimensions if it is maximized, mimimized or fullscreened */
    if (WIN_STATE_NORMAL == gGlobalPrefs.windowState &&
        !IsIconic(win.hwndFrame) && !win.presentation) {
        // TODO: Use Get/SetWindowPlacement (otherwise we'd have to separately track
        //       the non-maximized dimensions for proper restoration)
        gGlobalPrefs.windowPos = WindowRect(win.hwndFrame);
    }
}

// update global windowState for next default launch when either
// no pdf is opened or a document without window dimension information
static void RememberDefaultWindowPosition(EbookWindow *win)
{
    if (IsZoomed(win->hwndFrame))
        gGlobalPrefs.windowState = WIN_STATE_MAXIMIZED;
    else if (!IsIconic(win->hwndFrame))
        gGlobalPrefs.windowState = WIN_STATE_NORMAL;

    // don't touch gGlobalPrefs.sidebarDx as it's only relevant to non-mobi windows

    /* don't update the window's dimensions if it is maximized, mimimized or fullscreened */
    if (WIN_STATE_NORMAL == gGlobalPrefs.windowState && !IsIconic(win->hwndFrame)) {
        // TODO: Use Get/SetWindowPlacement (otherwise we'd have to separately track
        //       the non-maximized dimensions for proper restoration)
        gGlobalPrefs.windowPos = WindowRect(win->hwndFrame);
    }
}

static void UpdateDisplayStateWindowRect(WindowInfo& win, DisplayState& ds, bool updateGlobal=true)
{
    if (updateGlobal)
        RememberDefaultWindowPosition(win);

    ds.windowState = gGlobalPrefs.windowState;
    ds.windowPos   = gGlobalPrefs.windowPos;
    ds.sidebarDx   = gGlobalPrefs.sidebarDx;
}

static void UpdateSidebarDisplayState(WindowInfo *win, DisplayState *ds)
{
    ds->tocVisible = win->tocVisible;

    if (win->tocLoaded) {
        win->tocState.Reset();
        HTREEITEM hRoot = TreeView_GetRoot(win->sideBar()->hwndTocTree);
        if (hRoot)
            UpdateTocExpansionState(win, hRoot);
    }

    delete ds->tocState;
    ds->tocState = NULL;
    if (win->tocState.Count() > 0)
        ds->tocState = new Vec<int>(win->tocState);
}

static void UpdateSidebarDisplayState(EbookWindow *win, DisplayState *ds)
{
    ds->tocVisible = false;
    delete ds->tocState;
    ds->tocState = NULL;
}

static void DisplayStateFromEbookWindow(EbookWindow* win, DisplayState* ds)
{
    if (!ds->filePath || !str::EqI(ds->filePath, win->LoadedFilePath()))
        ds->filePath.Set(str::Dup(win->LoadedFilePath()));

    // don't modify any of the other DisplayState values
    // as long as they're not used, so that the same
    // DisplayState settings can also be used for MobiEngine
    // (in debug builds and in case we ever want to allow to
    // switch between the interfaces); we get reasonable
    // defaults from DisplayState's constructor anyway
    ds->reparseIdx = win->ebookController->CurrPageReparseIdx();
}

static void UpdateCurrentFileDisplayStateForWinMobi(EbookWindow* win)
{
    RememberDefaultWindowPosition(win);
    DisplayState *ds = gFileHistory.Find(win->LoadedFilePath());
    if (!ds)
        return;
    DisplayStateFromEbookWindow(win, ds);
    ds->useGlobalValues = gGlobalPrefs.globalPrefsOnly;
    ds->windowState = gGlobalPrefs.windowState;
    ds->windowPos   = gGlobalPrefs.windowPos;
    UpdateSidebarDisplayState(win, ds);
}

static void UpdateCurrentFileDisplayStateForWinInfo(WindowInfo* win)
{
    RememberDefaultWindowPosition(*win);
    if (!win->IsDocLoaded())
        return;
    DisplayState *ds = gFileHistory.Find(win->loadedFilePath);
    if (!ds)
        return;
    win->dm->DisplayStateFromModel(ds);
    ds->useGlobalValues = gGlobalPrefs.globalPrefsOnly;
    UpdateDisplayStateWindowRect(*win, *ds, false);
    UpdateSidebarDisplayState(win, ds);
}

void UpdateCurrentFileDisplayStateForWin(SumatraWindow& win)
{
    if (win.AsWindowInfo())
        UpdateCurrentFileDisplayStateForWinInfo(win.AsWindowInfo());
    else if (win.AsEbookWindow())
        UpdateCurrentFileDisplayStateForWinMobi(win.AsEbookWindow());
}

bool IsUIRightToLeft()
{
    return trans::IsCurrLangRtl();
}

// updates the layout for a window to either left-to-right or right-to-left
// depending on the currently used language (cf. IsUIRightToLeft)
static void UpdateWindowRtlLayout(WindowInfo *win)
{
    ToolbarInfo *toolBar = win->toolBar();

    bool isRTL = IsUIRightToLeft();
    bool wasRTL = (GetWindowLong(win->hwndFrame, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
    if (wasRTL == isRTL)
        return;

    bool tocVisible = win->tocVisible;
    if (tocVisible || gGlobalPrefs.favVisible)
        SetSidebarVisibility(win, false, false);

    // cf. http://www.microsoft.com/middleeast/msdn/mirror.aspx
    ToggleWindowStyle(win->hwndFrame, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);

    ToggleWindowStyle(win->sideBar()->hwndTocBox, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    HWND tocBoxTitle = GetDlgItem(win->sideBar()->hwndTocBox, IDC_TOC_TITLE);
    ToggleWindowStyle(tocBoxTitle, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);

    ToggleWindowStyle(win->sideBar()->hwndFavBox, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    HWND favBoxTitle = GetDlgItem(win->sideBar()->hwndFavBox, IDC_FAV_TITLE);
    ToggleWindowStyle(favBoxTitle, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(win->sideBar()->hwndFavTree, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);

    ToggleWindowStyle(toolBar->hwndReBar, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(toolBar->hwndToolbar, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(toolBar->hwndFindBox, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(toolBar->hwndFindText, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    ToggleWindowStyle(toolBar->hwndPageText, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);

    win->notifications->Relayout();

    // TODO: also update the canvas scrollbars (?)

    // ensure that the ToC sidebar is on the correct side and that its
    // title and close button are also correctly laid out
    if (tocVisible || gGlobalPrefs.favVisible) {
        SetSidebarVisibility(win, tocVisible, gGlobalPrefs.favVisible);
        if (tocVisible)
            SendMessage(win->sideBar()->hwndTocBox, WM_SIZE, 0, 0);
        if (gGlobalPrefs.favVisible)
            SendMessage(win->sideBar()->hwndFavBox, WM_SIZE, 0, 0);
    }
}

void UpdateRtlLayoutForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        UpdateWindowRtlLayout(gWindows.At(i));
    }
}

static int GetPolicies(bool isRestricted)
{
    static struct {
        const char *name;
        int perm;
    } policies[] = {
        { "InternetAccess", Perm_InternetAccess },
        { "DiskAccess",     Perm_DiskAccess },
        { "SavePreferences",Perm_SavePreferences },
        { "RegistryAccess", Perm_RegistryAccess },
        { "PrinterAccess",  Perm_PrinterAccess },
        { "CopySelection",  Perm_CopySelection },
        { "FullscreenAccess",Perm_FullscreenAccess },
    };

    // allow to restrict SumatraPDF's functionality from an INI file in the
    // same directory as SumatraPDF.exe (cf. ../docs/sumatrapdfrestrict.ini)
    ScopedMem<WCHAR> restrictPath(GetExePath());
    if (restrictPath) {
        restrictPath.Set(path::GetDir(restrictPath));
        restrictPath.Set(path::Join(restrictPath, RESTRICTIONS_FILE_NAME));
    }
    gAllowedLinkProtocols.Reset();
    gAllowedFileTypes.Reset();
    if (file::Exists(restrictPath)) {
        int policy = Perm_RestrictedUse;

        IniFile ini(restrictPath);
        IniSection *polsec = ini.FindSection("Policies");
        IniLine *line;
        if (polsec) {
            for (size_t i = 0; i < dimof(policies); i++) {
                line = polsec->FindLine(policies[i].name);
                if (line && atoi(line->value) != 0)
                    policy = policy | policies[i].perm;
            }
        }
        // determine the list of allowed link protocols and perceived file types
        if ((policy & Perm_DiskAccess) && polsec) {
            if ((line = polsec->FindLine("LinkProtocols"))) {
                ScopedMem<WCHAR> protocols(str::conv::FromUtf8(line->value));
                str::ToLower(protocols);
                str::TransChars(protocols, L":; ", L",,,");
                gAllowedLinkProtocols.Split(protocols, L",", true);
            }
            if ((line = polsec->FindLine("SafeFileTypes"))) {
                ScopedMem<WCHAR> protocols(str::conv::FromUtf8(line->value));
                str::ToLower(protocols);
                str::TransChars(protocols, L":; ", L",,,");
                gAllowedFileTypes.Split(protocols, L",", true);
            }
        }

        return policy;
    }

    if (isRestricted)
        return Perm_RestrictedUse;

    gAllowedLinkProtocols.Split(DEFAULT_LINK_PROTOCOLS, L",");
    gAllowedFileTypes.Split(DEFAULT_FILE_PERCEIVED_TYPES, L",");
    return Perm_All;
}

void SaveThumbnailForFile(const WCHAR *filePath, RenderedBitmap *bmp)
{
    DisplayState *ds = gFileHistory.Find(filePath);
    if (!ds || !bmp) {
        delete bmp;
        return;
    }
    delete ds->thumbnail;
    ds->thumbnail = bmp;
    SaveThumbnail(*ds);
}

class ChmThumbnailTask : public UITask, public ChmNavigationCallback
{
    ChmEngine *engine;
    HWND hwnd;
    RenderedBitmap *bmp;

public:
    ChmThumbnailTask(ChmEngine *engine, HWND hwnd) :
        engine(engine), hwnd(hwnd), bmp(NULL) { }

    ~ChmThumbnailTask() {
        delete engine;
        DestroyWindow(hwnd);
        delete bmp;
    }

    virtual void Execute() {
        SaveThumbnailForFile(engine->FileName(), bmp);
        bmp = NULL;
    }

    virtual void PageNoChanged(int pageNo) {
        CrashIf(pageNo != 1);
        RectI area(0, 0, THUMBNAIL_DX * 2, THUMBNAIL_DY * 2);
        bmp = engine->TakeScreenshot(area, SizeI(THUMBNAIL_DX, THUMBNAIL_DY));
        uitask::Post(this);
    }

    virtual void LaunchBrowser(const WCHAR *url) { }
    virtual void FocusFrame(bool always) { }
};

// Create a thumbnail of chm document by loading it again and rendering
// its first page to a hwnd specially created for it.
static void CreateChmThumbnail(WindowInfo& win)
{
    ChmEngine *engine = ChmEngine::CreateFromFile(win.loadedFilePath);
    if (!engine)
        return;

    // We render twice the size of thumbnail and scale it down
    int winDx = THUMBNAIL_DX * 2 + GetSystemMetrics(SM_CXVSCROLL);
    int winDy = THUMBNAIL_DY * 2 + GetSystemMetrics(SM_CYHSCROLL);
    // reusing WC_STATIC. I don't think exact class matters (WndProc
    // will be taken over by HtmlWindow anyway) but it can't be NULL.
    HWND hwnd = CreateWindow(WC_STATIC, L"BrowserCapture", WS_POPUP,
                             0, 0, winDx, winDy, NULL, NULL, NULL, NULL);
    if (!hwnd) {
        delete engine;
        return;
    }
    bool ok = engine->SetParentHwnd(hwnd);
    if (!ok) {
        DestroyWindow(hwnd);
        delete engine;
        return;
    }

#if 0 // when debugging set to 1 to see the window
    ShowWindow(hwnd, SW_SHOW);
#endif

    // engine and window will be destroyed by the callback once it's invoked
    ChmThumbnailTask *callback = new ChmThumbnailTask(engine, hwnd);
    engine->SetNavigationCalback(callback);
    engine->DisplayPage(1);
}

class ThumbnailRenderingTask : public UITask, public RenderingCallback
{
    ScopedMem<WCHAR> filePath;
    RenderedBitmap *bmp;

public:
    ThumbnailRenderingTask(const WCHAR *filePath) :
        filePath(str::Dup(filePath)), bmp(NULL) {
    }

    ~ThumbnailRenderingTask() {
        delete bmp;
    }

    virtual void Callback(RenderedBitmap *bmp) {
        this->bmp = bmp;
        uitask::Post(this);
    }

    virtual void Execute() {
        SaveThumbnailForFile(filePath, bmp);
        bmp = NULL;
    }
};

bool ShouldSaveThumbnail(DisplayState& ds)
{
    // don't create thumbnails if we won't be needing them at all
    if (!HasPermission(Perm_SavePreferences))
        return false;

    // don't create thumbnails for files that won't need them anytime soon
    Vec<DisplayState *> list;
    gFileHistory.GetFrequencyOrder(list);
    int idx = list.Find(&ds);
    if (idx < 0 || FILE_HISTORY_MAX_FREQUENT * 2 <= idx)
        return false;

    if (HasThumbnail(ds))
        return false;
    return true;
}

static void CreateThumbnailForFile(WindowInfo& win, DisplayState& ds)
{
    if (!ShouldSaveThumbnail(ds))
        return;

    assert(win.IsDocLoaded() && win.dm->engine);
    if (!win.IsDocLoaded() || !win.dm->engine) return;

    // don't create thumbnails for password protected documents
    // (unless we're also remembering the decryption key anyway)
    if (win.dm->engine->IsPasswordProtected() &&
        !ScopedMem<char>(win.dm->engine->GetDecryptionKey())) {
        RemoveThumbnail(ds);
        return;
    }

    if (win.IsChm()) {
        CreateChmThumbnail(win);
        return;
    }

    RectD pageRect = win.dm->engine->PageMediabox(1);
    if (pageRect.IsEmpty())
        return;

    pageRect = win.dm->engine->Transform(pageRect, 1, 1.0f, 0);
    float zoom = THUMBNAIL_DX / (float)pageRect.dx;
    if (pageRect.dy > (float)THUMBNAIL_DY / zoom)
        pageRect.dy = (float)THUMBNAIL_DY / zoom;
    pageRect = win.dm->engine->Transform(pageRect, 1, 1.0f, 0, true);

    RenderingCallback *callback = new ThumbnailRenderingTask(win.loadedFilePath);
    gRenderCache.Render(win.dm, 1, 0, zoom, pageRect, *callback);
}

static void RebuildMenuBarForWindow(WindowInfo *win)
{
    HMENU oldMenu = win->menu();
    win->panel->WIN->menu = BuildMenu(win);
    if (!win->presentation && !win->fullScreen)
        SetMenu(win->hwndFrame, win->menu());
    DestroyMenu(oldMenu);
}

static void RebuildMenuBarForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        RebuildMenuBarForWindow(gWindows.At(i));
    }
    RebuildMenuBarForEbookWindows();
}

// When displaying CHM document we subclass hwndCanvas. UnsubclassCanvas() reverts that.
static void UnsubclassCanvas(HWND hwnd)
{
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcCanvas);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)0);
}

void SetWinTitle(WindowInfo *win)
{
    // Notice that we have to initialize win->title to NULL in WindowInfo.cpp.
    // Otherwise a bug.
    if (!win->title == NULL)
        free(win->title);

    if (win->IsDocLoaded()) {
        WCHAR title[MAX_PATH] = {'\0'};
        GetWindowText(win->panel->WIN->hwndFrame, title, MAX_PATH);
        // We need to free win->title when we do not need it anymore, since it is something global (created by new).
        // This is why we need the check above.
        win->title = str::Format(L"%s", title);
    } else
        win->title = str::Format(L"%s", APP_NAME_STR);
}

// isNewWindow : if true then 'win' refers to a newly created window that needs
//   to be resized and placed
// allowFailure : if false then keep displaying the previously loaded document
//   if the new one is broken
// placeWindow : if true then the Window will be moved/sized according
//   to the 'state' information even if the window was already placed
//   before (isNewWindow=false)
static bool LoadDocIntoWindow(LoadArgs& args, PasswordUI *pwdUI,
    DisplayState *state, bool isNewWindow, bool allowFailure, bool placeWindow)
{
    ScopedMem<WCHAR> title;
    WindowInfo *win = args.win;

    // TODO: remove time logging before release
    Timer t(true);
    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if placeWindow == true)
    if (placeWindow && (gGlobalPrefs.globalPrefsOnly || state && state->useGlobalValues)) {
        state = NULL;
    } else if (NULL == state) {
        state = gFileHistory.Find(args.fileName);
        if (state) {
            if (state->windowPos.IsEmpty())
                state->windowPos = gGlobalPrefs.windowPos;
            EnsureAreaVisibility(state->windowPos);
        }
    }
    DisplayMode displayMode = gGlobalPrefs.defaultDisplayMode;
    int startPage = 1;
    ScrollState ss(1, -1, -1);
    bool showAsFullScreen = WIN_STATE_FULLSCREEN == gGlobalPrefs.windowState;
    int showType = SW_NORMAL;
    if (gGlobalPrefs.windowState == WIN_STATE_MAXIMIZED || showAsFullScreen)
        showType = SW_MAXIMIZE;

    bool tocVisible = gGlobalPrefs.tocVisible;
    if (state) {
        startPage = state->pageNo;
        displayMode = state->displayMode;
        showAsFullScreen = WIN_STATE_FULLSCREEN == state->windowState;
        if (state->windowState == WIN_STATE_NORMAL)
            showType = SW_NORMAL;
        else if (state->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen)
            showType = SW_MAXIMIZE;
        else if (state->windowState == WIN_STATE_MINIMIZED)
            showType = SW_MINIMIZE;
        tocVisible = state->tocVisible;
    }

    DisplayModel *prevModel = win->dm;
    AbortFinding(args.win);
    delete win->pdfsync;
    win->pdfsync = NULL;

    str::ReplacePtr(&win->loadedFilePath, args.fileName);
    DocType engineType;
    BaseEngine *engine = EngineManager::CreateEngine(args.fileName, pwdUI, &engineType, !gUserPrefs.traditionalEbookUI);

    if (engine && Engine_Chm == engineType) {
        // make sure that MSHTML can't be used as a potential exploit
        // vector through another browser and our plugin (which doesn't
        // advertise itself for Chm documents but could be tricked into
        // loading one nonetheless)
        if (gPluginMode && IsUntrustedFile(args.fileName, gPluginURL)) {
            delete engine;
            engine = NULL;
            engineType = Engine_None;
        }
        // if CLSID_WebBrowser isn't available, fall back on Chm2Engine
        else if (!static_cast<ChmEngine *>(engine)->SetParentHwnd(win->hwndCanvas)) {
            delete engine;
            DebugAlternateChmEngine(true);
            engine = EngineManager::CreateEngine(args.fileName, pwdUI, &engineType);
            DebugAlternateChmEngine(false);
            CrashIf(engineType != (engine ? Engine_Chm2 : Engine_None));
        }
    }

    if (engine)
        win->dm = new DisplayModel(engine, engineType, win);
    else
        win->dm = NULL;

    bool needRefresh = !win->dm;

    // ToC items might hold a reference to an Engine, so make sure to
    // delete them before destroying the whole DisplayModel
    if (win->dm || allowFailure)
        ClearTocBox(win);

    assert(!win->IsAboutWindow() && win->IsDocLoaded() == (win->dm != NULL));
    /* see http://code.google.com/p/sumatrapdf/issues/detail?id=1570
    if (!win.dm) {
        // TODO: this should be "Error opening %s". Change after 1.7 is released
        ScopedMem<WCHAR> msg(str::Format(_TR("Error loading %s"), win.loadedFilePath));
        ShowNotification(&win, msg, true, false, NG_RESPONSE_TO_ACTION);
        // TODO: CloseWindow() does slightly more than this
        //       (also, some code presumes that there is at least one window with
        //        IsAboutWindow() == true and that that window is gWindows.At(0))
        str::ReplacePtr(&win.loadedFilePath, NULL);
    }
    */
    if (win->dm) {
        win->dm->SetInitialViewSettings(displayMode, startPage, win->GetViewPortSize(), win->panel->WIN->dpi);
        if (engineType == Engine_ComicBook)
            win->dm->SetDisplayR2L(gGlobalPrefs.cbxR2L);
        if (prevModel && str::Eq(win->dm->FilePath(), prevModel->FilePath())) {
            gRenderCache.KeepForDisplayModel(prevModel, win->dm);
            win->dm->CopyNavHistory(*prevModel);
        }
        delete prevModel;
    } else if (allowFailure) {
        delete prevModel;
        ScopedMem<WCHAR> title2(str::Format(L"%s - %s", path::GetBaseName(args.fileName), SUMATRA_WINDOW_TITLE));
        win::SetText(win->hwndFrame, title2);
        goto Error;
    } else {
        // if there is an error while reading the document and a repair is not requested
        // then fallback to the previous state
        win->dm = prevModel;
    }

    float zoomVirtual = gGlobalPrefs.defaultZoom;
    int rotation = 0;

    if (state) {
        if (win->dm->ValidPageNo(startPage)) {
            ss.page = startPage;
            if (ZOOM_FIT_CONTENT != state->zoomVirtual) {
                ss.x = state->scrollPos.x;
                ss.y = state->scrollPos.y;
            }
            // else let win.dm->Relayout() scroll to fit the page (again)
        } else if (startPage > win->dm->PageCount()) {
            ss.page = win->dm->PageCount();
        }
        zoomVirtual = state->zoomVirtual;
        rotation = state->rotation;

        win->tocState.Reset();
        if (state->tocState)
            win->tocState = *state->tocState;
    }

    win->dm->Relayout(zoomVirtual, rotation);

    if (!isNewWindow) {
        win->RedrawAll();
        OnMenuFindMatchCase(win);
    }
    UpdateFindbox(win);

    // menu for chm docs is different, so we have to re-create it
    RebuildMenuBarForWindow(win);

    int pageCount = win->dm->PageCount();
    if (pageCount > 0) {
        UpdateToolbarPageText(win, pageCount);
        UpdateToolbarFindText(win);
    }

    const WCHAR *baseName = path::GetBaseName(win->dm->FilePath());
    WCHAR *docTitle = win->dm->engine ? win->dm->engine->GetProperty(Prop_Title) : NULL;
    if (docTitle) {
        str::NormalizeWS(docTitle);
        if (!str::IsEmpty(docTitle)) {
            ScopedMem<WCHAR> docTitleBit(str::Format(L"- [%s] ", docTitle));
            free(docTitle);
            docTitle = docTitleBit.StealData();
        }
    }
    title.Set(str::Format(L"%s %s- %s", baseName, docTitle ? docTitle : L"", SUMATRA_WINDOW_TITLE));
    if (IsUIRightToLeft()) {
        // explicitly revert the title, so that filenames aren't garbled
        title.Set(str::Format(L"%s %s- %s", SUMATRA_WINDOW_TITLE, docTitle ? docTitle : L"", baseName));
    }
    free(docTitle);
    if (needRefresh)
        title.Set(str::Format(_TR("[Changes detected; refreshing] %s"), title));
    win::SetText(win->hwndFrame, title);

    // This just set win->title to be title; // We don't have to reset the window's title.
    SetWinTitle(win);

    if (HasPermission(Perm_DiskAccess) && Engine_PDF == win->dm->engineType) {
        int res = Synchronizer::Create(args.fileName,
            static_cast<PdfEngine *>(win->dm->engine), &win->pdfsync);
        // expose SyncTeX in the UI
        if (PDFSYNCERR_SUCCESS == res)
            gGlobalPrefs.enableTeXEnhancements = true;
    }

Error:
    if (isNewWindow || placeWindow && state) {
        if (isNewWindow && state && !state->windowPos.IsEmpty()) {
            // Make sure it doesn't have a position like outside of the screen etc.
            RectI rect = ShiftRectToWorkArea(state->windowPos);
            // This shouldn't happen until !win.IsAboutWindow(), so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win->hwndFrame, rect);
        }

        if (args.showWin)
            ShowWindow(win->hwndFrame, showType);

        UpdateWindow(win->hwndFrame);
    }
    if (win->IsDocLoaded()) {
        bool enable = !win->dm->engine || !win->dm->engine->HasPageLabels();
        ToggleWindowStyle(win->toolBar()->hwndPageBox, ES_NUMBER, enable);
        // if the window isn't shown and win.canvasRc is still empty, zoom
        // has not been determined yet
        assert(!args.showWin || !win->canvasRc.IsEmpty() || win->IsChm());
        if (args.showWin || ss.page != 1)
            win->dm->SetScrollState(ss);
        UpdateToolbarState(win);
        // Note: this is a hack. Somewhere between r4593 and r4629
        // restoring zoom for chm files from history regressed and
        // I'm too lazy to figure out where and why. This forces
        // setting zoom level after a page has been displayed
        // (indirectly triggered via UpdateToolbarState()).
        if (win->IsChm())
            win->dm->Relayout(zoomVirtual, rotation);
    }

    SetSidebarVisibility(win, tocVisible, gGlobalPrefs.favVisible);
    win->RedrawAll(true);

    UpdateToolbarAndScrollbarState(*win);
    if (!win->IsDocLoaded()) {
        win->RedrawAll();
        return false;
    }
    // This should only happen after everything else is ready
    if ((isNewWindow || placeWindow) && args.showWin && showAsFullScreen)
        EnterFullscreen(*win);
    if (!isNewWindow && win->presentation && win->dm)
        win->dm->SetPresentationMode(true);

    t.Stop();
    lf("LoadDocIntoWindow() time: %.2f", t.GetTimeInMs());

    return true;
}

void ReloadDocument(WindowInfo *win, bool autorefresh)
{
    DisplayState ds;
    ds.useGlobalValues = gGlobalPrefs.globalPrefsOnly;
    if (!win->IsDocLoaded()) {
        if (!autorefresh && win->loadedFilePath) {
            LoadArgs args(win->loadedFilePath, win);
            LoadDocument(args);
        }
        return;
    }
    win->dm->DisplayStateFromModel(&ds);
    UpdateDisplayStateWindowRect(*win, ds);
    UpdateSidebarDisplayState(win, &ds);
    // Set the windows state based on the actual window's placement
    ds.windowState =  win->fullScreen ? WIN_STATE_FULLSCREEN
                    : IsZoomed(win->hwndFrame) ? WIN_STATE_MAXIMIZED
                    : IsIconic(win->hwndFrame) ? WIN_STATE_MINIMIZED
                    : WIN_STATE_NORMAL ;

    // We don't allow PDF-repair if it is an autorefresh because
    // a refresh event can occur before the file is finished being written,
    // in which case the repair could fail. Instead, if the file is broken,
    // we postpone the reload until the next autorefresh event
    bool allowFailure = !autorefresh;
    bool isNewWindow = false;
    bool showWin = true;
    bool placeWindow = false;
    ScopedMem<WCHAR> path(str::Dup(win->loadedFilePath));
    HwndPasswordUI pwdUI(win->hwndFrame);
    LoadArgs args(path, win);
    args.showWin = showWin;
    if (!LoadDocIntoWindow(args, &pwdUI, &ds, isNewWindow, allowFailure, placeWindow))
        return;

    if (gGlobalPrefs.showStartPage) {
        // refresh the thumbnail for this file
        DisplayState *state = gFileHistory.Find(ds.filePath);
        if (state)
            CreateThumbnailForFile(*win, *state);
    }

    // save a newly remembered password into file history so that
    // we don't ask again at the next refresh
    ScopedMem<char> decryptionKey(win->dm->engine->GetDecryptionKey());
    if (decryptionKey) {
        DisplayState *state = gFileHistory.Find(ds.filePath);
        if (state && !str::Eq(state->decryptionKey, decryptionKey))
            state->decryptionKey.Set(decryptionKey.StealData());
    }
}

static void UpdateToolbarAndScrollbarState(WindowInfo& win)
{
    ToolbarUpdateStateForWindow(&win, true);
    if (win.IsDocLoaded())
        return;
    ShowScrollBar(win.hwndCanvas, SB_BOTH, FALSE);
    if (win.IsAboutWindow())
        win::SetText(win.hwndFrame, SUMATRA_WINDOW_TITLE);
}

static void CreateSidebar(WinInfo& winInfo, bool sidebarForEachPanel=false)
{
    if (winInfo.AsPanel() && !sidebarForEachPanel && (sidebarForEachPanel == gGlobalPrefs.sidebarForEachPanel))
        return;

    CreateSidebarBox(winInfo);

    winInfo.sideBar->hwndSidebarSplitter = CreateWindowEx(
        NULL,
        SIDEBAR_SPLITTER_CLASS_NAME, NULL,
        WS_CHILDWINDOW | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        winInfo.Hwnd(), NULL,
        ghinst, NULL);

    // For now temp.
    ShowWindow(winInfo.sideBar->hwndSidebarSplitter, SW_SHOW);
    UpdateWindow(winInfo.sideBar->hwndSidebarSplitter);
}

static WindowInfo* CreateWindowInfo()
{
    RectI windowPos = gGlobalPrefs.windowPos;
    if (!windowPos.IsEmpty())
        EnsureAreaVisibility(windowPos);
    else
        windowPos = GetDefaultWindowPos();

    HWND hwndFrame = CreateWindow(
        FRAME_CLASS_NAME, SUMATRA_WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
        NULL, NULL,
        ghinst, NULL);
    if (!hwndFrame)
        return NULL;

    HWND hwndContainer = CreateWindowEx(
        NULL,
        CONTAINER_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        hwndFrame, NULL,
        ghinst, NULL);
    if (!hwndContainer) {
        return NULL;
    }

    HWND hwndPanel = CreateWindowEx(
        NULL,
        PANEL_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        hwndContainer, NULL,
        ghinst, NULL);
    if (!hwndPanel) {
        return NULL;
    }

    HWND hwndCanvas = CreateWindowEx(
        NULL,
        CANVAS_CLASS_NAME, NULL,
        WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_CLIPSIBLINGS,
        0, 0, 0, 0, /* position and size determined in OnSize */
        hwndPanel, NULL,
        ghinst, NULL);
    if (!hwndCanvas) {
        return NULL;
    }

    assert(NULL == FindTopWindowInfoByHwnd(hwndFrame));

    TopWindowInfo *WIN = new TopWindowInfo(hwndFrame);
    // gWIN.Append(WIN); // We append this later so FrameOnSize is called only when everything is done.

    ContainerInfo *container = new ContainerInfo(hwndContainer);
    WIN->gContainer.Append(container);
    WIN->container = container;

    ////container->parentContainer = NULL;
    ////container->container1 = NULL;
    ////container->container2 = NULL;
    container->isAtTop = true;

    PanelInfo *panel = new PanelInfo(hwndPanel);
    WIN->gPanel.Append(panel);
    WIN->panel = panel;
    container->panel = panel;

    panel->container = container;

    WindowInfo *win = new WindowInfo(hwndCanvas);
    panel->gWin.Append(win);
    panel->WIN = WIN;
    panel->win = win;

    win->panel = panel;
    win->hwndFrame = hwndFrame;

    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);

    assert(!WIN->menu);
    WIN->menu = BuildMenu(win);
    SetMenu(WIN->hwndFrame, WIN->menu);

    // At this point, it will
    // call FrameOnSize, but now toolBar and sideBar are still NULL.
    // We append WIN to gWIN later to avoid resize problem.

    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    ShowWindow(panel->hwndPanel, SW_SHOW);
    UpdateWindow(panel->hwndPanel);

    ShowWindow(container->hwndContainer, SW_SHOW);
    UpdateWindow(container->hwndContainer);

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

    // One needs to use gGlobalPrefs.ToolbarForEachPanel to determine WIN or panel.
    // Here, it always creates a toolbar, either for WIN or for panel.
    CreateToolbar(WinInfo::Make(gGlobalPrefs.toolbarForEachPanel, WIN, panel));
    CreateSidebar(WinInfo::Make(gGlobalPrefs.sidebarForEachPanel, WIN, panel));

    CreateTabControl(panel);

    UpdateFindbox(win);
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        DragAcceptFiles(win->hwndCanvas, TRUE);

    gWIN.Append(WIN);

    gWindows.Append(win);
    UpdateWindowRtlLayout(win);

    if (Touch::SupportsGestures()) {
        GESTURECONFIG gc = { 0, GC_ALLGESTURES, 0 };
        Touch::SetGestureConfig(win->hwndCanvas, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    SetWinTitle(win); // We need to initialize win->title.

    AddTab(win);

    return win;
}

static WindowInfo* CreatePanel(ContainerInfo *container)
{
    assert(container->panel != NULL);

    TopWindowInfo *WIN = container->panel->WIN;

    HWND hwndContainer1 = CreateWindowEx(
        NULL,
        CONTAINER_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        container->hwndContainer, NULL,
        ghinst, NULL);
    if (!hwndContainer1) {
        return NULL;
    }

    HWND hwndContainer2 = CreateWindowEx(
        NULL,
        CONTAINER_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        container->hwndContainer, NULL,
        ghinst, NULL);
    if (!hwndContainer2) {
        return NULL;
    }

    SetParent(container->panel->hwndPanel, hwndContainer1);

    HWND hwndPanel = CreateWindowEx(
        NULL,
        PANEL_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0,
        hwndContainer2, NULL,
        ghinst, NULL);
    if (!hwndPanel) {
        return NULL;
    }

    HWND hwndCanvas = CreateWindowEx(
        NULL,
        CANVAS_CLASS_NAME, NULL,
        WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_CLIPSIBLINGS,
        0, 0, 0, 0, /* position and size determined in OnSize */
        hwndPanel, NULL,
        ghinst, NULL);
    if (!hwndCanvas) {
        return NULL;
    }

    container->hwndSplitter = CreateWindowEx(
        NULL,
        PANEL_SPLITTER_CLASS_NAME, NULL,
        WS_CHILDWINDOW | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        container->hwndContainer, NULL,
        ghinst, NULL);

    assert(NULL == FindContainerInfoByHwnd(hwndContainer1));
    assert(NULL == FindContainerInfoByHwnd(hwndContainer2));

    ContainerInfo *container1 = new ContainerInfo(hwndContainer1);
    ContainerInfo *container2 = new ContainerInfo(hwndContainer2);
    WIN->gContainer.Append(container1);
    WIN->gContainer.Append(container2);
    container->container1 = container1;
    container->container2 = container2;

    container1->parentContainer = container;
    container2->parentContainer = container; 

    PanelInfo *panel = new PanelInfo(hwndPanel);
    WIN->gPanel.Append(panel);
    WIN->panel = panel;
    container1->panel = container->panel;
    container2->panel = panel;
    container->panel = NULL;

    container1->panel->container = container1;
    panel->WIN = WIN;
    panel->container= container2;

    WindowInfo *win = new WindowInfo(hwndCanvas);
    panel->gWin.Append(win);
    panel->win = win;

    win->panel = panel;
    win->hwndFrame = WIN->hwndFrame;

    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);

    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    ShowWindow(panel->hwndPanel, SW_SHOW);
    UpdateWindow(panel->hwndPanel);

    ShowWindow(container1->hwndContainer, SW_SHOW);
    UpdateWindow(container1->hwndContainer);

    ShowWindow(container2->hwndContainer, SW_SHOW);
    UpdateWindow(container2->hwndContainer);

    ShowWindow(container2->hwndContainer, SW_SHOW);
    UpdateWindow(container2->hwndContainer);

    ShowWindow(container->hwndSplitter, SW_SHOW);
    UpdateWindow(container->hwndSplitter);

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);
    
    // Here it doesn't create any toolbar if gGlobalPrefs.toolbarForEachPanel is false.
    CreateToolbar(WinInfo::Make(panel), gGlobalPrefs.toolbarForEachPanel);
    CreateSidebar(WinInfo::Make(panel), gGlobalPrefs.sidebarForEachPanel);

    CreateTabControl(panel);
 
    UpdateFindbox(win);
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        DragAcceptFiles(win->hwndCanvas, TRUE);

    gWindows.Append(win);
    UpdateWindowRtlLayout(win);

    if (Touch::SupportsGestures()) {
        GESTURECONFIG gc = { 0, GC_ALLGESTURES, 0 };
        Touch::SetGestureConfig(win->hwndCanvas, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    SetWinTitle(win); // We need to initialize win->title.

    AddTab(win);

    return win;
}

static WindowInfo* CreateCanvas(PanelInfo *panel)
{
    HWND hwndCanvas = CreateWindowEx(
        NULL,
        CANVAS_CLASS_NAME, NULL,
        WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_CLIPSIBLINGS,
        0, 0, 0, 0, /* position and size determined in OnSize */
        panel->hwndPanel, NULL,
        ghinst, NULL);
    if (!hwndCanvas) {
        return NULL;
    }

    WindowInfo *winOld = panel->win;

    assert(NULL == FindWindowInfoByHwnd(hwndCanvas));

    WindowInfo *win = new WindowInfo(hwndCanvas);
    panel->gWin.Append(win);
    panel->win = win;

    win->panel = panel;
    win->hwndFrame = panel->WIN->hwndFrame;

    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(win->hwndCanvas, SB_BOTH, FALSE);

    int dx = ClientRect(panel->hwndPanel).dx;
    int dy = ClientRect(panel->hwndPanel).dy;

    ShowWindow(winOld->hwndCanvas, SW_HIDE);

    SendMessage(panel->hwndPanel, WM_SIZE, 0, MAKELPARAM(dx, dy));

    ShowWindow(win->hwndCanvas, SW_SHOW);
    // UpdateWindow(win->hwndCanvas);

    if (gGlobalPrefs.sidebarForEachPanel) {
        ShowWindow(win->sideBar()->hwndSidebar, SW_HIDE);
        ShowWindow(win->sideBar()->hwndSidebarSplitter, SW_HIDE);
    }

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

    // We need to delete winOld's TOC first, otherwise the new win's TOC
    // will be inserted after the old one's. See PopulateTocTreeView, AddTocItemToView and LoadTocTree.
    ClearTocBox(winOld);

    UpdateFindbox(win);
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        DragAcceptFiles(win->hwndCanvas, TRUE);

    gWindows.Append(win);
    UpdateWindowRtlLayout(win);

    if (Touch::SupportsGestures()) {
        GESTURECONFIG gc = { 0, GC_ALLGESTURES, 0 };
        Touch::SetGestureConfig(win->hwndCanvas, 0, 1, &gc, sizeof(GESTURECONFIG));
    }

    SetWinTitle(win); // We need to initialize win->title.

    AddTab(win);

    return win;
}

WindowInfo *CreateAndShowWindowInfo()
{
    bool enterFullscreen = (WIN_STATE_FULLSCREEN == gGlobalPrefs.windowState);
    WindowInfo *win = CreateWindowInfo();
    if (!win)
        return NULL;

    if (WIN_STATE_FULLSCREEN == gGlobalPrefs.windowState ||
        WIN_STATE_MAXIMIZED == gGlobalPrefs.windowState)
        ShowWindow(win->hwndFrame, SW_MAXIMIZE);
    else
        ShowWindow(win->hwndFrame, SW_SHOW);
    UpdateWindow(win->hwndFrame);

    if (enterFullscreen)
        EnterFullscreen(*win);
    return win;
}

static void DeleteWindowInfo(WindowInfo *win)
{
    Timer t(true);

    FileWatcherUnsubscribe(win->watcher);
    win->watcher = NULL;
    // Need to check this function.
    DeletePropertiesWindow(win->panel->WIN->hwndFrame);

    gWindows.Remove(win);
    // Even if win is removed from panel in CloseDocumentInWindow, it is still safe to remove again,
    // because Remove() don't remove if it can't find win.
    win->panel->gWin.Remove(win);

    DragAcceptFiles(win->hwndCanvas, FALSE);

    AbortFinding(win);
    AbortPrinting(win);

    delete win;

    t.Stop();
    lf("DeleteWindowInfo() time: %.2f", t.GetTimeInMs());
}

static void DeletePanelInfo(PanelInfo *panel)
{
    if (gGlobalPrefs.toolbarForEachPanel)
        ImageList_Destroy((HIMAGELIST)SendMessage(panel->toolBar->hwndToolbar, TB_GETIMAGELIST, 0, 0));    

    size_t CountWinInPanel = panel->gWin.Count();

    for (size_t i = 0; i < CountWinInPanel; i++) {
        WindowInfo *win = panel->gWin.At(0);
        panel->gWin.Remove(win);
        DeleteWindowInfo(win);
    }
    delete panel;
}

static void DeleteTopWindowInfo(TopWindowInfo *WIN)
{
    gWIN.Remove(WIN);

    if (!gGlobalPrefs.toolbarForEachPanel)
        ImageList_Destroy((HIMAGELIST)SendMessage(WIN->toolBar->hwndToolbar, TB_GETIMAGELIST, 0, 0));   

    size_t CountPanelInWIN = WIN->gPanel.Count();

    for (int i = 0; i < CountPanelInWIN; i++) {
        PanelInfo *panel = WIN->gPanel.At(0);
        WIN->gPanel.Remove(panel);
        DeletePanelInfo(panel);
    }
    delete WIN;    
}

class FileChangeCallback : public UITask, public FileChangeObserver
{
    WindowInfo *win;
public:
    FileChangeCallback(WindowInfo *win) : win(win) { }

    virtual void OnFileChanged() {
        // We cannot call win->Reload directly as it could cause race conditions
        // between the watching thread and the main thread (and only pass a copy of this
        // callback to uitask::Post, as the object will be deleted after use)
        uitask::Post(new FileChangeCallback(win));
    }

    virtual void Execute() {
        if (WindowInfoStillValid(win)) {
            // delay the reload slightly, in case we get another request immediately after this one
            SetTimer(win->hwndCanvas, AUTO_RELOAD_TIMER_ID, AUTO_RELOAD_DELAY_IN_MS, NULL);
        }
    }
};

static void RenameFileInHistory(const WCHAR *oldPath, const WCHAR *newPath)
{
    DisplayState *ds = gFileHistory.Find(newPath);
    bool oldIsPinned = false;
    int oldOpenCount = 0;
    if (ds) {
        oldIsPinned = ds->isPinned;
        oldOpenCount = ds->openCount;
        gFileHistory.Remove(ds);
        delete ds;
    }
    ds = gFileHistory.Find(oldPath);
    if (ds) {
        ds->filePath.Set(str::Dup(newPath));
        // merge Frequently Read data, so that a file
        // doesn't accidentally vanish from there
        ds->isPinned = ds->isPinned || oldIsPinned;
        ds->openCount += oldOpenCount;
        // the thumbnail is recreated by LoadDocument
        delete ds->thumbnail;
        ds->thumbnail = NULL;
    }

    FileFavs *oldFav = gFavorites->GetFavByFilePath(oldPath);
    if (oldFav) {
        // move all favorites of the old file over the the new one
        for (size_t i = 0; i < oldFav->favNames.Count(); i++) {
            FavName *fn = oldFav->favNames.At(i);
            gFavorites->AddOrReplace(newPath, fn->pageNo, fn->name);
        }
        gFavorites->RemoveAllForFile(oldPath);
    }
}

// document path is either a file or a directory
// (when browsing images inside directory).
static bool DocumentPathExists(const WCHAR *path)
{
    if (file::Exists(path) || dir::Exists(path))
        return true;
    if (str::FindChar(path + 2, ':')) {
        // remove information needed for pointing at embedded documents
        // (e.g. "C:\path\file.pdf:3:0") to check at least whether the
        // container document exists
        ScopedMem<WCHAR> realPath(str::DupN(path, str::FindChar(path + 2, ':') - path));
        return file::Exists(realPath);
    }
    return false;
}

void LoadDocument2(const WCHAR *fileName, SumatraWindow& win)
{
    // TODO: opening non-mobi files from mobi window doesn't work exactly
    // the same as opening them from non-mobi window
    if (win.AsWindowInfo()) {
        LoadArgs args(fileName, win.AsWindowInfo());
        LoadDocument(args);
        return;
    }
    CrashIf(!win.AsEbookWindow());
    // TODO: LoadDocument() needs to handle EbookWindow, for now
    // we force opening in a new window
    LoadArgs args(fileName, NULL);
    LoadDocument(args);
}

// Load a file into a new or existing window, show error message
// if loading failed, set the right window position (based on history
// settings for this file or default position), update file history,
// update frequently read information, generate a thumbnail if necessary
// TODO: write me
static WindowInfo* LoadDocumentNew(LoadArgs& args)
{
    ScopedMem<WCHAR> fullPath(path::Normalize(args.fileName));
    // TODO: try to find file on other drives if doesn't exist

    CrashIf(true);
    return NULL;
}

// TODO: eventually I would like to move all loading to be async. To achieve that
// we need clear separatation of loading process into 2 phases: loading the
// file (and showing progress/load failures in topmost window) and placing
// the loaded document in the window (either by replacing document in existing
// window or creating a new window for the document)
// TODO: loading a document should never be slow enough to require async loading
static WindowInfo* LoadDocumentOld(LoadArgs& args)
{
    if (gCrashOnOpen)
        CrashMe();

    ScopedMem<WCHAR> fullPath(path::Normalize(args.fileName));
    WindowInfo *win = args.win;

    bool failEarly = win && !args.forceReuse && !DocumentPathExists(fullPath);
    // try to find inexistent files with history data
    // on a different removable drive before failing
    if (failEarly && gFileHistory.Find(fullPath)) {
        ScopedMem<WCHAR> adjPath(str::Dup(fullPath));
        if (AdjustVariableDriveLetter(adjPath)) {
            RenameFileInHistory(fullPath, adjPath);
            fullPath.Set(adjPath.StealData());
            failEarly = false;
        }
    }

    // fail with a notification if the file doesn't exist and
    // there is a window the user has just been interacting with
    if (failEarly) {
        ScopedMem<WCHAR> msg(str::Format(_TR("File %s not found"), fullPath));
        ShowNotification(win, msg, true /* autoDismiss */, true /* highlight */);
        // display the notification ASAP (SavePrefs() can introduce a notable delay)
        win->RedrawAll(true);

        if (gFileHistory.MarkFileInexistent(fullPath)) {
            SavePrefs();
            // update the Frequently Read list
            if (1 == gWindows.Count() && gWindows.At(0)->IsAboutWindow())
                gWindows.At(0)->RedrawAll(true);
        }
        return NULL;
    }

    if (!gUserPrefs.traditionalEbookUI && IsEbookFile(fullPath)) {
        if (!win) {
            if ((1 == gWindows.Count()) && gWindows.At(0)->IsAboutWindow())
                win = gWindows.At(0);
        } else if (!win->IsAboutWindow() && !args.forceReuse)
            win = NULL;
        if (!win) {
            // create a dummy window so that we can return
            // a non-NULL value to indicate loading success
            win = CreateWindowInfo();
        }
        if (win->IsAboutWindow()) {
            // don't crash if multiple ebook files are opened at once
            // (e.g. via dragging on the window)
            // TODO: figure out a better way to handle this
            win->loadedFilePath = str::Dup(fullPath);
        }
        LoadEbookAsync(fullPath, SumatraWindow::Make(win));
        // TODO: we should show a notification in the window user is looking at
        return win;
    }

    bool isNewWindow = false;
    if (!win && 1 == gWindows.Count() && gWindows.At(0)->IsAboutWindow()) {
        win = gWindows.At(0);
        args.win = win;
    } else if (!win || win->IsDocLoaded() && !args.forceReuse) {
        WindowInfo *currWin = win;
        win = CreateWindowInfo();
        if (!win)
            return NULL;
        args.win = win;
        isNewWindow = true;
        if (currWin) {
            RememberFavTreeExpansionState(currWin);
            win->expandedFavorites = currWin->expandedFavorites;
        }
    }

    DeleteOldSelectionInfo(win, true);
    win->fwdSearchMark.show = false;
    win->notifications->RemoveAllInGroup(NG_RESPONSE_TO_ACTION);
    win->notifications->RemoveAllInGroup(NG_PAGE_INFO_HELPER);

    HwndPasswordUI pwdUI(win->hwndFrame);
    args.fileName = fullPath;
    bool loaded = LoadDocIntoWindow(args, &pwdUI, NULL, isNewWindow,
        true /* allowFailure */, true /* placeWindow */);

    if (gPluginMode) {
        // hide the menu for embedded documents opened from the plugin
        SetMenu(win->hwndFrame, NULL);
        return win;
    }

    if (!loaded) {
        if (gFileHistory.MarkFileInexistent(fullPath))
            SavePrefs();
        return win;
    }

    SetTabTitle(win);
    SetTabToolTipText(win);

    FileWatcherUnsubscribe(win->watcher);
    win->watcher = FileWatcherSubscribe(fullPath, new FileChangeCallback(win));

    if (IsStressTesting()) {
        // don't modify file history during stress testing
        return win;
    }

    if (gGlobalPrefs.rememberOpenedFiles) {
        CrashIf(!str::Eq(fullPath, win->loadedFilePath));
        DisplayState *ds = gFileHistory.MarkFileLoaded(fullPath);
        if (gGlobalPrefs.showStartPage)
            CreateThumbnailForFile(*win, *ds);
        SavePrefs();
    }

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    if (HasPermission(Perm_DiskAccess) && !gPluginMode)
        SHAddToRecentDocs(SHARD_PATH, fullPath);

    return win;
}

WindowInfo* LoadDocument(LoadArgs& args)
{
#if 0
    return LoadDocumentNew(args);
#else
    return LoadDocumentOld(args);
#endif
}

// The current page edit box is updated with the current page number
void WindowInfo::PageNoChanged(int pageNo)
{
    assert(dm && dm->PageCount() > 0);
    if (!dm || dm->PageCount() == 0)
        return;

    if (INVALID_PAGE_NO != pageNo) {
        ScopedMem<WCHAR> buf(dm->engine->GetPageLabel(pageNo));
        win::SetText(toolBar()->hwndPageBox, buf);
        ToolbarUpdateStateForWindow(this, false);
        if (dm->engine && dm->engine->HasPageLabels())
            UpdateToolbarPageText(this, dm->PageCount(), true);
    }
    if (pageNo == currPageNo)
        return;

    UpdateTocSelection(this, pageNo);
    currPageNo = pageNo;

    NotificationWnd *wnd = notifications->GetFirstInGroup(NG_PAGE_INFO_HELPER);
    if (NULL == wnd)
        return;

    ScopedMem<WCHAR> pageInfo(str::Format(L"%s %d / %d", _TR("Page:"), pageNo, dm->PageCount()));
    if (dm->engine && dm->engine->HasPageLabels()) {
        ScopedMem<WCHAR> label(dm->engine->GetPageLabel(pageNo));
        pageInfo.Set(str::Format(L"%s %s (%d / %d)", _TR("Page:"), label, pageNo, dm->PageCount()));
    }
    wnd->UpdateMessage(pageInfo);
}

bool DoCachePageRendering(WindowInfo *win, int pageNo)
{
    assert(win->dm && win->dm->engine);
    if (!win->dm || !win->dm->engine || !win->dm->engine->IsImageCollection())
        return true;

    // cache large images (mainly photos), as shrinking them
    // for every UI update (WM_PAINT) can cause notable lags
    // TODO: stretching small images also causes minor lags
    RectD page = win->dm->engine->PageMediabox(pageNo);
    return page.dx * page.dy > 1024 * 1024;
}

/* Send the request to render a given page to a rendering thread */
void WindowInfo::RequestRendering(int pageNo)
{
    assert(dm);
    if (!dm) return;
    // don't render any plain images on the rendering thread,
    // they'll be rendered directly in DrawDocument during
    // WM_PAINT on the UI thread
    if (!DoCachePageRendering(this, pageNo))
        return;

    gRenderCache.RequestRendering(dm, pageNo);
}

void WindowInfo::CleanUp(DisplayModel *dm)
{
    assert(dm);
    if (!dm)
        return;

    gRenderCache.CancelRendering(dm);
    gRenderCache.FreeForDisplayModel(dm);
}

static void UpdateCanvasScrollbars(DisplayModel *dm, HWND hwndCanvas, SizeI canvas)
{
    SCROLLINFO si = { 0 };
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;

    SizeI viewPort = dm->viewPort.Size();

    if (viewPort.dx >= canvas.dx) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = dm->viewPort.x;
        si.nMin = 0;
        si.nMax = canvas.dx - 1;
        si.nPage = viewPort.dx;
    }
    ShowScrollBar(hwndCanvas, SB_HORZ, viewPort.dx < canvas.dx);
    SetScrollInfo(hwndCanvas, SB_HORZ, &si, TRUE);

    if (viewPort.dy >= canvas.dy) {
        si.nPos = 0;
        si.nMin = 0;
        si.nMax = 99;
        si.nPage = 100;
    } else {
        si.nPos = dm->viewPort.y;
        si.nMin = 0;
        si.nMax = canvas.dy - 1;
        si.nPage = viewPort.dy;

        if (ZOOM_FIT_PAGE != dm->ZoomVirtual()) {
            // keep the top/bottom 5% of the previous page visible after paging down/up
            si.nPage = (UINT)(si.nPage * 0.95);
            si.nMax -= viewPort.dy - si.nPage;
        }
    }
    ShowScrollBar(hwndCanvas, SB_VERT, viewPort.dy < canvas.dy);
    SetScrollInfo(hwndCanvas, SB_VERT, &si, TRUE);
}

void WindowInfo::UpdateScrollbars(SizeI canvas)
{
    UpdateCanvasScrollbars(dm, hwndCanvas, canvas);
}

void AssociateExeWithPdfExtension()
{
    if (!HasPermission(Perm_RegistryAccess)) return;

    DoAssociateExeWithPdfExtension(HKEY_CURRENT_USER);
    DoAssociateExeWithPdfExtension(HKEY_LOCAL_MACHINE);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, 0, 0);

    // Remind the user, when a different application takes over
    gGlobalPrefs.pdfAssociateShouldAssociate = true;
    gGlobalPrefs.pdfAssociateDontAskAgain = false;
}

// Registering happens either through the Installer or the Options dialog;
// here we just make sure that we're still registered
static bool RegisterForPdfExtentions(HWND hwnd)
{
    if (IsRunningInPortableMode() || !HasPermission(Perm_RegistryAccess) || gPluginMode)
        return false;

    if (IsExeAssociatedWithPdfExtension())
        return true;

    /* Ask user for permission, unless he previously said he doesn't want to
       see this dialog */
    if (!gGlobalPrefs.pdfAssociateDontAskAgain) {
        INT_PTR result = Dialog_PdfAssociate(hwnd, &gGlobalPrefs.pdfAssociateDontAskAgain);
        assert(IDYES == result || IDNO == result);
        gGlobalPrefs.pdfAssociateShouldAssociate = (IDYES == result);
    }
    if (!gGlobalPrefs.pdfAssociateShouldAssociate)
        return false;

    AssociateExeWithPdfExtension();
    return true;
}

void OnDropFiles(HDROP hDrop, bool dragFinish)
{
    WCHAR       filePath[MAX_PATH];
    const int   count = DragQueryFile(hDrop, DRAGQUERY_NUMFILES, 0, 0);

    for (int i = 0; i < count; i++) {
        DragQueryFile(hDrop, i, filePath, dimof(filePath));
        if (str::EndsWithI(filePath, L".lnk")) {
            ScopedMem<WCHAR> resolved(ResolveLnk(filePath));
            if (resolved)
                str::BufSet(filePath, dimof(filePath), resolved);
        }
        // The first dropped document may override the current window
        LoadArgs args(filePath);
        LoadDocument(args);
    }
    if (dragFinish)
        DragFinish(hDrop);
}

static void MessageBoxWarning(HWND hwnd, const WCHAR *msg, const WCHAR *title = NULL)
{
    UINT type =  MB_OK | MB_ICONEXCLAMATION | (IsUIRightToLeft() ? MB_RTLREADING : 0);
    if (!title)
        title = _TR("Warning");
    MessageBox(hwnd, msg, title, type);
}

static DWORD ShowAutoUpdateDialog(HWND hParent, HttpReq *ctx, bool silent)
{
    if (ctx->error)
        return ctx->error;
    if (!str::StartsWith(ctx->url, SUMATRA_UPDATE_INFO_URL))
        return ERROR_INTERNET_INVALID_URL;

    // See http://code.google.com/p/sumatrapdf/issues/detail?id=725
    // If a user configures os-wide proxy that is not regular ie proxy
    // (which we pick up) we might get complete garbage in response to
    // our query and it might accidentally contain a number bigger than
    // our version number which will make us ask to upgrade every time.
    // To fix that, we reject text that doesn't look like a valid version number.
    ScopedMem<char> txt(ctx->data->StealData());
    if (!IsValidProgramVersion(txt))
        return ERROR_INTERNET_INVALID_URL;

    ScopedMem<WCHAR> verTxt(str::conv::FromAnsi(txt));
    /* reduce the string to a single line (resp. drop the newline) */
    str::TransChars(verTxt, L"\r\n", L"\0\0");
    if (CompareVersion(verTxt, UPDATE_CHECK_VER) <= 0) {
        /* if automated => don't notify that there is no new version */
        if (!silent)
            MessageBoxWarning(hParent, _TR("You have the latest version."), _TR("SumatraPDF Update"));
        return 0;
    }

    // if automated, respect gGlobalPrefs.versionToSkip
    if (silent && str::EqI(gGlobalPrefs.versionToSkip, verTxt))
        return 0;

    // ask whether to download the new version and allow the user to
    // either open the browser, do nothing or don't be reminded of
    // this update ever again
    bool skipThisVersion = false;
    INT_PTR res = Dialog_NewVersionAvailable(hParent, UPDATE_CHECK_VER, verTxt, &skipThisVersion);
    if (skipThisVersion)
        gGlobalPrefs.versionToSkip.Set(verTxt.StealData());
    if (IDYES == res) {
#ifdef SUPPORTS_AUTO_UPDATE
        if (str::EndsWith(SVN_UPDATE_LINK, L".exe")) {
            ScopedMem<WCHAR> updater(GetExePath());
            updater.Set(str::Join(updater, L"-updater.exe"));
            bool ok = HttpGetToFile(SVN_UPDATE_LINK, updater);
            if (ok) {
                ok = LaunchFile(updater, L"-autoupdate replace");
                if (ok) {
                    OnMenuExit();
                    return 0;
                }
            }
        }
#endif
        LaunchBrowser(SVN_UPDATE_LINK);
    }
    SavePrefs();

    return 0;
}

#ifdef SUPPORTS_AUTO_UPDATE
#include "CmdLineParser.h"

static bool AutoUpdateMain()
{
    WStrVec argList;
    ParseCmdLine(GetCommandLine(), argList, 4);
    if (argList.Count() != 3 || !str::Eq(argList.At(1), L"-autoupdate")) {
        // the argument was misinterpreted, let SumatraPDF start as usual
        return false;
    }
    ScopedMem<WCHAR> thisExe(GetExePath());
    ScopedMem<WCHAR> otherExe;
    bool beforeUpdate = str::Eq(argList.At(2), L"replace");
    if (beforeUpdate) {
        CrashIf(!str::EndsWith(thisExe, L".exe-updater.exe"));
        otherExe.Set(str::DupN(thisExe, str::Len(thisExe) - 12));
    }
    else {
        CrashIf(!str::Eq(argList.At(2), L"cleanup"));
        otherExe.Set(str::Join(thisExe, L"-updater.exe"));
    }
    for (int tries = 10; tries > 0; tries--) {
        if (file::Delete(otherExe))
            break;
        Sleep(200);
    }
    if (!beforeUpdate) {
        // continue startup
        // TODO: restore previous session?
        return false;
    }
    bool ok = CopyFile(thisExe, otherExe, FALSE);
    // TODO: somehow indicate success or failure
    for (int tries = 10; tries > 0; tries--) {
        ok = LaunchFile(otherExe, L"-autoupdate cleanup");
        if (ok)
            break;
        Sleep(200);
    }
    return true;
}
#endif

static void ProcessAutoUpdateCheckResult(HWND hwnd, HttpReq *req, bool autoCheck)
{
    ScopedMem<WCHAR> msg;

    if (!IsWindowVisible(hwnd) || !req)
        goto Exit;
    DWORD error = ShowAutoUpdateDialog(hwnd, req, autoCheck);
    if ((0 == error) || autoCheck)
        goto Exit;

    // notify the user about network error during a manual update check
    msg.Set(str::Format(_TR("Can't connect to the Internet (error %#x)."), error));
    MessageBoxWarning(hwnd, msg, _TR("SumatraPDF Update"));
Exit:
    delete req;
}

class UpdateDownloadTask : public UITask, public HttpReqCallback
{
    HWND        hwnd;
    bool        autoCheck;
    HttpReq *   req;

public:
    UpdateDownloadTask(HWND hwnd, bool autoCheck) :
        hwnd(hwnd), autoCheck(autoCheck), req(NULL) { }

    virtual void Callback(HttpReq *aReq) {
        req = aReq;
        uitask::Post(this);
    }

    virtual void Execute() {
        ProcessAutoUpdateCheckResult(hwnd, req, autoCheck);
    }
};

// start auto-update check by downloading auto-update information from url
// on a background thread and processing the retrieved data on ui thread
// if autoCheck is true, this is a check *not* triggered by explicit action
// of the user and therefore will show less UI
void AutoUpdateCheckAsync(HWND hwnd, bool autoCheck)
{
    if (!HasPermission(Perm_InternetAccess) || gPluginMode)
        return;

    // don't check for updates at the first start, so that privacy
    // sensitive users can disable the update check in time
    if (autoCheck && 0 == gGlobalPrefs.lastUpdateTime.dwLowDateTime &&
                     0 == gGlobalPrefs.lastUpdateTime.dwHighDateTime) {
        return;
    }

    /* For auto-check, only check if at least a day passed since last check */
    if (autoCheck && (gGlobalPrefs.lastUpdateTime.dwLowDateTime != 0 ||
                      gGlobalPrefs.lastUpdateTime.dwHighDateTime != 0)) {
        FILETIME currentTimeFt;
        GetSystemTimeAsFileTime(&currentTimeFt);
        int secs = FileTimeDiffInSecs(currentTimeFt, gGlobalPrefs.lastUpdateTime);
        assert(secs >= 0);
        // if secs < 0 => somethings wrong, so ignore that case
        if ((secs > 0) && (secs < SECS_IN_DAY))
            return;
    }

    const WCHAR *url = SUMATRA_UPDATE_INFO_URL L"?v=" UPDATE_CHECK_VER;
    new HttpReq(url, new UpdateDownloadTask(hwnd, autoCheck));

    GetSystemTimeAsFileTime(&gGlobalPrefs.lastUpdateTime);
}

class FileExistenceChecker : public ThreadBase, public UITask
{
    WStrVec paths;

public:
    FileExistenceChecker() {
        DisplayState *state;
        for (size_t i = 0; i < 2 * FILE_HISTORY_MAX_RECENT && (state = gFileHistory.Get(i)); i++) {
            if (!state->isMissing)
                paths.Append(str::Dup(state->filePath));
        }
        // add missing paths from the list of most frequently opened documents
        Vec<DisplayState *> frequencyList;
        gFileHistory.GetFrequencyOrder(frequencyList);
        for (size_t i = 0; i < 2 * FILE_HISTORY_MAX_FREQUENT && i < frequencyList.Count(); i++) {
            state = frequencyList.At(i);
            if (!paths.Contains(state->filePath))
                paths.Append(str::Dup(state->filePath));
        }
    }

    virtual void Run() {
        // filters all file paths on network drives, removable drives and
        // all paths which still exist from the list (remaining paths will
        // be marked as inexistent in gFileHistory)
        for (size_t i = 0; i < paths.Count() && !WasCancelRequested(); i++) {
            WCHAR *path = paths.At(i);
            bool check = path::IsOnFixedDrive(path);
            if (!check || file::Exists(path)) {
                paths.RemoveAt(i--);
                delete path;
            }
        }
        if (!WasCancelRequested())
            uitask::Post(this);
    }

    virtual void Execute() {
        for (size_t i = 0; i < paths.Count(); i++) {
            gFileHistory.MarkFileInexistent(paths.At(i), true);
        }
        // update the Frequently Read page in case it's been displayed already
        if (paths.Count() > 0 && gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow())
            gWindows.At(0)->RedrawAll(true);
        // prepare for clean-up (Join() just to be safe)
        gFileExistenceChecker = NULL;
        Join();
    }
};

#ifdef DRAW_PAGE_SHADOWS
#define BORDER_SIZE   1
#define SHADOW_OFFSET 4
static void PaintPageFrameAndShadow(HDC hdc, RectI& bounds, RectI& pageRect, bool presentation)
{
    // Frame info
    RectI frame = bounds;
    frame.Inflate(BORDER_SIZE, BORDER_SIZE);

    // Shadow info
    RectI shadow = frame;
    shadow.Offset(SHADOW_OFFSET, SHADOW_OFFSET);
    if (frame.x < 0) {
        // the left of the page isn't visible, so start the shadow at the left
        int diff = min(-pageRect.x, SHADOW_OFFSET);
        shadow.x -= diff; shadow.dx += diff;
    }
    if (frame.y < 0) {
        // the top of the page isn't visible, so start the shadow at the top
        int diff = min(-pageRect.y, SHADOW_OFFSET);
        shadow.y -= diff; shadow.dy += diff;
    }

    // Draw shadow
    if (!presentation) {
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(COL_PAGE_SHADOW));
        FillRect(hdc, &shadow.ToRECT(), brush);
    }

    // Draw frame
    ScopedGdiObj<HPEN> pe(CreatePen(PS_SOLID, 1, presentation ? TRANSPARENT : COL_PAGE_FRAME));
    ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(gRenderCache.colorRange[1]));
    SelectObject(hdc, pe);
    SelectObject(hdc, brush);
    Rectangle(hdc, frame.x, frame.y, frame.x + frame.dx, frame.y + frame.dy);
}
#else
static void PaintPageFrameAndShadow(HDC hdc, RectI& bounds, RectI& pageRect, bool presentation)
{
    ScopedGdiObj<HPEN> pe(CreatePen(PS_NULL, 0, 0));
    ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(gRenderCache.colorRange[1]));
    SelectObject(hdc, pe);
    SelectObject(hdc, brush);
    Rectangle(hdc, bounds.x, bounds.y, bounds.x + bounds.dx + 1, bounds.y + bounds.dy + 1);
}
#endif

/* debug code to visualize links (can block while rendering) */
static void DebugShowLinks(DisplayModel& dm, HDC hdc)
{
    if (!gDebugShowLinks)
        return;

    RectI viewPortRect(PointI(), dm.viewPort.Size());
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x00, 0xff, 0xff));
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    for (int pageNo = dm.PageCount(); pageNo >= 1; --pageNo) {
        PageInfo *pageInfo = dm.GetPageInfo(pageNo);
        if (!pageInfo || !pageInfo->shown || 0.0 == pageInfo->visibleRatio)
            continue;

        Vec<PageElement *> *els = dm.engine->GetElements(pageNo);
        if (els) {
            for (size_t i = 0; i < els->Count(); i++) {
                if (els->At(i)->GetType() == Element_Image)
                    continue;
                RectI rect = dm.CvtToScreen(pageNo, els->At(i)->GetRect());
                RectI isect = viewPortRect.Intersect(rect);
                if (!isect.IsEmpty())
                    PaintRect(hdc, isect);
            }
            DeleteVecMembers(*els);
            delete els;
        }
    }

    DeletePen(SelectObject(hdc, oldPen));

    if (dm.ZoomVirtual() == ZOOM_FIT_CONTENT) {
        // also display the content box when fitting content
        pen = CreatePen(PS_SOLID, 1, RGB(0xff, 0x00, 0xff));
        oldPen = SelectObject(hdc, pen);

        for (int pageNo = dm.PageCount(); pageNo >= 1; --pageNo) {
            PageInfo *pageInfo = dm.GetPageInfo(pageNo);
            if (!pageInfo->shown || 0.0 == pageInfo->visibleRatio)
                continue;

            RectI rect = dm.CvtToScreen(pageNo, dm.engine->PageContentBox(pageNo));
            PaintRect(hdc, rect);
        }

        DeletePen(SelectObject(hdc, oldPen));
    }
}

static void DrawDocument(WindowInfo& win, HDC hdc, RECT *rcArea)
{
    DisplayModel* dm = win.dm;
    assert(dm);
    if (!dm) return;

    bool paintOnBlackWithoutShadow = win.presentation ||
    // draw comic books and single images on a black background (without frame and shadow)
                                     dm->engine && dm->engine->IsImageCollection();
    if (paintOnBlackWithoutShadow) {
        ScopedGdiObj<HBRUSH> brush(CreateSolidBrush(gRenderCache.colorRange[0]));
        FillRect(hdc, rcArea, brush);
    } else
        FillRect(hdc, rcArea, gBrushNoDocBg);

    bool rendering = false;
    RectI screen(PointI(), dm->viewPort.Size());

    for (int pageNo = 1; pageNo <= dm->PageCount(); ++pageNo) {
        PageInfo *pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || 0.0f == pageInfo->visibleRatio)
            continue;
        assert(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        RectI bounds = pageInfo->pageOnScreen.Intersect(screen);
        // don't paint the frame background for images
        if (!(dm->engine && dm->engine->IsImageCollection()))
            PaintPageFrameAndShadow(hdc, bounds, pageInfo->pageOnScreen, win.presentation);

        bool renderOutOfDateCue = false;
        UINT renderDelay = 0;
        if (!DoCachePageRendering(&win, pageNo)) {
            if (dm->engine)
                dm->engine->RenderPage(hdc, pageInfo->pageOnScreen, pageNo, dm->ZoomReal(pageNo), dm->Rotation());
        }
        else
            renderDelay = gRenderCache.Paint(hdc, bounds, dm, pageNo, pageInfo, &renderOutOfDateCue);

        if (renderDelay) {
            ScopedFont fontRightTxt(GetSimpleFont(hdc, L"MS Shell Dlg", 14));
            HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
            SetTextColor(hdc, gRenderCache.colorRange[0]);
            if (renderDelay != RENDER_DELAY_FAILED) {
                if (renderDelay < REPAINT_MESSAGE_DELAY_IN_MS)
                    win.RepaintAsync(REPAINT_MESSAGE_DELAY_IN_MS / 4);
                else
                    DrawCenteredText(hdc, bounds, _TR("Please wait - rendering..."), IsUIRightToLeft());
                rendering = true;
            } else {
                DrawCenteredText(hdc, bounds, _TR("Couldn't render the page"), IsUIRightToLeft());
            }
            SelectObject(hdc, hPrevFont);
            continue;
        }

        if (!renderOutOfDateCue)
            continue;

        HDC bmpDC = CreateCompatibleDC(hdc);
        if (bmpDC) {
            SelectObject(bmpDC, gBitmapReloadingCue);
            int size = (int)(16 * win.panel->WIN->uiDPIFactor);
            int cx = min(bounds.dx, 2 * size), cy = min(bounds.dy, 2 * size);
            StretchBlt(hdc, bounds.x + bounds.dx - min((cx + size) / 2, cx),
                bounds.y + max((cy - size) / 2, 0), min(cx, size), min(cy, size),
                bmpDC, 0, 0, 16, 16, SRCCOPY);

            DeleteDC(bmpDC);
        }
    }

    if (win.showSelection)
        PaintSelection(&win, hdc);

    if (win.fwdSearchMark.show)
        PaintForwardSearchMark(&win, hdc);

    if (!rendering)
        DebugShowLinks(*dm, hdc);
}

static void RerenderEverything()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        DisplayModel *dm = gWindows.At(i)->dm;
        if (dm) {
            gRenderCache.CancelRendering(dm);
            gRenderCache.KeepForDisplayModel(dm, dm);
            gWindows.At(i)->RedrawAll(true);
        }
    }
}

void UpdateDocumentColors(COLORREF fore, COLORREF back)
{
    //COLORREF fore = WIN_COL_BLACK;
    //COLORREF back = WIN_COL_WHITE;
    if (gGlobalPrefs.useSysColors) {
        fore = GetSysColor(COLOR_WINDOWTEXT);
        back = GetSysColor(COLOR_WINDOW);
    }
    // update document color range
    if (
        fore != gRenderCache.colorRange[0] ||
        back != gRenderCache.colorRange[1])
    {
        gRenderCache.colorRange[0] = fore;
        gRenderCache.colorRange[1] = back;
        RerenderEverything();
    }
    // update document background
    DeleteObject(gBrushNoDocBg);
    if (gGlobalPrefs.useSysColors && (fore != WIN_COL_BLACK || back != WIN_COL_WHITE))
        gBrushNoDocBg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    else
        gBrushNoDocBg = CreateSolidBrush(gGlobalPrefs.noDocBgColor);

    DeleteObject(gBrushLogoBg);
    gBrushLogoBg = CreateSolidBrush(gGlobalPrefs.bgColor);

#ifndef ABOUT_USE_LESS_COLORS
    DeleteObject(gBrushAboutBg);
    gBrushAboutBg = CreateSolidBrush(gGlobalPrefs.bgColor);
#else
    DeleteObject(gBrushAboutBg);
    gBrushAboutBg = CreateSolidBrush(ABOUT_BG_GRAY_COLOR);
#endif
}

void UpdateColorAll(COLORREF docFore, COLORREF docBack, COLORREF tocBg, COLORREF favBg)
{
    UpdateDocumentColors(docFore, docBack);

    for (size_t i = 0; i < gWIN.Count(); i++) {
        TopWindowInfo *WIN = gWIN.At(i);
        for (size_t j = 0; j < WIN->gPanel.Count(); j++) {
            PanelInfo *panel = WIN->gPanel.At(j);
            for (size_t k = 0; k < panel->gWin.Count(); k++) {
                WindowInfo *win = panel->gWin.At(k);
                if (!win->dm)
                    InvalidateRect(win->hwndCanvas, NULL, TRUE);
                TreeView_SetBkColor(win->sideBar()->hwndTocTree, tocBg);
                TreeView_SetBkColor(win->sideBar()->hwndFavTree, favBg);
            }
        }
    }
}

static void ToggleGdiDebugging()
{
    gUseGdiRenderer = !gUseGdiRenderer;
    DebugGdiPlusDevice(gUseGdiRenderer);
    RerenderEverything();
}

static void OnDraggingStart(WindowInfo& win, int x, int y, bool right=false)
{
    SetCapture(win.hwndCanvas);
    win.mouseAction = right ? MA_DRAGGING_RIGHT : MA_DRAGGING;
    win.dragPrevPos = PointI(x, y);
    if (GetCursor())
        SetCursor(gCursorDrag);
}

static void OnDraggingStop(WindowInfo& win, int x, int y, bool aborted)
{
    if (GetCapture() != win.hwndCanvas)
        return;

    if (GetCursor())
        SetCursor(gCursorArrow);
    ReleaseCapture();

    if (aborted)
        return;

    SizeI drag(x - win.dragPrevPos.x, y - win.dragPrevPos.y);
    win.MoveDocBy(drag.dx, -2 * drag.dy);
}

static void OnMouseMove(WindowInfo& win, int x, int y, WPARAM flags)
{
    if (!win.IsDocLoaded())
        return;
    assert(win.dm);

    if (win.presentation) {
        // shortly display the cursor if the mouse has moved and the cursor is hidden
        if (PointI(x, y) != win.dragPrevPos && !GetCursor()) {
            if (win.mouseAction == MA_IDLE)
                SetCursor(gCursorArrow);
            else
                SendMessage(win.hwndCanvas, WM_SETCURSOR, 0, 0);
            SetTimer(win.hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, NULL);
        }
    }

    if (win.dragStartPending) {
        // have we already started a proper drag?
        if (abs(x - win.dragStart.x) <= GetSystemMetrics(SM_CXDRAG) &&
            abs(y - win.dragStart.y) <= GetSystemMetrics(SM_CYDRAG)) {
            return;
        }
        win.dragStartPending = false;
        delete win.linkOnLastButtonDown;
        win.linkOnLastButtonDown = NULL;
    }

    SizeI drag;
    switch (win.mouseAction) {
    case MA_SCROLLING:
        win.yScrollSpeed = (y - win.dragStart.y) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        win.xScrollSpeed = (x - win.dragStart.x) / SMOOTHSCROLL_SLOW_DOWN_FACTOR;
        break;
    case MA_SELECTING_TEXT:
        if (GetCursor())
            SetCursor(gCursorIBeam);
        /* fall through */
    case MA_SELECTING:
        win.selectionRect.dx = x - win.selectionRect.x;
        win.selectionRect.dy = y - win.selectionRect.y;
        OnSelectionEdgeAutoscroll(&win, x, y);
        win.RepaintAsync();
        break;
    case MA_DRAGGING:
    case MA_DRAGGING_RIGHT:
        drag = SizeI(win.dragPrevPos.x - x, win.dragPrevPos.y - y);
        win.MoveDocBy(drag.dx, drag.dy);
        break;
    }

    win.dragPrevPos = PointI(x, y);
}

static void OnMouseLeftButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    //lf("Left button clicked on %d %d", x, y);
    if (win.IsAboutWindow())
        // remember a link under so that on mouse up we only activate
        // link if mouse up is on the same link as mouse down
        win.url = GetStaticLink(win.staticLinks, x, y);
    if (!win.IsDocLoaded())
        return;

    if (MA_DRAGGING_RIGHT == win.mouseAction)
        return;

    if (MA_SCROLLING == win.mouseAction) {
        win.mouseAction = MA_IDLE;
        return;
    }
    assert(win.mouseAction == MA_IDLE);
    assert(win.dm);

    SetFocus(win.hwndFrame);

    assert(!win.linkOnLastButtonDown);
    PageElement *pageEl = win.dm->GetElementAtPos(PointI(x, y));
    if (pageEl && pageEl->GetType() == Element_Link)
        win.linkOnLastButtonDown = pageEl;
    else
        delete pageEl;
    win.dragStartPending = true;
    win.dragStart = PointI(x, y);

    // - without modifiers, clicking on text starts a text selection
    //   and clicking somewhere else starts a drag
    // - pressing Shift forces dragging
    // - pressing Ctrl forces a rectangular selection
    // - pressing Ctrl+Shift forces text selection
    // - not having CopySelection permission forces dragging
    if (!HasPermission(Perm_CopySelection) || ((key & MK_SHIFT) || !win.dm->IsOverText(PointI(x, y))) && !(key & MK_CONTROL))
        OnDraggingStart(win, x, y);
    else
        OnSelectionStart(&win, x, y, key);
}

static void OnMouseLeftButtonUp(WindowInfo& win, int x, int y, WPARAM key)
{
    if (win.IsAboutWindow()) {
        SetFocus(win.hwndFrame);
        const WCHAR *url = GetStaticLink(win.staticLinks, x, y);
        if (url && url == win.url) {
            if (str::Eq(url, SLINK_OPEN_FILE))
                SendMessage(win.hwndFrame, WM_COMMAND, IDM_OPEN, 0);
            else if (str::Eq(url, SLINK_LIST_HIDE)) {
                gGlobalPrefs.showStartPage = false;
                win.RedrawAll(true);
            } else if (str::Eq(url, SLINK_LIST_SHOW)) {
                gGlobalPrefs.showStartPage = true;
                win.RedrawAll(true);
            } else if (!str::StartsWithI(url, L"http:") &&
                       !str::StartsWithI(url, L"https:") &&
                       !str::StartsWithI(url, L"mailto:"))
            {
                LoadArgs args(url, &win);
                LoadDocument(args);
            } else
                LaunchBrowser(url);
        }
        win.url = NULL;
    }
    if (!win.IsDocLoaded())
        return;

    assert(win.dm);
    if (MA_IDLE == win.mouseAction || MA_DRAGGING_RIGHT == win.mouseAction)
        return;
    assert(MA_SELECTING == win.mouseAction || MA_SELECTING_TEXT == win.mouseAction || MA_DRAGGING == win.mouseAction);

    bool didDragMouse = !win.dragStartPending ||
        abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
    if (MA_DRAGGING == win.mouseAction)
        OnDraggingStop(win, x, y, !didDragMouse);
    else
        OnSelectionStop(&win, x, y, !didDragMouse);

    PointD ptPage = win.dm->CvtFromScreen(PointI(x, y));
    // TODO: win.linkHandler->GotoLink might spin the event loop
    PageElement *activeLink = win.linkOnLastButtonDown;
    win.linkOnLastButtonDown = NULL;
    win.mouseAction = MA_IDLE;

    if (didDragMouse)
        /* pass */;
    else if (activeLink && activeLink->GetRect().Contains(ptPage)) {
        win.linkHandler->GotoLink(activeLink->AsLink());
        SetCursor(gCursorArrow);
    }
    /* if we had a selection and this was just a click, hide the selection */
    else if (win.showSelection)
        ClearSearchResult(&win);
    /* in presentation mode, change pages on left/right-clicks */
    else if (win.fullScreen || PM_ENABLED == win.presentation) {
        if ((key & MK_SHIFT))
            win.dm->GoToPrevPage(0);
        else
            win.dm->GoToNextPage(0);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation)
        win.ChangePresentationMode(PM_ENABLED);

    delete activeLink;
}

static void OnMouseLeftButtonDblClk(WindowInfo& win, int x, int y, WPARAM key)
{
    //lf("Left button clicked on %d %d", x, y);
    if ((win.fullScreen || win.presentation) && !(key & ~MK_LBUTTON) || win.IsAboutWindow()) {
        // in presentation and fullscreen modes, left clicks turn the page,
        // make two quick left clicks (AKA one double-click) turn two pages
        OnMouseLeftButtonDown(win, x, y, key);
        return;
    }

    bool dontSelect = false;
    if (gGlobalPrefs.enableTeXEnhancements && !(key & ~MK_LBUTTON))
        dontSelect = OnInverseSearch(&win, x, y);
    if (dontSelect || !win.IsDocLoaded())
        return;

    if (win.dm->IsOverText(PointI(x, y))) {
        int pageNo = win.dm->GetPageNoByPoint(PointI(x, y));
        if (win.dm->ValidPageNo(pageNo)) {
            PointD pt = win.dm->CvtFromScreen(PointI(x, y), pageNo);
            win.dm->textSelection->SelectWordAt(pageNo, pt.x, pt.y);
            UpdateTextSelection(&win, false);
            win.RepaintAsync();
        }
        return;
    }

    PageElement *pageEl = win.dm->GetElementAtPos(PointI(x, y));
    if (pageEl && pageEl->GetType() == Element_Link) {
        // speed up navigation in a file where navigation links are in a fixed position
        OnMouseLeftButtonDown(win, x, y, key);
    }
    else if (pageEl && pageEl->GetType() == Element_Image) {
        // select an image that could be copied to the clipboard
        RectI rc = win.dm->CvtToScreen(pageEl->GetPageNo(), pageEl->GetRect());

        DeleteOldSelectionInfo(&win, true);
        win.selectionOnPage = SelectionOnPage::FromRectangle(win.dm, rc);
        win.showSelection = win.selectionOnPage != NULL;
        win.RepaintAsync();
    }
    delete pageEl;
}

static void OnMouseMiddleButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    // Handle message by recording placement then moving document as mouse moves.

    switch (win.mouseAction) {
    case MA_IDLE:
        win.mouseAction = MA_SCROLLING;

        // record current mouse position, the farther the mouse is moved
        // from this position, the faster we scroll the document
        win.dragStart = PointI(x, y);
        SetCursor(gCursorScroll);
        break;

    case MA_SCROLLING:
        win.mouseAction = MA_IDLE;
        break;
    }
}

static void OnMouseRightButtonDown(WindowInfo& win, int x, int y, WPARAM key)
{
    //lf("Right button clicked on %d %d", x, y);
    if (!win.IsDocLoaded()) {
        SetFocus(win.hwndFrame);
        win.dragStart = PointI(x, y);
        return;
    }

    if (MA_SCROLLING == win.mouseAction)
        win.mouseAction = MA_IDLE;
    else if (win.mouseAction != MA_IDLE)
        return;
    assert(win.dm);

    SetFocus(win.hwndFrame);

    win.dragStartPending = true;
    win.dragStart = PointI(x, y);

    OnDraggingStart(win, x, y, true);
}

static void OnMouseRightButtonUp(WindowInfo& win, int x, int y, WPARAM key)
{
    if (!win.IsDocLoaded()) {
        bool didDragMouse =
            abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
            abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
        if (!didDragMouse)
            OnAboutContextMenu(&win, x, y);
        return;
    }

    assert(win.dm);
    if (MA_DRAGGING_RIGHT != win.mouseAction)
        return;

    bool didDragMouse = !win.dragStartPending ||
        abs(x - win.dragStart.x) > GetSystemMetrics(SM_CXDRAG) ||
        abs(y - win.dragStart.y) > GetSystemMetrics(SM_CYDRAG);
    OnDraggingStop(win, x, y, !didDragMouse);

    win.mouseAction = MA_IDLE;

    if (didDragMouse)
        /* pass */;
    else if (win.fullScreen || PM_ENABLED == win.presentation) {
        if ((key & MK_CONTROL))
            OnContextMenu(&win, x, y);
        else if ((key & MK_SHIFT))
            win.dm->GoToNextPage(0);
        else
            win.dm->GoToPrevPage(0);
    }
    /* return from white/black screens in presentation mode */
    else if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation)
        win.ChangePresentationMode(PM_ENABLED);
    else
        OnContextMenu(&win, x, y);
}

static void OnMouseRightButtonDblClick(WindowInfo& win, int x, int y, WPARAM key)
{
    if ((win.fullScreen || win.presentation) && !(key & ~MK_RBUTTON)) {
        // in presentation and fullscreen modes, right clicks turn the page,
        // make two quick right clicks (AKA one double-click) turn two pages
        OnMouseRightButtonDown(win, x, y, key);
        return;
    }
}

static void OnPaint(WindowInfo& win)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win.hwndCanvas, &ps);

    if (win.IsAboutWindow()) {
        if (HasPermission(Perm_SavePreferences | Perm_DiskAccess) && gGlobalPrefs.rememberOpenedFiles && gGlobalPrefs.showStartPage)
            DrawStartPage(win, win.buffer->GetDC(), gFileHistory, gRenderCache.colorRange);
        else
            DrawAboutPage(win, win.buffer->GetDC());
        win.buffer->Flush(hdc);
    } else if (!win.IsDocLoaded()) {
        // TODO: replace with notifications as far as reasonably possible
        // note: currently it's possible to easily reload the document
        // and/or open it in an external viewer (e.g. Adobe Reader might
        // be able to handle PDFs that are too broken for MuPDF),
        // a notification would break this
        ScopedFont fontRightTxt(GetSimpleFont(hdc, L"MS Shell Dlg", 14));
        HGDIOBJ hPrevFont = SelectObject(hdc, fontRightTxt);
        FillRect(hdc, &ps.rcPaint, gBrushNoDocBg);
        ScopedMem<WCHAR> msg(str::Format(_TR("Error loading %s"), win.loadedFilePath));
        DrawCenteredText(hdc, ClientRect(win.hwndCanvas), msg, IsUIRightToLeft());
        SelectObject(hdc, hPrevFont);
    } else {
        switch (win.presentation) {
        case PM_BLACK_SCREEN:
            FillRect(hdc, &ps.rcPaint, GetStockBrush(BLACK_BRUSH));
            break;
        case PM_WHITE_SCREEN:
            FillRect(hdc, &ps.rcPaint, GetStockBrush(WHITE_BRUSH));
            break;
        default:
            DrawDocument(win, win.buffer->GetDC(), &ps.rcPaint);
            win.buffer->Flush(hdc);
        }
    }

    EndPaint(win.hwndCanvas, &ps);
}

void OnMenuExit(TopWindowInfo *WIN, HWND hwnd)
{
    if (gPluginMode)
        return;

    ShowWindow(hwnd, SW_HIDE);

    HWND hwndToDestroy = hwnd;

    bool lastTopWindow = (1 == gWIN.Count() + gEbookWindows.Count());

    // WIN == NULL means that we want to exit a Ebook Window.
    if (WIN != NULL) {

        size_t CountPanelInWIN = WIN->gPanel.Count();

        for (size_t i = 0; i < CountPanelInWIN; i++) {
            PanelInfo *panel = WIN->gPanel.At(i);

            size_t CountWinInPanel = panel->gWin.Count();

            for (size_t j = 0; j < CountWinInPanel; j++) {
                WindowInfo *win = panel->gWin.At(j);

                if (win->printThread && !win->printCanceled) {
                    int res = MessageBox(win->hwndFrame, _TR("Printing is still in progress. Abort and quit?"), _TR("Printing in progress."), MB_ICONEXCLAMATION | MB_YESNO | (IsUIRightToLeft() ? MB_RTLREADING : 0));
                    if (IDNO == res)
                        return;
                }

                AbortFinding(win);
                AbortPrinting(win);

                // Need to check this function.
                SavePrefs();
            }
        }
        DeleteTopWindowInfo(WIN);
    }

    DestroyWindow(hwndToDestroy);

    if (lastTopWindow) {
        assert(0 == gWIN.Count());
        PostQuitMessage(0);
    }
}

size_t TotalWindowsCount()
{
    return gWindows.Count() + gEbookWindows.Count();
}

// closes a document inside a WindowInfo and turns it into
// about window
void CloseDocumentInWindow(WindowInfo *win)
{
    // TODO: remove time logging before release
    Timer t(true);
    bool wasChm = win->IsChm();
    if (wasChm)
        UnsubclassCanvas(win->hwndCanvas);

    PanelInfo *panel = win->panel;

    // We have to make sure that : if we can use menu->close to close a window and it's already the last win in a panel, then there is a doc loaded.
    if (1 == panel->gWin.Count()) {

        CrashIf(!win->IsDocLoaded());

        FileWatcherUnsubscribe(win->watcher);
        win->watcher = NULL;

        // We need to hide hwndCanvas first, otherwise we will see the scroll bar for a while after start page is shown.
        // Another may be: don't update hwndCanvas in SetSidebarVisibility.
        // This is similar to the fixed issue in rev. 164.
        ShowWindow(win->hwndCanvas, SW_HIDE);
        SetSidebarVisibility(win, false, gGlobalPrefs.favVisible);
        ShowWindow(win->hwndCanvas, SW_SHOW); // We need to show it again, since it doesn't set hwndCanvas to be shown in the call to SetSidebarVisibility.

        ClearTocBox(win); // When load a doc, it will clear toc also. But we still to make sure everything is cleaned.
        AbortFinding(win, true);
        delete win->dm;
        win->dm = NULL;
        str::ReplacePtr(&win->loadedFilePath, NULL);
        delete win->pdfsync;
        win->pdfsync = NULL;
        win->notifications->RemoveAllInGroup(NG_RESPONSE_TO_ACTION);
        win->notifications->RemoveAllInGroup(NG_PAGE_INFO_HELPER);

        // Need to look the property window careful.
        DeletePropertiesWindow(win->hwndFrame);

        UpdateToolbarPageText(win, 0); // Make sure no page text remains on toolbar.
        // We don't these one here, because we have UpdateToolbarAndScrollbarState(*win); below.
        // UpdateToolbarFindText(win);
        if (wasChm) {
            // restore the non-Chm menu
            RebuildMenuBarForWindow(win);
        }

        DeleteOldSelectionInfo(win, true);
    }

    int tabIndex = panel->gWin.Find(win);
    int tabIndexNew;

    if ( ! (1 == panel->gWin.Count()) ) {

        if (!(win == panel->win)) { // We don't have to change panel->win.
            panel->gWin.Remove(win); // We need this to get the new tab index.
            tabIndexNew = panel->gWin.Find(panel->win);
        } else {
            if ( tabIndex < (int) panel->gWin.Count() - 1) // Show the next doc.
                tabIndexNew = tabIndex;
            else
                tabIndexNew = tabIndex - 1; // Show the previous doc.

            panel->gWin.Remove(win);
            panel->win = panel->gWin.At(tabIndexNew);

            ShowDocument(panel, win, panel->win, true);
        }

        SendMessage(panel->hwndTab, TCM_DELETEITEM, tabIndex, NULL);
        SendMessage(panel->hwndTab, TCM_SETCURSEL, tabIndexNew, NULL);

    } else {
        win->RedrawAll();
        UpdateFindbox(win); // Need to check this function.
        // UpdateToolbarAndScrollbarState will set the title also, so we don't need it here.
        // win::SetText(win->panel->WIN->hwndFrame, APP_NAME_STR);
        UpdateToolbarAndScrollbarState(*win); // This will call ToolbarUpdateStateForWindow -> UpdateToolbarFindText, which positions FindText (label).
        SetTabTitle(win);
        SetWinTitle(win); // Update win->title;
        SetFocus(win->panel->WIN->hwndFrame);
    }

    t.Stop();
    lf("CloseDocumentInWindow() time: %.2f", t.GetTimeInMs());

#ifdef DEBUG
    // cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2039
    // HeapValidate() is left here to help us catch the possibility that the fix
    // in FileWatcher::SynchronousAbort() isn't correct
    HeapValidate((HANDLE)_get_heap_handle(), 0, NULL);
#endif
}

void QuitIfNoMoreWindows()
{
    if (0 == TotalWindowsCount()) {
        PostQuitMessage(0);
    }
}

void CloseDocumentAndDeleteWindowInfo(WindowInfo *win)
{
    if (!win)
        return;
    HWND hwndToDestroy = win->hwndFrame;
    CloseDocumentInWindow(win);
    DeleteWindowInfo(win);
    DestroyWindow(hwndToDestroy);
}

/* Close the document associated with window 'hwnd'.
   Closes the window unless this is the last window in which
   case it switches to empty window and disables the "File\Close"
   menu item. */
void CloseWindow(WindowInfo *win, bool quitIfLast, bool forceClose)
{
    // forceClose is true only when CloseWindow is call from WM_DESTROY.
    // But in my version, WM_DESTROY calls OnMenuExit, so forceClose is
    // not used, and is always false.

    PanelInfo *panel = win->panel;
    TopWindowInfo *WIN = panel->WIN;

    CrashIf(!win);
    if (!win) return;

    // Not used, but keep it.
    CrashIf(forceClose && !quitIfLast);
        if (forceClose) quitIfLast = true;

    // In original version, WM_DESTROY call CloseWindow with forceClose == true;

    // when used as an embedded plugin, closing should happen automatically
    // when the parent window is destroyed (cf. WM_DESTROY)
    //if (gPluginMode && gWindows.Find(win) == 0 && !forceClose)
    //    return;

    if (win->printThread && !win->printCanceled) {
        int res = MessageBox(win->hwndFrame, _TR("Printing is still in progress. Abort and quit?"), _TR("Printing in progress."), MB_ICONEXCLAMATION | MB_YESNO | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        if (IDNO == res)
            return;
    }

    if (win->IsDocLoaded())
        win->dm->dontRenderFlag = true;
    if (win->presentation)
        ExitFullscreen(*win);

    bool lastWinInPanel = (1 == panel->gWin.Count());
    bool lastPanelInWIN = (1 == WIN->gPanel.Count());
    bool lastWIN = (1 == gWIN.Count());

    if (lastWinInPanel && lastPanelInWIN && lastWIN) {
        // Saves the preference.
        if (quitIfLast && !forceClose) // forceClose is not used and is false.
            // hide the window before saving prefs (closing seems slightly faster that way)
            ShowWindow(win->panel->WIN->hwndFrame, SW_HIDE);
        SavePrefs();
    } else
        // This saves the file display state.
        UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(win));

    if (quitIfLast && lastWinInPanel && lastPanelInWIN && lastWIN) {
        OnMenuExit(WIN, WIN->hwndFrame);
    }  else if (!quitIfLast && lastWinInPanel && lastPanelInWIN && lastWIN) {
        CloseDocumentInWindow(win);
    }  else if (!lastWinInPanel && lastPanelInWIN && lastWIN) {
        CloseDocumentInWindow(win);
        DestroyWindow(win->hwndCanvas);
        DeleteWindowInfo(win);
    } else if (lastWinInPanel && !lastPanelInWIN && lastWIN) {
        CloseDocumentInWindow(win);
    }  else if (!lastWinInPanel && !lastPanelInWIN && lastWIN) {
        CloseDocumentInWindow(panel->win);
        DestroyWindow(win->hwndCanvas);
        DeleteWindowInfo(win);
    }  else if (lastWinInPanel && lastPanelInWIN && !lastWIN) {
        OnMenuExit(WIN, WIN->hwndFrame);
    }  else if (!lastWinInPanel && lastPanelInWIN && !lastWIN) {
        CloseDocumentInWindow(win);
        DestroyWindow(win->hwndCanvas);
        DeleteWindowInfo(win);
    }  else if (lastWinInPanel && !lastPanelInWIN && !lastWIN) {
        CloseDocumentInWindow(win);
    }  else if (!lastWinInPanel && !lastPanelInWIN && !lastWIN) {
        CloseDocumentInWindow(win);
        DestroyWindow(win->hwndCanvas);
        DeleteWindowInfo(win);
    }
}

// returns false if no filter has been appended
static bool AppendFileFilterForDoc(DisplayModel *dm, str::Str<WCHAR>& fileFilter)
{
    const WCHAR *defExt = dm->engine->GetDefaultFileExt();
    switch (dm->engineType) {
        case Engine_XPS:    fileFilter.Append(_TR("XPS documents")); break;
        case Engine_DjVu:   fileFilter.Append(_TR("DjVu documents")); break;
        case Engine_ComicBook: fileFilter.Append(_TR("Comic books")); break;
        case Engine_Image:  fileFilter.AppendFmt(_TR("Image files (*.%s)"), defExt + 1); break;
        case Engine_ImageDir: return false; // only show "All files"
        case Engine_PS:     fileFilter.Append(_TR("Postscript documents")); break;
        case Engine_Chm:    fileFilter.Append(_TR("CHM documents")); break;
        case Engine_Epub:   fileFilter.Append(_TR("EPUB ebooks")); break;
        case Engine_Mobi:   fileFilter.Append(_TR("Mobi documents")); break;
        case Engine_Fb2:    fileFilter.Append(_TR("FictionBook documents")); break;
        case Engine_Pdb:    fileFilter.Append(L"PalmDOC"); break;
        case Engine_Chm2:   fileFilter.Append(_TR("CHM documents")); break;
        case Engine_Tcr:    fileFilter.Append(L"TCR ebooks"); break;
        case Engine_Txt:    fileFilter.Append(_TR("Text documents")); break;
        default:            fileFilter.Append(_TR("PDF documents")); break;
    }
    return true;
}

static void OnMenuSaveAs(WindowInfo& win)
{
    if (!HasPermission(Perm_DiskAccess)) return;
    assert(win.dm);
    if (!win.IsDocLoaded()) return;

    const WCHAR *srcFileName = win.dm->FilePath();
    ScopedMem<WCHAR> urlName;
    if (gPluginMode) {
        urlName.Set(ExtractFilenameFromURL(gPluginURL));
        // fall back to a generic "filename" instead of the more confusing temporary filename
        srcFileName = urlName ? urlName : L"filename";
    }

    assert(srcFileName);
    if (!srcFileName) return;

    // Can't save a document's content as plain text if text copying isn't allowed
    bool hasCopyPerm = !win.dm->engine->IsImageCollection() &&
                       win.dm->engine->AllowsCopyingText();
    bool canConvertToPDF = Engine_PS == win.dm->engineType;

    const WCHAR *defExt = win.dm->engine->GetDefaultFileExt();
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Str<WCHAR> fileFilter(256);
    if (AppendFileFilterForDoc(win.dm, fileFilter))
        fileFilter.AppendFmt(L"\1*%s\1", defExt);
    if (hasCopyPerm) {
        fileFilter.Append(_TR("Text documents"));
        fileFilter.Append(L"\1*.txt\1");
    }
    if (canConvertToPDF) {
        fileFilter.Append(_TR("PDF documents"));
        fileFilter.Append(L"\1*.pdf\1");
    }
    fileFilter.Append(_TR("All files"));
    fileFilter.Append(L"\1*.*\1");
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    WCHAR dstFileName[MAX_PATH];
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseName(srcFileName));
    if (str::FindChar(dstFileName, ':')) {
        // handle embed-marks (for embedded PDF documents):
        // remove the container document's extension and include
        // the embedding reference in the suggested filename
        WCHAR *colon = (WCHAR *)str::FindChar(dstFileName, ':');
        str::TransChars(colon, L":", L"_");
        WCHAR *ext;
        for (ext = colon; ext > dstFileName && *ext != '.'; ext--);
        if (ext == dstFileName)
            ext = colon;
        memmove(ext, colon, (str::Len(colon) + 1) * sizeof(WCHAR));
    }
    // Remove the extension so that it can be re-added depending on the chosen filter
    else if (str::EndsWithI(dstFileName, defExt))
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win.hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = defExt + 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    // note: explicitly not setting lpstrInitialDir so that the OS
    // picks a reasonable default (in particular, we don't want this
    // in plugin mode, which is likely the main reason for saving as...)

    bool ok = GetSaveFileName(&ofn);
    if (!ok)
        return;

    WCHAR * realDstFileName = dstFileName;
    // Make sure that the file has a valid ending
    if (!str::EndsWithI(dstFileName, defExt) &&
        !(hasCopyPerm && str::EndsWithI(dstFileName, L".txt")) &&
        !(canConvertToPDF && str::EndsWithI(dstFileName, L".pdf"))) {
        if (hasCopyPerm && 2 == ofn.nFilterIndex)
            defExt = L".txt";
        else if (canConvertToPDF && (hasCopyPerm ? 3 : 2) == ofn.nFilterIndex)
            defExt = L".pdf";
        realDstFileName = str::Format(L"%s%s", dstFileName, defExt);
    }

    ScopedMem<WCHAR> errorMsg;
    // Extract all text when saving as a plain text file
    if (hasCopyPerm && str::EndsWithI(realDstFileName, L".txt")) {
        str::Str<WCHAR> text(1024);
        for (int pageNo = 1; pageNo <= win.dm->PageCount(); pageNo++) {
            WCHAR *tmp = win.dm->engine->ExtractPageText(pageNo, L"\r\n", NULL, Target_Export);
            text.AppendAndFree(tmp);
        }

        ScopedMem<char> textUTF8(str::conv::ToUtf8(text.LendData()));
        ScopedMem<char> textUTF8BOM(str::Join(UTF8_BOM, textUTF8));
        ok = file::WriteAll(realDstFileName, textUTF8BOM, str::Len(textUTF8BOM));
    }
    // Convert the Postscript file into a PDF one
    else if (Engine_PS == win.dm->engineType && str::EndsWithI(realDstFileName, L".pdf")) {
        ok = static_cast<PsEngine *>(win.dm->engine)->SaveFileAsPDF(realDstFileName);
    }
    // Recreate inexistant files from memory...
    else if (!file::Exists(srcFileName)) {
        ok = win.dm->engine->SaveFileAs(realDstFileName);
    }
#ifdef DEBUG
    // ... as well as files containing annotations ...
    else if (win.dm->engine->SupportsAnnotation(true)) {
        ok = win.dm->engine->SaveFileAs(realDstFileName);
    }
#endif
    // ... else just copy the file
    else {
        WCHAR *msgBuf;
        ok = CopyFile(srcFileName, realDstFileName, FALSE);
        if (ok) {
            // Make sure that the copy isn't write-locked or hidden
            const DWORD attributesToDrop = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
            DWORD attributes = GetFileAttributes(realDstFileName);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & attributesToDrop))
                SetFileAttributes(realDstFileName, attributes & ~attributesToDrop);
        } else if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, (LPWSTR)&msgBuf, 0, NULL)) {
            errorMsg.Set(str::Format(L"%s\n\n%s", _TR("Failed to save a file"), msgBuf));
            LocalFree(msgBuf);
        }
    }
    if (!ok)
        MessageBoxWarning(win.hwndFrame, errorMsg ? errorMsg : _TR("Failed to save a file"));

    if (ok && IsUntrustedFile(win.dm->FilePath(), gPluginURL))
        file::SetZoneIdentifier(realDstFileName);

    if (realDstFileName != dstFileName)
        free(realDstFileName);
}

bool LinkSaver::SaveEmbedded(unsigned char *data, size_t len)
{
    if (!HasPermission(Perm_DiskAccess))
        return false;

    WCHAR dstFileName[MAX_PATH];
    str::BufSet(dstFileName, dimof(dstFileName), fileName ? fileName : L"");

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    ScopedMem<WCHAR> fileFilter(str::Format(L"%s\1*.*\1", _TR("All files")));
    str::TransChars(fileFilter, L"\1", L"\0");

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    bool ok = GetSaveFileName(&ofn);
    if (!ok)
        return false;
    ok = file::WriteAll(dstFileName, data, len);
    if (ok && IsUntrustedFile(owner->dm ? owner->dm->FilePath() : owner->loadedFilePath, gPluginURL))
        file::SetZoneIdentifier(dstFileName);
    return ok;
}

static void OnMenuRenameFile(WindowInfo &win)
{
    if (!HasPermission(Perm_DiskAccess)) return;
    CrashIf(!win.dm);
    if (!win.IsDocLoaded()) return;
    if (gPluginMode) return;

    const WCHAR *srcFileName = win.dm->FilePath();
    // this happens e.g. for embedded documents
    if (!file::Exists(srcFileName))
        return;

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    const WCHAR *defExt = win.dm->engine->GetDefaultFileExt();
    str::Str<WCHAR> fileFilter(256);
    bool ok = AppendFileFilterForDoc(win.dm, fileFilter);
    CrashIf(!ok);
    fileFilter.AppendFmt(L"\1*%s\1", defExt);
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    WCHAR dstFileName[MAX_PATH];
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseName(srcFileName));
    // Remove the extension so that it can be re-added depending on the chosen filter
    if (str::EndsWithI(dstFileName, defExt))
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';

    ScopedMem<WCHAR> initDir(path::GetDir(srcFileName));

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win.hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    // note: the other two dialogs are named "Open" and "Save As"
    ofn.lpstrTitle = _TR("Rename To");
    ofn.lpstrInitialDir = initDir;
    ofn.lpstrDefExt = defExt + 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    ok = GetSaveFileName(&ofn);
    if (!ok)
        return;

    UpdateCurrentFileDisplayStateForWinInfo(&win);
    // note: srcFileName is deleted together with the DisplayModel
    ScopedMem<WCHAR> srcFilePath(str::Dup(srcFileName));
    CloseDocumentInWindow(&win);

    DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING;
    BOOL moveOk = MoveFileEx(srcFilePath.Get(), dstFileName, flags);
    if (!moveOk) {
        LogLastError();
        LoadArgs args(srcFilePath, &win);
        LoadDocument(args);
        ShowNotification(&win, _TR("Failed to rename the file!"), false /* autoDismiss */, true /* highlight */);
        return;
    }

    ScopedMem<WCHAR> newPath(path::Normalize(dstFileName));
    RenameFileInHistory(srcFilePath, newPath);

    LoadArgs args(dstFileName, &win);
    LoadDocument(args);
}

static void OnMenuSaveBookmark(WindowInfo& win)
{
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) return;
    CrashIf(!win.dm);
    if (!win.IsDocLoaded()) return;

    const WCHAR *defExt = win.dm->engine->GetDefaultFileExt();

    WCHAR dstFileName[MAX_PATH];
    // Remove the extension so that it can be replaced with .lnk
    str::BufSet(dstFileName, dimof(dstFileName), path::GetBaseName(win.dm->FilePath()));
    str::TransChars(dstFileName, L":", L"_");
    if (str::EndsWithI(dstFileName, defExt))
        dstFileName[str::Len(dstFileName) - str::Len(defExt)] = '\0';

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    ScopedMem<WCHAR> fileFilter(str::Format(L"%s\1*.lnk\1", _TR("Bookmark Shortcuts")));
    str::TransChars(fileFilter, L"\1", L"\0");

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win.hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = fileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"lnk";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetSaveFileName(&ofn))
        return;

    ScopedMem<WCHAR> fileName(str::Dup(dstFileName));
    if (!str::EndsWithI(dstFileName, L".lnk"))
        fileName.Set(str::Join(dstFileName, L".lnk"));

    ScrollState ss = win.dm->GetScrollState();
    const WCHAR *viewMode = DisplayModeConv::NameFromEnum(win.dm->GetDisplayMode());
    ScopedMem<WCHAR> ZoomVirtual(str::Format(L"%.2f", win.dm->ZoomVirtual()));
    if (ZOOM_FIT_PAGE == win.dm->ZoomVirtual())
        ZoomVirtual.Set(str::Dup(L"fitpage"));
    else if (ZOOM_FIT_WIDTH == win.dm->ZoomVirtual())
        ZoomVirtual.Set(str::Dup(L"fitwidth"));
    else if (ZOOM_FIT_CONTENT == win.dm->ZoomVirtual())
        ZoomVirtual.Set(str::Dup(L"fitcontent"));

    ScopedMem<WCHAR> exePath(GetExePath());
    ScopedMem<WCHAR> args(str::Format(L"\"%s\" -page %d -view \"%s\" -zoom %s -scroll %d,%d -reuse-instance",
                          win.dm->FilePath(), ss.page, viewMode, ZoomVirtual, (int)ss.x, (int)ss.y));
    ScopedMem<WCHAR> label(win.dm->engine->GetPageLabel(ss.page));
    ScopedMem<WCHAR> desc(str::Format(_TR("Bookmark shortcut to page %s of %s"),
                          label, path::GetBaseName(win.dm->FilePath())));

    CreateShortcut(fileName, exePath, args, desc, 1);
}

#if 0
// code adapted from http://support.microsoft.com/kb/131462/en-us
static UINT_PTR CALLBACK FileOpenHook(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uiMsg) {
    case WM_INITDIALOG:
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);
        break;
    case WM_NOTIFY:
        if (((LPOFNOTIFY)lParam)->hdr.code == CDN_SELCHANGE) {
            LPOPENFILENAME lpofn = (LPOPENFILENAME)GetWindowLongPtr(hDlg, GWLP_USERDATA);
            // make sure that the filename buffer is large enough to hold
            // all the selected filenames
            int cbLength = CommDlg_OpenSave_GetSpec(GetParent(hDlg), NULL, 0) + MAX_PATH;
            if (cbLength >= 0 && lpofn->nMaxFile < (DWORD)cbLength) {
                WCHAR *oldBuffer = lpofn->lpstrFile;
                lpofn->lpstrFile = (LPWSTR)realloc(lpofn->lpstrFile, cbLength * sizeof(WCHAR));
                if (lpofn->lpstrFile)
                    lpofn->nMaxFile = cbLength;
                else
                    lpofn->lpstrFile = oldBuffer;
            }
        }
        break;
    }

    return 0;
}
#endif

HWND GetSumatraWindowHwnd(const SumatraWindow& win)
{
    if (win.AsWindowInfo()) {
        return win.AsWindowInfo()->hwndFrame;
    } else {
        CrashIf(!win.AsEbookWindow());
        return win.AsEbookWindow()->hwndFrame;
    }
    return NULL;
}

void OnMenuOpen(SumatraWindow& win)
{
    if (!HasPermission(Perm_DiskAccess)) return;
    // don't allow opening different files in plugin mode
    if (gPluginMode)
        return;

    struct {
        const WCHAR *name; /* NULL if only to include in "All supported documents" */
        WCHAR *filter;
        bool available;
    } fileFormats[] = {
        { _TR("PDF documents"),         L"*.pdf",       true },
        { _TR("XPS documents"),         L"*.xps;*.oxps",true },
        { _TR("DjVu documents"),        L"*.djvu",      true },
        { _TR("Postscript documents"),  L"*.ps;*.eps",  PsEngine::IsAvailable() },
        { _TR("Comic books"),           L"*.cbz;*.cbr", true },
        { _TR("CHM documents"),         L"*.chm",       true },
        { _TR("Mobi documents"),        L"*.mobi",      true },
        { _TR("EPUB ebooks"),           L"*.epub",      true },
        { _TR("FictionBook documents"), L"*.fb2;*.fb2z;*.zfb2", true },
        { NULL, /* multi-page images */ L"*.tif;*.tiff",true },
        { NULL, /* further ebooks */    L"*.pdb;*.tcr", gUserPrefs.traditionalEbookUI },
        { _TR("Text documents"),        L"*.txt;*.log;*.nfo;file_id.diz;read.me", gUserPrefs.traditionalEbookUI },
    };
    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    str::Str<WCHAR> fileFilter;
    WStrVec filters;
    for (int i = 0; i < dimof(fileFormats); i++) {
        if (fileFormats[i].available)
            filters.Append(fileFormats[i].filter);
    }
    fileFilter.Append(_TR("All supported documents"));
    fileFilter.Append('\1');
    fileFilter.AppendAndFree(filters.Join(L";"));
    fileFilter.Append('\1');
    filters.Reset();

    for (int i = 0; i < dimof(fileFormats); i++) {
        if (fileFormats[i].available && fileFormats[i].name) {
            const WCHAR *name = fileFormats[i].name;
            WCHAR *filter = fileFormats[i].filter;
            fileFilter.AppendAndFree(str::Format(L"%s\1%s\1", name, filter));
        }
    }
    fileFilter.AppendAndFree(str::Format(L"%s\1*.*\1", _TR("All files")));
    str::TransChars(fileFilter.Get(), L"\1", L"\0");

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetSumatraWindowHwnd(win);

    ofn.lpstrFilter = fileFilter.Get();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY |
                OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    // OFN_ENABLEHOOK disables the new Open File dialog under Windows Vista
    // and later, so don't use it and just allocate enough memory to contain
    // several dozen file paths and hope that this is enough
    // TODO: Use IFileOpenDialog instead (requires a Vista SDK, though)
    ofn.nMaxFile = MAX_PATH * 100;
#if 0
    if (!IsVistaOrGreater())
    {
        ofn.lpfnHook = FileOpenHook;
        ofn.Flags |= OFN_ENABLEHOOK;
        ofn.nMaxFile = MAX_PATH / 2;
    }
    // note: ofn.lpstrFile can be reallocated by GetOpenFileName -> FileOpenHook
#endif
    ScopedMem<WCHAR> file(AllocArray<WCHAR>(ofn.nMaxFile));
    ofn.lpstrFile = file;

    if (!GetOpenFileName(&ofn))
        return;

    WCHAR *fileName = ofn.lpstrFile + ofn.nFileOffset;
    if (*(fileName - 1)) {
        // special case: single filename without NULL separator
        LoadDocument2(ofn.lpstrFile, win);
        return;
    }

    while (*fileName) {
        ScopedMem<WCHAR> filePath(path::Join(ofn.lpstrFile, fileName));
        if (filePath)
            LoadDocument2(filePath, win);
        fileName += str::Len(fileName) + 1;
    }
}

static void BrowseFolder(WindowInfo& win, bool forward)
{
    assert(win.loadedFilePath);
    if (win.IsAboutWindow()) return;
    if (!HasPermission(Perm_DiskAccess) || gPluginMode) return;

    // TODO: browse through all supported file types at the same time?
    const WCHAR *fileExt = path::GetExt(win.loadedFilePath);
    if (win.IsDocLoaded())
        fileExt = win.dm->engine->GetDefaultFileExt();
    ScopedMem<WCHAR> pattern(str::Format(L"*%s", fileExt));
    ScopedMem<WCHAR> dir(path::GetDir(win.loadedFilePath));
    pattern.Set(path::Join(dir, pattern));

    WStrVec files;
    if (!CollectPathsFromDirectory(pattern, files))
        return;

    if (!files.Contains(win.loadedFilePath))
        files.Append(str::Dup(win.loadedFilePath));
    files.SortNatural();

    int index = files.Find(win.loadedFilePath);
    if (forward)
        index = (index + 1) % files.Count();
    else
        index = (int)(index + files.Count() - 1) % files.Count();

    UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(&win));
    LoadArgs args(files.At(index), &win, true, true);
    LoadDocument(args);
}

// scrolls half a page down/up (needed for Shift+Up/Down)
#define SB_HPAGEUP   (WM_USER + 1)
#define SB_HPAGEDOWN (WM_USER + 2)

static void OnVScroll(WindowInfo& win, WPARAM wParam)
{
    if (!win.IsDocLoaded())
        return;
    AssertCrash(win.dm);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win.hwndCanvas, SB_VERT, &si);

    int iVertPos = si.nPos;
    int lineHeight = 16;
    if (!IsContinuous(win.dm->GetDisplayMode()) && ZOOM_FIT_PAGE == win.dm->ZoomVirtual())
        lineHeight = 1;

    switch (LOWORD(wParam)) {
    case SB_TOP:        si.nPos = si.nMin; break;
    case SB_BOTTOM:     si.nPos = si.nMax; break;
    case SB_LINEUP:     si.nPos -= lineHeight; break;
    case SB_LINEDOWN:   si.nPos += lineHeight; break;
    case SB_HPAGEUP:    si.nPos -= si.nPage / 2; break;
    case SB_HPAGEDOWN:  si.nPos += si.nPage / 2; break;
    case SB_PAGEUP:     si.nPos -= si.nPage; break;
    case SB_PAGEDOWN:   si.nPos += si.nPage; break;
    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win.hwndCanvas, SB_VERT, &si, TRUE);
    GetScrollInfo(win.hwndCanvas, SB_VERT, &si);

    // If the position has changed, scroll the window and update it
    if (si.nPos != iVertPos)
        win.dm->ScrollYTo(si.nPos);
}

static void OnHScroll(WindowInfo& win, WPARAM wParam)
{
    if (!win.IsDocLoaded())
        return;
    AssertCrash(win.dm);

    SCROLLINFO si = { 0 };
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(win.hwndCanvas, SB_HORZ, &si);

    int iVertPos = si.nPos;
    switch (LOWORD(wParam)) {
    case SB_LEFT:       si.nPos = si.nMin; break;
    case SB_RIGHT:      si.nPos = si.nMax; break;
    case SB_LINELEFT:   si.nPos -= 16; break;
    case SB_LINERIGHT:  si.nPos += 16; break;
    case SB_PAGELEFT:   si.nPos -= si.nPage; break;
    case SB_PAGERIGHT:  si.nPos += si.nPage; break;
    case SB_THUMBTRACK: si.nPos = si.nTrackPos; break;
    }

    // Set the position and then retrieve it.  Due to adjustments
    // by Windows it may not be the same as the value set.
    si.fMask = SIF_POS;
    SetScrollInfo(win.hwndCanvas, SB_HORZ, &si, TRUE);
    GetScrollInfo(win.hwndCanvas, SB_HORZ, &si);

    // If the position has changed, scroll the window and update it
    if (si.nPos != iVertPos)
        win.dm->ScrollXTo(si.nPos);
}

static void AdjustWindowEdge(WindowInfo& win)
{
    DWORD exStyle = GetWindowLong(win.hwndCanvas, GWL_EXSTYLE);
    DWORD newStyle = exStyle;

    // Remove the canvas' edge in the cases where the vertical scrollbar
    // would otherwise touch the screen's edge, making the scrollbar much
    // easier to hit with the mouse (cf. Fitts' law)
    // TODO: should we just always remove the canvas' edge?
    if (IsZoomed(win.hwndFrame) || win.fullScreen || win.presentation || gPluginMode)
        newStyle &= ~WS_EX_STATICEDGE;
    else
        newStyle &= WS_EX_STATICEDGE;

    if (newStyle != exStyle) {
        SetWindowLong(win.hwndCanvas, GWL_EXSTYLE, newStyle);
        SetWindowPos(win.hwndCanvas, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

static LRESULT DrawMenuBarBackground(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = CallWindowProc(DefWindowProc, hwnd, msg, wParam, lParam);

    HDC hdc = GetWindowDC(hwnd); // Not GetDC().

    HMENU hMenu = GetMenu(hwnd); // Not GetSystemMenu().
    RECT rcItem;
    GetMenuItemRect(hwnd, hMenu, GetMenuItemCount(hMenu) - 1, &rcItem);

    RECT rcFrame;
    GetWindowRect(hwnd, &rcFrame);

    RECT rcMenu;

    rcMenu.left = rcItem.right - rcFrame.left;
    rcMenu.top = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXFRAME);
    rcMenu.right = rcFrame.right - rcFrame.left - GetSystemMetrics(SM_CXFRAME);
    rcMenu.bottom = rcMenu.top + GetSystemMetrics(SM_CYMENU);

    HBRUSH hBrush = CreateSolidBrush(RGB(0xDE, 0xDE, 0xDE));

    FillRect(hdc, &rcMenu, hBrush);

    rcMenu.left = GetSystemMetrics(SM_CXFRAME);
    rcMenu.top = rcMenu.bottom - 1;
    rcMenu.bottom = rcMenu.top + 1;
    rcMenu.right = rcFrame.right - rcFrame.left - GetSystemMetrics(SM_CXFRAME);
    FillRect(hdc, &rcMenu, gBrushSepLineBg);

    DeleteObject(hBrush);

    return result;
}

static void FrameOnMeasureItem(HWND hwnd, LPARAM lParam)
{
    LPMEASUREITEMSTRUCT lpMis = (LPMEASUREITEMSTRUCT)lParam;

    WCHAR *text = (WCHAR *)lpMis->itemData;
    WCHAR *textTmp = str::Format(L"%s", text);
    str::RemoveChars(textTmp, L"&");

    HDC hdc = GetDC(hwnd);

    HFONT fontOld = (HFONT)SelectObject(hdc, gDefaultGuiFont);
    SIZE size;
    GetTextExtentPoint32(hdc, textTmp, str::Len(textTmp), &size);
    SelectObject(hdc, fontOld);

    lpMis->itemWidth = size.cx;
    lpMis->itemHeight = size.cy;

    free(textTmp);
}

static void FrameOnDrawItem(HWND hwnd, LPARAM lParam)
{
    LPDRAWITEMSTRUCT lpDis = (LPDRAWITEMSTRUCT)lParam;

    COLORREF fore = GetSysColor(COLOR_MENUTEXT);

    // 30 = COLOR_MENUBAR;
    // Using COLOR_MENU (which is 4) has problem in XP (color not matched).
    COLORREF back = RGB(0xDE, 0xDE, 0xDE);

    COLORREF boundary;
    COLORREF boundaryBottom;

    // This only affects the rectangles containing the texts.
    bool needDrawBoundary = false;

    if ((lpDis->itemState & ODS_HOTLIGHT) ||
        (lpDis->itemState & ODS_SELECTED))
    {
        if (lpDis->itemState & ODS_HOTLIGHT) {
            back = RGB(0xD5, 0xE7, 0xF8); //GetSysColor(COLOR_HIGHLIGHT);
            boundary = RGB(0x7A, 0xB1, 0xE8);
            boundaryBottom = RGB(0x74, 0xAA, 0xE2);
        }
        else {
            back = RGB(0xB8, 0xD8, 0xF9); //GetSysColor(COLOR_HIGHLIGHT);
            boundary = RGB(0x62, 0xA3, 0xE5);
            boundaryBottom = RGB(0x5D, 0x9E, 0xE0);
        }

        needDrawBoundary = true;

        // If conditions are false, we don't have to change the color.
        //SetTextColor(lpDis->hDC, fore);
        //SetBkColor(lpDis->hDC, back);
    }

    // We should put the code below here rather than inside the if statement above,
    // in order to draw non-highlighted items correctly.
    // This is for the case that one uses "Alt" key to active the menu bar.
    SetTextColor(lpDis->hDC, fore);
    SetBkColor(lpDis->hDC, back);

    HDC hdc = lpDis->hDC;

    HBRUSH hBrush = CreateSolidBrush(back);
    FillRect(hdc, &lpDis->rcItem, hBrush);
    DeleteObject(hBrush);

    if (needDrawBoundary) {

        int x1 = lpDis->rcItem.left;
        int y1 = lpDis->rcItem.top;
        int x2 = lpDis->rcItem.right;
        int y2 = lpDis->rcItem.bottom;

        HRGN hrgn = CreateRectRgn(x1, y1, x2, y2);
        SelectClipRgn(hdc, hrgn);
        HRGN rgnInterior = CreateRectRgn(x1 + 1, y1 + 1, x2 - 1, y2);
        ExtSelectClipRgn(hdc, rgnInterior, RGN_DIFF);

        hBrush = CreateSolidBrush(boundary);
        FillRgn(hdc, hrgn, hBrush);
        DeleteObject(hBrush);

        SelectClipRgn(hdc, NULL);
        DeleteObject(hrgn);
        DeleteObject(rgnInterior);

        RECT rc;
        rc.left = x1;
        rc.top = y2 - 1;
        rc.right = x2;
        rc.bottom = y2;

        hBrush = CreateSolidBrush(boundaryBottom);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
    }

    RECT rc;
    rc.left = lpDis->rcItem.left + 7;
    rc.top  = lpDis->rcItem.top + 2; 
    if ( !(gOS.dwMajorVersion >= 6 && gOS.dwMinorVersion >= 1) )
        rc.top = rc.top + 2;
    rc.right = lpDis->rcItem.right;
    rc.bottom = lpDis->rcItem.bottom;

    WCHAR *text = (WCHAR *)lpDis->itemData;

    DrawTextEx(lpDis->hDC, text, str::Len(text),
        &rc, lpDis->itemState & ODS_NOACCEL ? DT_HIDEPREFIX : NULL, NULL);
}

static void FrameOnSize(WindowInfo* win, int dx, int dy)
{
    // FrameOnSize is called only when all childs are created and all infos are recorded.

    int rebBarDy = 0;
    if (!gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && !(win->presentation || win->fullScreen)) {
        SetWindowPos(win->toolBar()->hwndReBar, NULL, 0, 0, dx, 0, SWP_NOZORDER); // win->toolBar() is not null, see CreateWindowInfo.
        rebBarDy = WindowRect(win->toolBar()->hwndReBar).dy;
    }

    // If using only one sidebar, which is a child of hwndFrame,
    if (!gGlobalPrefs.sidebarForEachPanel) {

        bool tocVisible = false;

        // show sidebar if one of the gPanel.At(i)->win's toc is visible.
        TopWindowInfo *WIN = win->panel->WIN;

        for (size_t i = 0; i < WIN->gPanel.Count(); i++) {
            PanelInfo *panel = WIN->gPanel.At(i);
            for (size_t j = 0; j < panel->gWin.Count(); j++) {
                WindowInfo *win = panel->gWin.At(j);
                if (win->IsDocLoaded() && win->tocVisible) {
                    tocVisible = true;
                    break;
                }
            }
        }

        if (tocVisible || gGlobalPrefs.favVisible) {
            SetSidebarVisibility(win, tocVisible, gGlobalPrefs.favVisible);
            return;
        }
    }

    if (gGlobalPrefs.toolbarForEachPanel) // The top container fills the client area of hwndFrame.
        SetWindowPos(win->panel->WIN->container->hwndContainer, NULL, 0, rebBarDy, dx, dy - rebBarDy, SWP_NOZORDER); // SetWindowPos(win->panel->WIN->gContainer.At(0)->hwndContainer, NULL, 0, 0, dx, dy, SWP_NOZORDER);
    else // One should combine this with above, but now we separate them for clearness.
        SetWindowPos(win->panel->WIN->container->hwndContainer, NULL, 0, rebBarDy, dx, dy - rebBarDy, SWP_NOZORDER);

    if (win->presentation || win->fullScreen) {
        RectI fullscreen = GetFullscreenRect(win->hwndFrame);
        WindowRect rect(win->hwndFrame);
        // Windows XP sometimes seems to change the window size on it's own
        if (rect != fullscreen && rect != GetVirtualScreenRect())
            MoveWindow(win->hwndFrame, fullscreen);
    }
}

void SetCurrentLanguageAndRefreshUi(const char *langCode)
{
    if (!langCode || (str::Eq(langCode, trans::GetCurrentLangCode())))
        return;
    SetCurrentLang(langCode);
    UpdateRtlLayoutForAllWindows();
    RebuildMenuBarForAllWindows();
    UpdateUITextForLanguage();
    if (gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow())
        gWindows.At(0)->RedrawAll(true);
    SavePrefs();
}

void OnMenuChangeLanguage(HWND hwnd)
{
    const char *newLangCode = Dialog_ChangeLanguge(hwnd, trans::GetCurrentLangCode());
    SetCurrentLanguageAndRefreshUi(newLangCode);
}

static void OnMenuViewShowHideToolbar()
{
    gGlobalPrefs.toolbarVisible = !gGlobalPrefs.toolbarVisible;
    ShowOrHideToolbarGlobally();
}

static void OnMenuViewShowHideTab()
{
    gGlobalPrefs.tabVisible = !gGlobalPrefs.tabVisible;
    ShowOrHideTabGlobally();
}

void OnMenuPreference(HWND hwnd)
{
    if (!HasPermission(Perm_SavePreferences)) return;

    bool useSysColors = gGlobalPrefs.useSysColors;

    if (IDOK != Dialog_Preference(hwnd, &gGlobalPrefs))
        return;

    if (!gGlobalPrefs.rememberOpenedFiles) {
        gFileHistory.Clear();
        CleanUpThumbnailCache(gFileHistory);
    }
    if (useSysColors != gGlobalPrefs.useSysColors)
        UpdateDocumentColors();

    SavePrefs();
}

static void OnMenuPreference(WindowInfo& win)
{
    OnMenuPreference(win.panel->WIN->hwndFrame);
    if (gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow())
        gWindows.At(0)->RedrawAll(true);
}

// toggles 'show pages continuously' state
static void OnMenuViewContinuous(WindowInfo& win)
{
    if (!win.IsDocLoaded())
        return;

    DisplayMode newMode = win.dm->GetDisplayMode();
    switch (newMode) {
        case DM_SINGLE_PAGE:
        case DM_CONTINUOUS:
            newMode = IsContinuous(newMode) ? DM_SINGLE_PAGE : DM_CONTINUOUS;
            break;
        case DM_FACING:
        case DM_CONTINUOUS_FACING:
            newMode = IsContinuous(newMode) ? DM_FACING : DM_CONTINUOUS_FACING;
            break;
        case DM_BOOK_VIEW:
        case DM_CONTINUOUS_BOOK_VIEW:
            newMode = IsContinuous(newMode) ? DM_BOOK_VIEW : DM_CONTINUOUS_BOOK_VIEW;
            break;
    }
    SwitchToDisplayMode(&win, newMode);
}

static void ChangeZoomLevel(WindowInfo *win, float newZoom, bool pagesContinuously)
{
    if (!win->IsDocLoaded())
        return;

    float zoom = win->dm->ZoomVirtual();
    DisplayMode mode = win->dm->GetDisplayMode();
    DisplayMode newMode = pagesContinuously ? DM_CONTINUOUS : DM_SINGLE_PAGE;

    if (mode != newMode || zoom != newZoom) {
        DisplayMode prevMode = win->prevDisplayMode;
        float prevZoom = win->prevZoomVirtual;

        if (mode != newMode)
            SwitchToDisplayMode(win, newMode);
        OnMenuZoom(win, MenuIdFromVirtualZoom(newZoom));

        // remember the previous values for when the toolbar button is unchecked
        if (INVALID_ZOOM == prevZoom) {
            win->prevZoomVirtual = zoom;
            win->prevDisplayMode = mode;
        }
        // keep the rememberd values when toggling between the two toolbar buttons
        else {
            win->prevZoomVirtual = prevZoom;
            win->prevDisplayMode = prevMode;
        }
    }
    else if (win->prevZoomVirtual != INVALID_ZOOM) {
        float prevZoom = win->prevZoomVirtual;
        SwitchToDisplayMode(win, win->prevDisplayMode);
        ZoomToSelection(win, prevZoom);
    }
}

static void FocusPageNoEdit(HWND hwndPageBox)
{
    if (GetFocus() == hwndPageBox)
        SendMessage(hwndPageBox, WM_SETFOCUS, 0, 0);
    else
        SetFocus(hwndPageBox);
}

static void OnMenuGoToPage(WindowInfo& win)
{
    if (!win.IsDocLoaded())
        return;

    // Don't show a dialog if we don't have to - use the Toolbar instead
    if (gGlobalPrefs.toolbarVisible && !win.fullScreen && !win.presentation) {
        FocusPageNoEdit(win.toolBar()->hwndPageBox);
        return;
    }

    ScopedMem<WCHAR> label(win.dm->engine->GetPageLabel(win.dm->CurrentPageNo()));
    ScopedMem<WCHAR> newPageLabel(Dialog_GoToPage(win.hwndFrame, label, win.dm->PageCount(),
                                                  !win.dm->engine->HasPageLabels()));
    if (!newPageLabel)
        return;

    int newPageNo = win.dm->engine->GetPageByLabel(newPageLabel);
    if (win.dm->ValidPageNo(newPageNo))
        win.dm->GoToPage(newPageNo, 0, true);
}

static void EnterFullscreen(WindowInfo& win, bool presentation)
{
    if (!HasPermission(Perm_FullscreenAccess))
        return;

    if ((presentation ? win.presentation : win.fullScreen) ||
        !IsWindowVisible(win.hwndFrame) || gPluginMode)
        return;

    assert(presentation ? !win.fullScreen : !win.presentation);
    if (presentation) {
        assert(win.dm);
        if (!win.IsDocLoaded())
            return;

        if (IsZoomed(win.hwndFrame))
            win.windowStateBeforePresentation = WIN_STATE_MAXIMIZED;
        else
            win.windowStateBeforePresentation = WIN_STATE_NORMAL;
        win.presentation = PM_ENABLED;
        win.tocBeforeFullScreen = win.tocVisible;

        SetTimer(win.hwndCanvas, HIDE_CURSOR_TIMER_ID, HIDE_CURSOR_DELAY_IN_MS, NULL);
    }
    else {
        win.fullScreen = true;
        win.tocBeforeFullScreen = win.IsDocLoaded() ? win.tocVisible : false;
    }

    // Remove TOC and favorites from full screen, add back later on exit fullscreen
    bool favVisibleTmp = gGlobalPrefs.favVisible;
    if (win.tocVisible || gGlobalPrefs.favVisible) {
        SetSidebarVisibility(&win, false, false);
        // restore gGlobalPrefs.favVisible changed by SetSidebarVisibility()
    }

    long ws = GetWindowLong(win.hwndFrame, GWL_STYLE);
    if (!presentation || !win.fullScreen)
        win.prevStyle = ws;
    ws &= ~(WS_BORDER|WS_CAPTION|WS_THICKFRAME);
    ws |= WS_MAXIMIZE;

    win.frameRc = WindowRect(win.hwndFrame);
    RectI rect = GetFullscreenRect(win.hwndFrame);

    SetMenu(win.hwndFrame, NULL);
    ShowWindow(win.toolBar()->hwndReBar, SW_HIDE);

    SetWindowLong(win.hwndFrame, GWL_STYLE, ws);
    SetWindowPos(win.hwndFrame, NULL, rect.x, rect.y, rect.dx, rect.dy, SWP_FRAMECHANGED | SWP_NOZORDER);
    SetWindowPos(win.hwndCanvas, NULL, 0, 0, rect.dx, rect.dy, SWP_NOZORDER);

    if (presentation)
        win.dm->SetPresentationMode(true);

    // Make sure that no toolbar/sidebar keeps the focus
    SetFocus(win.hwndFrame);
    gGlobalPrefs.favVisible = favVisibleTmp;
}

static void ExitFullscreen(WindowInfo& win)
{
    if (!win.fullScreen && !win.presentation)
        return;

    bool wasPresentation = PM_DISABLED != win.presentation;
    if (wasPresentation && win.dm) {
        win.dm->SetPresentationMode(false);
        win.presentation = PM_DISABLED;
    }
    else
        win.fullScreen = false;

    if (wasPresentation) {
        KillTimer(win.hwndCanvas, HIDE_CURSOR_TIMER_ID);
        SetCursor(gCursorArrow);
    }

    bool tocVisible = win.IsDocLoaded() && win.tocBeforeFullScreen;
    if (tocVisible || gGlobalPrefs.favVisible)
        SetSidebarVisibility(&win, tocVisible, gGlobalPrefs.favVisible);

    if (gGlobalPrefs.toolbarVisible)
        ShowWindow(win.toolBar()->hwndReBar, SW_SHOW);
    SetMenu(win.hwndFrame, win.menu());

    SetWindowLong(win.hwndFrame, GWL_STYLE, win.prevStyle);
    UINT flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win.hwndFrame, NULL, 0, 0, 0, 0, flags);
    MoveWindow(win.hwndFrame, win.frameRc);
    assert(WindowRect(win.hwndFrame) == win.frameRc);
}

static void OnMenuViewFullscreen(WindowInfo& win, bool presentation=false)
{
    bool enterFullscreen = presentation ? !win.presentation : !win.fullScreen;

    if (!win.presentation && !win.fullScreen)
        RememberDefaultWindowPosition(win);
    else
        ExitFullscreen(win);

    if (enterFullscreen && (!presentation || win.IsDocLoaded()))
        EnterFullscreen(win, presentation);
}

static void OnMenuViewPresentation(WindowInfo& win)
{
    OnMenuViewFullscreen(win, true);
}

void AdvanceFocus(WindowInfo* win)
{
    // Tab order: Frame -> Page -> Find -> ToC -> Favorites -> Frame -> ...

    bool hasToolbar = !win->fullScreen && !win->presentation &&
                      gGlobalPrefs.toolbarVisible && win->IsDocLoaded();
    int direction = IsShiftPressed() ? -1 : 1;

    struct {
        HWND hwnd;
        bool isAvailable;
    } tabOrder[] = {
        { win->hwndFrame,   true                                },
        { win->toolBar()->hwndPageBox, hasToolbar                          },
        { win->toolBar()->hwndFindBox, hasToolbar && NeedsFindUI(win)      },
        { win->sideBar()->hwndTocTree, win->tocLoaded && win->tocVisible   },
        { win->sideBar()->hwndFavTree, gGlobalPrefs.favVisible             },
    };

    /* // make sure that at least one element is available
    bool hasAvailable = false;
    for (int i = 0; i < dimof(tabOrder) && !hasAvailable; i++)
        hasAvailable = tabOrder[i].isAvailable;
    if (!hasAvailable)
        return;
    // */

    // find the currently focused element
    HWND current = GetFocus();
    int ix;
    for (ix = 0; ix < dimof(tabOrder); ix++)
        if (tabOrder[ix].hwnd == current)
            break;
    // if it's not in the tab order, start at the beginning
    if (ix == dimof(tabOrder))
        ix = -direction;

    // focus the next available element
    do {
        ix = (ix + direction + dimof(tabOrder)) % dimof(tabOrder);
    } while (!tabOrder[ix].isAvailable);
    SetFocus(tabOrder[ix].hwnd);
}

// allow to distinguish a '/' caused by VK_DIVIDE (rotates a document)
// from one typed on the main keyboard (focuses the find textbox)
static bool gIsDivideKeyDown = false;

static bool ChmForwardKey(WPARAM key)
{
    if ((VK_LEFT == key) || (VK_RIGHT == key))
        return true;
    if ((VK_UP == key) || (VK_DOWN == key))
        return true;
    if ((VK_HOME == key) || (VK_END == key))
        return true;
    if ((VK_PRIOR == key) || (VK_NEXT == key))
        return true;
    if ((VK_MULTIPLY == key) || (VK_DIVIDE == key))
        return true;
    return false;
}

bool FrameOnKeydown(WindowInfo *win, WPARAM key, LPARAM lparam, bool inTextfield)
{
    bool isCtrl = IsCtrlPressed();
    bool isShift = IsShiftPressed();

    if ((VK_LEFT == key || VK_RIGHT == key) &&
        isShift && isCtrl &&
        win->loadedFilePath && !inTextfield) {
        // folder browsing should also work when an error page is displayed,
        // so special-case it before the win.IsDocLoaded() check
        BrowseFolder(*win, VK_RIGHT == key);
        return true;
    }

    if (VK_LEFT == key && IsCtrlPressed() && IsAltPressed()) {
        ShowPreviousDocument(win->panel);
        return true;
    } else if (VK_RIGHT == key && IsCtrlPressed() && IsAltPressed()) {
        ShowNextDocument(win->panel);
        return true;
    }

    if ('T' == key && IsCtrlPressed() && !IsAltPressed()) {
        OpenNewTab(win->panel);
        return true;
    } else if ('V' == key && IsCtrlPressed() && IsAltPressed()) {
        SplitPanel(win->panel->container, L"Vertically");
        return true;
    } else if ('H' == key && IsCtrlPressed() && IsAltPressed()) {
        SplitPanel(win->panel->container, L"Horizontally");
        return true;
    }

    if (!win->IsDocLoaded())
        return false;

    // some of the chm key bindings are different than the rest and we
    // need to make sure we don't break them
    bool isChm = win->IsChm();

    bool isPageUp = (isCtrl && (VK_UP == key));
    if (!isChm)
        isPageUp |= (VK_PRIOR == key);

    if (isPageUp) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        if (win->dm->ZoomVirtual() != ZOOM_FIT_CONTENT)
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEUP, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->GoToPrevPage(-1);
        return true;
    }

    bool isPageDown = (isCtrl && (VK_DOWN == key));
    if (!isChm)
        isPageDown |= (VK_NEXT == key);
    if (isPageDown) {
        int currentPos = GetScrollPos(win->hwndCanvas, SB_VERT);
        if (win->dm->ZoomVirtual() != ZOOM_FIT_CONTENT)
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_PAGEDOWN, 0);
        if (GetScrollPos(win->hwndCanvas, SB_VERT) == currentPos)
            win->dm->GoToNextPage(0);
        return true;
    }

    if (isChm) {
        if (ChmForwardKey(key)) {
            win->dm->AsChmEngine()->PassUIMsg(WM_KEYDOWN, key, lparam);
            return true;
        }
    }
    //lf("key=%d,%c,shift=%d\n", key, (char)key, (int)WasKeyDown(VK_SHIFT));

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation)
        return false;

    if (VK_UP == key) {
        if (win->dm->NeedVScroll())
            SendMessage(win->hwndCanvas, WM_VSCROLL, isShift ? SB_HPAGEUP : SB_LINEUP, 0);
        else
            win->dm->GoToPrevPage(-1);
    } else if (VK_DOWN == key) {
        if (win->dm->NeedVScroll())
            SendMessage(win->hwndCanvas, WM_VSCROLL, isShift ? SB_HPAGEDOWN : SB_LINEDOWN, 0);
        else
            win->dm->GoToNextPage(0);
    } else if (VK_HOME == key && isCtrl) {
        win->dm->GoToFirstPage();
    } else if (VK_END == key && isCtrl) {
        if (!win->dm->GoToLastPage())
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
    } else if (inTextfield) {
        // The remaining keys have a different meaning
        return false;
    } else if (VK_LEFT == key) {
        if (win->dm->NeedHScroll())
            SendMessage(win->hwndCanvas, WM_HSCROLL, isShift ? SB_PAGELEFT : SB_LINELEFT, 0);
        else
            win->dm->GoToPrevPage(0);
    } else if (VK_RIGHT == key) {
        if (win->dm->NeedHScroll())
            SendMessage(win->hwndCanvas, WM_HSCROLL, isShift ? SB_PAGERIGHT : SB_LINERIGHT, 0);
        else
            win->dm->GoToNextPage(0);
    } else if (VK_HOME == key) {
        win->dm->GoToFirstPage();
    } else if (VK_END == key) {
        if (!win->dm->GoToLastPage())
            SendMessage(win->hwndCanvas, WM_VSCROLL, SB_BOTTOM, 0);
    } else if (VK_MULTIPLY == key) {
        win->dm->RotateBy(90);
    } else if (VK_DIVIDE == key) {
        win->dm->RotateBy(-90);
        gIsDivideKeyDown = true;
    } else {
        return false;
    }

    return true;
}

static void FrameOnChar(WindowInfo& win, WPARAM key)
{
    if (IsCharUpper((WCHAR)key))
        key = (WCHAR)CharLower((LPWSTR)(WCHAR)key);

    if (PM_BLACK_SCREEN == win.presentation || PM_WHITE_SCREEN == win.presentation) {
        win.ChangePresentationMode(PM_ENABLED);
        return;
    }

    switch (key) {
    case VK_ESCAPE:
        if (win.findThread)
            AbortFinding(&win);
        else if (win.notifications->GetFirstInGroup(NG_PAGE_INFO_HELPER))
            win.notifications->RemoveAllInGroup(NG_PAGE_INFO_HELPER);
        else if (win.presentation)
            OnMenuViewPresentation(win);
        else if (gGlobalPrefs.escToExit)
            CloseWindow(&win, true);
        else if (win.fullScreen)
            OnMenuViewFullscreen(win);
        else if (win.showSelection)
            ClearSearchResult(&win);
        return;
    case 'q':
        // close the current document/window. Quit if this is the last window
        CloseWindow(&win, true);
        return;
    case 'r':
        ReloadDocument(&win);
        return;
    case VK_TAB:
        AdvanceFocus(&win);
        break;
    }

    if (!win.IsDocLoaded())
        return;

    switch (key) {
    case VK_SPACE:
    case VK_RETURN:
        FrameOnKeydown(&win, IsShiftPressed() ? VK_PRIOR : VK_NEXT, 0);
        break;
    case VK_BACK:
        {
            bool forward = IsShiftPressed();
            win.dm->Navigate(forward ? 1 : -1);
        }
        break;
    case 'g':
        OnMenuGoToPage(win);
        break;
    case 'j':
        FrameOnKeydown(&win, VK_DOWN, 0);
        break;
    case 'k':
        FrameOnKeydown(&win, VK_UP, 0);
        break;
    case 'n':
        win.dm->GoToNextPage(0);
        break;
    case 'p':
        win.dm->GoToPrevPage(0);
        break;
    case 'z':
        win.ToggleZoom();
        break;
    // per http://en.wikipedia.org/wiki/Keyboard_layout
    // almost all keyboard layouts allow to press either
    // '+' or '=' unshifted (and one of them is also often
    // close to '-'); the other two alternatives are for
    // the major exception: the two Swiss layouts
    case '+': case '=': case 0xE0: case 0xE4:
        ZoomToSelection(&win, win.dm->NextZoomStep(ZOOM_MAX), false);
        break;
    case '-':
        ZoomToSelection(&win, win.dm->NextZoomStep(ZOOM_MIN), false);
        break;
    case '/':
        if (!gIsDivideKeyDown)
            OnMenuFind(&win);
        gIsDivideKeyDown = false;
        break;
    case 'c':
        OnMenuViewContinuous(win);
        break;
    case 'b':
        if (!IsSingle(win.dm->GetDisplayMode())) {
            // "e-book view": flip a single page
            bool forward = !IsShiftPressed();
            int currPage = win.dm->CurrentPageNo();
            if (forward ? win.dm->LastBookPageVisible() : win.dm->FirstBookPageVisible())
                break;

            DisplayMode newMode = DM_BOOK_VIEW;
            if (DisplayModeShowCover(win.dm->GetDisplayMode()))
                newMode = DM_FACING;
            SwitchToDisplayMode(&win, newMode, true);

            if (forward && currPage >= win.dm->CurrentPageNo() && (currPage > 1 || newMode == DM_BOOK_VIEW))
                win.dm->GoToNextPage(0);
            else if (!forward && currPage <= win.dm->CurrentPageNo())
                win.dm->GoToPrevPage(0);
        }
        else if (win.presentation)
            win.ChangePresentationMode(PM_BLACK_SCREEN);
        break;
    case '.':
        // for Logitech's wireless presenters which target PowerPoint's shortcuts
        if (win.presentation)
            win.ChangePresentationMode(PM_BLACK_SCREEN);
        break;
    case 'w':
        if (win.presentation)
            win.ChangePresentationMode(PM_WHITE_SCREEN);
        break;
    case 'i':
        // experimental "page info" tip: make figuring out current page and
        // total pages count a one-key action (unless they're already visible)
        if (!gGlobalPrefs.toolbarVisible || win.fullScreen || PM_ENABLED == win.presentation) {
            int current = win.dm->CurrentPageNo(), total = win.dm->PageCount();
            ScopedMem<WCHAR> pageInfo(str::Format(L"%s %d / %d", _TR("Page:"), current, total));
            if (win.dm->engine && win.dm->engine->HasPageLabels()) {
                ScopedMem<WCHAR> label(win.dm->engine->GetPageLabel(current));
                pageInfo.Set(str::Format(L"%s %s (%d / %d)", _TR("Page:"), label, current, total));
            }
            bool autoDismiss = !IsShiftPressed();
            ShowNotification(&win, pageInfo, autoDismiss, false, NG_PAGE_INFO_HELPER);
        }
        break;
#ifdef DEBUG
    case '$':
        ToggleGdiDebugging();
        break;
    case 0xA7: case 0xB0:
        if (win.dm->engine->SupportsAnnotation()) {
            Vec<PageAnnotation> annots;
            // load previously saved annotations
            Vec<PageAnnotation> *savedAnnots = LoadFileModifications(win.loadedFilePath);
            if (savedAnnots) {
                annots = *savedAnnots;
                delete savedAnnots;
            }
            // convert the current selection into a text highlighting annotation
            if (win.showSelection && win.selectionOnPage) {
                for (size_t i = 0; i < win.selectionOnPage->Count(); i++) {
                    SelectionOnPage& sel = win.selectionOnPage->At(i);
                    annots.Append(PageAnnotation(Annot_Highlight, sel.pageNo, sel.rect, PageAnnotation::Color(0xE2, 0xC4, 0xE2, 0xCC)));
                    // annots.Append(PageAnnotation(Annot_Underline, sel.pageNo, sel.rect, PageAnnotation::Color(0x00, 0x00, 0x00)));
                    // annots.Append(PageAnnotation(Annot_StrikeOut, sel.pageNo, sel.rect, PageAnnotation::Color(0x80, 0x80, 0x80)));
                    annots.Append(PageAnnotation(Annot_Squiggly, sel.pageNo, sel.rect, PageAnnotation::Color(0xFF, 0x00, 0x00)));
                }
            }
            if (annots.Count() > 0) {
                win.dm->engine->UpdateUserAnnotations(&annots);
                gRenderCache.CancelRendering(win.dm);
                gRenderCache.KeepForDisplayModel(win.dm, win.dm);
                ClearSearchResult(&win);
                // optionally save the annotations to disk
                if (!win.dm->engine->SupportsAnnotation(true) && IsShiftPressed())
                    SaveFileModifictions(win.loadedFilePath, &annots);
            }
            else
                win.dm->engine->UpdateUserAnnotations(NULL);
        }
#endif
    }
}

static void UpdateSidebarTitles(WindowInfo& win)
{
    HWND tocTitle = GetDlgItem(win.sideBar()->hwndTocBox, IDC_TOC_TITLE);
    win::SetText(tocTitle, _TR("Bookmarks"));
    if (win.tocVisible) {
        InvalidateRect(win.sideBar()->hwndTocBox, NULL, TRUE);
        UpdateWindow(win.sideBar()->hwndTocBox);
    }

    HWND favTitle = GetDlgItem(win.hwndFavBox, IDC_FAV_TITLE);
    win::SetText(favTitle, _TR("Favorites"));
    if (gGlobalPrefs.favVisible) {
        InvalidateRect(win.hwndFavBox, NULL, TRUE);
        UpdateWindow(win.hwndFavBox);
    }
}

static void UpdateUITextForLanguage()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        WindowInfo *win = gWindows.At(i);
        UpdateToolbarPageText(win, -1);
        UpdateToolbarFindText(win);
        UpdateToolbarButtonsToolTipsForWindow(win);
        // also update the sidebar title at this point
        UpdateSidebarTitles(*win);
    }
}

static void ResizePanel(ContainerInfo *container)
{
    HWND hwndC1 = container->container1->hwndContainer;
    HWND hwndC2 = container->container2->hwndContainer;
    HWND hwndParent = container->hwndContainer;

    POINT pcur;
    GetCursorPosInHwnd(hwndParent, pcur);
    int newSizeC1;
    if (container->isSplitVertical)
        newSizeC1 = pcur.x; // without splitter
    else
        newSizeC1 = pcur.y;

    ClientRect r1(hwndC1);
    ClientRect rParent(hwndParent);

    int parentD;
    int D1;
    int splitterD;
    if (container->isSplitVertical) {
        parentD = rParent.dx;
        D1 = r1.dx;
        splitterD = SPLITTER_DX;
    } else {
        parentD = rParent.dy;
        D1 = r1.dy;
        splitterD = SPLITTER_DY;
    }

    if (newSizeC1 < min(1 * (parentD - splitterD) / 4, D1) ||
        newSizeC1 > max(3 * (parentD - splitterD) / 4, D1)) {
            SetCursor(gCursorNo);
            return;
    }

    if (container->isSplitVertical)
        SetCursor(gCursorSizeWE);
    else
        SetCursor(gCursorSizeNS);

    int totalD;
    if (container->isSplitVertical)
        totalD = rParent.dy;
    else 
        totalD = rParent.dx;

    HDWP hdwp = BeginDeferWindowPos(3);

    if (container->isSplitVertical) {
        DeferWindowPos(hdwp, hwndC1, NULL, 0, 0, newSizeC1, totalD, SWP_NOZORDER);
        DeferWindowPos(hdwp, container->hwndSplitter, NULL, newSizeC1, 0, SPLITTER_DX, totalD, SWP_NOZORDER);
        DeferWindowPos(hdwp, hwndC2, NULL, newSizeC1 + SPLITTER_DX, 0, rParent.dx - newSizeC1 - SPLITTER_DX, totalD, SWP_NOZORDER);
    } else {
        DeferWindowPos(hdwp, hwndC1, NULL, 0, 0, totalD, newSizeC1, SWP_NOZORDER);
        DeferWindowPos(hdwp, container->hwndSplitter, NULL, 0, newSizeC1, totalD, SPLITTER_DY, SWP_NOZORDER);
        DeferWindowPos(hdwp, hwndC2, NULL, 0, newSizeC1 + SPLITTER_DY, totalD, rParent.dy - newSizeC1 - SPLITTER_DY, SWP_NOZORDER);
    }

    EndDeferWindowPos(hdwp);

    TopWindowInfo *WIN = FindTopWindowInfoByHwnd(container->hwndContainer);

    InvalidateRect(WIN->hwndFrame, NULL, TRUE);
    UpdateWindow(WIN->hwndFrame);
}

// TODO: the layout logic here is similar to what we do in SetSidebarVisibility()
// would be nice to consolidate.
static void ResizeSidebar(WindowInfo *win)
{
    HWND hwndParent = win->panel->WIN->hwndFrame;
    HWND hwnd = win->panel->WIN->gContainer.At(0)->hwndContainer;
    if (gGlobalPrefs.sidebarForEachPanel) {
        hwndParent = win->panel->hwndPanel;
        hwnd = win->hwndCanvas;
    }

    POINT pcur;
    GetCursorPosInHwnd(hwndParent, pcur);
    int sidebarDx = pcur.x - (SPLITTER_DX - 1) / 2; // without splitter

    ClientRect rSidebar(win->sideBar()->hwndSidebar);
    ClientRect rParent(hwndParent);

    // make sure to keep this in sync with the calculations in SetSidebarVisibility
    // note: without the min/max(..., rToc.dx), the sidebar will be
    //       stuck at its width if it accidentally got too wide or too narrow
    if (sidebarDx < min(SIDEBAR_MIN_WIDTH, rSidebar.dx) ||
        sidebarDx > max((rParent.dx - SPLITTER_DX) / 2, rSidebar.dx)) {
        SetCursor(gCursorNo);
        return;
    }

    SetCursor(gCursorSizeWE);

    int toolbarDy = -1;
    int tabControlDy = 0;

    if (!gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && win->panel->container->isAtTop)
        toolbarDy = 0;

    if (gGlobalPrefs.toolbarVisible && (gGlobalPrefs.toolbarForEachPanel == gGlobalPrefs.sidebarForEachPanel) && !win->fullScreen && !win->presentation)
        toolbarDy = WindowRect(win->toolBar()->hwndReBar).dy;

    if (gGlobalPrefs.sidebarForEachPanel && gGlobalPrefs.tabVisible)
        tabControlDy = TAB_CONTROL_DY;

    int sidebar_y = toolbarDy + tabControlDy;

    int y = 0;
    y += gGlobalPrefs.sidebarForEachPanel ? sidebar_y + 1 : sidebar_y;
    
    int sidebarDy = rParent.dy - sidebar_y;
    int Dy = rParent.dy - y;

    // rToc.y is always 0, as rToc is a ClientRect, so we first have
    // to convert it into coordinates relative to hwndFrame:
    assert(MapRectToWindow(rSidebar, win->sideBar()->hwndSidebar, hwndParent).y == sidebar_y);
    //assert(totalDy == (rToc.dy + rFav.dy));

    HDWP hdwp = BeginDeferWindowPos(3);

    // Using this reduces flicker.
    DeferWindowPos(hdwp, win->sideBar()->hwndSidebar, NULL, 0, sidebar_y, sidebarDx, sidebarDy, SWP_NOZORDER);
    DeferWindowPos(hdwp, win->sideBar()->hwndSidebarSplitter, NULL, sidebarDx, sidebar_y, SPLITTER_DX, sidebarDy, SWP_NOZORDER);
    DeferWindowPos(hdwp, hwnd, NULL, sidebarDx + SPLITTER_DX, y, rParent.dx - sidebarDx - SPLITTER_DX, Dy, SWP_NOZORDER);

    EndDeferWindowPos(hdwp);

    // In XP, even hwndFrame is updated, we still need to update hwndSidebar.
    // Under Windows 7 or above, we don't need to update hwndSidebar if hwndFrame is updated.
    if ( !(gOS.dwMajorVersion >= 6 && gOS.dwMinorVersion >= 1) || gGlobalPrefs.sidebarForEachPanel) {
        InvalidateRect(win->sideBar()->hwndSidebar, NULL, TRUE);
        UpdateWindow(win->sideBar()->hwndSidebar);
    }

    // If all the panels share the same sidebar, we need to update WIN.
    if (!gGlobalPrefs.sidebarForEachPanel) {
        InvalidateRect(win->panel->WIN->hwndFrame, NULL, TRUE);
        UpdateWindow(win->panel->WIN->hwndFrame);
    } else {
        InvalidateRect(win->hwndCanvas, NULL, TRUE);
        UpdateWindow(win->hwndCanvas);

        InvalidateRect(win->sideBar()->hwndSidebarSplitter, NULL, TRUE);
        UpdateWindow(win->sideBar()->hwndSidebarSplitter);
    }
}

// TODO: the layout logic here is similar to what we do in SetSidebarVisibility()
// would be nice to consolidate.
static void ResizeFav(WindowInfo *win)
{
    POINT pcur;
    GetCursorPosInHwnd(win->sideBar()->hwndSidebar, pcur);
    int topDy = pcur.y - (SPLITTER_DY - 1) / 2; // without splitter

    ClientRect rTop(win->sideBar()->hwndSidebarTop);
    ClientRect rBottom(win->sideBar()->hwndSidebarBottom);
    assert(rTop.dx == rBottom.dx);
    ClientRect rSidebar(win->sideBar()->hwndSidebar);
    int topDx = rTop.dx;

    // make sure to keep this in sync with the calculations in SetSidebarVisibility
    if (topDy < min(TOC_MIN_DY, rTop.dy) ||
        topDy > max(rSidebar.dy - TOC_MIN_DY - SPLITTER_DY, rTop.dy)) {
        SetCursor(gCursorNo);
        return;
    }

    SetCursor(gCursorSizeNS);

    int totalDy = rSidebar.dy;
    // rToc.y is always 0, as rToc is a ClientRect, so we first have
    // to convert it into coordinates relative to hwndFrame:
    assert(MapRectToWindow(rTop, win->sideBar()->hwndSidebarTop,  win->sideBar()->hwndSidebar).y == 0);
    //assert(totalDy == (rToc.dy + rFav.dy));
    int bottomDy = totalDy - topDy - SPLITTER_DY;
    assert(bottomDy >= 0);

    HDWP hdwp = BeginDeferWindowPos(3);

    DeferWindowPos(hdwp, win->sideBar()->hwndSidebarTop, NULL, 0, 0, topDx, topDy, SWP_NOZORDER);
    DeferWindowPos(hdwp, win->sideBar()->hwndFavSplitter, NULL, 0, topDy, topDx, SPLITTER_DY, SWP_NOZORDER);
    DeferWindowPos(hdwp, win->sideBar()->hwndSidebarBottom, NULL, 0, topDy + SPLITTER_DY, topDx, bottomDy, SWP_NOZORDER);

    EndDeferWindowPos(hdwp);

    //InvalidateRect(win->panel->WIN->hwndFrame, NULL, TRUE);
    //UpdateWindow(win->panel->WIN->hwndFrame);

    gGlobalPrefs.tocDy = topDy;
}

static void FavSplitterOnPaint(HWND hwnd)
{
    int dx = ClientRect(hwnd).dx;
    int dy = ClientRect(hwnd).dy;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    rc.left = 0;
    rc.right = dx;
    rc.top = 1;
    rc.bottom = dy;
    FillRect(hdc, &rc, gBrushSidebarSplitterBg);

    rc.left = 0;
    rc.right = dx;
    rc.top = 0;
    rc.bottom = 1;
    FillRect(hdc, &rc, gBrushSidebarSplitterEdgeBg);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProcFavSplitter(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case WM_MOUSEMOVE:
            if (hwnd == GetCapture()) {
                ResizeFav(win);
                return 0;
            }
            break;
        case WM_PAINT:
            FavSplitterOnPaint(hwnd);
            return 0;
            break;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            break;
        case WM_LBUTTONUP:
            ReleaseCapture();
            break;
        case WM_ERASEBKGND:
            return 1;
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static void PanelSplitterOnPaint(HWND hwnd)
{
    ContainerInfo *container = FindContainerInfoByHwnd(hwnd);

    int dx = ClientRect(hwnd).dx;
    int dy = ClientRect(hwnd).dy;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    rc.left = 0;
    rc.right = dx;
    rc.top = 0;
    rc.bottom = dy;
    FillRect(hdc, &rc, gBrushPanelSplitterBg);

    if (!container->isSplitVertical) {
        rc.left = 0;
        rc.right = dx;
        rc.top = 0;
        rc.bottom = 1;
        FillRect(hdc, &rc, gBrushPanelSplitterEdgeBg);

        rc.left = 0;
        rc.right = dx;
        rc.top = dy - 1;
        rc.bottom = dy;
        FillRect(hdc, &rc, gBrushPanelSplitterEdgeBg);
    } else {
        rc.left = 0;
        rc.right = 1;
        rc.top = 0;
        rc.bottom = dy;
        FillRect(hdc, &rc, gBrushPanelSplitterEdgeBg);

        rc.left = dx - 1;
        rc.right = dx;
        rc.top = 0;
        rc.bottom = dy;
        FillRect(hdc, &rc, gBrushPanelSplitterEdgeBg);

        if (!gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && container->isAtTop) {
            rc.left = 0;
            rc.right = dx;
            rc.top = 0;
            rc.bottom = 1;
            FillRect(hdc, &rc, gBrushSepLineBg);
        }
    }

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProcPanelSplitter(HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam)
{
    ContainerInfo *container = FindContainerInfoByHwnd(hwnd);
    if (!container)
        return DefWindowProc(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_MOUSEMOVE:
        if (container->isSplitVertical)
            SetCursor(gCursorSizeWE);
        else
            SetCursor(gCursorSizeNS);

        if (hwnd == GetCapture()) {
            ResizePanel(container);
            return 0;
        }
        break;
    case WM_PAINT:
        PanelSplitterOnPaint(hwnd);
        return 0;
        break;
    case WM_LBUTTONDOWN:
        if (container->isSplitVertical)
            SetCursor(gCursorSizeWE);
        else
            SetCursor(gCursorSizeNS);
        SetCapture(hwnd);
        break;
    case WM_LBUTTONUP:
        ReleaseCapture();
        break;
    case WM_ERASEBKGND:
        return 1;
        break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static void SidebarSplitterOnPaint(HWND hwnd)
{
    // Using memDC doesn't solve flicker.

    int dx = ClientRect(hwnd).dx;
    int dy = ClientRect(hwnd).dy;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    // Interior
    RECT rc;
    rc.left = 1;
    rc.right = dx - 1;
    rc.top = 1;
    rc.bottom = dy;
    FillRect(hdc, &rc, gBrushSidebarSplitterBg);

    // Left boundary
    rc.left = 0;
    rc.right = 1;
    rc.top = 1;
    rc.bottom = dy;
    FillRect(hdc, &rc, gBrushSidebarSplitterEdgeBg);

    // Right boundary
    rc.left = dx - 1;
    rc.right = dx;
    rc.top = 1;
    rc.bottom = dy;
    FillRect(hdc, &rc, gBrushSidebarSplitterEdgeBg);

    // Top boundary
    rc.left = 0;
    rc.right = dx;
    rc.top = 0;
    rc.bottom = 1;
    FillRect(hdc, &rc, gBrushSepLineBg);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProcSidebarSplitter(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case WM_MOUSEMOVE:
            if (hwnd == GetCapture()) {
                ResizeSidebar(win);
                return 0;
            }
            break;
        case WM_PAINT:
            SidebarSplitterOnPaint(hwnd);
            return 0;
            break;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            break;
        case WM_LBUTTONUP:
            ReleaseCapture();
            break;
        case WM_ERASEBKGND:
            return 1;
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

// A tree container, used for toc and favorites, is a window with following children:
// - title (id + 1)
// - close button (id + 2)
// - tree window (id + 3)
// This function lays out the child windows inside the container based
// on the container size.
void LayoutTreeContainer(HWND hwndContainer, int id)
{
    HWND hwndTree  = GetDlgItem(hwndContainer, id + 3);
    ClientRect rc(hwndContainer);
    MoveWindow(hwndTree, 0, 0, rc.dx, rc.dy, TRUE);
}

WNDPROC DefWndProcCloseButton = NULL;
// logic needed to track OnHover state of a close button by looking at
// WM_MOUSEMOVE and WM_MOUSELEAVE messages.
// We call it a button, but hwnd is really a static text.
// We persist the state by setting hwnd's text: if cursor is over the hwnd,
// text is "1", else it's something else.
LRESULT CALLBACK WndProcCloseButton(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    bool stateChanged = false;
    if (WM_MOUSEMOVE == msg) {
        ScopedMem<WCHAR> s(win::GetText(hwnd));
        if (!str::Eq(s, BUTTON_HOVER_TEXT)) {
            win::SetText(hwnd, BUTTON_HOVER_TEXT);
            stateChanged = true;
            // ask for WM_MOUSELEAVE notifications
            TRACKMOUSEEVENT tme = { 0 };
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
    } else if (WM_MOUSELEAVE == msg) {
        win::SetText(hwnd, L"");
        stateChanged = true;
    } else if (WM_ERASEBKGND == msg) {
        return FALSE;
    }

    if (stateChanged) {
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    }

    return CallWindowProc(DefWndProcCloseButton, hwnd, msg, wParam, lParam);
}

static void SetWinPos(HWND hwnd, RectI r, bool isVisible)
{
    UINT flags = SWP_NOZORDER | (isVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
    SetWindowPos(hwnd, NULL, r.x, r.y, r.dx, r.dy, flags);
}

void SetSidebarVisibility(WindowInfo *win, bool tocVisible, bool favVisible, bool switchDoc)
{
    if (gPluginMode || !HasPermission(Perm_DiskAccess))
        favVisible = false;

    if (!win->IsDocLoaded() || !win->dm->HasTocTree())
        tocVisible = false;

    if (PM_BLACK_SCREEN == win->presentation || PM_WHITE_SCREEN == win->presentation) {
        tocVisible = false;
        favVisible = false;
    }

    if (tocVisible)
        LoadTocTree(win);
    if (favVisible)
        PopulateFavTreeIfNeeded(win);

    win->tocVisible = tocVisible;
    gGlobalPrefs.favVisible = favVisible;

    bool sidebarVisible = tocVisible || favVisible;

    TopWindowInfo *WIN = FindTopWindowInfoByHwnd(win->hwndCanvas);

    // If using only one sidebar, which is a child of hwndFrame.
    if (!gGlobalPrefs.sidebarForEachPanel) {
        for (size_t i = 0; i < WIN->gPanel.Count(); i++) {
            PanelInfo *panel = WIN->gPanel.At(i);
            for (size_t j = 0; j < panel->gWin.Count(); j++) {
                WindowInfo *win = panel->gWin.At(j);
                if (win->IsDocLoaded() && win->tocVisible)
                    sidebarVisible = true;
            }
        }
    }

    // Determine the geometry of the toolbar and the tab control.
    int toolbarDy = -1;
    int tabControlDy = 0;

    if (!gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && win->panel->container->isAtTop)
        toolbarDy = 0;

    if (gGlobalPrefs.toolbarVisible && (gGlobalPrefs.toolbarForEachPanel == gGlobalPrefs.sidebarForEachPanel) && !win->fullScreen && !win->presentation)
        toolbarDy = WindowRect(win->toolBar()->hwndReBar).dy;

    if (gGlobalPrefs.sidebarForEachPanel && gGlobalPrefs.tabVisible)
        tabControlDy = TAB_CONTROL_DY;

    HWND hwndParentForToolbar = win->panel->WIN->hwndFrame;
    if (gGlobalPrefs.toolbarForEachPanel)
        hwndParentForToolbar = win->panel->hwndPanel;

    HWND hwndParentForSidebar = win->panel->WIN->hwndFrame;
    if (gGlobalPrefs.sidebarForEachPanel)
        hwndParentForSidebar = win->panel->hwndPanel;

    HWND hwnd = WIN->container->hwndContainer;
    if (switchDoc || gGlobalPrefs.sidebarForEachPanel)
        hwnd = win->hwndCanvas;

    ClientRect rParentForToolbar(hwndParentForToolbar);
    ClientRect rParentForSidebar(hwndParentForSidebar);

    int y;
    int sidebar_y = y = toolbarDy + tabControlDy;
    if (gGlobalPrefs.sidebarForEachPanel)
        y++;

    int sidebar_Dy = rParentForToolbar.dy - sidebar_y;
    int Dy = rParentForToolbar.dy - y;

    if (switchDoc && !gGlobalPrefs.sidebarForEachPanel) {
        ClientRect rPanel(win->panel->hwndPanel);
        SetWindowPos(hwnd, NULL, 0, TAB_CONTROL_DY + 1, rPanel.dx, rPanel.dy - 0 - TAB_CONTROL_DY - 1, SWP_NOZORDER);
        return;
    }

    if (!sidebarVisible) {
        if (GetFocus() == win->sideBar()->hwndTocTree || GetFocus() == win->sideBar()->hwndFavTree)
            SetFocus(win->panel->WIN->hwndFrame);

        SetWindowPos(hwnd, NULL, 0, y, rParentForSidebar.dx, Dy, SWP_NOZORDER);
        ShowWindow(win->sideBar()->hwndSidebar, SW_HIDE);
        ShowWindow(win->sideBar()->hwndSidebarSplitter, SW_HIDE);
        return;
    }

    if (rParentForSidebar.IsEmpty()) {
        // don't adjust the ToC sidebar size while the window is minimized
        if (win->tocVisible)
            UpdateTocSelection(win, win->dm->CurrentPageNo());
        return;
    }

    ClientRect sidebarRc(win->sideBar()->hwndSidebar);
    int sidebarDx = sidebarRc.dx;
    if (sidebarDx == 0) {
        // TODO: use saved panelDx from saved preferences
        sidebarDx = rParentForSidebar.dx / 4;
    }

    // make sure that the sidebar is never too wide or too narrow
    // (when changing these values, also adjust ResizeSidebar() and ResizeFav())
    // TODO: we should also limit minimum size of the frame or else
    // limitValue() blows up with an assert() if frame.dx / 2 < SIDEBAR_MIN_WIDTH
    sidebarDx = limitValue(sidebarDx, SIDEBAR_MIN_WIDTH, rParentForSidebar.dx / 2);

    HDWP hdwp = BeginDeferWindowPos(3);

    DeferWindowPos(hdwp, win->sideBar()->hwndSidebar, NULL, 0, sidebar_y, sidebarDx, sidebar_Dy, SWP_NOZORDER);
    DeferWindowPos(hdwp, win->sideBar()->hwndSidebarSplitter, NULL, sidebarDx, sidebar_y, SPLITTER_DX, sidebar_Dy, SWP_NOZORDER);
    DeferWindowPos(hdwp, hwnd, NULL, sidebarDx + SPLITTER_DX, y, rParentForSidebar.dx - sidebarDx - SPLITTER_DX, Dy,  SWP_NOZORDER);

    EndDeferWindowPos(hdwp);

    ShowWindow(win->sideBar()->hwndSidebar, SW_SHOW);
    ShowWindow(win->sideBar()->hwndSidebarSplitter, SW_SHOW);

    InvalidateRect(win->sideBar()->hwndSidebar, NULL, TRUE);
    UpdateWindow(win->sideBar()->hwndSidebar);
}

void SplitPanel(ContainerInfo *container, WCHAR const *direction)
{
    WindowInfo *win = CreatePanel(container);

    int dx = ClientRect(container->hwndContainer).dx;
    int dy = ClientRect(container->hwndContainer).dy;
    int dx_1 = (dx - SPLITTER_DX) / 2;
    int dy_1 = (dy - SPLITTER_DY) / 2;

    if (str::Eq(direction, L"Vertically")) {
        container->isSplitVertical = true;
        if (container->isAtTop) {
            container->container1->isAtTop = true;
            container->container2->isAtTop = true;
        }
        SetWindowPos(container->container1->hwndContainer, NULL, 0, 0, dx_1, dy, SWP_NOZORDER);
        SetWindowPos(container->hwndSplitter, NULL, dx_1, 0, SPLITTER_DX, dy, SWP_NOZORDER);
        SetWindowPos(container->container2->hwndContainer, NULL, dx_1 + SPLITTER_DX, 0, dx - dx_1 - SPLITTER_DX, dy, SWP_NOZORDER);
    } else if (str::Eq(direction, L"Horizontally")) {
        container->isSplitVertical = false;
        if (container->isAtTop)
            container->container1->isAtTop = true;
        SetWindowPos(container->container1->hwndContainer, NULL, 0, 0, dx, dy_1, SWP_NOZORDER);
        SetWindowPos(container->hwndSplitter, NULL, 0, dy_1, dx, SPLITTER_DY, SWP_NOZORDER);
        SetWindowPos(container->container2->hwndContainer, NULL, 0, dy_1 + SPLITTER_DY, dx, dy - dy_1 - SPLITTER_DY, SWP_NOZORDER);
    }
}

void ShowDocument(PanelInfo *panel, WindowInfo *win,  WindowInfo *winNew, bool HideOldDocument)
{
    if (win == winNew)
        return;

    if (win->fullScreen || win->presentation)
        ExitFullscreen(*win);

    panel->win = winNew;

    // We need to remember the tocState and this works.
    UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(win));

    ClearTocBox(win);

    // Should we put this into SetSidebarVisibility()?
    //if (winNew->IsDocLoaded() && winNew->dm->HasTocTree())
    //    LoadTocTree(winNew);

    // I think it always needs to hide the old document.
    if (HideOldDocument) {
        ShowWindow(win->hwndCanvas, SW_HIDE);
        UpdateWindow(win->hwndCanvas);
    }
    
    // If sidebarForEachPanel, we need to use the original SetSidebarVisibility.
    // Only if not sidebarForEachPanel, we need to resize win->hwndCanvas manually.
    SetSidebarVisibility(winNew, winNew->tocVisible, gGlobalPrefs.favVisible, !gGlobalPrefs.sidebarForEachPanel);
    ShowWindow(winNew->hwndCanvas, SW_SHOW);

    // We have a document opened. Then press "Ctrl + T" to open a
    // "Start Page" in a new tab. Go back to the original document.
    // Click on the FindBox. Then use "Ctrl + Arrow" to switch to the
    // "Start Page".
    // Without the following code, we get stuck, i.e "Ctrl + Arrow"
    // doesn't work anymore.

    if (!winNew->IsDocLoaded())
        SetFocus(panel->WIN->hwndFrame);
    //else if (winNew->IsChm() && GetFocus() == panel->hwndFindBox)
    //    SetFocus(winNew->hwndCanvas);

    if (winNew->IsDocLoaded()) {
        int pageCount = winNew->dm->PageCount();
        UpdateToolbarPageText(winNew, pageCount); // Change positions. Update page count (Page Total).
        ScrollState ss = winNew->dm->GetScrollState();
        winNew->dm->SetScrollState(ss); // Update page number (PageBox).
    } else {
        UpdateToolbarPageText(winNew, 0); // Change positions. Update page count (Page Total).
    }

    UpdateToolbarFindText(winNew); // Update ToolbarFindText (label)'s position.
    UpdateFindbox(winNew); // Enable or disable FindBox and change cursor.
    UpdateToolbarAndScrollbarState(*winNew); // We need it to update the toolbar buttons. // Fix chm <-> pdf issue. (?)

    // With the code below, ShowDocument() is still called only once when we use
    // keyboard to switch between tab pages. This is because using
    // TCM_SETCURSEL to change selection won't generate TCN_SELCHANGE notification code.
    // See Remarks in http://msdn.microsoft.com/en-us/library/windows/desktop/bb760612%28v=vs.85%29.aspx
    SendMessage(panel->hwndTab, TCM_SETCURSEL, panel->gWin.Find(winNew), NULL);

    // Update title;
    win::SetText(winNew->panel->WIN->hwndFrame, winNew->title);

    RebuildMenuBarForWindow(winNew);
}

void ShowPreviousDocument(PanelInfo *panel){
    WindowInfo *win = panel->gWin.At(max(panel->gWin.Find(panel->win) - 1, 0));
    ShowDocument(panel, panel->win, win, true);
}

void ShowNextDocument(PanelInfo *panel){
    WindowInfo *win = panel->gWin.At(min(panel->gWin.Find(panel->win) + 1, (int) panel->gWin.Count() - 1));
    ShowDocument(panel, panel->win, win, true);
}

void OpenNewTab(PanelInfo *panel) {
    WindowInfo *winOld = panel->win;
    WindowInfo *win = CreateCanvas(panel);
}

static LRESULT OnSetCursor(WindowInfo& win, HWND hwnd)
{
    POINT pt;

    if (win.IsAboutWindow()) {
        if (GetCursorPosInHwnd(hwnd, pt)) {
            StaticLinkInfo linkInfo;
            if (GetStaticLink(win.staticLinks, pt.x, pt.y, &linkInfo)) {
                win.CreateInfotip(linkInfo.infotip, linkInfo.rect);
                SetCursor(gCursorHand);
                return TRUE;
            }
        }
    }
    if (!win.IsDocLoaded()) {
        win.DeleteInfotip();
        return FALSE;
    }

    if (win.mouseAction != MA_IDLE)
        win.DeleteInfotip();

    switch (win.mouseAction) {
        case MA_DRAGGING:
        case MA_DRAGGING_RIGHT:
            SetCursor(gCursorDrag);
            return TRUE;
        case MA_SCROLLING:
            SetCursor(gCursorScroll);
            return TRUE;
        case MA_SELECTING_TEXT:
            SetCursor(gCursorIBeam);
            return TRUE;
        case MA_SELECTING:
            break;
        case MA_IDLE:
            if (GetCursor() && GetCursorPosInHwnd(hwnd, pt)) {
                PointI pti(pt.x, pt.y);
                PageElement *pageEl = win.dm->GetElementAtPos(pti);
                if (pageEl) {
                    ScopedMem<WCHAR> text(pageEl->GetValue());
                    RectI rc = win.dm->CvtToScreen(pageEl->GetPageNo(), pageEl->GetRect());
                    win.CreateInfotip(text, rc, true);

                    bool isLink = pageEl->GetType() == Element_Link;
                    delete pageEl;

                    if (isLink) {
                        SetCursor(gCursorHand);
                        return TRUE;
                    }
                }
                else
                    win.DeleteInfotip();
                if (win.dm->IsOverText(pti))
                    SetCursor(gCursorIBeam);
                else
                    SetCursor(gCursorArrow);
                return TRUE;
            }
            win.DeleteInfotip();
            break;
    }
    if (win.presentation)
        return TRUE;
    return FALSE;
}

static void OnTimer(WindowInfo& win, HWND hwnd, WPARAM timerId)
{
    POINT pt;

    switch (timerId) {
    case REPAINT_TIMER_ID:
        win.delayedRepaintTimer = 0;
        KillTimer(hwnd, REPAINT_TIMER_ID);
        win.RedrawAll();
        break;

    case SMOOTHSCROLL_TIMER_ID:
        if (MA_SCROLLING == win.mouseAction)
            win.MoveDocBy(win.xScrollSpeed, win.yScrollSpeed);
        else if (MA_SELECTING == win.mouseAction || MA_SELECTING_TEXT == win.mouseAction) {
            GetCursorPosInHwnd(win.hwndCanvas, pt);
            if (NeedsSelectionEdgeAutoscroll(&win, pt.x, pt.y))
                OnMouseMove(win, pt.x, pt.y, MK_CONTROL);
        }
        else {
            KillTimer(hwnd, SMOOTHSCROLL_TIMER_ID);
            win.yScrollSpeed = 0;
            win.xScrollSpeed = 0;
        }
        break;

    case HIDE_CURSOR_TIMER_ID:
        KillTimer(hwnd, HIDE_CURSOR_TIMER_ID);
        if (win.presentation)
            SetCursor(NULL);
        break;

    case HIDE_FWDSRCHMARK_TIMER_ID:
        win.fwdSearchMark.hideStep++;
        if (1 == win.fwdSearchMark.hideStep) {
            SetTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID, HIDE_FWDSRCHMARK_DECAYINTERVAL_IN_MS, NULL);
        }
        else if (win.fwdSearchMark.hideStep >= HIDE_FWDSRCHMARK_STEPS) {
            KillTimer(hwnd, HIDE_FWDSRCHMARK_TIMER_ID);
            win.fwdSearchMark.show = false;
            win.RepaintAsync();
        }
        else
            win.RepaintAsync();
        break;

    case AUTO_RELOAD_TIMER_ID:
        KillTimer(hwnd, AUTO_RELOAD_TIMER_ID);
        ReloadDocument(&win, true);
        break;

    default:
        OnStressTestTimer(&win, (int)timerId);
        break;
    }
}

// these can be global, as the mouse wheel can't affect more than one window at once
static int  gDeltaPerLine = 0;         // for mouse wheel logic
static bool gWheelMsgRedirect = false; // set when WM_MOUSEWHEEL has been passed on (to prevent recursion)
static bool gSuppressAltKey = false;   // set after scrolling horizontally (to prevent the menu from getting the focus)

static LRESULT CanvasOnMouseWheel(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!win.IsDocLoaded())
        return 0;

    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win.tocVisible && IsCursorOverWindow(win.sideBar()->hwndTocTree) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessage(win.sideBar()->hwndTocTree, message, wParam, lParam);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wParam);

    // Note: not all mouse drivers correctly report the Ctrl key's state
    if ((LOWORD(wParam) & MK_CONTROL) || IsCtrlPressed() || (LOWORD(wParam) & MK_RBUTTON)) {
        POINT pt;
        GetCursorPosInHwnd(win.hwndCanvas, pt);

        float zoom = win.dm->NextZoomStep(delta < 0 ? ZOOM_MIN : ZOOM_MAX);
        win.dm->ZoomTo(zoom, &PointI(pt.x, pt.y));
        UpdateToolbarState(&win);

        // don't show the context menu when zooming with the right mouse-button down
        if ((LOWORD(wParam) & MK_RBUTTON))
            win.dragStartPending = false;

        return 0;
    }

    // make sure to scroll whole pages in non-continuous Fit Content mode
    if (!IsContinuous(win.dm->GetDisplayMode()) &&
        ZOOM_FIT_CONTENT == win.dm->ZoomVirtual()) {
        if (delta > 0)
            win.dm->GoToPrevPage(0);
        else
            win.dm->GoToNextPage(0);
        return 0;
    }

    if (gDeltaPerLine == 0)
        return 0;

    bool horizontal = (LOWORD(wParam) & MK_ALT) || IsAltPressed();
    if (horizontal)
        gSuppressAltKey = true;

    if (gDeltaPerLine < 0) {
        // scroll by (fraction of a) page
        SCROLLINFO si = { 0 };
        si.cbSize = sizeof(si);
        si.fMask  = SIF_PAGE;
        GetScrollInfo(win.hwndCanvas, horizontal ? SB_HORZ : SB_VERT, &si);
        if (horizontal)
            win.dm->ScrollXBy(-MulDiv(si.nPage, delta, WHEEL_DELTA));
        else
            win.dm->ScrollYBy(-MulDiv(si.nPage, delta, WHEEL_DELTA), true);
        return 0;
    }

    win.wheelAccumDelta += delta;
    int currentScrollPos = GetScrollPos(win.hwndCanvas, SB_VERT);

    while (win.wheelAccumDelta >= gDeltaPerLine) {
        if (horizontal)
            SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
        else
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEUP, 0);
        win.wheelAccumDelta -= gDeltaPerLine;
    }
    while (win.wheelAccumDelta <= -gDeltaPerLine) {
        if (horizontal)
            SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
        else
            SendMessage(win.hwndCanvas, WM_VSCROLL, SB_LINEDOWN, 0);
        win.wheelAccumDelta += gDeltaPerLine;
    }

    if (!horizontal && !IsContinuous(win.dm->GetDisplayMode()) &&
        GetScrollPos(win.hwndCanvas, SB_VERT) == currentScrollPos) {
        if (delta > 0)
            win.dm->GoToPrevPage(-1);
        else
            win.dm->GoToNextPage(0);
    }

    return 0;
}

static LRESULT CanvasOnMouseHWheel(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!win.IsDocLoaded())
        return 0;

    // Scroll the ToC sidebar, if it's visible and the cursor is in it
    if (win.tocVisible && IsCursorOverWindow(win.sideBar()->hwndTocTree) && !gWheelMsgRedirect) {
        // Note: hwndTocTree's window procedure doesn't always handle
        //       WM_MOUSEHWHEEL and when it's bubbling up, we'd return
        //       here recursively - prevent that
        gWheelMsgRedirect = true;
        LRESULT res = SendMessage(win.sideBar()->hwndTocTree, message, wParam, lParam);
        gWheelMsgRedirect = false;
        return res;
    }

    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    win.wheelAccumDelta += delta;

    while (win.wheelAccumDelta >= gDeltaPerLine) {
        SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINERIGHT, 0);
        win.wheelAccumDelta -= gDeltaPerLine;
    }
    while (win.wheelAccumDelta <= -gDeltaPerLine) {
        SendMessage(win.hwndCanvas, WM_HSCROLL, SB_LINELEFT, 0);
        win.wheelAccumDelta += gDeltaPerLine;
    }

    return TRUE;
}

static LRESULT OnGesture(WindowInfo& win, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!Touch::SupportsGestures())
        return DefWindowProc(win.hwndFrame, message, wParam, lParam);

    HGESTUREINFO hgi = (HGESTUREINFO)lParam;
    GESTUREINFO gi = { 0 };
    gi.cbSize = sizeof(GESTUREINFO);

    BOOL ok = Touch::GetGestureInfo(hgi, &gi);
    if (!ok || !win.IsDocLoaded()) {
        Touch::CloseGestureInfoHandle(hgi);
        return 0;
    }

    switch (gi.dwID) {
        case GID_ZOOM:
            if (gi.dwFlags != GF_BEGIN) {
                float zoom = (float)LODWORD(gi.ullArguments) / (float)win.touchState.startArg;
                ZoomToSelection(&win, zoom, false, true);
            }
            win.touchState.startArg = LODWORD(gi.ullArguments);
            break;

        case GID_PAN:
            // Flicking left or right changes the page,
            // panning moves the document in the scroll window
            if (gi.dwFlags == GF_BEGIN) {
                win.touchState.panStarted = true;
                win.touchState.panPos = gi.ptsLocation;
                win.touchState.panScrollOrigX = GetScrollPos(win.hwndCanvas, SB_HORZ);
            }
            else if (win.touchState.panStarted) {
                int deltaX = win.touchState.panPos.x - gi.ptsLocation.x;
                int deltaY = win.touchState.panPos.y - gi.ptsLocation.y;
                win.touchState.panPos = gi.ptsLocation;

                if ((gi.dwFlags & GF_INERTIA) && abs(deltaX) > abs(deltaY)) {
                    // Switch pages once we hit inertia in a horizontal direction
                    if (deltaX < 0)
                        win.dm->GoToPrevPage(0);
                    else if (deltaX > 0)
                        win.dm->GoToNextPage(0);
                    // When we switch pages, go back to the initial scroll position
                    // and prevent further pan movement caused by the inertia
                    win.dm->ScrollXTo(win.touchState.panScrollOrigX);
                    win.touchState.panStarted = false;
                }
                else {
                    // Pan/Scroll
                    win.MoveDocBy(deltaX, deltaY);
                }
            }
            break;

        case GID_ROTATE:
            // Rotate the PDF 90 degrees in one direction
            if (gi.dwFlags == GF_END) {
                // This is in radians
                double rads = GID_ROTATE_ANGLE_FROM_ARGUMENT(LODWORD(gi.ullArguments));
                // The angle from the rotate is the opposite of the Sumatra rotate, thus the negative
                double degrees = -rads * 180 / M_PI;

                // Playing with the app, I found that I often didn't go quit a full 90 or 180
                // degrees. Allowing rotate without a full finger rotate seemed more natural.
                if (degrees < -120 || degrees > 120)
                    win.dm->RotateBy(180);
                else if (degrees < -45)
                    win.dm->RotateBy(-90);
                else if (degrees > 45)
                    win.dm->RotateBy(90);
            }
            break;

        case GID_TWOFINGERTAP:
            // Two-finger tap toggles fullscreen mode
            OnMenuViewFullscreen(win);
            break;

        case GID_PRESSANDTAP:
            // Toggle between Fit Page, Fit Width and Fit Content (same as 'z')
            if (gi.dwFlags == GF_BEGIN)
                win.ToggleZoom();
            break;

        default:
            // A gesture was not recognized
            break;
    }

    Touch::CloseGestureInfoHandle(hgi);
    return 0;
}

static void ContainerOnSize(ContainerInfo* container, int dx, int dy)
{
    if (container->panel != NULL) {
        SetWindowPos(container->panel->hwndPanel, NULL, 0, 0, dx, dy, SWP_NOZORDER);
        // We should not use this. This is a bad solution to ensure that containers are resized.
        // SendMessage(container->panel->hwndPanel, WM_SIZE, NULL, MAKELPARAM(dx,dy));
    } else if (container->container1 != NULL && container->container2 != NULL) {

        int dx_1 = ClientRect(container->container1->hwndContainer).dx;
        int dy_1 = ClientRect(container->container1->hwndContainer).dy;

        dx_1 = limitValue(dx_1, (dx - SPLITTER_DX) / 4, 3 * (dx - SPLITTER_DX) / 4);
        dy_1 = limitValue(dy_1, (dy - SPLITTER_DY) / 4, 3 * (dy - SPLITTER_DY) / 4);

        HDWP hdwp = BeginDeferWindowPos(3);

        if (container->isSplitVertical) {
            DeferWindowPos(hdwp, container->container1->hwndContainer, NULL, 0, 0, dx_1, dy, SWP_NOZORDER);
            DeferWindowPos(hdwp, container->hwndSplitter, NULL, dx_1, 0, SPLITTER_DX, dy, SWP_NOZORDER);
            DeferWindowPos(hdwp, container->container2->hwndContainer, NULL, dx_1 + SPLITTER_DX, 0, dx - dx_1 - SPLITTER_DX, dy, SWP_NOZORDER);
        } else {
            DeferWindowPos(hdwp, container->container1->hwndContainer, NULL, 0, 0, dx, dy_1, SWP_NOZORDER);
            DeferWindowPos(hdwp, container->hwndSplitter, NULL, 0, dy_1, dx, SPLITTER_DY, SWP_NOZORDER);
            DeferWindowPos(hdwp, container->container2->hwndContainer, NULL, 0, dy_1 + SPLITTER_DY, dx, dy - dy_1 - SPLITTER_DY, SWP_NOZORDER);
        }

        EndDeferWindowPos(hdwp);
    }
}

static void PanelOnSize(PanelInfo* panel, int dx, int dy)
{
    int rebBarDy = -1;

    if (!gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && panel->container->isAtTop)
        rebBarDy = 0;

    if (gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && !(panel->win->presentation || panel->win->fullScreen)) {
        SetWindowPos(panel->win->toolBar()->hwndReBar, NULL, 0, 0, dx, 0, SWP_NOZORDER);
        rebBarDy = WindowRect(panel->toolBar->hwndReBar).dy;
    }

    int tabDy = 0;
    if (gGlobalPrefs.tabVisible)
        tabDy = TAB_CONTROL_DY;
    SetWindowPos(panel->hwndTab, NULL, 0, rebBarDy, dx, tabDy, SWP_NOZORDER);

    if (gGlobalPrefs.sidebarForEachPanel) {

        bool tocVisible = panel->win->tocLoaded && panel->win->tocVisible;
        if (tocVisible || gGlobalPrefs.favVisible) {
            SetSidebarVisibility(panel->win, tocVisible, gGlobalPrefs.favVisible);
            return;
        }
    }

    if (gGlobalPrefs.toolbarForEachPanel)
        SetWindowPos(panel->win->hwndCanvas, NULL, 0, rebBarDy + tabDy + 1, dx, dy - rebBarDy - tabDy - 1, SWP_NOZORDER);
    else
        SetWindowPos(panel->win->hwndCanvas, NULL, 0, rebBarDy + tabDy + 1, dx, dy - rebBarDy - tabDy - 1, SWP_NOZORDER);
}

static void ContainerOnPaint(ContainerInfo& container)
{
    int dx = ClientRect(container.hwndContainer).dx;
    int dy = ClientRect(container.hwndContainer).dy;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(container.hwndContainer, &ps);

    RECT rc;
    rc.left = 0;
    rc.right = dx;
    rc.top = 0 ;
    rc.bottom = dy;
    FillRect(hdc, &rc, gBrushNoDocBg);

    EndPaint(container.hwndContainer, &ps);
}

static LRESULT CustomDrawReBar(HWND hwnd, LPARAM lParam)
{
    LPNMCUSTOMDRAW lpnmcustom = (LPNMCUSTOMDRAW)lParam;

    switch(lpnmcustom->dwDrawStage) {

        case CDDS_PREPAINT:
        {
            HDC hdc = lpnmcustom->hdc;

            RECT rc;
            GetClientRect(hwnd, &rc);

            int dx = rc.right - rc.left;
            int dy = rc.bottom - rc.top;

            TRIVERTEX        vert[2];
            GRADIENT_RECT    gRect;

            vert[0].x      = 0;
            vert[0].y      = 0;
            vert[0].Red    = 0xEF00;
            vert[0].Green  = 0xEF00;
            vert[0].Blue   = 0xEF00;
            vert[0].Alpha  = 0x0000;

            vert[1].x      = dx;
            vert[1].y      = dy; 
            vert[1].Red    = 0xD900;
            vert[1].Green  = 0xD900;
            vert[1].Blue   = 0xD900;
            vert[1].Alpha  = 0x0000;

            gRect.UpperLeft  = 0;
            gRect.LowerRight = 1;

            GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

            return CDRF_NOTIFYITEMDRAW;
        }

        default:
            return CDRF_DODEFAULT;
    }

    return CDRF_DODEFAULT;
}

static LRESULT CustomDrawToolbar(HWND hwnd, LPARAM lParam)
{
    LPNMTBCUSTOMDRAW lpnmtbcustom = (LPNMTBCUSTOMDRAW)lParam;

    switch(lpnmtbcustom->nmcd.dwDrawStage) {

        case CDDS_PREPAINT:
        {
            HDC hdc = lpnmtbcustom->nmcd.hdc;

            RECT rc;
            GetClientRect(hwnd, &rc);

            int dx = rc.right - rc.left;
            int dy = rc.bottom - rc.top;

            TRIVERTEX        vert[2];
            GRADIENT_RECT    gRect;

            vert[0].x      = 0;
            vert[0].y      = 0;
            vert[0].Red    = 0xEE00;
            vert[0].Green  = 0xEE00;
            vert[0].Blue   = 0xEE00;
            vert[0].Alpha  = 0x0000;

            vert[1].x      = dx;
            vert[1].y      = dy; 
            vert[1].Red    = 0xD900;
            vert[1].Green  = 0xD900;
            vert[1].Blue   = 0xD900;
            vert[1].Alpha  = 0x0000;

            gRect.UpperLeft  = 0;
            gRect.LowerRight = 1;

            GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

            return CDRF_NOTIFYITEMDRAW;
        }

        default:
            return CDRF_DODEFAULT;
    }

    return CDRF_DODEFAULT;
}

static LRESULT CustomDrawInPanel(PanelInfo *panel, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LPNMHDR lpnmhdr = (LPNMHDR)lParam;
    HWND hwndFrom = lpnmhdr->hwndFrom;

    if (hwndFrom == panel->win->toolBar()->hwndReBar)
        return CustomDrawReBar(hwndFrom, lParam);
    else if (hwndFrom == panel->win->toolBar()->hwndToolbar)
        return CustomDrawToolbar(hwndFrom, lParam);

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT PanelOnNotify(PanelInfo *panel, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LPNMHDR lpnmhdr = (LPNMHDR)lParam;

    switch(lpnmhdr->code) {
    case TCN_SELCHANGE:
        if (IsCursorOverWindow(panel->hwndTab)) {
            int tabIndex = SendMessage(panel->hwndTab, TCM_GETCURSEL, NULL, NULL); // tabIndex is the new index.
            ShowDocument(panel, panel->win, panel->gWin.At(tabIndex), true); // panel->win is the current win before calling ShowDocument().

            //RECT rc;
            //SendMessage(panel->hwndTab, TCM_GETITEMRECT, tabIndex, (LPARAM)&rc); // For new selected tab item.
            //SetWindowPos(panel->gWin.At(tabIndex)->hwndTabStatic, NULL, rc.right - 16, 7, 12, 12, SWP_NOZORDER);
            //SendMessage(panel->hwndTab, TCM_GETITEMRECT, panel->gWin.Find(panel->win), (LPARAM)&rc); // For selected tab item before change.
            //SetWindowPos(panel->win->hwndTabStatic, NULL, rc.right - 16, 8, 12, 12, SWP_NOZORDER);
        }
        break;

    case TTN_GETDISPINFO:
        int tabIndex;
        tabIndex = (int) wParam;
        if (IsCursorOverWindow(panel->hwndTab)) {
            LPNMTTDISPINFO lpnmtdi = (LPNMTTDISPINFO)lParam;
            lpnmtdi->lpszText = panel->gWin.At(tabIndex)->TabToolTipText;
        }
        break;

    case NM_CUSTOMDRAW:
        return CustomDrawInPanel(panel, hwnd, msg, wParam, lParam);

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

static LRESULT PanelOnCommand(PanelInfo *panel, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = panel->win;
    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    int wmId = LOWORD(wParam);

    // most of them require a win, the few exceptions are no-ops
    switch (wmId)
    {
        case IDM_PRINT:
            OnMenuPrint(win);
            break;

        case IDT_VIEW_FIT_WIDTH:
            ChangeZoomLevel(win, ZOOM_FIT_WIDTH, true);
            break;

        case IDT_VIEW_FIT_PAGE:
            ChangeZoomLevel(win, ZOOM_FIT_PAGE, false);
            break;

        case IDT_VIEW_ZOOMIN:
            if (win->IsDocLoaded())
                ZoomToSelection(win, win->dm->NextZoomStep(ZOOM_MAX));
            break;

        case IDT_VIEW_ZOOMOUT:
            if (win->IsDocLoaded())
                ZoomToSelection(win, win->dm->NextZoomStep(ZOOM_MIN));
            break;

        case IDM_GOTO_NEXT_PAGE:
            if (win->IsDocLoaded())
                win->dm->GoToNextPage(0);
            break;

        case IDM_GOTO_PREV_PAGE:
            if (win->IsDocLoaded())
                win->dm->GoToPrevPage(0);
            break;

        case IDM_GOTO_PAGE:
            OnMenuGoToPage(*win);
            break;

        case IDM_FIND_FIRST:
            OnMenuFind(win);
            break;

        case IDM_FIND_NEXT:
            OnMenuFindNext(win);
            break;

        case IDM_FIND_PREV:
            OnMenuFindPrev(win);
            break;

        case IDM_FIND_MATCH:
            OnMenuFindMatchCase(win);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static void PanelOnPaint(PanelInfo& panel)
{
    //int rebBarDy = WindowRect(panel.win->toolBar()->hwndReBar).dy;
    //if (!gGlobalPrefs.toolbarVisible || !gGlobalPrefs.toolbarForEachPanel)
    //    rebBarDy = -1;

    int rebBarDy = -1;

    if (!gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && panel.container->isAtTop)
        rebBarDy = 0;

    if (gGlobalPrefs.toolbarForEachPanel && gGlobalPrefs.toolbarVisible && !(panel.win->presentation || panel.win->fullScreen))
        rebBarDy = WindowRect(panel.toolBar->hwndReBar).dy;

    int tabDy = 0;
    if (gGlobalPrefs.tabVisible)
        tabDy = TAB_CONTROL_DY;

    int dx = ClientRect(panel.hwndPanel).dx;
    int dy = ClientRect(panel.hwndPanel).dy;

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(panel.hwndPanel, &ps);

    RECT rc;

    rc.left = 0;
    rc.right = dx;
    rc.top = rebBarDy + tabDy;
    rc.bottom = rebBarDy + tabDy + 1;
    FillRect(hdc, &rc, gBrushSepLineBg);

    EndPaint(panel.hwndPanel, &ps);
}

static LRESULT CALLBACK WndProcContainer(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ContainerInfo *container = FindContainerInfoByHwnd(hwnd);
    if (!container)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            ContainerOnSize(container, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            break;

        case WM_PAINT:
            ContainerOnPaint(*container);
            break;

        case WM_ERASEBKGND:
            return 1;
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static LRESULT CALLBACK WndProcPanel(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PanelInfo *panel = FindPanelInfoByHwnd(hwnd);
    if (!panel)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            PanelOnSize(panel, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            break;

        // When LBUTTONDOWN on a panel, change WIN->panel.
        case WM_MOUSEACTIVATE:            
            panel->WIN->panel = panel;
            win::SetText(panel->WIN->hwndFrame, panel->win->title);
            break;

        case WM_NOTIFY:
            return PanelOnNotify(panel, hwnd, msg, wParam, lParam);

        case WM_COMMAND:
            return PanelOnCommand(panel, hwnd, msg, wParam, lParam);

        case WM_PAINT:
            PanelOnPaint(*panel);
            break;

        case WM_ERASEBKGND:
            return 1;
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static LRESULT CALLBACK WndProcCanvas(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // messages that don't require win
    switch (msg) {
        case WM_DROPFILES:
            CrashIf(lParam != 0 && lParam != 1);
            OnDropFiles((HDROP)wParam, !lParam);
            break;

        case WM_ERASEBKGND:
            // do nothing, helps to avoid flicker
            return TRUE;
    }

    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    // messages that require win
    switch (msg) {
        case WM_VSCROLL:
            OnVScroll(*win, wParam);
            break;

        case WM_HSCROLL:
            OnHScroll(*win, wParam);
            break;

        case WM_MOUSEMOVE:
            OnMouseMove(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDBLCLK:
            OnMouseLeftButtonDblClk(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONDOWN:
            OnMouseLeftButtonDown(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_LBUTTONUP:
            OnMouseLeftButtonUp(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_MBUTTONDOWN:
            if (win->IsDocLoaded()) {
                SetTimer(hwnd, SMOOTHSCROLL_TIMER_ID, SMOOTHSCROLL_DELAY_IN_MS, NULL);
                // TODO: Create window that shows location of initial click for reference
                OnMouseMiddleButtonDown(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            }
            break;

        case WM_RBUTTONDBLCLK:
            OnMouseRightButtonDblClick(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_RBUTTONDOWN:
            OnMouseRightButtonDown(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_RBUTTONUP:
            OnMouseRightButtonUp(*win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
            break;

        case WM_CONTEXTMENU:
            if (win->IsDocLoaded())
                OnContextMenu(win, 0, 0);
            else
                OnAboutContextMenu(win, 0, 0);
            break;

        case WM_SETCURSOR:
            if (OnSetCursor(*win, hwnd))
                return TRUE;
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_TIMER:
            OnTimer(*win, hwnd, wParam);
            break;

        case WM_PAINT:
            OnPaint(*win);
            break;

        case WM_SIZE:
            if (!IsIconic(win->hwndFrame))
                win->UpdateCanvasSize();
            break;

        case WM_MOUSEWHEEL:
            return CanvasOnMouseWheel(*win, msg, wParam, lParam);

        case WM_MOUSEHWHEEL:
            return CanvasOnMouseHWheel(*win, msg, wParam, lParam);

        case WM_GESTURE:
            return OnGesture(*win, msg, wParam, lParam);

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static LRESULT CALLBACK WndProcSidebar(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProc(WndProcSidebarCB, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcSidebarTop(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProc(WndProcSidebarTopCB, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcSidebarBottom(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProc(WndProcSidebarBottomCB, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProc(WndProcTocBoxCB, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return CallWindowProc(WndProcFavBoxCB, hwnd, msg, wParam, lParam);
}

class RepaintCanvasTask : public UITask
{
    UINT delay;
    WindowInfo *win;

public:
    RepaintCanvasTask(WindowInfo *win, UINT delay)
        : win(win), delay(delay) {
        name = "RepaintCanvasTask";
    }

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        if (!delay)
            WndProcCanvas(win->hwndCanvas, WM_TIMER, REPAINT_TIMER_ID, 0);
        else if (!win->delayedRepaintTimer)
            win->delayedRepaintTimer = SetTimer(win->hwndCanvas, REPAINT_TIMER_ID, delay, NULL);
    }
};

void WindowInfo::RepaintAsync(UINT delay)
{
    // even though RepaintAsync is mostly called from the UI thread,
    // we depend on the repaint message to happen asynchronously
    uitask::Post(new RepaintCanvasTask(this, delay));
}

// Tests that various ways to crash will generate crash report.
// Commented-out because they are ad-hoc. Left in code because
// I don't want to write them again if I ever need to test crash reporting
#if 0
#include <signal.h>
static void TestCrashAbort()
{
    raise(SIGABRT);
}

struct Base;
void foo(Base* b);

struct Base {
    Base() {
        foo(this);
    }
    virtual void pure() = 0;
};
struct Derived : public Base {
    void pure() { }
};

void foo(Base* b) {
    b->pure();
}

static void TestCrashPureCall()
{
    Derived d; // should crash
}

// tests that making a big allocation with new raises an exception
static int TestBigNew()
{
    size_t size = 1024*1024*1024*1;  // 1 GB should be out of reach
    char *mem = (char*)1;
    while (mem) {
        mem = new char[size];
    }
    // just some code so that compiler doesn't optimize this code to null
    for (size_t i = 0; i < 1024; i++) {
        mem[i] = i & 0xff;
    }
    int res = 0;
    for (size_t i = 0; i < 1024; i++) {
        res += mem[i];
    }
    return res;
}
#endif

static LRESULT FrameOnCommand(WindowInfo *win, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int wmId = LOWORD(wParam);

    // check if the menuId belongs to an entry in the list of
    // recently opened files and load the referenced file if it does
    if ((wmId >= IDM_FILE_HISTORY_FIRST) && (wmId <= IDM_FILE_HISTORY_LAST))
    {
        DisplayState *state = gFileHistory.Get(wmId - IDM_FILE_HISTORY_FIRST);
        if (state && HasPermission(Perm_DiskAccess)) {
            LoadArgs args(state->filePath, win);
            LoadDocument(args);
        }
        return 0;
    }

    if (win && IDM_OPEN_WITH_EXTERNAL_FIRST <= wmId && wmId <= IDM_OPEN_WITH_EXTERNAL_LAST)
    {
        ViewWithExternalViewer(wmId - IDM_OPEN_WITH_EXTERNAL_FIRST, win->loadedFilePath, win->dm ? win->dm->CurrentPageNo() : 0);
        return 0;
    }

    // 10 submenus max with 10 items each max (=100) plus generous buffer => 200
    STATIC_ASSERT(IDM_FAV_LAST - IDM_FAV_FIRST == 200, enough_fav_menu_ids);
    if ((wmId >= IDM_FAV_FIRST) && (wmId <= IDM_FAV_LAST))
    {
        GoToFavoriteByMenuId(win, wmId);
    }

    if (!win)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    // most of them require a win, the few exceptions are no-ops
    switch (wmId)
    {
        case IDM_OPEN:
        case IDT_FILE_OPEN:
            OnMenuOpen(SumatraWindow::Make(win));
            break;
        case IDM_SAVEAS:
            OnMenuSaveAs(*win);
            break;

        case IDM_RENAME_FILE:
            OnMenuRenameFile(*win);
            break;

        case IDT_FILE_PRINT:
        case IDM_PRINT:
            OnMenuPrint(win);
            break;

        case IDT_FILE_EXIT:
        case IDM_CLOSE:
            // close the document and its window, unless it's the last window
            // in which case we close the document but convert the window
            // to about window
            CloseWindow(win, false);
            break;

        case IDM_EXIT:
            OnMenuExit(win->panel->WIN, win->panel->WIN->hwndFrame);
            break;

        case IDM_REFRESH:
            ReloadDocument(win);
            break;

        case IDM_SAVEAS_BOOKMARK:
            OnMenuSaveBookmark(*win);
            break;

        case IDT_VIEW_FIT_WIDTH:
            ChangeZoomLevel(win, ZOOM_FIT_WIDTH, true);
            break;

        case IDT_VIEW_FIT_PAGE:
            ChangeZoomLevel(win, ZOOM_FIT_PAGE, false);
            break;

        case IDT_VIEW_ZOOMIN:
            if (win->IsDocLoaded())
                ZoomToSelection(win, win->dm->NextZoomStep(ZOOM_MAX), false);
            break;

        case IDT_VIEW_ZOOMOUT:
            if (win->IsDocLoaded())
                ZoomToSelection(win, win->dm->NextZoomStep(ZOOM_MIN), false);
            break;

        case IDM_ZOOM_6400:
        case IDM_ZOOM_3200:
        case IDM_ZOOM_1600:
        case IDM_ZOOM_800:
        case IDM_ZOOM_400:
        case IDM_ZOOM_200:
        case IDM_ZOOM_150:
        case IDM_ZOOM_125:
        case IDM_ZOOM_100:
        case IDM_ZOOM_50:
        case IDM_ZOOM_25:
        case IDM_ZOOM_12_5:
        case IDM_ZOOM_8_33:
        case IDM_ZOOM_FIT_PAGE:
        case IDM_ZOOM_FIT_WIDTH:
        case IDM_ZOOM_FIT_CONTENT:
        case IDM_ZOOM_ACTUAL_SIZE:
            OnMenuZoom(win, wmId);
            break;

        case IDM_ZOOM_CUSTOM:
            OnMenuCustomZoom(win);
            break;

        case IDM_VIEW_SINGLE_PAGE:
            SwitchToDisplayMode(win, DM_SINGLE_PAGE, true);
            break;

        case IDM_VIEW_FACING:
            SwitchToDisplayMode(win, DM_FACING, true);
            break;

        case IDM_VIEW_BOOK:
            SwitchToDisplayMode(win, DM_BOOK_VIEW, true);
            break;

        case IDM_VIEW_CONTINUOUS:
            OnMenuViewContinuous(*win);
            break;

        case IDM_VIEW_SHOW_HIDE_TOOLBAR:
            OnMenuViewShowHideToolbar();
            break;

        case IDM_VIEW_SHOW_HIDE_TAB:
            OnMenuViewShowHideTab();
            break;

        case IDM_CHANGE_LANGUAGE:
            OnMenuChangeLanguage(win->hwndFrame);
            break;

        case IDM_VIEW_BOOKMARKS:
            ToggleTocBox(win);
            break;

        case IDM_GOTO_NEXT_PAGE:
            if (win->IsDocLoaded())
                win->dm->GoToNextPage(0);
            break;

        case IDM_GOTO_PREV_PAGE:
            if (win->IsDocLoaded())
                win->dm->GoToPrevPage(0);
            break;

        case IDM_GOTO_FIRST_PAGE:
            if (win->IsDocLoaded())
                win->dm->GoToFirstPage();
            break;

        case IDM_GOTO_LAST_PAGE:
            if (win->IsDocLoaded())
                win->dm->GoToLastPage();
            break;

        case IDM_GOTO_PAGE:
            OnMenuGoToPage(*win);
            break;

        case IDM_VIEW_PRESENTATION_MODE:
            OnMenuViewPresentation(*win);
            break;

        case IDM_VIEW_FULLSCREEN:
            OnMenuViewFullscreen(*win);
            break;

        case IDM_VIEW_ROTATE_LEFT:
            if (win->IsDocLoaded())
                win->dm->RotateBy(-90);
            break;

        case IDM_VIEW_ROTATE_RIGHT:
            if (win->IsDocLoaded())
                win->dm->RotateBy(90);
            break;

        case IDM_FIND_FIRST:
            OnMenuFind(win);
            break;

        case IDM_FIND_NEXT:
            OnMenuFindNext(win);
            break;

        case IDM_FIND_PREV:
            OnMenuFindPrev(win);
            break;

        case IDM_FIND_MATCH:
            OnMenuFindMatchCase(win);
            break;

        case IDM_FIND_NEXT_SEL:
            OnMenuFindSel(win, FIND_FORWARD);
            break;

        case IDM_FIND_PREV_SEL:
            OnMenuFindSel(win, FIND_BACKWARD);
            break;

        case IDM_VISIT_WEBSITE:
            LaunchBrowser(WEBSITE_MAIN_URL);
            break;

        case IDM_MANUAL:
            LaunchBrowser(WEBSITE_MANUAL_URL);
            break;

        case IDM_CONTRIBUTE_TRANSLATION:
            LaunchBrowser(WEBSITE_TRANSLATIONS_URL);
            break;

        case IDM_ABOUT:
#ifdef DEBUG
            OnMenuAbout2();
#else
            OnMenuAbout();
#endif
            break;

        case IDM_CHECK_UPDATE:
            AutoUpdateCheckAsync(win->hwndFrame, false);
            break;

        case IDM_PREFERENCE:
            OnMenuPreference(*win);
            break;

        case IDM_VIEW_WITH_ACROBAT:
            ViewWithAcrobat(win);
            break;

        case IDM_VIEW_WITH_FOXIT:
            ViewWithFoxit(win);
            break;

        case IDM_VIEW_WITH_PDF_XCHANGE:
            ViewWithPDFXChange(win);
            break;

        case IDM_VIEW_WITH_XPS_VIEWER:
            ViewWithXPSViewer(win);
            break;

        case IDM_VIEW_WITH_HTML_HELP:
            ViewWithHtmlHelp(win);
            break;

        case IDM_SEND_BY_EMAIL:
            SendAsEmailAttachment(win);
            break;

        case IDM_PROPERTIES:
            OnMenuProperties(SumatraWindow::Make(win));
            break;

        case IDM_MOVE_FRAME_FOCUS:
            if (win->hwndFrame != GetFocus())
                SetFocus(win->hwndFrame);
            else if (win->tocVisible)
                SetFocus(win->sideBar()->hwndTocTree);
            break;

        case IDM_GOTO_NAV_BACK:
            if (win->IsDocLoaded())
                win->dm->Navigate(-1);
            break;

        case IDM_GOTO_NAV_FORWARD:
            if (win->IsDocLoaded())
                win->dm->Navigate(1);
            break;

        case IDM_COPY_SELECTION:
            // Don't break the shortcut for text boxes
            if (win->toolBar()->hwndFindBox == GetFocus() || win->toolBar()->hwndPageBox == GetFocus())
                SendMessage(GetFocus(), WM_COPY, 0, 0);
            else if (!HasPermission(Perm_CopySelection))
                break;
            else if (win->IsChm())
                win->dm->AsChmEngine()->CopySelection();
            else if (win->selectionOnPage)
                CopySelectionToClipboard(win);
            else
                ShowNotification(win, _TR("Select content with Ctrl+left mouse button"));
            break;

        case IDM_SELECT_ALL:
            OnSelectAll(win);
            break;

#ifdef SHOW_DEBUG_MENU_ITEMS
        case IDM_DEBUG_SHOW_LINKS:
            gDebugShowLinks = !gDebugShowLinks;
            for (size_t i = 0; i < gWindows.Count(); i++)
                gWindows.At(i)->RedrawAll(true);
            break;

        case IDM_DEBUG_GDI_RENDERER:
            ToggleGdiDebugging();
            break;

        case IDM_DEBUG_EBOOK_UI:
            gUserPrefs.traditionalEbookUI = !gUserPrefs.traditionalEbookUI;
            DebugAlternateChmEngine(gUserPrefs.traditionalEbookUI);
            break;

        case IDM_DEBUG_MUI:
            SetDebugPaint(!IsDebugPaint());
            win::menu::SetChecked(GetMenu(win->hwndFrame), IDM_DEBUG_MUI, !IsDebugPaint());
            break;

        case IDM_DEBUG_ANNOTATION:
            if (win)
                FrameOnChar(*win, 0xA7);
            break;

        case IDM_DEBUG_CRASH_ME:
            CrashMe();
            break;
#endif

        case IDM_FAV_ADD:
            AddFavorite(win);
            break;

        case IDM_FAV_DEL:
            DelFavorite(win);
            break;

        case IDM_FAV_TOGGLE:
            ToggleFavorites(win);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

static LRESULT OnFrameGetMinMaxInfo(MINMAXINFO *info)
{
    info->ptMinTrackSize.x = MIN_WIN_DX;
    info->ptMinTrackSize.y = MIN_WIN_DY;
    return 0;
}

static LRESULT CustomDrawInWIN(TopWindowInfo *WIN, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LPNMHDR lpnmhdr = (LPNMHDR)lParam;
    HWND hwndFrom = lpnmhdr->hwndFrom;

    if (hwndFrom == WIN->panel->win->toolBar()->hwndReBar)
        return CustomDrawReBar(hwndFrom, lParam);
    else if (hwndFrom == WIN->panel->win->toolBar()->hwndToolbar)
        return CustomDrawToolbar(hwndFrom, lParam);

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT FrameOnNotify(TopWindowInfo *WIN, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LPNMHDR lpnmhdr = (LPNMHDR)lParam;

    if (lpnmhdr->code == NM_CUSTOMDRAW)
        return CustomDrawInWIN(WIN, hwnd, msg, wParam, lParam);

     return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProcFrame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *    win;
    ULONG           ulScrollLines;                   // for mouse wheel logic
    
    win = FindWindowInfoByHwnd(hwnd);

    switch (msg)
    {
        case WM_NCPAINT:
        case WM_ACTIVATEAPP:
        case WM_NCACTIVATE:
            return DrawMenuBarBackground(hwnd, msg, wParam, lParam);

        case WM_MEASUREITEM:
            FrameOnMeasureItem(hwnd, lParam);
            return TRUE;

        case WM_DRAWITEM:
            FrameOnDrawItem(hwnd, lParam);
            return TRUE;

        case WM_CREATE:
            // do nothing
            goto InitMouseWheelInfo;

        case WM_SIZE:
            if (win && SIZE_MINIMIZED != wParam) {
                RememberDefaultWindowPosition(*win);
                AdjustWindowEdge(*win);

                int dx = LOWORD(lParam);
                int dy = HIWORD(lParam);
                FrameOnSize(win, dx, dy);
            }
            break;

        case WM_GETMINMAXINFO:
            return OnFrameGetMinMaxInfo((MINMAXINFO*)lParam);

        case WM_MOVE:
            if (win) {
                RememberDefaultWindowPosition(*win);
                AdjustWindowEdge(*win);
            }
            break;

        case WM_INITMENUPOPUP:
            UpdateMenu(win, (HMENU)wParam);
            break;

        case WM_COMMAND:
            return FrameOnCommand(win, hwnd, msg, wParam, lParam);

        case WM_NOTIFY:
            if (win)
                return FrameOnNotify(win->panel->WIN, hwnd, msg, wParam, lParam);
            else
                return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_APPCOMMAND:
            // both keyboard and mouse drivers should produce WM_APPCOMMAND
            // messages for their special keys, so handle these here and return
            // TRUE so as to not make them bubble up further
            switch (GET_APPCOMMAND_LPARAM(lParam)) {
            case APPCOMMAND_BROWSER_BACKWARD:
                SendMessage(hwnd, WM_COMMAND, IDM_GOTO_NAV_BACK, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_FORWARD:
                SendMessage(hwnd, WM_COMMAND, IDM_GOTO_NAV_FORWARD, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_REFRESH:
                SendMessage(hwnd, WM_COMMAND, IDM_REFRESH, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_SEARCH:
                SendMessage(hwnd, WM_COMMAND, IDM_FIND_FIRST, 0);
                return TRUE;
            case APPCOMMAND_BROWSER_FAVORITES:
                SendMessage(hwnd, WM_COMMAND, IDM_VIEW_BOOKMARKS, 0);
                return TRUE;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_CHAR:
            if (win)
                FrameOnChar(*win, wParam);
            break;

        case WM_KEYDOWN:
            if (win)
                FrameOnKeydown(win, wParam, lParam);
            break;

        case WM_SYSKEYUP:
            // pressing and releasing the Alt key focuses the menu even if
            // the wheel has been used for scrolling horizontally, so we
            // have to suppress that effect explicitly in this situation
            if (VK_MENU == wParam && gSuppressAltKey) {
                gSuppressAltKey = false;
                return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_CONTEXTMENU:
            // opening the context menu with a keyboard doesn't call the canvas'
            // WM_CONTEXTMENU, as it never has the focus (mouse right-clicks are
            // handled as expected)
            if (win && GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1 && GetFocus() != win->sideBar()->hwndTocTree)
                return SendMessage(win->hwndCanvas, WM_CONTEXTMENU, wParam, lParam);
            return DefWindowProc(hwnd, msg, wParam, lParam);

        case WM_SETTINGCHANGE:
InitMouseWheelInfo:
            SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);
            // ulScrollLines usually equals 3 or 0 (for no scrolling) or -1 (for page scrolling)
            // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40
            if (ulScrollLines == (ULONG)-1)
                gDeltaPerLine = -1;
            else if (ulScrollLines != 0)
                gDeltaPerLine = WHEEL_DELTA / ulScrollLines;
            else
                gDeltaPerLine = 0;
            return 0;

        case WM_SYSCOLORCHANGE:
            if (gGlobalPrefs.useSysColors)
                UpdateDocumentColors();
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            if (!win)
                break;

            if (win->IsChm()) {
                return win->dm->AsChmEngine()->PassUIMsg(msg, wParam, lParam);
            }

            // Pass the message to the canvas' window procedure
            // (required since the canvas itself never has the focus and thus
            // never receives WM_MOUSEWHEEL messages)
            return SendMessage(win->hwndCanvas, msg, wParam, lParam);

        case WM_CLOSE:
            OnMenuExit(win->panel->WIN, win->panel->WIN->hwndFrame);
            break;

        case WM_DESTROY:
            /* WM_DESTROY is generated by windows when close button is pressed
               or if we explicitly call DestroyWindow()
               It might be sent as a result of File\Close, in which
               case CloseWindow() has already been called. */
            if (win) {
                OnMenuExit(win->panel->WIN, win->panel->WIN->hwndFrame);
            }
            break;

        case WM_ENDSESSION:
            // TODO: check for unfinished print jobs in WM_QUERYENDSESSION?
            SavePrefs();
            break;

        case WM_DDE_INITIATE:
            if (gPluginMode)
                break;
            return OnDDEInitiate(hwnd, wParam, lParam);
        case WM_DDE_EXECUTE:
            return OnDDExecute(hwnd, wParam, lParam);
        case WM_DDE_TERMINATE:
            return OnDDETerminate(hwnd, wParam, lParam);

        case UWM_PREFS_FILE_UPDATED:
            ReloadPrefs();
            break;

        case WM_TIMER:
            OnTimer(*win, hwnd, wParam);
            break;

        case WM_MOUSEACTIVATE:
            if ((win->presentation || win->fullScreen) && hwnd != GetForegroundWindow())
                return MA_ACTIVATEANDEAT;
            return MA_ACTIVATE;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static int RunMessageLoop()
{
    HACCEL accTable = LoadAccelerators(ghinst, MAKEINTRESOURCE(IDC_SUMATRAPDF));
    MSG msg = { 0 };

    while (GetMessage(&msg, NULL, 0, 0)) {
        // dispatch the accelerator to the correct window
        WindowInfo *win = FindWindowInfoByHwnd(msg.hwnd);
        HWND accHwnd = win ? win->hwndFrame : msg.hwnd;
        if (TranslateAccelerator(accHwnd, accTable, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

void GetProgramInfo(str::Str<char>& s)
{
    s.AppendFmt("Ver: %s", QM(CURR_VERSION));
#ifdef SVN_PRE_RELEASE_VER
    s.AppendFmt(".%s pre-release", QM(SVN_PRE_RELEASE_VER));
#endif
#ifdef DEBUG
    s.Append(" dbg");
#endif
    s.Append("\r\n");
    s.AppendFmt("Browser plugin: %s\r\n", gPluginMode ? "yes" : "no");
}

bool CrashHandlerCanUseNet()
{
    return HasPermission(Perm_InternetAccess);
}

void CrashHandlerMessage()
{
    // don't show a message box in restricted use, as the user most likely won't be
    // able to do anything about it anyway and it's up to the application provider
    // to fix the unexpected behavior (of which for a restricted set of documents
    // there should be much less, anyway)
    if (HasPermission(Perm_DiskAccess)) {
        int res = MessageBox(NULL, _TR("Sorry, that shouldn't have happened!\n\nPlease press 'Cancel', if you want to help us fix the cause of this crash."), _TR("SumatraPDF crashed"), MB_ICONERROR | MB_OKCANCEL | (IsUIRightToLeft() ? MB_RTLREADING : 0));
        if (IDCANCEL == res)
            LaunchBrowser(CRASH_REPORT_URL);
    }
}

// TODO: a hackish but cheap way to separate startup code.
// Could be made to compile stand-alone
#include "SumatraStartup.cpp"
