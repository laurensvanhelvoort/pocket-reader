#include "book.h"

#include "text.h"

namespace {

bool isWordBreak(int c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

bool startsWith(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

}

bool Book::open(const char* path) {
    close();
    file_ = LittleFS.open(path, "r");
    if (!file_) return false;
    size_ = file_.size();
    bufStart_ = 0;
    bufLen_ = 0;
    copyUtf8(path_, path, sizeof(path_));
    title_[0] = author_[0] = '\0';

    char line[256];
    uint32_t pos = 0;
    if (!readLine(pos, line, sizeof(line)) || strcmp(line, "PRB1") != 0) {
        file_.close();
        return false;
    }
    while (readLine(pos, line, sizeof(line))) {
        if (strcmp(line, "---") == 0) {
            bodyStart_ = pos;
            open_ = true;
            break;
        }
        if (startsWith(line, "title: ")) {
            copyUtf8(title_, line + 7, sizeof(title_));
        } else if (startsWith(line, "author: ")) {
            copyUtf8(author_, line + 8, sizeof(author_));
        } else if (startsWith(line, "chapter: ")) {
            Chapter c;
            char* rest;
            c.offset = strtoul(line + 9, &rest, 10);
            while (*rest == ' ') ++rest;
            copyUtf8(c.title, rest, sizeof(c.title));
            chapters_.push_back(c);
        }
    }
    if (!open_) {
        file_.close();
        chapters_.clear();
        return false;
    }
    for (auto& c : chapters_) c.offset += bodyStart_;
    if (title_[0] == '\0') copyUtf8(title_, path, sizeof(title_));
    cur_ = bodyStart_;
    return true;
}

void Book::close() {
    if (open_) file_.close();
    open_ = false;
    chapters_.clear();
    bufLen_ = 0;
}

int Book::chapterAt(uint32_t pos) const {
    int found = -1;
    for (int i = 0; i < (int)chapters_.size(); ++i) {
        if (chapters_[i].offset <= pos) found = i;
    }
    return found;
}

int Book::percent(uint32_t pos) const {
    if (size_ <= bodyStart_ || pos <= bodyStart_) return 0;
    const uint64_t p = (uint64_t)(pos - bodyStart_) * 100 / (size_ - bodyStart_);
    return p > 100 ? 100 : (int)p;
}

int Book::byteAt(uint32_t i) {
    if (i >= size_) return -1;
    if (i < bufStart_ || i >= bufStart_ + bufLen_) {
        bufStart_ = i > 512 ? i - 512 : 0;
        file_.seek(bufStart_);
        const int n = file_.read(buf_, sizeof(buf_));
        bufLen_ = n > 0 ? n : 0;
        if (i >= bufStart_ + bufLen_) return -1;
    }
    return buf_[i - bufStart_];
}

bool Book::readLine(uint32_t& pos, char* out, size_t cap) {
    if (byteAt(pos) < 0) return false;
    size_t n = 0;
    int c;
    while ((c = byteAt(pos)) >= 0) {
        ++pos;
        if (c == '\n') break;
        if (c != '\r' && n + 1 < cap) out[n++] = (char)c;
    }
    out[n] = '\0';
    trimBrokenUtf8(out);
    return true;
}

void Book::seek(uint32_t pos) {
    cur_ = pos < bodyStart_ ? bodyStart_ : pos;
}

bool Book::next(Word& w) {
    int c;
    while ((c = byteAt(cur_)) >= 0 && isWordBreak(c)) ++cur_;
    if (c < 0) return false;

    w.offset = cur_;
    size_t n = 0;
    while ((c = byteAt(cur_)) >= 0 && !isWordBreak(c)) {
        if (n + 1 < sizeof(w.text)) w.text[n++] = (char)c;
        ++cur_;
    }
    w.text[n] = '\0';
    trimBrokenUtf8(w.text);

    w.paragraphEnd = true;
    for (uint32_t j = cur_; (c = byteAt(j)) >= 0; ++j) {
        if (c == '\n') break;
        if (!isWordBreak(c)) {
            w.paragraphEnd = false;
            break;
        }
    }
    return true;
}

uint32_t Book::wordBefore(uint32_t end, char* out) {
    uint32_t start = end;
    while (start > bodyStart_ && !isWordBreak(byteAt(start - 1))) --start;
    const uint32_t from = end - start > 63 ? end - 63 : start;
    int n = 0;
    for (uint32_t i = from; i < end; ++i) out[n++] = (char)byteAt(i);
    out[n] = '\0';
    return start;
}

uint32_t Book::sentenceStart(uint32_t pos) {
    if (pos < bodyStart_) pos = bodyStart_;
    uint32_t i = pos;
    while (i < size_ && isWordBreak(byteAt(i))) ++i;
    if (i >= size_) {
        i = size_;
        while (i > bodyStart_ && isWordBreak(byteAt(i - 1))) --i;
        if (i > bodyStart_) --i;
    }
    while (i > bodyStart_ && !isWordBreak(byteAt(i - 1))) --i;

    for (;;) {
        uint32_t j = i;
        bool newline = false;
        while (j > bodyStart_ && isWordBreak(byteAt(j - 1))) {
            if (byteAt(j - 1) == '\n') newline = true;
            --j;
        }
        if (j <= bodyStart_ || newline) return i;
        char prev[64];
        const uint32_t k = wordBefore(j, prev);
        if (wordEnding(prev) == Ending::Sentence) return i;
        i = k;
    }
}

uint32_t Book::previousSentence(uint32_t start) {
    uint32_t j = start;
    while (j > bodyStart_ && isWordBreak(byteAt(j - 1))) --j;
    if (j <= bodyStart_) return bodyStart_;
    return sentenceStart(j - 1);
}

uint32_t Book::nextSentence(uint32_t pos) {
    seek(pos);
    Word w;
    while (next(w)) {
        if (w.paragraphEnd || wordEnding(w.text) == Ending::Sentence) {
            Word following;
            return next(following) ? following.offset : 0;
        }
    }
    return 0;
}
