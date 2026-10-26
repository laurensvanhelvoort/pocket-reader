#pragma once

#include <Arduino.h>
#include <LittleFS.h>

#include <vector>

// Positions are byte offsets into the .prb file.

struct Chapter {
    uint32_t offset;
    char title[84];
};

struct Word {
    char text[64];
    uint32_t offset;
    bool paragraphEnd;
};

class Book {
public:
    bool open(const char* path);
    void close();
    bool isOpen() const { return open_; }

    const char* path() const { return path_; }
    const char* title() const { return title_; }
    const char* author() const { return author_; }
    uint32_t bodyStart() const { return bodyStart_; }
    uint32_t size() const { return size_; }

    int chapterCount() const { return chapters_.size(); }
    const Chapter& chapter(int i) const { return chapters_[i]; }
    int chapterAt(uint32_t pos) const;  // -1 before the first chapter
    int percent(uint32_t pos) const;

    void seek(uint32_t pos);
    bool next(Word& w);

    uint32_t sentenceStart(uint32_t pos);
    uint32_t previousSentence(uint32_t start);
    uint32_t nextSentence(uint32_t pos);  // 0 if there is none

private:
    int byteAt(uint32_t i);
    bool readLine(uint32_t& pos, char* out, size_t cap);
    uint32_t wordBefore(uint32_t end, char* out);

    File file_;
    bool open_ = false;
    char path_[48] = "";
    char title_[84] = "";
    char author_[84] = "";
    uint32_t bodyStart_ = 0;
    uint32_t size_ = 0;
    std::vector<Chapter> chapters_;

    uint8_t buf_[2048];
    uint32_t bufStart_ = 0;
    int bufLen_ = 0;
    uint32_t cur_ = 0;
};
