/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DisplayState_h
#define DisplayState_h

#include "BaseEngine.h"

enum DisplayMode {
    DM_FIRST = 0,
    // automatic means: the continuous form of single page, facing or
    // book view - depending on the document's desired PageLayout
    DM_AUTOMATIC = DM_FIRST,
    DM_SINGLE_PAGE,
    DM_FACING,
    DM_BOOK_VIEW,
    DM_CONTINUOUS,
    DM_CONTINUOUS_FACING,
    DM_CONTINUOUS_BOOK_VIEW
};

#define ZOOM_FIT_PAGE       -1.f
#define ZOOM_FIT_WIDTH      -2.f
#define ZOOM_FIT_CONTENT    -3.f
#define ZOOM_ACTUAL_SIZE    100.0f
#define ZOOM_MAX            6400.f /* max zoom in % */
#define ZOOM_MIN            8.33f  /* min zoom in % */
#define INVALID_ZOOM        -99.0f

#include "AppPrefs2.h"

#endif
