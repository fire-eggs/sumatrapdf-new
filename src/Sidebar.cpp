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
	SetWindowPos(sideBar->hwndTocBox, NULL, 0, 0, dx, dy, SWP_NOZORDER);
}

void SidebarBottomOnSize(SidebarInfo *sideBar, int dx, int dy)
{
	SetWindowPos(sideBar->hwndFavBox, NULL, 0, 0, dx, dy, SWP_NOZORDER);
}

void SidebarOnSize(SidebarInfo *sideBar, int dx, int dy)
{
	ClientRect rSidebar(sideBar->hwndSidebar);
	ClientRect rTop(sideBar->hwndSidebarTop);
	int sidebarTopDy = rTop.dy;
	if (sidebarTopDy == 0) {
		sidebarTopDy = rSidebar.dy / 2;
	}

	SetWindowPos(sideBar->hwndSidebarTop, NULL, 0, 0, rSidebar.dx, sidebarTopDy, SWP_NOZORDER);
	SetWindowPos(sideBar->hwndFavSplitter, NULL, 0, sidebarTopDy, rSidebar.dx, SPLITTER_DY, SWP_NOZORDER);
	SetWindowPos(sideBar->hwndSidebarBottom, NULL, 0, sidebarTopDy + SPLITTER_DY, dx, rSidebar.dy - sidebarTopDy - SPLITTER_DY, SWP_NOZORDER);
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
	rc.bottom = dy;
	FillRect(hdc, &rc, gBrushAboutBg);

	EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProcSidebarTopCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	WindowInfo *win = FindWindowInfoByHwnd(hwnd);

	switch (msg) {
	case WM_SIZE:
		SidebarTopOnSize(win->sideBar(), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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

LRESULT CALLBACK WndProcSidebarBottomCB(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	WindowInfo *win = FindWindowInfoByHwnd(hwnd);

	switch (msg) {
	case WM_SIZE:
		SidebarBottomOnSize(win->sideBar(), GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
		NULL,
		SIDEBAR_CLASS_NAME, NULL,
		WS_CHILD,
		0, 0, gGlobalPrefs.sidebarDx, 0,
		winInfo.Hwnd(), NULL,
		ghinst, NULL);
	if (!hwndSidebar)
		return;

	HWND hwndSidebarTop = CreateWindowEx(
		NULL,
		SIDEBAR_TOP_CLASS_NAME, NULL,
		WS_CHILD,
		0, 0, 0, 0,
		hwndSidebar, NULL,
		ghinst, NULL);
	if (!hwndSidebarTop)
		return;

	HWND hwndSidebarBottom = CreateWindowEx(
		NULL,
		SIDEBAR_BOTTOM_CLASS_NAME, NULL,
		WS_CHILD,
		0, 0, 0, 0,
		hwndSidebar, NULL,
		ghinst, NULL);
	if (!hwndSidebarBottom)
		return;

	HWND hwndFavSplitter = CreateWindowEx(
		NULL,
		FAV_SPLITTER_CLASS_NAME, NULL,
		WS_CHILDWINDOW,
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