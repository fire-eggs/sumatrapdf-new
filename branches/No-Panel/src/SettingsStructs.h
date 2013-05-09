/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 (see COPYING) */

// This file is auto-generated by gen_settingsstructs.py

#ifndef SettingsStructs_h
#define SettingsStructs_h

// top, right, bottom and left margin (in that order) between window and
// document
struct WindowMargin {
    // size of the top margin between window and document
    int top;
    // size of the right margin between window and document
    int right;
    // size of the bottom margin between window and document
    int bottom;
    // size of the left margin between window and document
    int left;
};

// customization options for PDF, XPS, DjVu and PostScript UI
struct FixedPageUI {
    // color value with which black (text) will be substituted
    COLORREF textColor;
    // color value with which white (background) will be substituted
    COLORREF backgroundColor;
    // top, right, bottom and left margin (in that order) between window
    // and document
    WindowMargin windowMargin;
    // horizontal and vertical distance between two pages in facing and
    // book view modes
    SizeI pageSpacing;
    // colors to use for the gradient from top to bottom (stops will be
    // inserted at regular intervals throughout the document); currently
    // only up to three colors are supported; the idea behind this
    // experimental feature is that the background might allow to
    // subconsciously determine reading progress; suggested values: #2828aa
    // #28aa28 #aa2828
    Vec<COLORREF> * gradientColors;
};

// customization options for eBooks (EPUB, Mobi, FictionBook) UI. If
// UseFixedPageUI is true, FixedPageUI settings apply instead
struct EbookUI {
    // name of the font
    WCHAR * fontName;
    // size of the font
    float fontSize;
    // color for text
    COLORREF textColor;
    // color of the background (page)
    COLORREF backgroundColor;
    // if true, the UI used for PDF documents will be used for ebooks as
    // well (enables printing and searching, disables automatic reflow)
    bool useFixedPageUI;
};

// customization options for Comic Book and images UI
struct ComicBookUI {
    // top, right, bottom and left margin (in that order) between window
    // and document
    WindowMargin windowMargin;
    // horizontal and vertical distance between two pages in facing and
    // book view modes
    SizeI pageSpacing;
    // if true, default to displaying Comic Book files in manga mode (from
    // right to left if showing 2 pages at a time)
    bool cbxMangaMode;
};

// customization options for CHM UI. If UseFixedPageUI is true,
// FixedPageUI settings apply instead
struct ChmUI {
    // if true, the UI used for PDF documents will be used for CHM
    // documents as well
    bool useFixedPageUI;
};

// list of additional external viewers for various file types (can have
// multiple entries for the same format)
struct ExternalViewer {
    // command line with which to call the external viewer, may contain %p
    // for page numer and %1 for the file name
    WCHAR * commandLine;
    // name of the external viewer to be shown in the menu (implied by
    // CommandLine if missing)
    WCHAR * name;
    // filter for which file types the menu item is to be shown (e.g.
    // "*.pdf;*.xps"; "*" if missing)
    WCHAR * filter;
};

// these override the default settings in the Print dialog
struct PrinterDefaults {
    // default value for scaling (shrink, fit, none)
    char * printScale;
    // default value for the compatibility option
    bool printAsImage;
};

// customization options for how we show forward search results (used
// from LaTeX editors)
struct ForwardSearch {
    // when set to a positive value, the forward search highlight style
    // will be changed to a rectangle at the left of the page (with the
    // indicated amount of margin from the page margin)
    int highlightOffset;
    // width of the highlight rectangle (if HighlightOffset is > 0)
    int highlightWidth;
    // color used for the forward search highlight
    COLORREF highlightColor;
    // if true, highlight remains visible until the next mouse click
    // (instead of fading away immediately)
    bool highlightPermanent;
};

// default values for user added annotations in FixedPageUI documents
struct AnnotationDefaults {
    // color used for the highlight tool (in prerelease builds, the current
    // selection can be converted into a highlight annotation by pressing
    // the 'h' key)
    COLORREF highlightColor;
};

