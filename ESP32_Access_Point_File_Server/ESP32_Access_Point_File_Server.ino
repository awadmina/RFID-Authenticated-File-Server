#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// Access Point credentials
const char* ssid = "ESP32-AP"; // Access Point SSID
const char* password = "12345678"; // Access Point password

AsyncWebServer server(80); // Create an asynchronous web server on port 80

String currentUID = "No UID scanned yet"; // Stores the last received UID
bool accessWaiting = false; // Tracks if the system is waiting for a valid UID
bool accessGranted = false; // Tracks if access has been granted
String requestedFile = ""; // Stores the file requested by the user

unsigned long accessRequestStartTime = 0; // Tracks the start time of access request
unsigned long accessGrantedStartTime = 0; // Tracks the start time of access granted

const unsigned long ACCESS_TIMEOUT = 15000; // 15 seconds timeout in milliseconds

// Structure to map UIDs to specific files
struct FileAccess {
  String uid; // UID associated with the file
  String fileName; // File name linked to the UID
};

// List of UIDs and their corresponding files
FileAccess fileAccessMapping[] = {
  { "a33e1af5", "SecretFile_1.txt" }, // UID "a33e1af5" maps to "SecretFile_1.txt"
  // Add more mappings if needed
};

// Serve the homepage
void handleRoot(AsyncWebServerRequest* request) {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP32 File Server</title>
    <script>
      function requestAccess(fileName) {
        fetch('/requestAccess', { 
          method: 'POST', 
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ file: fileName }) 
        })
        .then(response => response.text())
        .then(data => {
          document.getElementById('status').innerHTML = data;
          document.getElementById('downloadLink').style.display = 'none';
        });
      }

      // Poll the server for status and UID updates
      setInterval(() => {
        fetch('/getStatus')
          .then(response => response.text())
          .then(data => document.getElementById('status').innerHTML = data); // Update status dynamically

        fetch('/getUID')
          .then(response => response.text())
          .then(data => document.getElementById('uid').innerText = data); // Update UID dynamically
      }, 1000); // Update every 1 second
    </script>
  </head>
  <body>
    <h1>ESP32 File Server</h1>
    <p>Current UID: <span id="uid">No UID scanned yet</span></p>
    <button onclick="requestAccess('SecretFile_1.txt')">Request Access to SecretFile_1.txt</button>
    <p id="status">Idle</p>
    <br>
    <a id="downloadLink" style="display: none;" href="#">Download File</a>
  </body>
  </html>
  )rawliteral";

  request->send(200, "text/html", html);
}


// Handle UID updates
void handlePostData(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  currentUID = String((char*)data).substring(0, len);
  Serial.println("UID received: " + currentUID);

  if (accessWaiting) {
    for (FileAccess fa : fileAccessMapping) {
      if (requestedFile == fa.fileName && currentUID == fa.uid) {
        accessGranted = true;
        accessWaiting = false;
        accessGrantedStartTime = millis(); // Start the access granted timer
        Serial.println("Access Granted for file: " + requestedFile);
        break;
      }
    }
    if (!accessGranted) {
      Serial.println("UID does not match required UID for " + requestedFile);
    }
  }

  request->send(200, "text/plain", "UID received");
}

// Handle file access requests
void handleRequestAccess(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  String json = String((char*)data).substring(0, len);
  int start = json.indexOf(":\"") + 2;
  int end = json.indexOf("\"", start);
  requestedFile = json.substring(start, end);

  if (LittleFS.exists("/" + requestedFile)) {
    accessWaiting = true; // Enable waiting state
    accessGranted = false; // Reset granted state
    accessRequestStartTime = millis(); // Set the start time of the request
    Serial.println("Access requested for file: " + requestedFile);
    request->send(200, "text/plain", "Waiting for UID...");
  } else {
    Serial.println("File not found: " + requestedFile);
    request->send(404, "text/plain", "File not found");
  }
}


// Handle status polling
// Handle status polling with timer display
void handleGetStatus(AsyncWebServerRequest* request) {
  unsigned long currentTime = millis();
  String response;

  if (accessWaiting) {
    // Calculate remaining time for access request
int remainingTime = max(0, (int)((ACCESS_TIMEOUT - (currentTime - accessRequestStartTime)) / 1000));
    response = "Waiting for UID... (" + String(remainingTime) + " seconds left)";
  } else if (accessGranted) {
    // Calculate remaining time for access granted
    int remainingTime = max(0, (int)((ACCESS_TIMEOUT - (currentTime - accessGrantedStartTime)) / 1000));
    response = "Access Granted! <a id='downloadLink' href='/download?file=" + requestedFile + "'>Download File</a> (" + String(remainingTime) + " seconds left)";
  } else {
    response = "Idle";
  }

  request->send(200, "text/html", response); // Send response as HTML
}


// Handle UID polling
void handleGetUID(AsyncWebServerRequest* request) {
  request->send(200, "text/plain", currentUID);
}

// Handle file downloads
void handleDownload(AsyncWebServerRequest* request) {
  if (!accessGranted) {
    request->send(403, "text/plain", "Access Denied");
    return;
  }

  if (request->hasParam("file")) {
    String fileName = request->getParam("file")->value();
    if (LittleFS.exists("/" + fileName)) {
      AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/" + fileName, "application/octet-stream");
      response->addHeader("Content-Disposition", "attachment; filename=" + fileName);
      request->send(response);
    } else {
      request->send(404, "text/plain", "File not found");
    }
  } else {
    request->send(400, "text/plain", "Bad Request: Missing file parameter");
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  WiFi.softAP(ssid, password);
  Serial.println("Access Point Started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/sendUID", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL, handlePostData);
  server.on("/requestAccess", HTTP_POST, [](AsyncWebServerRequest* request){}, NULL, handleRequestAccess);
  server.on("/getStatus", HTTP_GET, handleGetStatus);
  server.on("/getUID", HTTP_GET, handleGetUID);
  server.on("/download", HTTP_GET, handleDownload);

  server.begin();
}

void loop() {
  unsigned long currentTime = millis();

  // Check if the access request timeout has elapsed
  if (accessWaiting && (currentTime - accessRequestStartTime >= ACCESS_TIMEOUT)) {
    accessWaiting = false; // Reset waiting state
    Serial.println("Access request timed out.");
  }

  // Check if the access granted timeout has elapsed
  if (accessGranted && (currentTime - accessGrantedStartTime >= ACCESS_TIMEOUT)) {
    accessGranted = false; // Reset granted state
    Serial.println("Access granted expired.");
  }
}

