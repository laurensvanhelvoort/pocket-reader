#include "library.h"

#include <LittleFS.h>
#include <strings.h>

#include <algorithm>

#include "book.h"
#include "settings.h"
#include "text.h"

std::vector<BookInfo> library;

namespace {

Book scanBook;

bool endsWith(const char* s, const char* suffix) {
    const size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcasecmp(s + a - b, suffix) == 0;
}

}

void scanLibrary() {
    library.clear();
    File dir = LittleFS.open("/books");
    if (!dir || !dir.isDirectory()) return;

    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        char path[64];
        copyUtf8(path, f.path(), sizeof(path));
        f.close();
        if (!endsWith(path, ".prb") || strlen(path) >= sizeof(BookInfo::path)) continue;
        if (!scanBook.open(path)) continue;

        BookInfo info;
        copyUtf8(info.path, path, sizeof(info.path));
        copyUtf8(info.title, scanBook.title(), sizeof(info.title));
        copyUtf8(info.author, scanBook.author(), sizeof(info.author));
        info.percent = scanBook.percent(loadPosition(path));
        info.size = scanBook.size();
        library.push_back(info);
        scanBook.close();
    }

    std::sort(library.begin(), library.end(),
              [](const BookInfo& a, const BookInfo& b) { return strcasecmp(a.title, b.title) < 0; });
}

int libraryIndexOf(const char* path) {
    for (int i = 0; i < (int)library.size(); ++i) {
        if (strcmp(library[i].path, path) == 0) return i;
    }
    return -1;
}
