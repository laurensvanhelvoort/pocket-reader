#include "text.h"

#include <ctype.h>
#include <string.h>

namespace {

const float kExtraPerLetter = 0.08f;

const char* const kAbbreviations[] = {
    "mr", "mrs", "ms", "dr", "st", "mt", "jr", "sr", "prof", "vs",
    "bijv", "dhr", "mevr", "blz", "nr", "ca", "jl", "zgn", "resp", "ir", "ing", "drs", "mw",
};

bool isLetterCp(uint32_t cp) {
    if (cp < 0x80) return isalnum((int)cp);
    if (cp < 0xC0) return false;
    if (cp == 0xD7 || cp == 0xF7) return false;
    if (cp >= 0x2000 && cp <= 0x206F) return false;
    if (cp >= 0x20A0 && cp <= 0x214F) return false;
    return true;
}

bool isCloser(uint32_t cp) {
    return cp == '"' || cp == '\'' || cp == ')' || cp == ']' || cp == '}' || cp == '*' || cp == 0x2019 ||
           cp == 0x201D || cp == 0xBB || cp == 0x203A;
}

int orpIndex(int letters) {
    if (letters <= 1) return 0;
    if (letters <= 5) return 1;
    if (letters <= 9) return 2;
    if (letters <= 13) return 3;
    return 4;
}

int charStarts(const char* w, int* starts, int cap) {
    int n = 0;
    for (int i = 0; w[i] != '\0' && n < cap - 1;) {
        starts[n++] = i;
        decodeUtf8(w, i);
    }
    starts[n] = strlen(w);
    return n;
}

bool isAbbreviation(const char* w, int end) {
    int start = end;
    while (start > 0 && isalpha((unsigned char)w[start - 1])) --start;
    const int n = end - start;
    if (n == 1 && isupper((unsigned char)w[start])) return true;
    if (n == 0 || n > 5) return false;
    char lower[8];
    for (int i = 0; i < n; ++i) lower[i] = tolower((unsigned char)w[start + i]);
    lower[n] = '\0';
    for (const char* a : kAbbreviations) {
        if (strcmp(lower, a) == 0) return true;
    }
    return false;
}

bool isVowelCp(uint32_t cp) {
    if (cp < 0x80) return cp != 0 && strchr("aeiouyAEIOUY", (int)cp) != nullptr;
    return isLetterCp(cp) && cp != 0xE7 && cp != 0xC7 && cp != 0xF1 && cp != 0xD1 && cp != 0xDF;
}

// 2 = between syllables, 1 = acceptable, 0 = do not cut here.
int cutScore(const uint32_t* cps, int n, int k) {
    if (k < 2 || k > n - 2) return 0;
    const uint32_t a = cps[k - 1], b = cps[k], c = cps[k + 1];
    if (!isLetterCp(a) || !isLetterCp(b)) return 0;
    if ((a == 'c' || a == 'C') && (b == 'h' || b == 'H')) return 0;
    const bool va = isVowelCp(a), vb = isVowelCp(b), vc = isLetterCp(c) && isVowelCp(c);
    if (va && !vb && vc) return 2;
    if (!va && !vb) return vc ? 2 : 1;
    return 0;
}

void copyRange(char* out, size_t cap, const char* w, int a, int b) {
    int n = b - a;
    if (n > (int)cap - 1) n = cap - 1;
    memcpy(out, w + a, n);
    out[n] = '\0';
}

}

uint32_t decodeUtf8(const char* s, int& i) {
    const uint8_t c = s[i++];
    if (c < 0x80) return c;
    const int n = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : 0;
    if (n == 0) return 0xFFFD;
    uint32_t cp = c & (0x3F >> n);
    for (int k = 0; k < n; ++k) {
        const uint8_t d = s[i];
        if ((d & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (d & 0x3F);
        ++i;
    }
    return cp;
}

void truncateUtf8(char* s, int maxBytes) {
    if ((int)strlen(s) <= maxBytes) return;
    int cut = maxBytes;
    while (cut > 0 && ((uint8_t)s[cut] & 0xC0) == 0x80) --cut;
    s[cut] = '\0';
}

void trimBrokenUtf8(char* s) {
    const int len = strlen(s);
    int i = len - 1;
    while (i > 0 && ((uint8_t)s[i] & 0xC0) == 0x80) --i;
    if (i < 0) return;
    const uint8_t lead = s[i];
    const int need = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
    if (len - i < need) s[i] = '\0';
}

void copyUtf8(char* dst, const char* src, size_t cap) {
    strlcpy(dst, src, cap);
    if (strlen(src) >= cap) trimBrokenUtf8(dst);
}

void splitWord(const char* w, Parts& p) {
    int starts[72];
    uint32_t cps[72];
    int n = 0;
    for (int i = 0; w[i] != '\0' && n < 71;) {
        starts[n] = i;
        cps[n++] = decodeUtf8(w, i);
    }
    starts[n] = strlen(w);

    int first = 0;
    while (first < n && !isLetterCp(cps[first])) ++first;
    int last = n - 1;
    while (last >= first && !isLetterCp(cps[last])) --last;

    int orp = 0;
    p.letters = 0;
    if (first < n) {
        p.letters = last - first + 1;
        orp = first + orpIndex(p.letters);
        while (orp < last && !isLetterCp(cps[orp])) ++orp;
    }
    if (n == 0) {
        p.before[0] = p.orp[0] = p.after[0] = '\0';
        return;
    }
    copyRange(p.before, sizeof(p.before), w, 0, starts[orp]);
    copyRange(p.orp, sizeof(p.orp), w, starts[orp], starts[orp + 1]);
    copyRange(p.after, sizeof(p.after), w, starts[orp + 1], starts[n]);
}

Ending wordEnding(const char* w) {
    int end = strlen(w);
    while (end > 0) {
        int start = end - 1;
        while (start > 0 && ((uint8_t)w[start] & 0xC0) == 0x80) --start;
        int i = start;
        const uint32_t cp = decodeUtf8(w, i);
        if (isCloser(cp)) {
            end = start;
            continue;
        }
        if (cp == '.' && isAbbreviation(w, start)) return Ending::None;
        if (cp == '.' || cp == '!' || cp == '?' || cp == 0x2026) return Ending::Sentence;
        if (cp == ',' || cp == ';' || cp == ':' || cp == 0x2013 || cp == 0x2014) return Ending::Clause;
        return Ending::None;
    }
    return Ending::None;
}

float durationFactor(int letters, Ending ending, bool paragraphEnd) {
    float factor = 1.0f;
    if (letters > 6) factor += kExtraPerLetter * (letters - 6);
    if (paragraphEnd) {
        factor += 1.5f;
    } else if (ending == Ending::Sentence) {
        factor += 1.0f;
    } else if (ending == Ending::Clause) {
        factor += 0.5f;
    }
    return factor;
}

uint32_t durationMs(int letters, Ending ending, bool paragraphEnd, int wpm) {
    return (uint32_t)(60000.0f / wpm * durationFactor(letters, ending, paragraphEnd));
}

bool splitChapterTitle(const char* title, char* prefix, size_t cap, const char** rest) {
    const char* const dash = " \xE2\x80\x94 ";
    const char* at = strstr(title, dash);
    if (at == nullptr || at[strlen(dash)] == '\0') return false;
    const size_t n = at - title;
    copyUtf8(prefix, title, n + 1 < cap ? n + 1 : cap);
    *rest = at + strlen(dash);
    return true;
}

void appendLetterKey(const char* s, char* out, size_t cap) {
    size_t len = strlen(out);
    for (int i = 0; s[i] != '\0';) {
        const int start = i;
        uint32_t cp = decodeUtf8(s, i);
        if (!isLetterCp(cp)) continue;
        if (cp < 0x80) {
            if (len + 1 >= cap) break;
            out[len++] = (char)tolower((int)cp);
        } else if (cp < 0x800) {
            if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) cp += 0x20;
            if (len + 2 >= cap) break;
            out[len++] = (char)(0xC0 | (cp >> 6));
            out[len++] = (char)(0x80 | (cp & 0x3F));
        } else {
            if (len + (i - start) >= cap) break;
            memcpy(out + len, s + start, i - start);
            len += i - start;
        }
    }
    out[len] = '\0';
}

void formatMinutes(uint32_t minutes, char* out, size_t cap) {
    if (minutes < 1) {
        snprintf(out, cap, "<1 min");
    } else if (minutes < 60) {
        snprintf(out, cap, "%lu min", (unsigned long)minutes);
    } else if (minutes % 60 == 0) {
        snprintf(out, cap, "%lu u", (unsigned long)(minutes / 60));
    } else {
        snprintf(out, cap, "%lu u %lu min", (unsigned long)(minutes / 60), (unsigned long)(minutes % 60));
    }
}

void formatCount(uint32_t n, char* out, size_t cap) {
    if (n >= 1000000) {
        snprintf(out, cap, "%lu.%03lu.%03lu", (unsigned long)(n / 1000000), (unsigned long)(n / 1000 % 1000),
                 (unsigned long)(n % 1000));
    } else if (n >= 1000) {
        snprintf(out, cap, "%lu.%03lu", (unsigned long)(n / 1000), (unsigned long)(n % 1000));
    } else {
        snprintf(out, cap, "%lu", (unsigned long)n);
    }
}

int chunkWord(const char* w, Chunk* out, bool (*fits)(const char*)) {
    if (fits(w)) {
        copyRange(out[0].text, sizeof(out[0].text), w, 0, strlen(w));
        return 1;
    }

    int starts[72];
    const int n = charStarts(w, starts, 72);

    int cuts[kMaxChunks + 1];
    int parts = 0;
    cuts[parts++] = 0;
    for (int k = 1; k < n - 1 && parts < kMaxChunks; ++k) {
        if (w[starts[k] - 1] == '-' && isalpha((unsigned char)w[starts[k]])) cuts[parts++] = starts[k];
    }
    if (parts > 1) {
        cuts[parts] = starts[n];
        bool ok = true;
        for (int k = 0; k < parts; ++k) {
            copyRange(out[k].text, sizeof(out[k].text), w, cuts[k], cuts[k + 1]);
            ok = ok && fits(out[k].text);
        }
        if (ok) return parts;
    }

    uint32_t cps[72];
    for (int k = 0; k < n; ++k) {
        int i = starts[k];
        cps[k] = decodeUtf8(w, i);
    }
    for (parts = 2; parts <= kMaxChunks; ++parts) {
        int cut[kMaxChunks + 1];
        cut[0] = 0;
        for (int j = 1; j < parts; ++j) {
            const int ideal = n * j / parts;
            int best = ideal, bestScore = -1;
            for (int k = ideal - 3; k <= ideal + 3; ++k) {
                if (k < cut[j - 1] + 3 || k > n - 3) continue;
                const int score = cutScore(cps, n, k);
                if (score > bestScore || (score == bestScore && abs(k - ideal) < abs(best - ideal))) {
                    best = k;
                    bestScore = score;
                }
            }
            cut[j] = best;
        }
        cut[parts] = n;

        bool ok = true;
        for (int k = 0; k < parts; ++k) {
            const int a = starts[cut[k]];
            const int b = starts[cut[k + 1]];
            copyRange(out[k].text, sizeof(out[k].text) - 1, w, a, b);
            if (k < parts - 1 && w[b - 1] != '-') strcat(out[k].text, "-");
            ok = ok && fits(out[k].text);
        }
        if (ok) break;
    }
    return parts > kMaxChunks ? kMaxChunks : parts;
}
