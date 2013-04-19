/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef WindowInfo_h
#define WindowInfo_h

#include "DisplayModel.h"

class ContainerInfo;
class PanelInfo;
class WindowInfo;
class ToolbarInfo;
class SidebarInfo;

class Synchronizer;
class DoubleBuffer;
class SelectionOnPage;
class LinkHandler;
class Notifications;
class StressTest;
struct WatchedFile;

/* Describes actions which can be performed by mouse */
enum MouseAction {
    MA_IDLE = 0,
    MA_DRAGGING,
    MA_DRAGGING_RIGHT,
    MA_SELECTING,
    MA_SCROLLING,
    MA_SELECTING_TEXT
};

enum PresentationMode {
    PM_DISABLED = 0,
    PM_ENABLED,
    PM_BLACK_SCREEN,
    PM_WHITE_SCREEN
};

// WM_GESTURE handling
struct TouchState {
    bool    panStarted;
    POINTS  panPos;
    int     panScrollOrigX;
    double  startArg;
};

/* Describes position, the target (URL or file path) and infotip of a "hyperlink" */
struct StaticLinkInfo {
    StaticLinkInfo() : target(NULL), infotip(NULL) { }
    StaticLinkInfo(RectI rect, const WCHAR *target, const WCHAR *infotip=NULL) :
        rect(rect), target(target), infotip(infotip) { }

    RectI rect;
    const WCHAR *target;
    const WCHAR *infotip;
};

/* Describes the top window information.
   A top window is an application window.
   A top window may have several panels, and
   each panel may have several (tabbed) documents opened. */
class TopWindowInfo
{
public:
    TopWindowInfo(HWND hwnd);
    ~TopWindowInfo();

    HWND            hwndFrame;
    HMENU           menu;

    Vec<ContainerInfo *> gContainer;
    ContainerInfo * container;

    Vec<PanelInfo *> gPanel; // Record the panels in a top window.
    PanelInfo *     panel; // Indicate which panel in a TopWindow is currently active.

    ToolbarInfo *   toolBar;
    SidebarInfo *   sideBar;

    int             dpi;
    float           uiDPIFactor;
};

/* */
class ContainerInfo
{
public:
    ContainerInfo(HWND hwnd);
    ~ContainerInfo();

    HWND            hwndContainer;

    ContainerInfo * parentContainer; // Need?

    ContainerInfo * container1;
    ContainerInfo * container2;

    PanelInfo *     panel;

    HWND            hwndSplitter;

    bool            isSplitVertical;
    bool            isAtTop;
};

/* Describes the panel information.
   A top window may have several panels.
   Panels are used for displaying two or more documents at the same time
   within a single top window. */
class PanelInfo
{
public:
    PanelInfo(HWND hwnd);
    ~PanelInfo();

    HWND            hwndPanel;
    HWND            hwndTab;
    HWND            hwndTabTooltip;

    TopWindowInfo * WIN; // Need?
    ContainerInfo * container; // Need?

    Vec<WindowInfo *> gWin; // Record the (tabbed) documents in a panel.
    WindowInfo *    win; // Indicate which document (tab page) in a panel is currently viewed.

    ToolbarInfo *   toolBar;
    SidebarInfo *   sideBar;

    int tabIndexMouseOver;
};

/* Describes information related to one window with (optional) a document
   on the screen */
class WindowInfo : public DisplayModelCallback
{
public:
    WindowInfo(HWND hwnd);
    ~WindowInfo();

    // TODO: error windows currently have
    //       !IsAboutWindow() && !IsDocLoaded()
    //       which doesn't allow distinction between PDF, XPS, etc. errors
    bool IsAboutWindow() const { return !loadedFilePath; }
    bool IsDocLoaded() const { return this->dm != NULL; }

    bool IsChm() const { return dm && dm->engineType == Engine_Chm; }
    bool IsNotPdf() const { return dm && dm->engineType != Engine_PDF; }

    HMENU menu() const { return panel->WIN->menu; }

    PanelInfo *     panel; // Need?

    WCHAR *         loadedFilePath;
    WCHAR *         TabToolTipText;
    WCHAR *         title;
    DisplayModel *  dm;

    HWND            hwndFrame;
    HWND            hwndCanvas;

    // state related to table of contents (PDF bookmarks etc.)
    HWND            hwndSidebar;