// Values which are persisted for bookmarks/favorites
struct Favorite {
    // name of this favorite as shown in the menu
    WCHAR * name;
    // number of the bookmarked page
    int pageNo;
    // label for this page (only present if logical and physical page
    // numbers are not the same)
    WCHAR * pageLabel;
    // id of this favorite in the menu (assigned by AppendFavMenuItems)
    int menuId;
};

// information about opened files (in most recently used order)
struct FileState {
    // path of the document
    WCHAR * filePath;
    // Values which are persisted for bookmarks/favorites
    Vec<Favorite *> * favorites;
    // a document can be "pinned" to the Frequently Read list so that it
    // isn't displaced by recently opened documents
    bool isPinned;
    // if a document can no longer be found but we still remember valuable
    // state, it's classified as missing so that it can be hidden instead
    // of removed
    bool isMissing;
    // in order to prevent documents that haven't been opened for a while
    // but used to be opened very frequently constantly remain in top
    // positions, the openCount will be cut in half after every week, so
    // that the Frequently Read list hopefully better reflects the
    // currently relevant documents
    int openCount;
    // Hex encoded MD5 fingerprint of file content (32 chars) followed by
    // crypt key (64 chars) - only applies for PDF documents
    char * decryptionKey;
    // if true, we use global defaults when opening this file (instead of
    // the values below)
    bool useDefaultState;
    // how pages should be laid out for this document, needs to be
    // synchronized with DefaultDisplayMode after deserialization and
    // before serialization
    WCHAR * displayMode;
    // how far this document has been scrolled (in x and y direction)
    PointI scrollPos;
    // number of the last read page
    int pageNo;
    // zoom (in %) or one of those values: fit page, fit width, fit content
    char * zoom;
    // how far pages have been rotated as a multiple of 90 degrees
    int rotation;
    // state of the window. 1 is normal, 2 is maximized, 3 is fullscreen, 4
    // is minimized
    int windowState;
    // default position (can be on any monitor)
    RectI windowPos;
    // if true, we show table of contents (Bookmarks) sidebar if it's
    // present in the document
    bool showToc;
    // width of the left sidebar panel containing the table of contents
    int sidebarDx;
    // if true, the document is displayed right-to-left in facing and book
    // view modes (only used for comic book documents)
    bool displayR2L;
    // index into an ebook's HTML data from which reparsing has to happen
    // in order to restore the last viewed page (i.e. the equivalent of
    // PageNo for the ebook UI)
    int reparseIdx;
    // tocState is an array of ids for ToC items that have been toggled by
    // the user (i.e. aren't in their default expansion state). - Note: We
    // intentionally track toggle state as opposed to expansion state so
    // that we only have to save a diff instead of all states for the whole
    // tree (which can be quite large) (internal)
    Vec<int> * tocState;
    // thumbnails are saved as PNG files in sumatrapdfcache directory
    RenderedBitmap * thumbnail;
    // temporary value needed for FileHistory::cmpOpenCount
    size_t index;
};

