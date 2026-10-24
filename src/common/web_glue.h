#pragma once

#include <WebServer.h>

#include "web.h"

// Arduino WebServer <-> lib/web. Register the routes with server.onNotFound(handler) and let the handler
// pass readRequest() to the lib router; the board main then has no route list of its own.

// Method, path, all arguments (query + form), JSON body (WebServer's "plain" argument) and the Cookie header
// (needs server.collectHeaders("Cookie")).
WebRequest readRequest(WebServer& server);

// Sends status, headers and body, and logs "[http] POST /kamera/einfahrt -> 303 (12 ms)".
void sendResponse(WebServer& server, const WebRequest& req, const WebResponse& res, unsigned long startMs);
