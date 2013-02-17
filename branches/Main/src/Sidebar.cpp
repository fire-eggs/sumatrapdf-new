/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Sidebar.h"

#include "AppPrefs.h"
#include "AppTools.h"
#include "Favorites.h"
//#include "FullSearch.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "resource.h"
#include "SumatraPDF.h"
//#include "Tab.h"
//#include "TabInSidebar.h"
#include "TableOfContents.h"
#include "TextSearch.h"
#include "WindowInfo.h"
#include "WinUtil.h"

void SidebarTopOnSize(SidebarInfo *sideBar, int dx, int dy)
{
    HWND hwndTitle = GetDlgItem(sideBar->hwndSidebarTop, IDC_SIDEBAR_TITLE_1);
    HWND hwndClose = GetDlgItem(sideBar->hwndSidebarTop, IDC_SIDEBAR_CLOSE_1);

    ScopedMem<TCHAR> title(win::GetText(hwndTitle));
    SizeI size = TextSizeInHwnd(hwndTitle, title);

    TopWindowInfo *WIN = FindTopWindowInfoByHwnd(sideBar->hwndSidebarTop);
    assert(WIN);
    int offset = WIN ? (int)(2 * WIN->uiDPIFactor) : 2;
    int offset_extra_for_top_boundary = 1;
    int offset_extra = 1;

    if (size.dy < 16)
        size.dy = 16;

    int title_y = offset_extra_for_top_boundary + offset_extra + offset;
    int title_dy = size.dy;

    int close_y = title_y + (title_dy - 16) / 2;

    size.dy = title_y + title_dy + offset;

    HDWP hdwp = BeginDeferWindowPos(3);

    DeferWindowPos(hdwp, hwndTitle, NULL, offset, title_y, dx - offset - 16, title_dy, NULL);
    DeferWindowPos(hdwp, hwndClose, NULL, dx - 16, close_y, 16, 16, NULL);
    DeferWindowPos(hdwp, sideBar->hwndTocBox, NULL, 0 , size.dy, dx, dy - size.dy, NULL);

    EndDeferWindowPos(hdwp);
}

void SidebarBottomOnSize(SidebarInfo *sideBar, int dx, int dy)
{
    HWND hwndTitle = GetDlgItem(sideBar->hwndSidebarBottom, IDC_SIDEBAR_TITLE_2);
    HWND hwndClose = GetDlgItem(sideBar->hwndSidebarBottom, IDC_SIDEBAR_CLOSE_2);

    ScopedMem<TCHAR> title(win::GetText(hwndTitle));
    SizeI size = TextSizeInHwnd(hwndTitle, title);

    TopWindowInfo *WIN = FindTopWindowInfoByHwnd(sideBar->hwndSidebarBottom);
    assert(WIN);
    int offset = WIN ? (int)(2 * WIN->uiDPIFactor) : 2;
    int offset_extra_for_top_boundary = 1;
    int offset_extra = 1;

    if (size.dy < 16)
        size.dy = 16;

    int title_y = offset_extra_for_top_boundary + offset_extra + offset;
    int title_dy = size.dy;

    int close_y = title_y + (title_dy - 16) / 2;

    size.dy = title_y + title_dy + offset;

    HDWP hdwp = BeginDeferWindowPos(3);

    DeferWindowPos(hdwp, hwndTitle, NULL, offset, title_y, dx - offset - 16, title_dy, NULL);
    DeferWindowPos(hdwp, hwndClose, NULL, dx - 16, close_y, 16, 16, NULL);
    DeferWindowPos(hdwp, sideBar->hwndFavBox, NULL, 0 , size.dy, dx, dy - size.dy, NULL);

    EndDeferWindowPos(hdwp);
}

void SidebarOnSize(SidebarInfo *sideBar, int dx, int dy)
{
    ClientRect rSidebar(sideBar->hwndSidebar);
    ClientRect rTop(sideBar->hwndSidebarTop);
    int sidebarTopDy = rTop.dy;
    if (sidebarTopDy == 0) {
        sidebarTopDy = rSidebar.dy / 2;
    }

    HDWP hdwp = BeginDeferWindowPos(3);

    DeferWindowPos(hdwp, sideBar->hwndSidebarTop, NULL, 0, 0, rSidebar.dx, sidebarTopDy, SWP_NOZORDER);
    DeferWindowPos(hdwp, sideBar->hwndFavSplitter, NULL, 0, sidebarTopDy, rSidebar.dx, SPLITTER_DY, SWP_NOZORDER);
    DeferWindowPos(hdwp, sideBar->hwndSidebarBottom, NULL, 0, sidebarTopDy + SPLITTER_DY, dx, rSidebar.dy - sidebarTopDy - SPLITTER_DY, SWP_NOZORDER);

    EndDeferWindowPos(hdwp);

    InvalidateRect(sideBar->hwndSidebar, NULL, TRUE);
    UpdateWindow(sideBar->hwndSidebar);
}

void SidebarOnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    int dx = rc.right - rc.left;
    int dy = rc.bottom - rc.top;

    rc.left = 0;
    rc.right = dx;
    rc.top = 0;
    rc.bottom = 1;
    FillRect(hdc, &rc, gBrushSepLineBg);

    rc.left = 0;
    rc.right = dx;
    rc.top = 1;
    rc.bottom = dy;
    FillRect(hdc, &rc, gBrushStaticBg);

    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProcSidebarTopCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);

    if (!win || !win->sideBar())
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            SidebarTopOnSize(win->sideBar(), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
            break;

        case WM_PAINT:
            SidebarOnPaint(hwnd);
            return 0;
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SIDEBAR_CLOSE_1 && HIWORD(wParam) == STN_CLICKED)
                ToggleTocBox(win);
            break;

        case WM_DRAWITEM:
            if (IDC_SIDEBAR_CLOSE_1 == wParam) {
                DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
                DrawCloseButton(dis);
                return TRUE;
            }
            break;

        case WM_ERASEBKGND:
            return 1;
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProcSidebarBottomCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);

    if (!win || !win->sideBar())
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            SidebarBottomOnSize(win->sideBar(), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
            break;

        case WM_PAINT:
            SidebarOnPaint(hwnd);
            return 0;
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SIDEBAR_CLOSE_2 && HIWORD(wParam) == STN_CLICKED)
                ToggleFavorites(win);
            break;

        case WM_DRAWITEM:
            if (IDC_SIDEBAR_CLOSE_2 == wParam) {
                DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
                DrawCloseButton(dis);
                return TRUE;
            }
            break;

        case WM_ERASEBKGND:
            return 1;
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProcSidebarCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);

    if (!win || !win->sideBar())
        return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            SidebarOnSize(win->sideBar(), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
            break;

        case WM_PAINT:
            SidebarOnPaint(hwnd);
            return 0;
            break;

        case WM_ERASEBKGND:
            return 1;
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CreateSidebarBox(WinInfo& winInfo)
{
    HWND hwndSidebar = CreateWindowEx(
        WS_CLIPCHILDREN,
        SIDEBAR_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, gGlobalPrefs.sidebarDx, 0,
        winInfo.Hwnd(), NULL,
        ghinst, NULL);
    if (!hwndSidebar)
        return;

    HWND hwndSidebarTop = CreateWindowEx(
        WS_CLIPCHILDREN,
        SIDEBAR_TOP_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        hwndSidebar, NULL,
        ghinst, NULL);
    if (!hwndSidebarTop)
        return;

    HWND title = CreateWindow(WC_STATIC, L"", SS_NOTIFY | WS_VISIBLE | WS_CHILD,
        0, 0, 0, 0, hwndSidebarTop, (HMENU)IDC_SIDEBAR_TITLE_1, ghinst, NULL);
    SetWindowFont(title, gDefaultGuiFont, FALSE);
    win::SetText(title, L"Sidebar 1");

    HWND hwndClose = CreateWindow(WC_STATIC, L"",
        SS_OWNERDRAW | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
        0, 0, 16, 16, hwndSidebarTop, (HMENU)IDC_SIDEBAR_CLOSE_1, ghinst, NULL);

    if (NULL == DefWndProcCloseButton)
        DefWndProcCloseButton = (WNDPROC)GetWindowLongPtr(hwndClose, GWLP_WNDPROC);
    SetWindowLongPtr(hwndClose, GWLP_WNDPROC, (LONG_PTR)WndProcCloseButton);

    HWND hwndSidebarBottom = CreateWindowEx(
        WS_CLIPCHILDREN,
        SIDEBAR_BOTTOM_CLASS_NAME, NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        hwndSidebar, NULL,
        ghinst, NULL);
    if (!hwndSidebarBottom)
        return;

    title = CreateWindow(WC_STATIC, L"", SS_NOTIFY | WS_VISIBLE | WS_CHILD,
        0, 0, 0, 0, hwndSidebarBottom, (HMENU)IDC_SIDEBAR_TITLE_2, ghinst, NULL);
    SetWindowFont(title, gDefaultGuiFont, FALSE);
    win::SetText(title, L"Sidebar 2");

    hwndClose = CreateWindow(WC_STATIC, L"",
        SS_OWNERDRAW | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
        0, 0, 16, 16, hwndSidebarBottom, (HMENU)IDC_SIDEBAR_CLOSE_2, ghinst, NULL);

    if (NULL == DefWndProcCloseButton)
        DefWndProcCloseButton = (WNDPROC)GetWindowLongPtr(hwndClose, GWLP_WNDPROC);
    SetWindowLongPtr(hwndClose, GWLP_WNDPROC, (LONG_PTR)WndProcCloseButton);

    HWND hwndFavSplitter = CreateWindowEx(
        NULL,
        FAV_SPLITTER_CLASS_NAME, NULL,
        WS_CHILDWINDOW | WS_CLIPSIBLINGS,
        0, 0, 0, 0,
        hwndSidebar, NULL,
        ghinst, NULL);

    // For now temp.
    ShowWindow(hwndSidebarTop, SW_SHOW);
    UpdateWindow(hwndSidebarTop);

    ShowWindow(hwndSidebarBottom, SW_SHOW);
    UpdateWindow(hwndSidebarBottom);

    ShowWindow(hwndFavSplitter, SW_SHOW);
    UpdateWindow(hwndFavSplitter);

    ShowWindow(hwndSidebar, SW_SHOW);
    UpdateWindow(hwndSidebar);

    SidebarInfo *sideBar = new SidebarInfo(hwndSidebar);
    winInfo.sideBar = sideBar;
    winInfo.sideBar->win = winInfo.isWIN() ? winInfo.AsWIN()->panel->win : winInfo.AsPanel()->win;
    winInfo.sideBar->hwndSidebarTop = hwndSidebarTop;
    winInfo.sideBar->hwndSidebarBottom = hwndSidebarBottom;
    winInfo.sideBar->hwndFavSplitter = hwndFavSplitter;

    winInfo.AssignSidebarInfo();

    CreateToc(sideBar, hwndSidebarTop);
    CreateFavorites(sideBar, hwndSidebarBottom);

    // For now temp.
    ShowWindow(sideBar->hwndTocBox, SW_SHOW);
    UpdateWindow(sideBar->hwndTocBox);

    ShowWindow(sideBar->hwndFavBox, SW_SHOW);
    UpdateWindow(sideBar->hwndFavBox);
}