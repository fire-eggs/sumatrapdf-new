/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Toolbar_h
#define Toolbar_h

class WindowInfo;
struct WinInfo;
class ToolbarInfo;

void CreateToolbar(WinInfo& winInfo);
void ToolbarUpdateStateForWindow(WindowInfo *win, bool showHide);
void UpdateToolbarButtonsToolTipsForWindow(WindowInfo *win);
void UpdateToolbarFindText(WindowInfo *win);
void UpdateToolbarPageText(WindowInfo *win, int pageCount, bool updateOnly=false);
void UpdateFindbox(WindowInfo* win);
void ShowOrHideToolbarGlobally();
void UpdateToolbarState(WindowInfo *win);

#endif
