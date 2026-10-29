#include "web_server.h"
#include "wiki_db.h"
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

static WebServer server(80);

// Wi-Fi Access Point configuration
static const char *AP_SSID = "Wikipedia-Offline";
static const char *AP_PASS = ""; // open hotspot

// Buffer for loading images
static uint8_t *g_web_img_buf = nullptr;
static const size_t WEB_IMG_BUF_SIZE = 128 * 1024;

// HTML template parts for responsive, fast web interface
static const char HTML_HEADER[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Offline Wikipedia</title>
<style>
:root { --bg: #f8f9fa; --card: #ffffff; --text: #202122; --sub: #54595d; --link: #3366cc; --border: #a2a9b1; }
@media (prefers-color-scheme: dark) {
  :root { --bg: #101418; --card: #1f2428; --text: #eaecf0; --sub: #9aa0a6; --link: #69a3ff; --border: #3c4043; }
}
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 0; line-height: 1.6; }
header { background: var(--card); border-bottom: 1px solid var(--border); padding: 12px 16px; position: sticky; top: 0; z-index: 100; box-shadow: 0 1px 3px rgba(0,0,0,0.05); }
.header-wrap { max-width: 800px; margin: 0 auto; display: flex; gap: 10px; align-items: center; }
.logo { font-size: 1.2rem; font-weight: bold; text-decoration: none; color: var(--text); display: flex; align-items: center; gap: 6px; }
.search-box { flex: 1; position: relative; }
input[type="search"] { width: 100%; box-sizing: border-box; padding: 10px 14px; font-size: 16px; border: 1px solid var(--border); border-radius: 20px; background: var(--bg); color: var(--text); outline: none; }
input[type="search"]:focus { border-color: var(--link); }
#suggestions { position: absolute; top: 46px; left: 0; right: 0; background: var(--card); border: 1px solid var(--border); border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.15); max-height: 360px; overflow-y: auto; display: none; z-index: 1000; }
.sug-item { padding: 12px 16px; border-bottom: 1px solid var(--border); cursor: pointer; display: block; text-decoration: none; color: var(--text); }
.sug-item:hover { background: rgba(51,102,204,0.1); }
main { max-width: 800px; margin: 20px auto; padding: 0 16px; }
.article-card { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 24px; box-shadow: 0 1px 4px rgba(0,0,0,0.04); }
h1 { margin-top: 0; border-bottom: 1px solid var(--border); padding-bottom: 10px; font-size: 2rem; }
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }
img { max-width: 100%; height: auto; border-radius: 6px; margin: 12px 0; background: #eee; }
.nav-pages { display: flex; justify-content: space-between; margin-top: 24px; padding-top: 16px; border-top: 1px solid var(--border); }
.btn { display: inline-block; padding: 8px 16px; background: var(--link); color: white !important; border-radius: 6px; font-weight: bold; }
.footer { text-align: center; font-size: 0.85rem; color: var(--sub); margin: 30px 0; }
</style>
<script>
let debounceTimer;
function onSearchInput(val) {
  clearTimeout(debounceTimer);
  val = val.trim();
  const box = document.getElementById("suggestions");
  if (val.length < 2) { box.style.display = "none"; return; }
  debounceTimer = setTimeout(() => {
    fetch('/api/search?q=' + encodeURIComponent(val))
      .then(res => res.json())
      .then(data => {
        if (!data || data.length === 0) {
          box.innerHTML = '<div class="sug-item">No results found</div>';
          box.style.display = "block";
          return;
        }
        let html = '';
        data.forEach(item => {
          html += '<a class="sug-item" href="/wiki/' + item.id + '">' + item.title + '</a>';
        });
        box.innerHTML = html;
        box.style.display = "block";
      }).catch(() => { box.style.display = "none"; });
  }, 250);
}
document.addEventListener('click', (e) => {
  if (!e.target.closest('.search-box')) {
    const box = document.getElementById("suggestions");
    if (box) box.style.display = "none";
  }
});
</script>
</head>
<body>
<header>
  <div class="header-wrap">
    <a href="/" class="logo">📖 Wikipedia</a>
    <div class="search-box">
      <input type="search" placeholder="Search Wikipedia..." autocomplete="off" oninput="onSearchInput(this.value)">
      <div id="suggestions"></div>
    </div>
  </div>
</header>
<main>
)rawliteral";

static const char HTML_FOOTER[] PROGMEM = R"rawliteral(
</main>
<div class="footer">
  ESP32-S3 Offline Wikipedia &bull; No internet connection required
</div>
</body>
</html>
)rawliteral";

static void handleRoot() {
    String html = FPSTR(HTML_HEADER);
    html += "<div class=\"article-card\">";
    html += "<h1>Welcome to Offline Wikipedia</h1>";
    html += "<p>This database is served from the microSD card on your <b>ESP32-S3</b> without any internet connection.</p>";
    html += "<div style=\"background: var(--bg); padding: 15px; border-radius: 6px; margin: 15px 0;\">";
    html += "<p><b>Database:</b> " + wikiDbName() + "</p>";
    html += "<p><b>Total Articles:</b> " + String(wikiDbArticleCount()) + "</p>";
    html += "<p><b>Images Available:</b> " + String(wikiDbImagesAvailable() ? "Yes (Photos & Diagrams)" : "No") + "</p>";
    html += "</div>";
    html += "<p>Use the search bar above to look up any topic.</p>";
    html += "</div>";
    html += FPSTR(HTML_FOOTER);

    server.send(200, "text/html; charset=utf-8", html);
}