// Most values on this structure can be updated through the UI and are
// persisted in SumatraPDF-settings.txt (previously in
// sumatrapdfprefs.dat)
struct GlobalPrefs {
    // background color of the non-document windows, traditionally yellow
    COLORREF mainWindowBackground;
    // if true, Esc key closes SumatraPDF
    bool escToExit;
    // if true, we'll always open files using existing SumatraPDF process
    bool reuseInstance;
    // customization options for PDF, XPS, DjVu and PostScript UI
    FixedPageUI fixedPageUI;
    // customization options for eBooks (EPUB, Mobi, FictionBook) UI. If
    // UseFixedPageUI is true, FixedPageUI settings apply instead
    EbookUI ebookUI;
    // customization options for Comic Book and images UI
    ComicBookUI comicBookUI;
    // customization options for CHM UI. If UseFixedPageUI is true,
    // FixedPageUI settings apply instead
    ChmUI chmUI;
    // list of additional external viewers for various file types (can have
    // multiple entries for the same format)
    Vec<ExternalViewer *> * externalViewers;
    // zoom levels which zooming steps through in addition to Fit Page, Fit
    // Width and the minimum and maximum allowed values (8.33 and 6400)
    Vec<float> * zoomLevels;
    // zoom step size in percents relative to the current zoom level. if
    // zero or negative, the values from ZoomLevels are used instead
    float zoomIncrement;
    // these override the default settings in the Print dialog
    PrinterDefaults printerDefaults;
    // customization options for how we show forward search results (used
    // from LaTeX editors)
    ForwardSearch forwardSearch;
    // default values for user added annotations in FixedPageUI documents
    AnnotationDefaults annotationDefaults;
    // if true, we store display settings for each document separately
    // (i.e. everything after UseDefaultState in FileStates)
    bool rememberStatePerDocument;
    // ISO code of the current UI language
    char * uiLanguage;
    // if true, we show the toolbar at the top of the window
    bool showToolbar;
    // if true, we show the Favorites sidebar
    bool showFavorites;
    // a list of extensions that SumatraPDF has associated itself with and
    // will reassociate if a different application takes over (e.g. ".pdf
    // .xps .epub")
    WCHAR * associatedExtensions;
    // whether file associations should be fixed silently or only after
    // user feedback
    bool associateSilently;
    // if true, we check once a day if an update is available
    bool checkForUpdates;
    // we won't ask again to update to this version
    WCHAR * versionToSkip;
    // if true, we remember which files we opened and their display
    // settings
    bool rememberOpenedFiles;
    // if true, we use Windows system colors for background/text color.
    // Over-rides other settings
    bool useSysColors;
    // pattern used to launch the LaTeX editor when doing inverse search
    WCHAR * inverseSearchCmdLine;
    // if true, we expose the SyncTeX inverse search command line in
    // Settings -> Options
    bool enableTeXEnhancements;
    // how pages should be laid out by default, needs to be synchronized
    // with DefaultDisplayMode after deserialization and before
    // serialization
    WCHAR * defaultDisplayMode;
    // default zoom (in %) or one of those values: fit page, fit width, fit
    // content
    char * defaultZoom;
    // default state of new windows (same as the last closed)
    int windowState;
    // default position (can be on any monitor)
    RectI windowPos;
    // if true, we show table of contents (Bookmarks) sidebar if it's
    // present in the document
    bool showToc;
    // width of favorites/bookmarks sidebar (if shown)
    int sidebarDx;
    // if both favorites and bookmarks parts of sidebar are visible, this
    // is the height of bookmarks (table of contents) part
    int tocDy;
    // if true, we show a list of frequently read documents when no
    // document is loaded
    bool showStartPage;
    // information about opened files (in most recently used order)
    Vec<FileState *> * fileStates;
    // timestamp of the last update check
    FILETIME timeOfLastUpdateCheck;
    // week count since 2011-01-01 needed to "age" openCount values in file
    // history
    int openCountWeek;
    // modification time of the preferences file when it was last read
    FILETIME lastPrefUpdate;
    // value of DefaultDisplayMode for internal usage
    DisplayMode defaultDisplayModeEnum;
    // value of DefaultZoom for internal usage
    float defaultZoomFloat;
};

#ifdef INCLUDE_SETTINGSSTRUCTS_METADATA

#include "SettingsUtil.h"

static const FieldInfo gWindowMarginFields[] = {
    { offsetof(WindowMargin, top),    Type_Int, 2 },
    { offsetof(WindowMargin, right),  Type_Int, 4 },
    { offsetof(WindowMargin, bottom), Type_Int, 2 },
    { offsetof(WindowMargin, left),   Type_Int, 4 },
};
static const StructInfo gWindowMarginInfo = { sizeof(WindowMargin), 4, gWindowMarginFields, "Top\0Right\0Bottom\0Left" };

static const FieldInfo gSizeIFields[] = {
    { offsetof(SizeI, dx), Type_Int, 4 },
    { offsetof(SizeI, dy), Type_Int, 4 },
};
static const StructInfo gSizeIInfo = { sizeof(SizeI), 2, gSizeIFields, "Dx\0Dy" };

static const FieldInfo gFixedPageUIFields[] = {
    { offsetof(FixedPageUI, textColor),       Type_Color,      0x000000                     },
    { offsetof(FixedPageUI, backgroundColor), Type_Color,      0xffffff                     },
    { offsetof(FixedPageUI, windowMargin),    Type_Compact,    (intptr_t)&gWindowMarginInfo },
    { offsetof(FixedPageUI, pageSpacing),     Type_Compact,    (intptr_t)&gSizeIInfo        },
    { offsetof(FixedPageUI, gradientColors),  Type_ColorArray, NULL                         },
};
static const StructInfo gFixedPageUIInfo = { sizeof(FixedPageUI), 5, gFixedPageUIFields, "TextColor\0BackgroundColor\0WindowMargin\0PageSpacing\0GradientColors" };

static const FieldInfo gEbookUIFields[] = {
    { offsetof(EbookUI, fontName),        Type_String, (intptr_t)L"Georgia" },
    { offsetof(EbookUI, fontSize),        Type_Float,  (intptr_t)"12.5"     },
    { offsetof(EbookUI, textColor),       Type_Color,  0x324b5f             },
    { offsetof(EbookUI, backgroundColor), Type_Color,  0xd9f0fb             },
    { offsetof(EbookUI, useFixedPageUI),  Type_Bool,   false                },
};
static const StructInfo gEbookUIInfo = { sizeof(EbookUI), 5, gEbookUIFields, "FontName\0FontSize\0TextColor\0BackgroundColor\0UseFixedPageUI" };

static const FieldInfo gWindowMargin_1_Fields[] = {
    { offsetof(WindowMargin, top),    Type_Int, 0 },
    { offsetof(WindowMargin, right),  Type_Int, 0 },
    { offsetof(WindowMargin, bottom), Type_Int, 0 },
    { offsetof(WindowMargin, left),   Type_Int, 0 },
};
static const StructInfo gWindowMargin_1_Info = { sizeof(WindowMargin), 4, gWindowMargin_1_Fields, "Top\0Right\0Bottom\0Left" };

static const FieldInfo gSizeI_1_Fields[] = {
    { offsetof(SizeI, dx), Type_Int, 4 },
    { offsetof(SizeI, dy), Type_Int, 4 },
};
static const StructInfo gSizeI_1_Info = { sizeof(SizeI), 2, gSizeI_1_Fields, "Dx\0Dy" };

static const FieldInfo gComicBookUIFields[] = {
    { offsetof(ComicBookUI, windowMargin), Type_Compact, (intptr_t)&gWindowMargin_1_Info },
    { offsetof(ComicBookUI, pageSpacing),  Type_Compact, (intptr_t)&gSizeI_1_Info        },
    { offsetof(ComicBookUI, cbxMangaMode), Type_Bool,    false                           },
};
static const StructInfo gComicBookUIInfo = { sizeof(ComicBookUI), 3, gComicBookUIFields, "WindowMargin\0PageSpacing\0CbxMangaMode" };

static const FieldInfo gChmUIFields[] = {
    { offsetof(ChmUI, useFixedPageUI), Type_Bool, false },
};
static const StructInfo gChmUIInfo = { sizeof(ChmUI), 1, gChmUIFields, "UseFixedPageUI" };

static const FieldInfo gExternalViewerFields[] = {
    { offsetof(ExternalViewer, commandLine), Type_String, NULL },
    { offsetof(ExternalViewer, name),        Type_String, NULL },
    { offsetof(ExternalViewer, filter),      Type_String, NULL },
};
static const StructInfo gExternalViewerInfo = { sizeof(ExternalViewer), 3, gExternalViewerFields, "CommandLine\0Name\0Filter" };

static const FieldInfo gPrinterDefaultsFields[] = {
    { offsetof(PrinterDefaults, printScale),   Type_Utf8String, (intptr_t)"shrink" },
    { offsetof(PrinterDefaults, printAsImage), Type_Bool,       false              },
};
static const StructInfo gPrinterDefaultsInfo = { sizeof(PrinterDefaults), 2, gPrinterDefaultsFields, "PrintScale\0PrintAsImage" };

static const FieldInfo gForwardSearchFields[] = {
    { offsetof(ForwardSearch, highlightOffset),    Type_Int,   0        },
    { offsetof(ForwardSearch, highlightWidth),     Type_Int,   15       },
    { offsetof(ForwardSearch, highlightColor),     Type_Color, 0x6581ff },
    { offsetof(ForwardSearch, highlightPermanent), Type_Bool,  false    },
};
static const StructInfo gForwardSearchInfo = { sizeof(ForwardSearch), 4, gForwardSearchFields, "HighlightOffset\0HighlightWidth\0HighlightColor\0HighlightPermanent" };

static const FieldInfo gAnnotationDefaultsFields[] = {
    { offsetof(AnnotationDefaults, highlightColor), Type_Color, 0x60ffff },
};
static const StructInfo gAnnotationDefaultsInfo = { sizeof(AnnotationDefaults), 1, gAnnotationDefaultsFields, "HighlightColor" };

static const FieldInfo gRectIFields[] = {
    { offsetof(RectI, x),  Type_Int, 0 },
    { offsetof(RectI, y),  Type_Int, 0 },
    { offsetof(RectI, dx), Type_Int, 0 },
    { offsetof(RectI, dy), Type_Int, 0 },
};
static const StructInfo gRectIInfo = { sizeof(RectI), 4, gRectIFields, "X\0Y\0Dx\0Dy" };

static const FieldInfo gFavoriteFields[] = {
    { offsetof(Favorite, name),      Type_String, NULL },
    { offsetof(Favorite, pageNo),    Type_Int,    0    },
    { offsetof(Favorite, pageLabel), Type_String, NULL },
};
static const StructInfo gFavoriteInfo = { sizeof(Favorite), 3, gFavoriteFields, "Name\0PageNo\0PageLabel" };

static const FieldInfo gPointIFields[] = {
    { offsetof(PointI, x), Type_Int, 0 },
    { offsetof(PointI, y), Type_Int, 0 },
};
static const StructInfo gPointIInfo = { sizeof(PointI), 2, gPointIFields, "X\0Y" };

static const FieldInfo gRectI_1_Fields[] = {
    { offsetof(RectI, x),  Type_Int, 0 },
    { offsetof(RectI, y),  Type_Int, 0 },
    { offsetof(RectI, dx), Type_Int, 0 },
    { offsetof(RectI, dy), Type_Int, 0 },
};
static const StructInfo gRectI_1_Info = { sizeof(RectI), 4, gRectI_1_Fields, "X\0Y\0Dx\0Dy" };