    HWND            hwndTocBox;
    HWND            hwndTocTree;
    bool            tocLoaded;
    bool            tocVisible;
    // set to temporarily disable UpdateTocSelection
    bool            tocKeepSelection;
    // an array of ids for ToC items that have been expanded/collapsed by user
    Vec<int>        tocState;
    DocTocItem *    tocRoot;

    // state related to favorites
    HWND            hwndFavBox;
    HWND            hwndFavTree;
    Vec<DisplayState *> expandedFavorites;

    // vertical splitter for resizing left side panel
    HWND            hwndSidebarSplitter;

    // horizontal splitter for resizing favorites and bookmars parts
    HWND            hwndFavSplitter;

    HWND            hwndInfotip;

    bool            infotipVisible;

    DoubleBuffer *  buffer;

    MouseAction     mouseAction;
    bool            dragStartPending;

    /* when dragging the document around, this is previous position of the
       cursor. A delta between previous and current is by how much we
       moved */
    PointI          dragPrevPos;
    /* when dragging, mouse x/y position when dragging was started */
    PointI          dragStart;

    /* when moving the document by smooth scrolling, this keeps track of
       the speed at which we should scroll, which depends on the distance
       of the mouse from the point where the user middle clicked. */
    int             xScrollSpeed, yScrollSpeed;

    bool            showSelection;

    /* selection rectangle in screen coordinates
     * while selecting, it represents area which is being selected */
    RectI           selectionRect;

    /* after selection is done, the selected area is converted
     * to user coordinates for each page which has not empty intersection with it */
    Vec<SelectionOnPage> *selectionOnPage;

    // a list of static links (mainly used for About and Frequently Read pages)
    Vec<StaticLinkInfo> staticLinks;

    // file change watcher
    WatchedFile *   watcher;

    bool            fullScreen;
    PresentationMode presentation;
    // were we showing toc before entering full screen or presentation mode
    bool            tocBeforeFullScreen;
    int             windowStateBeforePresentation;

    long            prevStyle;
    RectI           frameRc; // window position before entering presentation/fullscreen mode
    float           prevZoomVirtual;
    DisplayMode     prevDisplayMode;

    RectI           canvasRc; // size of the canvas (excluding any scroll bars)
    int             currPageNo; // cached value, needed to determine when to auto-update the ToC selection

    int             wheelAccumDelta;
    UINT_PTR        delayedRepaintTimer;

    Notifications * notifications; // only access from UI thread

    HANDLE          printThread;
    bool            printCanceled;

    HANDLE          findThread;
    bool            findCanceled;

    LinkHandler *   linkHandler;
    PageElement *   linkOnLastButtonDown;
    const WCHAR *   url;

    // synchronizer based on .pdfsync file
    Synchronizer *  pdfsync;

    /* when doing a forward search, the result location is highlighted with
     * rectangular marks in the document. These variables indicate the position of the markers
     * and whether they should be shown. */
    struct {
        bool show;          // are the markers visible?
        Vec<RectI> rects;   // location of the markers in user coordinates
        int page;
        int hideStep;       // value used to gradually hide the markers
    } fwdSearchMark;

    StressTest *    stressTest;

    TouchState      touchState;

    Vec<PageAnnotation> *userAnnots;
    bool            userAnnotsModified;

    ToolbarInfo * toolBar() const; // For functions like UpdateToolbarPageText(win), one needs "win to toolBar" to get hwnd.
    SidebarInfo * sideBar() const;

    void  UpdateCanvasSize();
    SizeI GetViewPortSize();
    void  RedrawAll(bool update=false);
    void  RepaintAsync(UINT delay=0);

    void ChangePresentationMode(PresentationMode mode) {
        presentation = mode;
        RedrawAll();
    }

    void Focus() {
        if (IsIconic(hwndFrame))
            ShowWindow(hwndFrame, SW_RESTORE);
        SetFocus(hwndFrame);
    }

    void ToggleZoom();
    void MoveDocBy(int dx, int dy);

    void CreateInfotip(const WCHAR *text, RectI& rc, bool multiline=false);
    void DeleteInfotip();

