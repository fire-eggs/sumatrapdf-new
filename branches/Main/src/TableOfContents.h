/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef TableOfContents_h
#define TableOfContents_h

class SidebarInfo;

#include <CommCtrl.h>

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

class WindowInfo;

void CreateToc(SidebarInfo *sideBar, HWND hwndParent);
void ClearTocBox(WindowInfo *win);
void ToggleTocBox(WindowInfo *win);
void LoadTocTree(WindowInfo *win);
void UpdateTocSelection(WindowInfo *win, int currPageNo);
void UpdateTocExpansionState(WindowInfo *win, HTREEITEM hItem);
LRESULT CALLBACK WndProcTocBoxCB(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif
