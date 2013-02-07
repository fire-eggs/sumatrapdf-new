/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileWatcher.h"

#include "FileUtil.h"
#include "WinUtil.h"

#define INVALID_TOKEN -1

struct WatchedDir {
    WatchedDir * next;
    const WCHAR *dirPath;
    HANDLE       hDir;
    OVERLAPPED   overlapped;
    // a double buffer where the Windows API ReadDirectoryChanges will store the list
    // of files that have been modified.
    FILE_NOTIFY_INFORMATION buffer[2][512];
    int          currBuffer; // current buffer used (alternate between 0 and 1)
};

struct WatchedFile {
    WatchedFile *           next;
    WatchedDir *            watchedDir;
    const WCHAR *           filePath;
    FileChangeObserver *    observer;
    FileWatcherToken        token;
};

static int              g_currentToken = 0;
static HANDLE           g_threadControlHandle = 0;
static WatchedDir *     g_firstDir = NULL;
static WatchedFile *    g_firstFile = NULL;

static WatchedDir *FindExistingWatchedDir(const WCHAR *dirPath)
{
    WatchedDir *curr = g_firstDir;
    while (curr) {
        // TODO: normalize dirPath?
        if (str::EqI(dirPath, curr->dirPath))
            return curr;
        curr = curr->next;
    }
    return NULL;
}

static WatchedFile *FindByToken(FileWatcherToken token)
{
    WatchedFile *curr = g_firstFile;
    while (curr) {
        if (curr->token == token)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

static void DeleteWatchedDir(WatchedDir *wd)
{
    free((void*)wd->dirPath);
    SafeCloseHandle(wd->hDir);
    free(wd);
}

static WatchedDir *NewWatchedDir(const WCHAR *dirPath)
{
    WatchedDir *wd = AllocStruct<WatchedDir>();
    wd->dirPath = str::Dup(dirPath);
    wd->hDir = CreateFile(
        dirPath, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS  | FILE_FLAG_OVERLAPPED, NULL);
    if (!wd->hDir)
        goto Failed;

    wd->overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!wd->overlapped.hEvent)
        goto Failed;

    // start watching the directory
    wd->currBuffer = 0;
    ReadDirectoryChangesW(
         wd->hDir,
         &wd->buffer[wd->currBuffer], /* read results buffer */
         sizeof(wd->buffer[wd->currBuffer]), /* length of buffer */
         FALSE, /* monitoring option */
         FILE_NOTIFY_CHANGE_LAST_WRITE, /* filter conditions */
         NULL, /* bytes returned */
         &wd->overlapped, /* overlapped buffer */
         NULL); /* completion routine */

    wd->next = g_firstDir;
    g_firstDir = wd;

    return wd;
Failed:
    DeleteWatchedDir(wd);
    return NULL;
}

static WatchedFile *NewWatchedFile(const WCHAR *filePath, FileChangeObserver *observer)
{
    ScopedMem<WCHAR> dirPath(path::GetDir(filePath));
    WatchedDir *wd = FindExistingWatchedDir(dirPath);
    if (!wd)
        wd = NewWatchedDir(dirPath);

    WatchedFile *wf = AllocStruct<WatchedFile>();
    wf->filePath = str::Dup(filePath);
    wf->watchedDir = wd;
    wf->observer = observer;
    wf->token = g_currentToken++;
    wf->next = g_firstFile;
    g_firstFile = wf;
    return wf;
}

static void DeleteWatchedFile(WatchedFile *wf)
{
    free((void*)wf->filePath);
    delete wf->observer;
    free(wf);
}

/* Subscribe for notifications about file changes. When a file changes, we'll
call observer->OnFileChanged(). We take ownership of observer object.

Returns a cancellation token that can be used in FileWatcherUnsubscribe(). That
way we can support multiple callers subscribing for the same file.
*/
FileWatcherToken FileWatcherSubscribe(const WCHAR *path, FileChangeObserver *observer)
{
    if (!file::Exists(path)) {
        delete observer;
        return INVALID_TOKEN;
    }
    // TODO: if the file is on a network drive we should periodically check
    // it ourselves, because ReadDirectoryChangesW()
    // doesn't work in that case
    WatchedFile *wf = NewWatchedFile(path, observer);
    return wf->token;
}

static bool IsWatchedDirReferenced(WatchedDir *wd)
{
    for (WatchedFile *wf = g_firstFile; wf; wf->next) {
        if (wf->watchedDir == wd)
            return true;
    }
    return false;
}

static void RemoveWatchedDirIfNotReferenced(WatchedDir *wd)
{
    if (IsWatchedDirReferenced(wd))
        return;
    WatchedDir **currPtr = &g_firstDir;
    WatchedDir *curr;
    for (;;) {
        curr = *currPtr;
        CrashAlwaysIf(!curr);
        if (curr == wd)
            break;
        currPtr = &(curr->next);
    }
    WatchedDir *toRemove = curr;
    *currPtr = toRemove->next;
    DeleteWatchedDir(toRemove);
}

static void RemoveWatchedFile(WatchedFile *wf)
{
    WatchedDir *wd = wf->watchedDir;

    WatchedFile **currPtr = &g_firstFile;
    WatchedFile *curr;
    for (;;) {
        curr = *currPtr;
        CrashAlwaysIf(!curr);
        if (curr == wf)
            break;
        currPtr = &(curr->next);
    }
    WatchedFile *toRemove = curr;
    *currPtr = toRemove->next;
    DeleteWatchedFile(toRemove);

    RemoveWatchedDirIfNotReferenced(wd);
}

void FileWatcherUnsubscribe(FileWatcherToken token)
{
    if (INVALID_TOKEN == token)
        return;
    WatchedFile *wf = FindByToken(token);
    if (!wf)
        return;
    RemoveWatchedFile(wf);
}

