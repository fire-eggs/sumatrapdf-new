/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "Tab.h"

#include "AppPrefs.h"
#include "AppTools.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "Menu.h"
#include "resource.h"
#include "Sidebar.h"
#include "SumatraPDF.h"
#include "TableOfContents.h" // Not good.
#include "Translations.h"
#include "WindowInfo.h"
#include "WinUtil.h"

//int GetTabIndex(PanelInfo *panel)
//{
//    POINT pt;
//    GetCursorPos(&pt);
//    ScreenToClient(panel->hwndTab, &pt);
//
//    TCHITTESTINFO hitInfo;
//    hitInfo.pt = pt;
//    hitInfo.flags = TCHT_ONITEM;
//
//    int tabIndex;
//    tabIndex = SendMessage(panel->hwndTab, TCM_HITTEST, NULL, (LPARAM)&hitInfo);
//
//    return tabIndex;
//}
//
//void UpdateCloseButtonBitmap(PanelInfo *panel, int tabIndex, int bmpIndex)
//{
//    //if (tabIndex == -1)
//    //    return;
//
//    assert(tabIndex > -1);
//
//    HBITMAP hBmp;
//    HBITMAP hBmpOld;
//
//    switch (bmpIndex)
//    {
//    case 1:
//        bmpIndex = IDB_CLOSE_TAB_NOT_HOVER;
//        break;
//    case 2:
//        bmpIndex = IDB_CLOSE_TAB_NOT_HOVER_SELECT;
//        break;
//    case 3:
//        bmpIndex = IDB_CLOSE_TAB_HIGHLIGHT;
//        break;
//    case 4:
//        bmpIndex = IDB_CLOSE_TAB_HOVER;
//        break;
//    }
//
//    hBmp = LoadBitmap(ghinst, MAKEINTRESOURCE(bmpIndex));
//    hBmpOld = (HBITMAP)SendMessage(panel->gWin.At(tabIndex)->hwndTabStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
//
//    // Clear the old image.
//    if(hBmpOld && hBmpOld != hBmp)
//        DeleteObject(hBmpOld);            
//}
//
//WNDPROC DefWndProcTabStatic = NULL;
//LRESULT CALLBACK WndProcTabStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
//{
//    PanelInfo  *panel = FindPanelInfoByHwnd(hwnd);
//    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
//
//    if (WM_MOUSEMOVE == msg) {
//        if (!str::Eq(win->HoverState, _T("Hover"))) { // If not over the close button last time
//            panel->closeButtonIndex = panel->gWin.Find(win); // Indicate over which close button
//            if (!panel->isTimer2) {
//                SetTimer(panel->hwndTab, 2, 10, timerProc);
//                panel->isTimer2 = true;
//            }
//        }
//    }
//
//    return CallWindowProc(DefWndProcTabStatic, hwnd, msg, wParam, lParam);
//}