static void handleSearchApi() {
    if (!server.hasArg("q")) {
        server.send(400, "application/json", "[]");
        return;
    }
    String query = server.arg("q");
    query.trim();
    if (query.length() == 0) {
        server.send(200, "application/json", "[]");
        return;
    }

    auto prefix_results = wikiDbSearch(query, 8);
    auto contains_results = wikiDbSearchContains(query, 6);

    String json = "[";
    size_t count = 0;

    auto addResult = [&](const SearchResult &sr) {
        if (count > 0) json += ",";
        json += "{\"id\":" + String(sr.article_id) + ",\"title\":\"";
        // Escape quotes
        String safeTitle = sr.title;
        safeTitle.replace("\"", "\\\"");
        json += safeTitle + "\"}";
        count++;
    };

    for (const auto &sr : prefix_results) addResult(sr);
    for (const auto &sr : contains_results) addResult(sr);

    json += "]";
    server.send(200, "application/json; charset=utf-8", json);
}

static void handleWikiArticle() {
    String uri = server.uri(); // e.g. /wiki/1234
    int idStart = uri.lastIndexOf('/');
    if (idStart < 0) {
        server.send(404, "text/plain", "Not Found");
        return;
    }

    uint32_t article_id = (uint32_t)uri.substring(idStart + 1).toInt();
    uint16_t block_num = 0;
    if (server.hasArg("page")) {
        block_num = (uint16_t)server.arg("page").toInt();
    }

    ArticleResult ar = wikiDbLoadArticle(article_id, block_num);
    if (!ar.found) {
        server.send(404, "text/html; charset=utf-8", "<h1>Article not found</h1>");
        return;
    }

    String html = FPSTR(HTML_HEADER);
    html += "<div class=\"article-card\">";
    html += "<h1>" + ar.title + "</h1>";

    // Article content with internal links and images
    html += ar.html;

    // Pagination buttons if multi-block
    if (ar.num_blocks > 1) {
        html += "<div class=\"nav-pages\">";
        if (block_num > 0) {
            html += "<a class=\"btn\" href=\"/wiki/" + String(article_id) + "?page=" + String(block_num - 1) + "\">&larr; Previous Page</a>";
        } else {
            html += "<span></span>";
        }
        html += "<span>Page " + String(block_num + 1) + " of " + String(ar.num_blocks) + "</span>";
        if (block_num + 1 < ar.num_blocks) {
            html += "<a class=\"btn\" href=\"/wiki/" + String(article_id) + "?page=" + String(block_num + 1) + "\">Next Page &rarr;</a>";
        } else {
            html += "<span></span>";
        }
        html += "</div>";
    }

    html += "</div>";
    html += FPSTR(HTML_FOOTER);

    server.send(200, "text/html; charset=utf-8", html);
}

static void handleImage() {
    String uri = server.uri(); // e.g. /img/42
    int idStart = uri.lastIndexOf('/');
    if (idStart < 0) {
        server.send(404, "text/plain", "Not Found");
        return;
    }

    uint32_t img_id = (uint32_t)uri.substring(idStart + 1).toInt();
    if (!g_web_img_buf) {
        server.send(500, "text/plain", "Image buffer not allocated");
        return;
    }

    size_t loaded_len = wikiDbLoadImage(img_id, g_web_img_buf, WEB_IMG_BUF_SIZE);
    if (loaded_len == 0) {
        server.send(404, "text/plain", "Image not found");
        return;
    }

    // Detect format
    const char *contentType = "image/jpeg";
    if (loaded_len >= 4) {
        if (g_web_img_buf[0] == 0x89 && g_web_img_buf[1] == 'P' && g_web_img_buf[2] == 'N' && g_web_img_buf[3] == 'G') {
            contentType = "image/png";
        } else if (g_web_img_buf[0] == 'q' && g_web_img_buf[1] == 'o' && g_web_img_buf[2] == 'i' && g_web_img_buf[3] == 'f') {
            contentType = "image/qoi";
        }
    }

    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.send_P(200, contentType, (const char *)g_web_img_buf, loaded_len);
}

void webServerInit() {
    Serial.println("\n[web_server] Initialising Wi-Fi and Web Server...");

    // Allocate image buffer in PSRAM if present, otherwise regular heap
#ifdef BOARD_HAS_PSRAM
    if (psramFound()) {
        g_web_img_buf = (uint8_t *)ps_malloc(WEB_IMG_BUF_SIZE);
        Serial.printf("[web_server] Image buffer (%u KB) allocated in PSRAM\n", (unsigned)(WEB_IMG_BUF_SIZE / 1024));
    }
#endif
    if (!g_web_img_buf) {
        g_web_img_buf = (uint8_t *)malloc(WEB_IMG_BUF_SIZE);
        Serial.printf("[web_server] Image buffer (%u KB) allocated in internal RAM\n", (unsigned)(WEB_IMG_BUF_SIZE / 1024));
    }

    // Set up Wi-Fi Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    IPAddress ip = WiFi.softAPIP();
    Serial.println("========================================================");
    Serial.println("   ESP32-S3 Wikipedia Offline Hotspot is READY!");
    Serial.printf("   Connect to Wi-Fi SSID : %s\n", AP_SSID);
    Serial.printf("   Open browser at       : http://%s/\n", ip.toString().c_str());
    if (MDNS.begin("wiki")) {
        Serial.println("   Or via mDNS URL       : http://wiki.local/");
    }
    Serial.println("========================================================");

    // Register HTTP routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/search", HTTP_GET, handleSearchApi);
    server.onNotFound([]() {
        if (server.uri().startsWith("/wiki/")) {
            handleWikiArticle();
        } else if (server.uri().startsWith("/img/")) {
            handleImage();
        } else {
            server.send(404, "text/plain", "Page Not Found");
        }
    });

    server.begin();
    Serial.println("[web_server] HTTP server started on port 80");
}

void webServerLoop() {
    server.handleClient();
}
