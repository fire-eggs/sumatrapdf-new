/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Favorites_h
#define Favorites_h

class WindowInfo;
class SidebarInfo;

/*
A favorite is a bookmark (we call it a favorite, like Internet Explorer, to
differentiate from bookmarks inside a PDF file (which really are
table of contents)).

We can have multiple favorites per file.

Favorites are accurate to a page - it's simple and should be good enough
for the user.

A favorite is identified by a (mandatory) page number and (optional) name
(provided by the user) and page label (from BaseEngine::GetPageLabel).

Favorites do not remember presentation settings like zoom or viewing mode -
they are for navigation only. Presentation settings are remembered on a
per-file basis in FileHistory.
*/

class FavName {
public:
    ScopedMem<WCHAR>    name;
    int                 pageNo;
    // TODO: persist pageLabel
    ScopedMem<WCHAR>    pageLabel;
    int                 menuId; // assigned in AppendFavMenuItems()

    FavName(int pageNo, const WCHAR *name, const WCHAR *pageLabel) :
        pageNo(pageNo), name(str::Dup(name)), pageLabel(str::Dup(pageLabel)) { }

    void ChangeName(const WCHAR *newName) {
        name.Set(str::Dup(newName));
    }
};

// list of favorites for one file
class FileFavs {

    int FindByPage(int pageNo, const WCHAR *pageLabel=NULL) const;

    static int SortByPageNo(const void *a, const void *b);

    public:
    ScopedMem<WCHAR>filePath;
    Vec<FavName *>  favNames;

    FileFavs(const WCHAR *fp) : filePath(str::Dup(fp)) { }
    ~FileFavs() { DeleteVecMembers(favNames); }

    bool IsEmpty() const {
        return favNames.Count() == 0;
    }

    bool Exists(int pageNo) const {
        return FindByPage(pageNo) != -1;
    }

    void ResetMenuIds();
    bool GetByMenuId(int menuId, size_t& idx);
    bool HasFavName(FavName *fn);
    bool Remove(int pageNo);
    void AddOrReplace(int pageNo, const WCHAR *name, const WCHAR *pageLabel);
};

class Favorites {

    // filePathCache points to a string inside FileFavs, so doesn't need to free()d
    const WCHAR *filePathCache;
    size_t       idxCache;

    void RemoveFav(FileFavs *fav, size_t idx);

public:
    Vec<FileFavs*> favs;

    Favorites() : filePathCache(NULL), idxCache((size_t)-1) { }
    ~Favorites() { DeleteVecMembers(favs); }

    size_t Count() const {
       return favs.Count();
    }

    FileFavs *GetByMenuId(int menuId, size_t& idx);
    FileFavs *GetByFavName(FavName *fn);
    void ResetMenuIds();
    FileFavs *GetFavByFilePath(const WCHAR *filePath, bool createIfNotExist=false, size_t *idx=NULL);
    bool IsPageInFavorites(const WCHAR *filePath, int pageNo);
    void AddOrReplace(const WCHAR *filePath, int pageNo, const WCHAR *name, const WCHAR *pageLabel=NULL);
    void Remove(const WCHAR *filePath, int pageNo);
    void RemoveAllForFile(const WCHAR *filePath);
};

void AddFavorite(WindowInfo *win);
void DelFavorite(WindowInfo *win);
void RebuildFavMenu(WindowInfo *win, HMENU menu);
void CreateFavorites(SidebarInfo *sideBar, HWND hwndParent);
void ToggleFavorites(WindowInfo *win);
void PopulateFavTreeIfNeeded(WindowInfo *win);
void RememberFavTreeExpansionStateForAllWindows();
void GoToFavoriteByMenuId(WindowInfo *win, int wmId);
void UpdateFavoritesTreeForAllWindows();
LRESULT CALLBACK WndProcFavBoxCB(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif
