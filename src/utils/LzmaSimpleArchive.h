/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef LzmaSimpleArchive_h
#define LzmaSimpleArchive_h

namespace lzma {

struct FileInfo {
    // public data
    size_t          uncompressedSize;
    size_t          compressedSize;
    uint32_t        compressedCrc32;
    FILETIME        ftModified;
    const char *    name;
    const char *    compressedData;

    // for internal use
    size_t          off;
};

// Note: good enough for our purposes, can be expanded when needed
#define MAX_LZMA_ARCHIVE_FILES 128

struct ArchiveInfo {
    int             filesCount;
    FileInfo        files[MAX_LZMA_ARCHIVE_FILES];
};

bool   ParseArchiveInfo(const char *archiveHeader, size_t dataLen, ArchiveInfo *archiveInfoOut);
char*  Decompress(const char *in, size_t inSize, size_t *uncompressedSizeOut, Allocator *allocator);
char * GetFileDataByIdx(ArchiveInfo *archive, int idx, Allocator *allocator);
bool   ExtractFiles(const char *archivePath, const char *dstDir, const char **files, Allocator *allocator=NULL);

}

#endif