static const FieldInfo gFileStateFields[] = {
    { offsetof(FileState, filePath),        Type_String,     NULL                     },
    { offsetof(FileState, favorites),       Type_Array,      (intptr_t)&gFavoriteInfo },
    { offsetof(FileState, isPinned),        Type_Bool,       false                    },
    { offsetof(FileState, isMissing),       Type_Bool,       false                    },
    { offsetof(FileState, openCount),       Type_Int,        0                        },
    { offsetof(FileState, decryptionKey),   Type_Utf8String, NULL                     },
    { offsetof(FileState, useDefaultState), Type_Bool,       false                    },
    { offsetof(FileState, displayMode),     Type_String,     (intptr_t)L"automatic"   },
    { offsetof(FileState, scrollPos),       Type_Compact,    (intptr_t)&gPointIInfo   },
    { offsetof(FileState, pageNo),          Type_Int,        1                        },
    { offsetof(FileState, zoom),            Type_Utf8String, (intptr_t)"fit page"     },
    { offsetof(FileState, rotation),        Type_Int,        0                        },
    { offsetof(FileState, windowState),     Type_Int,        0                        },
    { offsetof(FileState, windowPos),       Type_Compact,    (intptr_t)&gRectI_1_Info },
    { offsetof(FileState, showToc),         Type_Bool,       true                     },
    { offsetof(FileState, sidebarDx),       Type_Int,        0                        },
    { offsetof(FileState, displayR2L),      Type_Bool,       false                    },
    { offsetof(FileState, reparseIdx),      Type_Int,        0                        },
    { offsetof(FileState, tocState),        Type_IntArray,   NULL                     },
};
static StructInfo gFileStateInfo = { sizeof(FileState), 19, gFileStateFields, "FilePath\0Favorites\0IsPinned\0IsMissing\0OpenCount\0DecryptionKey\0UseDefaultState\0DisplayMode\0ScrollPos\0PageNo\0Zoom\0Rotation\0WindowState\0WindowPos\0ShowToc\0SidebarDx\0DisplayR2L\0ReparseIdx\0TocState" };

static const FieldInfo gFILETIMEFields[] = {
    { offsetof(FILETIME, dwHighDateTime), Type_Int, 0 },
    { offsetof(FILETIME, dwLowDateTime),  Type_Int, 0 },
};
static const StructInfo gFILETIMEInfo = { sizeof(FILETIME), 2, gFILETIMEFields, "DwHighDateTime\0DwLowDateTime" };

