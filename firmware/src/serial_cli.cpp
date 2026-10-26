#include "serial_cli.h"
#include "wiki_db.h"
#include "config.h"
#include <vector>

static uint32_t g_current_article_id = 0;
static uint16_t g_current_block_num  = 0;
static uint16_t g_total_blocks       = 0;
static String   g_current_title;
static std::vector<SearchResult> g_last_search;

static char g_input_line[256];
static size_t g_input_pos = 0;

static void printPrompt() {
    Serial.print("\nwiki> ");
}

static void printHelp() {
    Serial.println("\n--- Commands ---");
    Serial.println("  s <title>       : Prefix search (e.g. 's Nelson')");
    Serial.println("  c <query>       : Substring/contains search (e.g. 'c Afrika')");
    Serial.println("  <number>        : Open search result (e.g. '1' to open 1st result)");
    Serial.println("  r <id>          : Read article by numeric ID (e.g. 'r 42')");
    Serial.println("  n               : Next page of current article");
    Serial.println("  p               : Previous page of current article");
    Serial.println("  info            : Show database and memory statistics");
    Serial.println("  help, ?         : Show this help message");
}

static void printHtmlAsText(const char *html, uint32_t len) {
    if (!html || len == 0) return;

    bool in_tag = false;
    char tag_buf[32];
    size_t tag_idx = 0;
    int blank_lines = 0;

    for (uint32_t i = 0; i < len; ++i) {
        char c = html[i];

        if (c == '<') {
            in_tag = true;
            tag_idx = 0;
            continue;
        }

        if (in_tag) {
            if (c == '>') {
                in_tag = false;
                tag_buf[tag_idx] = '\0';
                
                // Inspect tag
                String tag = String(tag_buf);
                tag.trim();
                tag.toLowerCase();

                if (tag.startsWith("br") || tag.startsWith("p") || tag.startsWith("/p") ||
                    tag.startsWith("h1") || tag.startsWith("/h1") ||
                    tag.startsWith("h2") || tag.startsWith("/h2") ||
                    tag.startsWith("h3") || tag.startsWith("/h3") ||
                    tag.startsWith("tr") || tag.startsWith("/div")) {
                    if (blank_lines < 2) {
                        Serial.println();
                        blank_lines++;
                    }
                    if (tag.startsWith("h1") || tag.startsWith("h2")) {
                        Serial.println("----------------------------------------");
                    }
                } else if (tag.startsWith("li")) {
                    Serial.print("\n * ");
                    blank_lines = 0;
                } else if (tag.startsWith("th") || tag.startsWith("td")) {
                    Serial.print("\t| ");
                }
            } else if (tag_idx + 1 < sizeof(tag_buf)) {
                tag_buf[tag_idx++] = c;
            }
            continue;
        }

        // Entity decoding
        if (c == '&') {
            String entity = "";
            uint32_t j = i + 1;
            while (j < len && j < i + 10 && html[j] != ';') {
                entity += html[j++];
            }
            if (j < len && html[j] == ';') {
                i = j; // skip entity
                if (entity == "amp") Serial.print('&');
                else if (entity == "lt") Serial.print('<');
                else if (entity == "gt") Serial.print('>');
                else if (entity == "quot") Serial.print('"');
                else if (entity == "apos" || entity == "#39") Serial.print('\'');
                else if (entity == "nbsp") Serial.print(' ');
                else Serial.print(' ');
                blank_lines = 0;
                continue;
            }
        }

        // Normal characters
        if (c == '\r' || c == '\n') {
            if (blank_lines < 2) {
                Serial.println();
                blank_lines++;
            }
        } else {
            Serial.write(c);
            blank_lines = 0;
        }
    }
    Serial.println();
}

static void displayArticle(uint32_t article_id, uint16_t block_num) {
    Serial.printf("\n[Loading article ID: %u, block %u...]\n", article_id, block_num);
    ArticleResult ar = wikiDbLoadArticle(article_id, block_num);
    if (!ar.found) {
        Serial.printf("Error: Article ID %u (block %u) not found or could not be decompressed.\n",
                      article_id, block_num);
        return;
    }

    g_current_article_id = article_id;
    g_current_block_num  = block_num;
    g_total_blocks       = ar.num_blocks;
    if (ar.title.length() > 0) g_current_title = ar.title;

    Serial.println("\n========================================================");
    Serial.printf("  %s  (Page %u of %u)\n", g_current_title.c_str(), block_num + 1, g_total_blocks);
    Serial.println("========================================================");

    printHtmlAsText(ar.html, ar.html_len);

    Serial.println("\n--------------------------------------------------------");
    if (g_total_blocks > 1) {
        if (g_current_block_num + 1 < g_total_blocks) {
            Serial.printf("[Type 'n' for page %u of %u]  ", g_current_block_num + 2, g_total_blocks);
        }
        if (g_current_block_num > 0) {
            Serial.printf("[Type 'p' for page %u]  ", g_current_block_num);
        }
        Serial.println();
    }
}

