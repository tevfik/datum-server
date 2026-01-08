#include "web_routes.h"
#include "sd_storage.h"

static const char *WWW_USERNAME = "admin";
static String WWW_PASSWORD = "admin"; // Default password for easy access

void handleSDList(WebServer &server) {
  if (!server.authenticate(WWW_USERNAME, WWW_PASSWORD.c_str())) {
    return server.requestAuthentication();
  }
  String json = listSDFiles("/capture");
  server.send(200, "application/json", json);
}

void handleSDDownload(WebServer &server) {
  if (!server.authenticate(WWW_USERNAME, WWW_PASSWORD.c_str())) {
    return server.requestAuthentication();
  }

  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing 'file' parameter");
    return;
  }

  String filePath = "/capture/" + server.arg("file");

  if (SD_MMC.exists(filePath)) {
    File file = SD_MMC.open(filePath, "r");
    server.streamFile(file, "image/jpeg");
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

void handleSDGallery(WebServer &server) {
  if (!server.authenticate(WWW_USERNAME, WWW_PASSWORD.c_str())) {
    return server.requestAuthentication();
  }

  String html = "<!DOCTYPE html><html><head><title>SD Gallery</title>";
  html +=
      "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;background:#222;color:#fff} "
          "img{max-width:100%;height:auto;border:1px solid #555;margin:5px} "
          ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax("
          "150px,1fr));gap:10px} a{color:#4da3ff}</style>";
  html += "<script>";
  html += "function loadFiles() { "
          "fetch('/sd/list').then(r=>r.json()).then(files => {";
  html += "  const grid = document.getElementById('grid');";
  html += "  grid.innerHTML = files.map(f => `<div "
          "style='text-align:center'><a href='/sd/download?file=${f.name}' "
          "target='_blank'><img src='/sd/download?file=${f.name}' "
          "loading='lazy'></a><br><small>${f.name}</small></div>`).join('');";
  html += "}); } window.onload=loadFiles;";
  html += "</script></head><body>";
  html +=
      "<h2>SD Card Gallery</h2><p><a href='/'>&larr; Back to Stream</a></p>";
  html += "<div id='grid' class='grid'>Loading...</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setupWebRoutes(WebServer &server, const String &apiKey) {
  // WWW_PASSWORD = apiKey; // Ignored: Using "admin" for now per user request
  // due to app UI limitation

  server.on("/sd/list", HTTP_GET, [&server]() { handleSDList(server); });
  server.on("/sd/download", HTTP_GET,
            [&server]() { handleSDDownload(server); });
  server.on("/sd", HTTP_GET, [&server]() { handleSDGallery(server); });
}
