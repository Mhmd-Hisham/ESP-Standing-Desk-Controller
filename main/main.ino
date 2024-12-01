#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <map>

// Wifi and DNS settings
const char* SSID = "ssid";
const char* PASSWORD = "password";
const char* ESP_DNS_HOSTNAME = "standingdesk"; // http://standingdesk.local

// figured those values out by trial and error
const int standingTime = 13500;
const int seatingTime = 12000;
const int waitDelay = 200;

// static IP config
const IPAddress STATIC_IP(192, 168, 137, 136);
const IPAddress GATEWAY(192, 168, 1, 1);
const IPAddress SUBNET(255, 255, 255, 0);

// pins for motor control
const int UP_TRANSISTOR_PIN = 15;   // D7 on ESP
const int DOWN_TRANSISTOR_PIN = 13; // D8 on ESP

enum MovementCommand { STOP, STANDING, SEATING, UP, DOWN };

// map for movement command strings
const std::map<MovementCommand, const char*> COMMAND_STRINGS = {
    {STOP, "stop"},
    {STANDING, "standing"},
    {SEATING, "seating"},
    {UP, "up"},
    {DOWN, "down"}
};

// movement state variables
unsigned long movementStartTime = 0;
unsigned long movementDuration = 0;
bool isMoving = false;
MovementCommand currentCommand = STOP;

// html web page for the ESP, will be updated later by a function
const char* HTML_HOMEPAGE_STRING = "";
ESP8266WebServer server(80);

void connectToWiFi();
void executeCommand(MovementCommand command);
void stopMovement();
void updateMovementState();
void handleMovementRequest(MovementCommand command, int duration = 0);
void setupDNS();
void setHomePageString();

void setup() {
  // configure motor pins
  pinMode(UP_TRANSISTOR_PIN, OUTPUT);
  pinMode(DOWN_TRANSISTOR_PIN, OUTPUT);

  // start serial communication
  Serial.begin(115200);

  // connect to WiFi
  connectToWiFi();

  // configure server routes
  setHomePageString();
  server.on("/", [](){ server.send(200, "text/html", HTML_HOMEPAGE_STRING); });
  server.on("/up", []() { handleMovementRequest(UP); });
  server.on("/down", []() { handleMovementRequest(DOWN); });
  server.on("/standing", []() { handleMovementRequest(STANDING, standingTime); });
  server.on("/seating", []() { handleMovementRequest(SEATING, seatingTime); });
  server.on("/stop", []() {
    if (!isMoving && currentCommand == STOP) {
      server.send(400, "text/plain", "No active movement to stop");
    } else {
      server.send(200, "text/plain", "Stopping movement");
      stopMovement();
    }
  });

  // start the server
  server.begin();

  // set the desk state initially to be stopped
  stopMovement();
}

void loop() {
  // reconnect to WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // update mDNS
  MDNS.update();

  // handle incoming client requests
  server.handleClient();

  // update movement state
  updateMovementState();
}