static WNDPROC DefWndProcTabTooltip = NULL;
static LRESULT CALLBACK WndProcTabTooltip(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Remark: In order to set breakpoints here, we can't set breakpoints only inside if statement below,
    //         otherwise it doesn't work.
    //         Set breakpoints at "if (message == WM_WINDOWPOSCHANGING) {" and "return...".

    if (message == WM_WINDOWPOSCHANGING) {    // WM_WINDOWPOSCHANGING == 70 in Decimal.

        PanelInfo *panel = (PanelInfo *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (!panel)
            return CallWindowProc(DefWndProcTabTooltip, hwnd, message, wParam, lParam);

        LPWINDOWPOS winPos = (LPWINDOWPOS)lParam;

        WindowRect rCanvas(panel->win->hwndCanvas);

        winPos->x = winPos->x + 13; // Make sure that the tooltip shows up on the right of the cursor. // Could one get the size of the cursor?
        winPos->y = rCanvas.y - 1;  // The top edge of a tooltip will cover the separator between the tab control and the canvas.
    }

    if (message == WM_TIMER && wParam == 4) {
        static int counter = 0;
        counter++;
        if (counter != 60 * 2)
            return 0;
        else
            counter = 0;
    }

    return CallWindowProc(DefWndProcTabTooltip, hwnd, message, wParam, lParam);
}

//void ToggleTabPreview(PanelInfo *panel)
//{
//    // We don't want to create hwndTabPreview at start up.
//    if (!panel->hwndTabPreview) {
//        CreateTabPreview(panel);
//        ClientRect rSidebar1(panel->hwndLeftSidebarTop);
//        SendMessage(panel->hwndLeftSidebarTop, WM_SIZE, 0, MAKELPARAM((rSidebar1.dx),(rSidebar1.dy)));
//    }
//
//    SetTabPreviewVisibility(panel, true);
//    SetTocVisibility(panel, false);
//    SetSearchVisibility(panel, false);
//    SetSidebarVisibility(panel->win, true, panel->favVisible);
//    SetFocus(panel->hwndTabPreview);
//    SendMessage(panel->hwndTabInSidebar, TCM_SETCURSEL, 2, 0);
//    // A trick. When we go to search, we don't want to set tocVisible to be false. So next time when we open this file, it can show toc.
//    SetTocVisibility(panel, true);
//}
//
//void CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
//{
//    PanelInfo *panel = FindPanelInfoByHwnd(hwnd);
//
//    if (idEvent == 1) {
//
//        // Get the new tab index.
//        int tabIndex = GetTabIndex(panel);
//
//        // Mouse Leave
//        // No longer over any tab item (but was over a tab item last time),
//        if (tabIndex == -1 && panel->tabIndexMouseOver > -1 && !panel->hMenu && !panel->isTabPreviewVisible) {  // and the previous tab item is not the selected one
//            if (panel->gWin.At(panel->tabIndexMouseOver) != panel->win)
//                UpdateCloseButtonBitmap(panel, panel->tabIndexMouseOver, 1);
//
//            panel->tabIndexMouseOver = -1;
//            KillTimer(hwnd, 1);
//            panel->isTimer1 = false;
//        } 
//
//    } else if (idEvent == 2) {  // For close button
//
//        int closeButtonIndex = panel->closeButtonIndex;
//        WindowInfo *win = panel->gWin.At(closeButtonIndex); // Get the win associated to the close button which had mouse events.
//
//        int tabIndex = GetTabIndex(panel);
//
//        // If not over close button because move too fast, do nothing
//        if (!str::Eq(win->HoverState, _T("Hover")) && !IsCursorOverWindow(win->hwndTabStatic)) {
//
//            KillTimer(hwnd, 2);
//            panel->isTimer2 = false;
//        } else if (!str::Eq(win->HoverState, _T("Hover")) && IsCursorOverWindow(win->hwndTabStatic)) { // If from not hover to hover,
//            UpdateCloseButtonBitmap(panel, closeButtonIndex, 4);  // highlight the close button,
//            SetHoverState(win, true);
//
//        // go out from a close button of a selected tab item
//        } else if (str::Eq(win->HoverState, _T("Hover")) && !IsCursorOverWindow(win->hwndTabStatic) && win == panel->win) {
//            UpdateCloseButtonBitmap(panel, closeButtonIndex, 2);
//            SetHoverState(win, false);
//            KillTimer(hwnd, 2);
//            panel->isTimer2 = false;
//
//        // go out from a close button of a highlighted tab item to this tab item
//        } else if (str::Eq(win->HoverState, _T("Hover")) && !IsCursorOverWindow(win->hwndTabStatic) && tabIndex > -1 && win == panel->gWin.At(tabIndex)) {
//            UpdateCloseButtonBitmap(panel, tabIndex, 3);
//            SetHoverState(win, false);
//            KillTimer(hwnd, 2);
//            panel->isTimer2 = false;
//
//        } else if (str::Eq(win->HoverState, _T("Hover")) && !IsCursorOverWindow(win->hwndTabStatic)) {
//            if (panel->isTabPreviewVisible && IsCursorOverWindow(panel->hwndTabPreview) && closeButtonIndex == panel->tabIndexMouseOver) { // tabIndexMouseOver is updated.
//                UpdateCloseButtonBitmap(panel, closeButtonIndex, 3);
//            }
//            else
//                UpdateCloseButtonBitmap(panel, closeButtonIndex, 1);
//            SetHoverState(win, false);
//            KillTimer(hwnd, 2);
//            panel->isTimer2 = false;
//        }
//
//        if (tabIndex == -1 && panel->isTimer2 == false)
//            PostMessage(panel->hwndTab, WM_MOUSELEAVE, 0, 0);
//
//    } else if (idEvent == 3) { // For tab preview
//
//        if (!(panel->WIN->panel->tabIndexMouseOver == -1) && IsCursorOverWindow(panel->WIN->panel->hwndTab)) {
//
//            ToggleTabPreview(panel->WIN->panel);
//
//            if (!panel->WIN->panel->isTimer3) {
//                SetTimer(hwnd, 4, 10, timerProc);
//                panel->WIN->panel->isTimer3 = true;
//            }
//        }
//
//        KillTimer(hwnd, 3);
//
//    } else if (idEvent == 4) {
//
//        int tabIndex = GetTabIndex(panel->WIN->panel);
//        bool isOverTabPreview = IsCursorOverWindow(panel->WIN->panel->hwndTabPreview);
//        bool isOverSidebar1 = IsCursorOverWindow(panel->WIN->panel->hwndLeftSidebarTop);
//
//        if (tabIndex == -1 && !isOverTabPreview  && !isOverSidebar1 && panel->WIN->panel->isTabPreviewVisible) {
//            ToggleTocBox(panel->WIN->panel->win);
//
//            KillTimer(hwnd, 4);
//            panel->WIN->panel->isTimer3 = false;
//            PostMessage(panel->WIN->panel->hwndTab, WM_MOUSELEAVE, 0, 0);
//        }
//    }
//}
//
//void CreateOrShowTabPreview(PanelInfo *panel, int tabIndex)
//{
//    // We need this, other wise CPU blow up. I think this is caused in "else if (idEvent == 3)"..."if (!panel->WIN->panel->isTimer3) {"
//    panel->WIN->panel = panel;
//
//    if (!panel->tpp) {
//        CreateTabPreview(panel);
//        ClientRect rSidebar(panel->hwndLeftSidebarTop);
//        SendMessage(panel->hwndLeftSidebarTop, WM_SIZE, NULL, MAKELPARAM((rSidebar.dx), (rSidebar.dy)));
//        //ToggleTabPreview(panel);
//    } //else
//        //ToggleTabPreview(panel);
//
//    //LoadDocumentToTabPreview(panel, tabIndex, false); // Need to think about this.
//
//    int timer;
//
//    if (panel->isTabPreviewVisible)
//        timer = 100;
//    else
//        timer = 500;
//
//    SetTimer(panel->hwndTabPreview, 3, timer, timerProc);
//}
//
//static bool TabControlOnMouseLeave(PanelInfo *panel)
//{
//    //POINT pt;
//    //GetCursorPos(&pt);
//    if (panel->tabIndexMouseOver > -1 && IsCursorOverWindow(panel->gWin.At(panel->tabIndexMouseOver)->hwndTabStatic))
//        return false;
//
//    if (panel->hMenu)
//        return false;
//
//    if ((gGlobalPrefs.tabPreviewEnabled && panel->isTabPreviewVisible && panel->win->IsDocLoaded()))
//        return false;
//
//    //if (panel->LeaveMsgByGoCloseButton || panel->hMenu || (gGlobalPrefs.tabPreviewEnabled && panel->isTabPreviewVisible && panel->win->IsDocLoaded())) { // We need to add this check : panel->win->IsDocLoaded(), otherwise problem if doc is not loaded.
//    //    panel->LeaveMsgByGoCloseButton = false;
//    //    return false;
//    //}
//
//    //if (!(panel->tabIndexMouseOver == panel->gWin.Find(panel->win))) // if was over a non-selected tab item
//    //    UpdateCloseButtonBitmap(panel, panel->tabIndexMouseOver, 1);
//    //else
//    //    UpdateCloseButtonBitmap(panel, panel->tabIndexMouseOver, 2); // if was over a selected tab itme (only concer with over its close button)
//
//    //panel->isTabPreviewVisible = false;
//    //panel->isOverTabPreview = false;
//     panel->isLeave = true;
//
//    return true;
//}
//
//// We may put the command to timerProc to reduce the code. But we put it here to get the immediate response.
//// The timer is mainly for mouse leave.
//static void TabControlOnMouseMove(PanelInfo *panel)
//{
//    // Track Mouse Leave
//    TRACKMOUSEEVENT tme = { 0 };
//    tme.cbSize = sizeof(tme);
//    tme.dwFlags = TME_LEAVE;
//    tme.hwndTrack = panel->hwndTab;
//    TrackMouseEvent(&tme);
//
//    // Get tab index.
//    int tabIndex = GetTabIndex(panel);
//
//    // If over a tab item which is the same one as before, do nothing.
//    // But we have a timer already.
//    if (tabIndex > -1 && tabIndex == panel->tabIndexMouseOver)
//        return;
//
//    // If over a non-selected item, highlight its close button.
//    if (tabIndex > -1 && (panel->gWin.At(tabIndex) != panel->win)) {
//        UpdateCloseButtonBitmap(panel, tabIndex, 3);
//    }
//
//    // If over a non-selected item which is different from the previous tab item (which is not selected), set the close button of the previous tab item to non-highlight.
//    if (tabIndex > -1 && panel->tabIndexMouseOver > -1 && tabIndex != panel->tabIndexMouseOver && panel->gWin.At(panel->tabIndexMouseOver) != panel->win)
//        UpdateCloseButtonBitmap(panel, panel->tabIndexMouseOver, 1);
//
//    if (tabIndex > -1 && panel->gWin.At(tabIndex)->IsDocLoaded()) {
//        CreateOrShowTabPreview(panel, tabIndex);
//        //LoadDocumentToTabPreview(panel, tabIndex, false); // Need to think about this.
//    }
//
//    // Update panel->tabndexMouseOver
//    panel->tabIndexMouseOver = tabIndex;
//
//    // Set timer for mouse events about tab control
//    if (!panel->isTimer1) {
//        SetTimer(panel->hwndTab, 1, 10, timerProc);
//        panel->isTimer1 = true;
//    }
//}

static WNDPROC DefWndProcTabControl = NULL;
static LRESULT CALLBACK WndProcTabControl(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{   
    if (message == WM_PAINT) {

        CallWindowProc(DefWndProcTabControl, hwnd, message, wParam, lParam);

        HDC hdc = GetDC(hwnd);

        RECT rc;
        GetClientRect(hwnd, &rc);

        int dx = rc.right - rc.left;
        int dy = rc.bottom - rc.top;

        rc.top = 0;
        rc.bottom = 1;
        FillRect(hdc, &rc, gBrushSepLineBg);

        int tabItemCount = SendMessage(hwnd, TCM_GETITEMCOUNT, 0, 0);
        RECT rcLastItem;
        SendMessage(hwnd, TCM_GETITEMRECT, tabItemCount - 1, (LPARAM)&rcLastItem);
        int start = rcLastItem.right + 2;
        if (tabItemCount - 1 != SendMessage(hwnd, TCM_GETCURSEL, 0, 0)) {
            if (IsAppThemed())
                start -= 4;
            else
                start -= 2;
        }

        TRIVERTEX        vert[2];
        GRADIENT_RECT    gRect;

        vert[0].x      = start;
        vert[0].y      = 1;
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

        ReleaseDC(hwnd, hdc);

        return 0;
    }

    // hwnd is hwndTab;
    //PanelInfo *panel = FindPanelInfoByHwnd(hwnd);
    //
    //if (!panel)
    //    return DefWindowProc(hwnd, message, wParam, lParam);

    //WindowInfo *win = panel->win;

    //switch(message) {

    //    case WM_MOUSEMOVE:
    //        TabControlOnMouseMove(panel);
    //        break;

    //    case WM_MOUSELEAVE:
    //        if (!TabControlOnMouseLeave(panel))
    //            return 0;
    //        break;

    //    case WM_INITMENUPOPUP:
    //        panel->hMenu = (HMENU) wParam;
    //        break;

    //    case WM_UNINITMENUPOPUP:
    //        panel->hMenu = NULL;
    //        if (!IsCursorOverWindow(panel->hwndPanel))
    //            PostMessage(panel->hwndTab, WM_MOUSELEAVE, 0, 0); // When cancel the tab contextmenu outside the panel, make the highlighted tab item non-highlighted.
    //        break;

    //    case WM_ERASEBKGND:
    //        return 1;
    //        break;

    //    case WM_ENTERMENULOOP:
    //        break;
    //    
    //    case WM_COMMAND:
    //        if (HIWORD(wParam) == STN_CLICKED) { // When click on a static child window with style SS_NOTIFY.
    //            WindowInfo *winToClose = FindWindowInfoByHwnd((HWND)lParam);
    //            CloseWindowFromTab(winToClose);
    //        }
    //        break;

    //    case WM_PAINT: // We need this to make the TabStatics' pos behaves well.
    //        for (int i = 0; i < (int) panel->gWin.Count(); i++) {
    //            SetTabCloseButtonPos(panel->gWin.At(i));
    //        }
    //        break;

    //     //We can't deal thie case here. We do the task in CloseDocumentInWindow.
    //    case TCM_DELETEITEM:
    //        panel->tabIndexMouseOver = min(panel->tabIndexMouseOver, SendMessage(panel->hwndTab, TCM_GETITEMCOUNT, 0, 0) - 2);
    //        panel->closeButtonIndex = min(panel->closeButtonIndex, SendMessage(panel->hwndTab, TCM_GETITEMCOUNT, 0, 0) - 2);
    //        break;
    //}
    return CallWindowProc(DefWndProcTabControl, hwnd, message, wParam, lParam);
}

void CreateTabControl(PanelInfo *panel)
{
    HWND hwndTab = CreateWindowEx(
        NULL,
        WC_TABCONTROL,
        NULL,
        WS_CLIPCHILDREN | WS_CHILD | TCS_FOCUSNEVER | TCS_HOTTRACK | TCS_TOOLTIPS, // Use WS_CLIPCHILDREN, otherwise, the close buttons will disappear when insert new items or delete an item.
        0, 0, 0, 0, /* position and size determined in OnSize */
        panel->hwndPanel,
        NULL,
        ghinst,
        NULL);

    // Set the height of tab items.
    SendMessage(hwndTab, TCM_SETITEMSIZE, NULL, MAKELPARAM((0),(TAB_CONTROL_DY)));

    SetWindowFont(hwndTab, gDefaultGuiFont, FALSE);

    if (!DefWndProcTabControl)
        DefWndProcTabControl = (WNDPROC)GetWindowLongPtr(hwndTab, GWLP_WNDPROC);
    SetWindowLongPtr(hwndTab, GWLP_WNDPROC, (LONG_PTR)WndProcTabControl);

    panel->hwndTab = hwndTab;

    ShowWindow(panel->hwndTab, SW_SHOW);
    UpdateWindow(panel->hwndTab);

    // Get the handle of the tooltip of hwndTab.
    panel->hwndTabTooltip = (HWND) SendMessage(hwndTab, TCM_GETTOOLTIPS, NULL, NULL);

    // It seems that we can't set a tooltip to be a child of an application window.
    // Otherwise, hwndTabtooltip never receives WM_WINDOWPOSCHANGING message.
    //SetParent(panel->hwndTabTooltip, panel->hwndPanel);

    // In WndProcTabTooltip, we need to find the panel to which the hwndTabTooltip
    // is associated. Since hwndTabTooltip is not a child of hwndPanel,
    // FindPanelInfoByHwnd will return NULL hence we won't have the desired result.
    // Hence we embed the data "panel" into hwndTabTooltip.
    SetWindowLongPtr(panel->hwndTabTooltip, GWLP_USERDATA, (LONG_PTR)panel);

    if (NULL == DefWndProcTabTooltip)
        DefWndProcTabTooltip = (WNDPROC)GetWindowLongPtr(panel->hwndTabTooltip, GWLP_WNDPROC);
    SetWindowLongPtr(panel->hwndTabTooltip, GWLP_WNDPROC, (LONG_PTR)WndProcTabTooltip);

    // Set the timeout of hwndTabTooltip.
    SendMessage(panel->hwndTabTooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, MAKELPARAM((30000),(0)));
    SendMessage(panel->hwndTabTooltip, TTM_SETDELAYTIME, TTDT_INITIAL, MAKELPARAM((1),(0)));
}

void AddTab(WindowInfo *win)
{
    PanelInfo *panel = win->panel;

    int tabIndex = (int) panel->gWin.Find(win);

    // =====================================================================================
    // Prepare the TCITEM structure that specifies the attributes of the tab.

    TCITEM tie;

    tie.mask = TCIF_TEXT | TCIF_IMAGE;
    tie.iImage = -1;

    ScopedMem<WCHAR> title;
    title.Set(str::Format(L"%s", L"Start Page     "));
    tie.pszText = title;

    // =====================================================================================

    SendMessage(panel->hwndTab, TCM_INSERTITEM, tabIndex, (LPARAM)&tie); // Insert a new tab item.
    SendMessage(panel->hwndTab, TCM_SETCURSEL, tabIndex, 0); // Change selected tab item.

    SetTabToolTipText(win);

    //// =====================================================================================

    // Add close button to a tab item.
    // Maybe we could do this in TCM_INSERTITEM.
    //RECT rc;
    //SendMessage(panel->hwndTab, TCM_GETITEMRECT, tabIndex, (LPARAM)&rc); 

    //HWND hwndTabStatic = CreateWindow(WC_STATIC,
    //    _T(""),
    //    WS_CHILD | SS_BITMAP | WS_VISIBLE | SS_NOTIFY,
    //    rc.right - 16, 7, 12, 12,
    //    panel->hwndTab, NULL,
    //    (HINSTANCE)GetWindowLong(panel->hwndTab, GWL_HINSTANCE),
    //    NULL);

    //HBITMAP hBmp = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_CLOSE_TAB_NOT_HOVER_SELECT));

    //HBITMAP hBmpOld = (HBITMAP)SendMessage(hwndTabStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);

    //// Clear the old image.
    //if(hBmpOld && hBmpOld != hBmp)
    //    DeleteObject(hBmpOld);

    //for (int i = 0; i < (int) panel->gWin.Count() - 1; i++)
    //{
    //    hBmp = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_CLOSE_TAB_NOT_HOVER));

    //    hBmpOld = (HBITMAP)SendMessage(panel->gWin.At(i)->hwndTabStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);

    //    // Clear the old image.
    //    if(hBmpOld && hBmpOld != hBmp)
    //        DeleteObject(hBmpOld);
    //}

    ////// =====================================================================================

    //HWND hwndUpDown = FindWindowEx(panel->hwndTab, NULL, _T("msctls_updown32"), NULL);

    //if (!(hwndUpDown == NULL) && panel->hwndUpDown == NULL) {
    //    panel->hwndUpDown = hwndUpDown;
    //}

    ////// =====================================================================================

    //// If the mouse is over a tab item. Its close button image should be close_highlighted.
    //// Need to be integrated in above code.
    //POINT pt;
    //GetCursorPos(&pt);
    //MapWindowPoints(HWND_DESKTOP, panel->hwndTab, &pt, 1);

    //TCHITTESTINFO hitInfo;
    //hitInfo.pt = pt;
    //hitInfo.flags = TCHT_ONITEM;

    //int tabIndex2;
    //tabIndex2 = SendMessage(panel->hwndTab, TCM_HITTEST, NULL, (LPARAM)&hitInfo);

    //if ( !(tabIndex2 == -1) && !(tabIndex2 == tabIndex + 1) ) {

    //    hBmp = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_CLOSE_TAB_HIGHLIGHT));
    //    hBmpOld = (HBITMAP)SendMessage(panel->gWin.At(tabIndex2)->hwndTabStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);

    //    // Clear the old image.
    //    if(hBmpOld && hBmpOld != hBmp)
    //        DeleteObject(hBmpOld);
    //}

    ////// =====================================================================================

    //// Set the window procedure for hwndTabStatic.
    //if (NULL == DefWndProcTabStatic)
    //    DefWndProcTabStatic = (WNDPROC)GetWindowLongPtr(hwndTabStatic, GWLP_WNDPROC);
    //SetWindowLongPtr(hwndTabStatic, GWLP_WNDPROC, (LONG_PTR)WndProcTabStatic);

    //win->hwndTabStatic = hwndTabStatic;
}

void ShowOrHideTabGlobally()
{
    for (size_t i = 0; i < gWIN.Count(); i++) {
        TopWindowInfo *WIN = gWIN.At(i);
        for (size_t j = 0; j < WIN->gPanel.Count(); j++) {

            PanelInfo *panel = WIN->gPanel.At(j);

            if (gGlobalPrefs->enableTab)
                ShowWindow(panel->hwndTab, SW_SHOW);
            else
                ShowWindow(panel->hwndTab, SW_HIDE);

            ClientRect rect(panel->hwndPanel);
            SendMessage(panel->hwndPanel, WM_SIZE, 0, MAKELONG(rect.dx, rect.dy));
        }
    }
}

//void SetTabCloseButtonPos(WindowInfo *win)
//{
//    PanelInfo *panel = win->panel;
//
//    RECT rc;
//    SendMessage(panel->hwndTab, TCM_GETITEMRECT, panel->gWin.Find(win), (LPARAM)&rc);
//
//    if (win == panel->win)
//        SetWindowPos(win->hwndTabStatic, NULL, rc.right - 16, 7, 12, 12, SWP_NOZORDER);
//    else
//        SetWindowPos(win->hwndTabStatic, NULL, rc.right - 16, 8, 12, 12, SWP_NOZORDER);
//
//    InvalidateRect(win->hwndTabStatic, NULL, true);
//    UpdateWindow(win->hwndTabStatic);
//}

void SetTabTitle(WindowInfo *win)
{
    PanelInfo *panel = win->panel;

    // tabIndex is used in SendMessage for sending TCM_SETITEM.
    int tabIndex = (int) panel->gWin.Find(win);

    TCITEM tie;
    tie.mask = TCIF_TEXT | TCIF_IMAGE;
    tie.iImage = -1;

    ScopedMem<WCHAR> title;

    if (win->IsDocLoaded()) {
        const WCHAR *baseName = path::GetBaseName(win->dm->FilePath());
        title.Set(str::Format(L"%s", baseName));
    } else
        title.Set(str::Format(L"%s", L"Start Page")); // When one closes the only one document in a panel, we have to set the title of its tab item to "Start Page". 

    // ==============================================================
    // Make the text of a tab item have 30 characters at most.

    if (wcslen(title) > 30 ) {
        for(int i = 27; i < (int)wcslen(title) + 1; i++)
            title[i] = i < 30 ? L'.' : L'\0';
    }

    // ==============================================================

    // When one uses ScopedMem<Type>::Set.(Type *o), it will free the object first. See the definitioin.
    // So we don't have to worry about memory leak here.
    title.Set(str::Format(L"%s%s", title, L"     ")); 

    tie.pszText = title;

    SendMessage(panel->hwndTab, TCM_SETITEM, tabIndex, (LPARAM)&tie);
}

void SetTabToolTipText(WindowInfo *win)
{
    if (!win->TabToolTipText == NULL)
        free(win->TabToolTipText);

    if (win->IsDocLoaded()) {
        const WCHAR *baseName = path::GetBaseName(win->dm->FilePath());
        win->TabToolTipText = str::Format(L"%s", baseName);
    } else
        win->TabToolTipText = str::Format(L"%s", L"Start Page");
        // We have to use "str::Format(L"%s", L"Start Page");",
        // otherwsie the "free" above causes heap problem.
        // free() can't be used for a pointer to read-only data.
}

//// Used to communicate with tab close buttons.
//void SetHoverState(WindowInfo *win, bool hover)
//{
//    // Need this check, otherwise memory leak.
//    if (!win->HoverState == NULL)
//        free(win->HoverState);
//
//    if (hover)
//        win->HoverState = str::Format(_T("%s"), _T("Hover"));
//    else
//        win->HoverState = str::Format(_T("%s"), _T("Not Hover"));
//}