static const FieldInfo gGlobalPrefsFields[] = {
    { (size_t)-1,                                      Type_Comment,    (intptr_t)"For documentation, see http://blog.kowalczyk.info/software/sumatrapdf/settings2.3.html"                    },
    { (size_t)-1,                                      Type_Comment,    NULL                                                                                                                  },
    { offsetof(GlobalPrefs, mainWindowBackground),     Type_Color,      0x8000f2ff                                                                                                            },
    { offsetof(GlobalPrefs, escToExit),                Type_Bool,       false                                                                                                                 },
    { offsetof(GlobalPrefs, reuseInstance),            Type_Bool,       false                                                                                                                 },
    { offsetof(GlobalPrefs, fixedPageUI),              Type_Struct,     (intptr_t)&gFixedPageUIInfo                                                                                           },
    { offsetof(GlobalPrefs, ebookUI),                  Type_Struct,     (intptr_t)&gEbookUIInfo                                                                                               },
    { offsetof(GlobalPrefs, comicBookUI),              Type_Struct,     (intptr_t)&gComicBookUIInfo                                                                                           },
    { offsetof(GlobalPrefs, chmUI),                    Type_Struct,     (intptr_t)&gChmUIInfo                                                                                                 },
    { offsetof(GlobalPrefs, externalViewers),          Type_Array,      (intptr_t)&gExternalViewerInfo                                                                                        },
    { offsetof(GlobalPrefs, zoomLevels),               Type_FloatArray, (intptr_t)"8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400" },
    { offsetof(GlobalPrefs, zoomIncrement),            Type_Float,      (intptr_t)"0"                                                                                                         },
    { offsetof(GlobalPrefs, printerDefaults),          Type_Struct,     (intptr_t)&gPrinterDefaultsInfo                                                                                       },
    { offsetof(GlobalPrefs, forwardSearch),            Type_Struct,     (intptr_t)&gForwardSearchInfo                                                                                         },
    { offsetof(GlobalPrefs, annotationDefaults),       Type_Struct,     (intptr_t)&gAnnotationDefaultsInfo                                                                                    },
    { (size_t)-1,                                      Type_Comment,    NULL                                                                                                                  },
    { offsetof(GlobalPrefs, rememberStatePerDocument), Type_Bool,       true                                                                                                                  },
    { offsetof(GlobalPrefs, uiLanguage),               Type_Utf8String, NULL                                                                                                                  },
    { offsetof(GlobalPrefs, showToolbar),              Type_Bool,       true                                                                                                                  },
    { offsetof(GlobalPrefs, showFavorites),            Type_Bool,       false                                                                                                                 },
    { offsetof(GlobalPrefs, associatedExtensions),     Type_String,     NULL                                                                                                                  },
    { offsetof(GlobalPrefs, associateSilently),        Type_Bool,       false                                                                                                                 },
    { offsetof(GlobalPrefs, checkForUpdates),          Type_Bool,       true                                                                                                                  },
    { offsetof(GlobalPrefs, versionToSkip),            Type_String,     NULL                                                                                                                  },
    { offsetof(GlobalPrefs, rememberOpenedFiles),      Type_Bool,       true                                                                                                                  },
    { offsetof(GlobalPrefs, useSysColors),             Type_Bool,       false                                                                                                                 },
    { offsetof(GlobalPrefs, inverseSearchCmdLine),     Type_String,     NULL                                                                                                                  },
    { offsetof(GlobalPrefs, enableTeXEnhancements),    Type_Bool,       false                                                                                                                 },
    { offsetof(GlobalPrefs, defaultDisplayMode),       Type_String,     (intptr_t)L"automatic"                                                                                                },
    { offsetof(GlobalPrefs, defaultZoom),              Type_Utf8String, (intptr_t)"fit page"                                                                                                  },
    { offsetof(GlobalPrefs, windowState),              Type_Int,        1                                                                                                                     },
    { offsetof(GlobalPrefs, windowPos),                Type_Compact,    (intptr_t)&gRectIInfo                                                                                                 },
    { offsetof(GlobalPrefs, showToc),                  Type_Bool,       true                                                                                                                  },
    { offsetof(GlobalPrefs, sidebarDx),                Type_Int,        0                                                                                                                     },
    { offsetof(GlobalPrefs, tocDy),                    Type_Int,        0                                                                                                                     },
    { offsetof(GlobalPrefs, showStartPage),            Type_Bool,       true                                                                                                                  },
    { (size_t)-1,                                      Type_Comment,    NULL                                                                                                                  },
    { offsetof(GlobalPrefs, fileStates),               Type_Array,      (intptr_t)&gFileStateInfo                                                                                             },
    { offsetof(GlobalPrefs, timeOfLastUpdateCheck),    Type_Compact,    (intptr_t)&gFILETIMEInfo                                                                                              },
    { offsetof(GlobalPrefs, openCountWeek),            Type_Int,        0                                                                                                                     },
};
static const StructInfo gGlobalPrefsInfo = { sizeof(GlobalPrefs), 40, gGlobalPrefsFields, "\0\0MainWindowBackground\0EscToExit\0ReuseInstance\0FixedPageUI\0EbookUI\0ComicBookUI\0ChmUI\0ExternalViewers\0ZoomLevels\0ZoomIncrement\0PrinterDefaults\0ForwardSearch\0AnnotationDefaults\0\0RememberStatePerDocument\0UiLanguage\0ShowToolbar\0ShowFavorites\0AssociatedExtensions\0AssociateSilently\0CheckForUpdates\0VersionToSkip\0RememberOpenedFiles\0UseSysColors\0InverseSearchCmdLine\0EnableTeXEnhancements\0DefaultDisplayMode\0DefaultZoom\0WindowState\0WindowPos\0ShowToc\0SidebarDx\0TocDy\0ShowStartPage\0\0FileStates\0TimeOfLastUpdateCheck\0OpenCountWeek" };

#endif

#endif
