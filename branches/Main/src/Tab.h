/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Tab_h
#define Tab_h

#include "Version.h"
#include <CommCtrl.h>

#define TAB_CONTROL_DY      26

class PanelInfo;
class WindowInfo;

void AddTab(WindowInfo *win);
void ShowOrHideTabGlobally();
int  GetTabIndex(PanelInfo *panel);
void SetTabCloseButtonPos(WindowInfo *win);
void SetTabTitle(WindowInfo *win);
void SetTabToolTipText(WindowInfo *win);
void SetHoverState(WindowInfo *win, bool hover);
void CreateTabControl(PanelInfo *panel);
void CreateOrShowTabPreview(PanelInfo *panel, int tabIndex);
void ToggleTabPreview(PanelInfo *panel);
void UpdateCloseButtonBitmap(PanelInfo *panel, int tabIndex, int bmpIndex);
void CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

#endif