    // DisplayModelCallback implementation (incl. ChmNavigationCallback)
    virtual void PageNoChanged(int pageNo);
    virtual void LaunchBrowser(const WCHAR *url);
    virtual void FocusFrame(bool always);
    virtual void Repaint() { RepaintAsync(); };
    virtual void UpdateScrollbars(SizeI canvas);
    virtual void RequestRendering(int pageNo);
    virtual void CleanUp(DisplayModel *dm);
};

class ToolbarInfo
{
public:
    ToolbarInfo(HWND hwnd);
    ~ToolbarInfo();

    WindowInfo *    win; // When one deals with PageBox...etc, one needs "toolBar to win". It should be updated when switching between documents.

    HWND            hwndReBar;
    HWND            hwndToolbar;
    HWND            hwndFindText;
    HWND            hwndFindBox;
    HWND            hwndFindBg;
    HWND            hwndPageText;
    HWND            hwndPageBox;
    HWND            hwndPageBg;
    HWND            hwndPageTotal;
};
    
class SidebarInfo
{
public:
    SidebarInfo(HWND hwnd);
    ~SidebarInfo();

    WindowInfo *    win;

    // state related to table of contents (PDF bookmarks etc.)
    HWND            hwndSidebar;
    HWND            hwndSidebarTop;
    HWND            hwndSidebarBottom;

    HWND            hwndTocBox;
    HWND            hwndTocTree;

    // state related to favorites
    HWND            hwndFavBox;
    HWND            hwndFavTree;
    WStrVec         expandedFavorites;

    // vertical splitter for resizing left side panel
    HWND            hwndSidebarSplitter;

    // horizontal splitter for resizing favorites and bookmars parts
    HWND            hwndFavSplitter;
};

struct WinInfo {

    TopWindowInfo *AsWIN() const { return (type == Frame) ? WINInfo : NULL; }
    PanelInfo *AsPanel() const { return (type == Panel) ? panelInfo : NULL; }

    static WinInfo Make(TopWindowInfo *WIN) {
        WinInfo w; w.type = Frame; w.WINInfo = WIN;
        return w;
    }

    static WinInfo Make(PanelInfo *panel) {
        WinInfo w; w.type = Panel; w.panelInfo = panel;
        return w;
    }

    static WinInfo Make(bool forEachPanel,TopWindowInfo *WIN, PanelInfo *panel) {
        if (forEachPanel)
            return Make(panel);
        else
            return Make(WIN);
    }

    void AssignToolbaInfo() {
        if (type == Frame)
            WINInfo->toolBar = toolBar;
        else if (type == Panel)
            panelInfo->toolBar = toolBar;
    }

    void AssignSidebarInfo() {
        if (type == Frame)
            WINInfo->sideBar = sideBar;
        else if (type == Panel)
            panelInfo->sideBar = sideBar;
    }

    // This is used to determine the correct hwnd which will be the parent of those will be created later.
    HWND Hwnd() const {
        if (type == Frame)
            return WINInfo->hwndFrame;
        else if (type == Panel)
            return panelInfo->hwndPanel;
        else
            return NULL;
    }

    // This is used in CreateToolbar(WinInfo&);
    TopWindowInfo *WIN() const {
        if (type == Frame)
            return WINInfo;
        else if (type == Panel)
            return panelInfo->WIN;
        else
            return NULL;
    }

    bool isWIN() const {
        if (type == Frame)
            return true;
        return false;
    }

    ToolbarInfo *toolBar;
    SidebarInfo *sideBar;

private:
    enum Type { Frame, Panel };
    Type type;
    union {
        TopWindowInfo *WINInfo;
        PanelInfo *panelInfo;
    };
};

class LinkHandler {
    WindowInfo *owner;
    BaseEngine *engine() const;

    void ScrollTo(PageDestination *dest);
    void LaunchFile(const WCHAR *path, PageDestination *link);
    PageDestination *FindTocItem(DocTocItem *item, const WCHAR *name, bool partially=false);

public:
    LinkHandler(WindowInfo& win) : owner(&win) { }

    void GotoLink(PageDestination *link);
    void GotoNamedDest(const WCHAR *name);
};

class LinkSaver : public LinkSaverUI {
    WindowInfo *owner;
    const WCHAR *fileName;

public:
    LinkSaver(WindowInfo& win, const WCHAR *fileName) : owner(&win), fileName(fileName) { }

    virtual bool SaveEmbedded(unsigned char *data, size_t cbCount);
};

#endif
