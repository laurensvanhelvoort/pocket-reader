#include "web.h"

#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "book.h"
#include "library.h"
#include "settings.h"
#include "text.h"
#include "webdata/web_index.h"
#include "webdata/web_pdf.h"
#include "webdata/web_pdf_worker.h"

const char* const kWifiName = "pocket-reader";

namespace {

const char* const kTempPath = "/books/upload.tmp";  // not .prb, so never listed

WebServer server(80);
bool routesAdded = false;
WebStatus status;
void (*progressHook)() = nullptr;
uint32_t lastHookAt = 0;
char address[16] = "192.168.4.1";

File uploadFile;
bool uploadStarted = false;
bool uploadOk = false;
char uploadName[40] = "";
Book probe;

bool validName(const char* name) {
    const size_t n = strlen(name);
    if (n < 5 || n > 32 || strcmp(name + n - 4, ".prb") != 0) return false;
    for (size_t i = 0; i < n - 4; ++i) {
        const char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return true;
}

void appendJson(String& out, const char* s) {
    out += '"';
    for (const char* p = s; *p; ++p) {
        const uint8_t c = *p;
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\u%04x", c);
            out += esc;
        } else {
            out += (char)c;
        }
    }
    out += '"';
}

void sendJson(int code, const String& json) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(code, "application/json", json);
}

void sendError(int code, const char* message) {
    String json = "{\"ok\":false,\"error\":";
    appendJson(json, message);
    json += "}";
    sendJson(code, json);
}

void sendAsset(const uint8_t* data, size_t len, const char* type, bool cache) {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", cache ? "max-age=86400" : "no-cache");
    server.send_P(200, type, (const char*)data, len);
}

void handleBooks() {
    scanLibrary();
    String json = "{\"books\":[";
    for (size_t i = 0; i < library.size(); ++i) {
        const BookInfo& b = library[i];
        if (i > 0) json += ',';
        json += "{\"file\":";
        appendJson(json, b.path + 7);
        json += ",\"title\":";
        appendJson(json, b.title);
        json += ",\"author\":";
        appendJson(json, b.author);
        json += ",\"percent\":";
        json += b.percent;
        json += ",\"size\":";
        json += b.size;
        json += '}';
    }
    json += "],\"free\":";
    json += webFreeBytes();
    json += ",\"total\":";
    json += (uint32_t)LittleFS.totalBytes();
    json += '}';
    sendJson(200, json);
}

void failUpload(const char* message) {
    if (!uploadOk) return;
    uploadOk = false;
    copyUtf8(status.lastError, message, sizeof(status.lastError));
    if (uploadFile) uploadFile.close();
    LittleFS.remove(kTempPath);
}

void finishUpload() {
    char magic[5] = "";
    File f = LittleFS.open(kTempPath, "r");
    if (f) {
        f.read((uint8_t*)magic, 4);
        f.close();
    }
    if (strcmp(magic, "PRB1") != 0) {
        failUpload("geen pocket-reader-boek");
        return;
    }
    char path[48];
    snprintf(path, sizeof(path), "/books/%s", uploadName);
    if (LittleFS.exists(path)) LittleFS.remove(path);
    if (!LittleFS.rename(kTempPath, path)) {
        failUpload("opslaan mislukt");
        return;
    }
    touchBook(path);
    if (probe.open(path)) {
        copyUtf8(status.lastTitle, probe.title(), sizeof(status.lastTitle));
        probe.close();
    }
}

void handleUploadData() {
    HTTPUpload& up = server.upload();
    if (up.status == UPLOAD_FILE_START) {
        uploadStarted = true;
        uploadOk = true;
        status.lastError[0] = '\0';
        status.receiving = true;
        status.received = 0;
        ++status.changes;
        copyUtf8(uploadName, up.filename.c_str(), sizeof(uploadName));
        if (!validName(uploadName)) {
            failUpload("ongeldige bestandsnaam");
            return;
        }
        LittleFS.mkdir("/books");
        uploadFile = LittleFS.open(kTempPath, "w");
        if (!uploadFile) failUpload("kan niet schrijven");
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!uploadOk) return;
        if (uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
            failUpload("niet genoeg ruimte");
            return;
        }
        status.received += up.currentSize;
        if (progressHook != nullptr && millis() - lastHookAt > 250) {
            lastHookAt = millis();
            progressHook();
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (!uploadOk) return;
        uploadFile.close();
        finishUpload();
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        failUpload("upload afgebroken");
    }
}

void handleUploadDone() {
    status.receiving = false;
    ++status.changes;
    if (!uploadStarted) copyUtf8(status.lastError, "geen bestand ontvangen", sizeof(status.lastError));
    if (!uploadStarted || !uploadOk) {
        sendError(400, status.lastError);
    } else {
        String json = "{\"ok\":true,\"title\":";
        appendJson(json, status.lastTitle);
        json += "}";
        sendJson(200, json);
    }
    uploadStarted = false;
    uploadOk = false;
}

void handleDelete() {
    char name[40];
    copyUtf8(name, server.arg("file").c_str(), sizeof(name));
    if (!validName(name)) {
        sendError(400, "ongeldige bestandsnaam");
        return;
    }
    char path[48];
    snprintf(path, sizeof(path), "/books/%s", name);
    if (!LittleFS.exists(path) || !LittleFS.remove(path)) {
        sendError(404, "boek niet gevonden");
        return;
    }
    forgetBook(path);
    ++status.changes;
    sendJson(200, "{\"ok\":true}");
}

void addRoutes() {
    if (routesAdded) return;
    routesAdded = true;
    server.on("/", HTTP_GET, [] { sendAsset(web_index, sizeof(web_index), "text/html; charset=utf-8", false); });
    server.on("/pdf.min.mjs", HTTP_GET, [] { sendAsset(web_pdf, sizeof(web_pdf), "text/javascript", true); });
    server.on("/pdf.worker.min.mjs", HTTP_GET,
              [] { sendAsset(web_pdf_worker, sizeof(web_pdf_worker), "text/javascript", true); });
    server.on("/api/books", HTTP_GET, handleBooks);
    server.on("/api/upload", HTTP_POST, handleUploadDone, handleUploadData);
    server.on("/api/delete", HTTP_POST, handleDelete);
    server.onNotFound([] {
        server.sendHeader("Location", "/");
        server.send(302, "text/plain", "");
    });
}

}

void webBegin() {
    status = WebStatus();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kWifiName);
    copyUtf8(address, WiFi.softAPIP().toString().c_str(), sizeof(address));
    addRoutes();
    server.begin();
    Serial.printf("web: wifi \"%s\", http://%s, vrij geheugen %u bytes\n", kWifiName, address, ESP.getFreeHeap());
}

void webEnd() {
    server.stop();
    if (uploadFile) uploadFile.close();
    LittleFS.remove(kTempPath);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}

void webLoop() {
    server.handleClient();
}

const char* webAddress() {
    return address;
}

int webStations() {
    return WiFi.softAPgetStationNum();
}

uint32_t webFreeBytes() {
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

const WebStatus& webStatus() {
    return status;
}

void setWebProgressHook(void (*hook)()) {
    progressHook = hook;
}