static void handleSearchCommand(const String &term, bool isContains) {
    if (term.length() == 0) {
        Serial.println("Usage: s <query> or c <query>");
        return;
    }

    Serial.printf("Searching for \"%s\"...\n", term.c_str());
    if (isContains) {
        g_last_search = wikiDbSearchContains(term, 10);
    } else {
        g_last_search = wikiDbSearch(term, 10);
    }

    if (g_last_search.empty()) {
        Serial.println("No matching articles found.");
        return;
    }

    Serial.printf("Found %u results:\n", (unsigned)g_last_search.size());
    for (size_t i = 0; i < g_last_search.size(); ++i) {
        Serial.printf("  [%u] %s (ID: %u)\n", (unsigned)(i + 1),
                      g_last_search[i].title.c_str(),
                      g_last_search[i].article_id);
    }
    Serial.println("Type result number (1-10) to read, or 's <query>' to search again.");
}

static void executeCommand(const String &cmdLine) {
    String line = cmdLine;
    line.trim();
    if (line.length() == 0) return;

    // Check if input is a single number 1..10 to select from last search
    int selectedNum = line.toInt();
    if (selectedNum >= 1 && selectedNum <= (int)g_last_search.size()) {
        uint32_t aid = g_last_search[selectedNum - 1].article_id;
        displayArticle(aid, 0);
        return;
    }

    int spaceIdx = line.indexOf(' ');
    String cmd = (spaceIdx >= 0) ? line.substring(0, spaceIdx) : line;
    String arg = (spaceIdx >= 0) ? line.substring(spaceIdx + 1) : "";
    cmd.toLowerCase();
    arg.trim();

    if (cmd == "s" || cmd == "search") {
        handleSearchCommand(arg, false);
    } else if (cmd == "c" || cmd == "contains") {
        handleSearchCommand(arg, true);
    } else if (cmd == "r" || cmd == "read") {
        uint32_t aid = (uint32_t)arg.toInt();
        if (aid == 0 && arg != "0") {
            Serial.println("Usage: r <article_id> (e.g. 'r 10')");
        } else {
            displayArticle(aid, 0);
        }
    } else if (cmd == "n" || cmd == "next") {
        if (g_current_article_id == 0) {
            Serial.println("No article currently open.");
        } else if (g_current_block_num + 1 >= g_total_blocks) {
            Serial.println("Already on the last page of this article.");
        } else {
            displayArticle(g_current_article_id, g_current_block_num + 1);
        }
    } else if (cmd == "p" || cmd == "prev") {
        if (g_current_article_id == 0) {
            Serial.println("No article currently open.");
        } else if (g_current_block_num == 0) {
            Serial.println("Already on the first page of this article.");
        } else {
            displayArticle(g_current_article_id, g_current_block_num - 1);
        }
    } else if (cmd == "info") {
        Serial.println("\n--- Offline Wikipedia Status ---");
        Serial.printf("Database Name: %s\n", wikiDbName().c_str());
        Serial.printf("Total Articles: %u\n", wikiDbArticleCount());
        Serial.printf("Images Available: %s\n", wikiDbImagesAvailable() ? "Yes" : "No");
        Serial.printf("Free Heap: %u bytes (Max block: %u bytes)\n",
                      ESP.getFreeHeap(), ESP.getMaxAllocHeap());
#ifdef BOARD_HAS_PSRAM
        Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
#endif
    } else if (cmd == "help" || cmd == "?") {
        printHelp();
    } else {
        Serial.printf("Unknown command: '%s'. Type 'help' or '?' for available commands.\n", line.c_str());
    }
}

void serialCliInit() {
    Serial.println("\n========================================================");
    Serial.println("   ESP32-S3 Offline Wikipedia Reader (Serial CLI)");
    Serial.printf("   Database: %s\n", wikiDbName().c_str());
    Serial.printf("   Articles: %u\n", wikiDbArticleCount());
    Serial.println("========================================================");
    printHelp();
    printPrompt();
    g_input_pos = 0;
}

void serialCliLoop() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        // Echo typed character (with backspace handling)
        if (c == '\r' || c == '\n') {
            Serial.println();
            if (g_input_pos > 0) {
                g_input_line[g_input_pos] = '\0';
                String cmdLine = String(g_input_line);
                g_input_pos = 0;
                executeCommand(cmdLine);
            }
            printPrompt();
        } else if (c == '\b' || c == 127) { // backspace
            if (g_input_pos > 0) {
                g_input_pos--;
                Serial.print("\b \b");
            }
        } else if (g_input_pos + 1 < sizeof(g_input_line)) {
            g_input_line[g_input_pos++] = c;
            Serial.write(c);
        }
    }
}
