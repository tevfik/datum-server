#ifndef WEB_ROUTES_H
#define WEB_ROUTES_H

#include <WebServer.h>

// Setup Web Routes for SD Card
// Setup Web Routes (Maps all endpoints)
void setupWebRoutes(WebServer &server, const String &apiKey);

// Handlers
void handleOnboardingRoot(WebServer &server);
void handleStream(WebServer &server);
void handleCapture(WebServer &server);
void handleScan(WebServer &server);
void handleConfigure(WebServer &server);
void handleProvision(WebServer &server);
void handleDeviceInfo(WebServer &server);
void handleThingDescription(WebServer &server);
void handleAction(WebServer &server);

#endif
