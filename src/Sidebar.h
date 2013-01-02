/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Sidebar_h
#define Sidebar_h

class PanelInfo;
struct WinInfo;
class SidebarInfo;

//void ToggleSidebar(PanelInfo *panel);
void SidebarOnSize(SidebarInfo *sideBar, int dx, int dy);
//void SetTocVisibility(PanelInfo *panel, bool tocVisible);
//void SetFavVisibility(PanelInfo *panel, bool favVisible);
//void SetSearchVisibility(PanelInfo *panel, bool searchVisible);
//void SetTabPreviewVisibility(PanelInfo *panel, bool tabPreviewVisible);
void CreateSidebarBox(WinInfo& winInfo);

LRESULT CALLBACK WndProcSidebarCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcSidebarTopCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcSidebarBottomCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif