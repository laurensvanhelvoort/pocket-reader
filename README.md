# pocket-reader

RSVP reader for the LILYGO T-Display-S3. Books are shown one word at a time, with the focus letter always in the same place. The interface is in Dutch.

## Hardware

- LILYGO T-Display-S3 (16 MB flash)
- KY-040 rotary encoder: CLK to GPIO1, DT to GPIO2, SW to GPIO10

The two buttons on the board are used too.

## Build

Open the folder in VS Code with PlatformIO and press Upload, or run:

```
pio run -t upload
```

## Adding books

Pick the last item in the library ("boeken toevoegen"). The reader starts an open Wi-Fi network called `pocket-reader`. Connect to it, open http://192.168.4.1 and drop EPUB, PDF or TXT files on the page. They are converted in the browser and sent to the device. Long press to switch Wi-Fi off again.

## Controls

|                | Reading           | Lists  |
|----------------|-------------------|--------|
| Turn           | speed             | scroll |
| Press          | pause / resume    | select |
| Long press     | menu              | back   |
| GPIO0 button   | back a sentence   | back   |
| GPIO14 button  | next sentence     |        |

## License

MIT, see `LICENSE`. The firmware includes Literata (OFL), GNU FreeFont (GPL-3.0 with font exception) and pdf.js (Apache-2.0). Their licenses are in `licenses/`.
