# pocket-reader

A small RSVP reader for the LILYGO T-Display-S3. It shows a book one word at a time, with the focus letter always in the same spot, so your eyes can stay still while you read (ORP indexing). The interface is in Dutch.

This project started after I came across [RSVP Nano](https://github.com/ionutdecebal/rsvpnano) by ionutdecebal. 

## What it does

Each word is placed so that its focus letter (shown in gold) sits between two small markers. Short words get the focus on the first or second letter, longer words a bit further in. Long words stay on screen a little longer, and the reader pauses at commas, at the end of a sentence and at the end of a paragraph. Words that are too wide for the screen are split, first at an existing hyphen and otherwise between syllables.

Besides that it:

- remembers your speed, and for every book the start of the sentence you were reading
- reads chapters from the book's table of contents and shows a short title card when a new chapter starts
- keeps a library sorted by what you read last, with finished books greyed out at the bottom
- has an info screen with author, year, word count and reading time at your current speed
- lets you add books over Wi-Fi: the device runs its own network and a small web page converts EPUB, PDF and TXT in your browser

## Hardware

- LILYGO T-Display-S3 (ESP32-S3, 16 MB flash, 8 MB PSRAM)
- KY-040 rotary encoder with push button

| KY-040 | T-Display-S3 |
|--------|--------------|
| CLK    | GPIO1        |
| DT     | GPIO2        |
| SW     | GPIO10       |
| +      | 3V3          |
| GND    | GND          |

The two buttons on the board are used as well: GPIO0 (bottom left) and GPIO14 (top left).

## Building

The project uses PlatformIO. Open the folder in VS Code with the PlatformIO extension and press Upload, or run:

```
pio run -t upload
```

There is one environment, `firmware`. The partition table in `partitions.csv` gives the app 3 MB and leaves about 12.9 MB for books. You don't need to upload a filesystem image. On a fresh board the book storage is formatted on the first boot.

## Adding books

1. In the library, scroll down to "Boeken toevoegen" and press.
2. Connect your phone or laptop to the open Wi-Fi network `pocket-reader`.
3. Open http://192.168.4.1 and drop in EPUB, PDF or TXT files.
4. Long press when you're done. Wi-Fi switches off again and the new books are at the top of the library.

The conversion happens in the browser, so the device only ever receives a plain text file. The same page lists the books on the device, shows the free space and lets you delete books. Uploading a book with the same title replaces it and keeps your place.

Chapters come from the table of contents for EPUB, from the bookmarks for PDF and from lines like "Chapter 3" for TXT. How clean a PDF comes out depends a lot on the PDF.

## Controls

|                | Reading           | Lists  |
|----------------|-------------------|--------|
| Turn           | speed             | scroll |
| Press          | pause / resume    | select |
| Long press     | menu              | back   |
| GPIO0 button   | back a sentence   | back   |
| GPIO14 button  | next sentence     |        |

Pressing back once goes to the start of the current sentence, twice goes to the sentence before. The menu has continue, chapters, info, settings, back to the start and library. Any button skips the startup animation.

## Timing

At 300 wpm a plain word is on screen for 200 ms. Words longer than six letters get 8% extra per extra letter. A comma, semicolon or dash adds half a word, the end of a sentence adds a full word and the end of a paragraph one and a half. Abbreviations like "Mr." and initials don't count as the end of a sentence. With ramp-up on, the first eight words after you start go from 1.6 times the normal duration back to normal.

The speed goes from 100 to 900 wpm in steps of 25.

## Book format

Books are stored as `.prb` files in `/books` on the device. It's plain UTF-8: a short header, then one paragraph per line.

```
PRB1
title: Burning Sands
author: Arthur E. P. Brome Weigall
language: en
chapter: 2348 Chapter I — A Study in Behaviour
---
```

The number after `chapter:` is the byte offset from the start of the text, right after `---`. A `year:` line is added when an EPUB has a real publication date.

## Code

`src/app/`:

- `main.cpp` starts up and switches between screens
- `screen_*.cpp` has one file per screen: intro, library, reader, menu and chapters, info, settings and upload
- `book.*` reads a `.prb` file word by word and finds sentence starts
- `text.*` handles the focus letter, the timing and splitting long words
- `library.*` and `settings.*` hold the book list and everything stored in NVS
- `web.*` runs the Wi-Fi access point, the web server and the uploads
- `ui.*`, `fonts.*` and `controls.*` do the drawing, the fonts and the input
- `fontdata/` and `webdata/` contain generated data: the fonts and the gzipped upload page

## Credits

- [RSVP Nano](https://github.com/ionutdecebal/rsvpnano) for the concept
- [Literata](https://github.com/googlefonts/literata) reading font
- GNU FreeFont for the small monospace text
- [pdf.js](https://github.com/mozilla/pdf.js) PDF handling in browser
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) display