// loop to connect to wifi, set up static IP, run mDNS
void connectToWiFi() {
  WiFi.config(STATIC_IP, GATEWAY, SUBNET);
  WiFi.begin(SSID, PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n[INFO]Connected to WiFi");
  Serial.println(WiFi.localIP());

  setupDNS();
}

// setup the mDNS broadcast
void setupDNS(){
  if (MDNS.begin(ESP_DNS_HOSTNAME)) {
    Serial.println("[INFO] mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[ERROR] Error setting up mDNS responder!");
  }
}

// handle a movement request
void handleMovementRequest(MovementCommand command, int duration) {
  if (isMoving && (command == STANDING || command == SEATING)) {
    server.send(400, "text/plain", "Already executing a timed movement");
    return;
  }

  executeCommand(command);
  server.send(200, "text/plain", "Starting " + String(COMMAND_STRINGS.at(command)) + " command.");

  // if the command involves a timed movement, set up the timer
  if (command == STANDING || command == SEATING) {
    movementStartTime = millis();
    movementDuration = duration;
    isMoving = true;
  }
}

// execute a specific movement command
void executeCommand(MovementCommand command) {
  // ensure motors are off before starting any movement
  stopMovement();

  currentCommand = command;

  switch (command) {
    case UP:
    case STANDING:
      digitalWrite(UP_TRANSISTOR_PIN, HIGH);
      Serial.println("[INFO] Moving up (Command="+String(COMMAND_STRINGS.at(command))+").");
      break;
    case DOWN:
    case SEATING:
      digitalWrite(DOWN_TRANSISTOR_PIN, HIGH);
      Serial.println("[INFO] Moving down (Command="+String(COMMAND_STRINGS.at(command))+").");
      break;
    case STOP:
    default:
      stopMovement();
      Serial.println("[INFO] Stopping movement.");
      break;
  }
}

// stop all movement
void stopMovement() {
  digitalWrite(UP_TRANSISTOR_PIN, LOW);
  digitalWrite(DOWN_TRANSISTOR_PIN, LOW);
  isMoving = false;
  currentCommand = STOP;
  movementDuration = 0;
}

// update movement state
void updateMovementState() {
  if (isMoving && (currentCommand == STANDING || currentCommand == SEATING) &&
      millis() - movementStartTime >= movementDuration) {
    stopMovement();
    server.send(200, "text/plain", "Finished executing `" + String(COMMAND_STRINGS.at(currentCommand)) + "` command.");
    Serial.println("[INFO] Timed movement completed.");
  }
}

void setHomePageString(){
    HTML_HOMEPAGE_STRING = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>ESP Standing Desk Controller</title><style>body {font-family: Arial, sans-serif;margin: 0;padding: 20px;display: flex;flex-direction: column;align-items: center;justify-content: center;min-height: 80vh;background-color: #f0f0f0;}h1 {margin-bottom: 20px;color: #333;text-align: center;}.button-container {display: grid;grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));gap: 15px;width: 100%;max-width: 400px;margin-bottom: 20px;}.button {padding: 12px;font-size: 16px;color: white;background-color: #007BFF;border: none;border-radius: 5px;cursor: pointer;text-align: center;transition: background-color 0.3s;}.button:hover {background-color: #0056b3;}.terminal {width: 100%;max-width: 800px;height: 400px;background-color: #000;color: #0f0;padding: 10px;overflow-y: auto;border-radius: 5px;font-family: monospace;font-size: 14px;box-sizing: border-box;}.terminal p {margin: 0;}</style></head><body><h1>ESP Standing Desk Controller</h1><div class=\"button-container\"><button class=\"button\" onmousedown=\"startCommand('up')\" onmouseup=\"stopCommand()\">Up</button><button class=\"button\" onmousedown=\"startCommand('down')\" onmouseup=\"stopCommand()\">Down</button><button class=\"button\" onclick=\"sendCommand('stop')\">Stop</button><button class=\"button\" onclick=\"sendCommand('standing')\">Standing</button><button class=\"button\" onclick=\"sendCommand('seating')\">Seating</button></div><div class=\"terminal\" id=\"terminal\"></div><script>let commandSent = false;function logMessage(message) {const terminal = document.getElementById('terminal');const log = document.createElement('p');const now = new Date();const hours = now.getHours().toString().padStart(2, '0');const minutes = now.getMinutes().toString().padStart(2, '0');const seconds = now.getSeconds().toString().padStart(2, '0');const milliseconds = now.getMilliseconds().toString().padStart(3, '0');const timestamp = `${hours}:${minutes}:${seconds}.${milliseconds}`;log.textContent = `[${timestamp}] ${message}`;terminal.appendChild(log);terminal.scrollTop = terminal.scrollHeight;}function sendCommand(command) {if (commandSent) return;commandSent = true;logMessage(`Sending request: ${command}`);fetch(`/${command}`).then(response => {if (!response.ok) {throw new Error(`Server responded with status ${response.status}`);}return response.text();}).then(data => {logMessage(`Server response: ${data}`);}).catch(error => {logMessage(`Error: ${error.message}`);}).finally(() => {commandSent = false;});}function startCommand(command) {sendCommand(command);}function stopCommand() {sendCommand('stop');}</script></body></html>";
}