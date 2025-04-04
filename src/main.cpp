// Base ESP8266
#include <ESP8266WiFi.h>
//#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
//#include <math.h>
#include <EEPROM.h>
#include <Esp.h>
#include <DNSServer.h>

using namespace std;

// EEPROM Layout Configuration
namespace EEPROMConfig {
    static const int EEPROM_SIZE = 512;
    static const uint8_t EEPROM_MAGIC_NUMBER = 0xAB;
    static const int EEPROM_MAGIC_ADDR = 0;
    static const int EEPROM_DATA_ADDR = 1;
}

// Define a struct to hold all the settings
struct FireSettings {
    float minFireTime;
    float maxFireTime;
    float remoteFireTime;
    float resetLimit;
    float fireCycle;
    float minGyro;
    float maxGyro;
};

// Define the default values as constants
const FireSettings DEFAULT_SETTINGS = {
    .minFireTime = 1.0,
    .maxFireTime = 6.0,
    .remoteFireTime = 2.5,
    .resetLimit = 3.0,
    .fireCycle = 0.5,
    .minGyro = 50.0,
    .maxGyro = 750.0
};

// First, define the structures
struct SystemStats {
    // Core operational stats
    unsigned long remoteTriggersCount;     // Number of manual/remote button triggers
    unsigned long accelTriggersCount;      // Number of acceleration-based triggers
    float highestAccelReading;             // Highest recorded acceleration in g
    float totalFireTime;                   // Total cumulative firing time in seconds
    
    // Performance metrics
    float longestFireDuration;             // Longest single fire event duration in seconds
    float averageAccelTrigger;             // Running average of trigger acceleration values
    float peakMemoryUsage;                 // Peak heap memory usage as percentage (0-100%)
    float highestGyroReading;              // Highest angular velocity recorded
};

// Define default stats - match the order in the SystemStats struct
const SystemStats DEFAULT_STATS = {
    .remoteTriggersCount = 0,
    .accelTriggersCount = 0,
    .highestAccelReading = 0.0,
    .totalFireTime = 0.0,
    .longestFireDuration = 0.0,
    .averageAccelTrigger = 0.0,
    .peakMemoryUsage = 0.0,
    .highestGyroReading = 0.0
};

struct SystemState {
    // WiFi/Server settings
    ESP8266WebServer server{80};
    DNSServer dnsServer;

    // Pin assignments (keeping as constants)
    static const uint8_t FIRE_PIN_1 = 14;      // Labeled "D5"
    static const uint8_t FIRE_PIN_2 = 12;      // Labeled "D6"
    static const uint8_t MANUAL_TRIGGER_1 = 0;  // Labeled "D3"
    static const uint8_t MANUAL_TRIGGER_2 = 2;  // Labeled "D4"

    // System Settings
    float loopRate = 0.05;
    int remoteTriggerState = 1;
    int localTriggerState1 = 1;
    int localTriggerState2 = 1;
    int triggerState = 0;
    int fireOn = 0;
    float fireTimer = 0;
    String request = "null";
    int resetState = 1;

    // Accelerometer Settings
    static const int MPU_1 = 0x68;
    static const int MPU_2 = 0x69;
    float accel1[9] = {0};
    float accel2[9] = {0};
    float aveGyro = 0;
    float minGyro = 0;
    float maxGyro = 0;
    float gyroFactor = 0;
    float accelFactor = 0;

    // Fire control Settings
    float minFireTime = 0;
    float maxFireTime = 0;
    float fireTimeLimit = 0;
    float remoteFireTime = 0;
    float resetLimit = 0;
    float resetTimer = 0;
    float fireCycle = 0;
    bool fireCycleToggle = false;  // Instead of fireCycleCounter
    unsigned long fireStartTime = 0;

    // Current settings and stats
    FireSettings currentSettings;
    SystemStats currentStats;

    // Add pseudo fire pin states
    bool firePinState1 = false;
    bool firePinState2 = false;
};

// Create the single global state instance
SystemState state;

// Add these forward declarations after the struct definitions but before any function implementations
bool saveStats();
void loadDefaultStats();
bool loadStats();
void startFire();
void stopFire();
void logFireStatus(float currentFireDuration, const char* eventType = "Status");
void updateMotionStats();

// Modify the function that starts the fire (where FIRE_ON is set to true)
void startFire() {
    // Initiate firing sequence and update statistics
    state.fireOn = true;
    state.fireStartTime = millis();        // Track start time for duration calculation
    state.fireTimer = 0;                   // Reset active fire timer
    state.currentStats.remoteTriggersCount++;
    saveStats();
}

// Modify the function that stops the fire
void stopFire() {
    // Safely terminate firing sequence and update statistics
    if (state.fireOn) {
        state.fireOn = false;
        unsigned long currentTime = millis();
        float fireDuration = (currentTime - state.fireStartTime) / 1000.0;
        
        // Log the final state before stopping
        //logFireStatus(fireDuration, "Final");
        
        // Validate and record fire duration with increased tolerance (5 loop cycles worth)
        if (fireDuration > 0 && fireDuration <= state.fireTimeLimit + (state.loopRate * 5)) {
            // Double the recorded fire time if both pins were firing simultaneously (fireCycle = 0)
            float effectiveFireTime = (state.fireCycle == 0) ? fireDuration * 2 : fireDuration;
            state.currentStats.totalFireTime += effectiveFireTime;
            
            if (fireDuration > state.currentStats.longestFireDuration) {
                state.currentStats.longestFireDuration = fireDuration;
            }
            saveStats();
        } else {
            //logFireStatus(fireDuration, "Invalid");
        }
        
        // Ensure fire pins are disabled
        digitalWrite(state.FIRE_PIN_1, LOW);
        digitalWrite(state.FIRE_PIN_2, LOW);
    }
}

// Add these functions after the FireSettings functions
bool saveStats() {
    // Get the currently stored stats from EEPROM for comparison
    SystemStats oldStats;
    EEPROM.get(EEPROMConfig::EEPROM_DATA_ADDR + sizeof(FireSettings), oldStats);
    
    // Log changes if any values are different
    Serial.println(F("\n=== Statistics Update ==="));
    if (oldStats.remoteTriggersCount != state.currentStats.remoteTriggersCount)
        Serial.printf("Remote Triggers: %lu -> %lu\n", oldStats.remoteTriggersCount, state.currentStats.remoteTriggersCount);
    if (oldStats.accelTriggersCount != state.currentStats.accelTriggersCount)
        Serial.printf("Accel Triggers: %lu -> %lu\n", oldStats.accelTriggersCount, state.currentStats.accelTriggersCount);
    if (oldStats.highestAccelReading != state.currentStats.highestAccelReading)
        Serial.printf("Highest Accel Reading: %.2f -> %.2f g\n", oldStats.highestAccelReading, state.currentStats.highestAccelReading);
    if (oldStats.totalFireTime != state.currentStats.totalFireTime)
        Serial.printf("Total Fire Time: %.2f -> %.2f sec\n", oldStats.totalFireTime, state.currentStats.totalFireTime);
    if (oldStats.longestFireDuration != state.currentStats.longestFireDuration)
        Serial.printf("Longest Fire Duration: %.2f -> %.2f sec\n", oldStats.longestFireDuration, state.currentStats.longestFireDuration);
    if (oldStats.averageAccelTrigger != state.currentStats.averageAccelTrigger)
        Serial.printf("Average Accel Trigger: %.2f -> %.2f g\n", oldStats.averageAccelTrigger, state.currentStats.averageAccelTrigger);
    if (oldStats.peakMemoryUsage != state.currentStats.peakMemoryUsage)
        Serial.printf("Peak Memory Usage: %.2f -> %.2f%%\n", oldStats.peakMemoryUsage, state.currentStats.peakMemoryUsage);
    if (oldStats.highestGyroReading != state.currentStats.highestGyroReading)
        Serial.printf("Highest Gyro Reading: %.2f -> %.2f deg/s\n", oldStats.highestGyroReading, state.currentStats.highestGyroReading);

    // Save the new stats
    EEPROM.put(EEPROMConfig::EEPROM_DATA_ADDR + sizeof(FireSettings), state.currentStats);
    if (!EEPROM.commit()) {
        Serial.println(F("Error: Failed to commit stats to EEPROM"));
        return false;
    }
    Serial.println(F("=====================\n"));
    return true;
}

void loadDefaultStats() {
    state.currentStats = DEFAULT_STATS;
}

bool loadStats() {
    EEPROM.get(EEPROMConfig::EEPROM_DATA_ADDR + sizeof(FireSettings), state.currentStats);
    
    // Basic validation
    bool valid = true;
    valid &= state.currentStats.remoteTriggersCount < 1000000;  // Reasonable maximum
    valid &= state.currentStats.accelTriggersCount < 1000000;
    valid &= state.currentStats.highestAccelReading >= 0 && state.currentStats.highestAccelReading < 10000;
    valid &= state.currentStats.totalFireTime >= 0 && state.currentStats.totalFireTime < 1000000;
    
    if (!valid) {
        loadDefaultStats();
        return false;
    }
    return true;
}

// Add these functions for EEPROM management
bool saveSettings() {
    EEPROM.write(EEPROMConfig::EEPROM_MAGIC_ADDR, EEPROMConfig::EEPROM_MAGIC_NUMBER);
    EEPROM.put(EEPROMConfig::EEPROM_DATA_ADDR, state.currentSettings);
    if (!EEPROM.commit()) {
        Serial.println("Error: Failed to commit settings to EEPROM");
        return false;
    }
    return true;
}

void loadDefaultSettings() {
    state.currentSettings = DEFAULT_SETTINGS;
    // Update global variables
    state.minFireTime = state.currentSettings.minFireTime;
    state.maxFireTime = state.currentSettings.maxFireTime;
    state.remoteFireTime = state.currentSettings.remoteFireTime;
    state.resetLimit = state.currentSettings.resetLimit;
    state.fireCycle = state.currentSettings.fireCycle;
    state.minGyro = state.currentSettings.minGyro;
    state.maxGyro = state.currentSettings.maxGyro;
}

bool loadSettings() {
    if (EEPROM.read(EEPROMConfig::EEPROM_MAGIC_ADDR) != EEPROMConfig::EEPROM_MAGIC_NUMBER) {
        loadDefaultSettings();
        return false;
    }
    
    EEPROM.get(EEPROMConfig::EEPROM_DATA_ADDR, state.currentSettings);
    
    // Validate loaded values
    bool valid = true;
    valid &= state.currentSettings.minFireTime > 0 && state.currentSettings.minFireTime < 60;
    valid &= state.currentSettings.maxFireTime > 0 && state.currentSettings.maxFireTime < 60;
    valid &= state.currentSettings.remoteFireTime > 0 && state.currentSettings.remoteFireTime < 60;
    valid &= state.currentSettings.resetLimit > 0 && state.currentSettings.resetLimit < 60;
    valid &= state.currentSettings.fireCycle >= 0 && state.currentSettings.fireCycle < 60;
    valid &= state.currentSettings.minGyro > 0 && state.currentSettings.minGyro < 1000;
    valid &= state.currentSettings.maxGyro > 0 && state.currentSettings.maxGyro < 2000;
    valid &= state.currentSettings.minGyro < state.currentSettings.maxGyro;
    
    if (!valid) {
        loadDefaultSettings();
        return false;
    }
    
    // Update global variables
    state.minFireTime = state.currentSettings.minFireTime;
    state.maxFireTime = state.currentSettings.maxFireTime;
    state.remoteFireTime = state.currentSettings.remoteFireTime;
    state.resetLimit = state.currentSettings.resetLimit;
    state.fireCycle = state.currentSettings.fireCycle;
    state.minGyro = state.currentSettings.minGyro;
    state.maxGyro = state.currentSettings.maxGyro;
    state.resetTimer = state.resetLimit;
    return true;
}

// prepare a web page to be send to a client (web browser)
String prepare_Root_Page() {
    String htmlPage;
    htmlPage.reserve(2048);  // Pre-allocate memory
    
    htmlPage += "<!DOCTYPE HTML><HTML><HEAD>";
    htmlPage += "<TITLE>HighNoon</TITLE>";
    htmlPage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    htmlPage += "<meta charset='UTF-8'>";
    htmlPage += "<style>";
    // Base styling to match other pages
    htmlPage += "body { font-size:200%; background-color:black; color:white; padding: 20px; text-align: center; }";
    htmlPage += ".title { font-size: 2em; margin: 20px 0; text-shadow: 0 0 10px #ff6b6b; }";
    
    // Navigation card styling
    htmlPage += ".nav-grid {";
    htmlPage += "  display: grid;";
    htmlPage += "  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));";
    htmlPage += "  gap: 20px;";
    htmlPage += "  max-width: 1200px;";
    htmlPage += "  margin: 0 auto;";
    htmlPage += "  padding: 20px;";
    htmlPage += "}";
    
    htmlPage += ".nav-card {";
    htmlPage += "  background: linear-gradient(145deg, #333 0%, #222 100%);";
    htmlPage += "  border-radius: 15px;";
    htmlPage += "  padding: 20px;";
    htmlPage += "  text-align: center;";
    htmlPage += "  transition: transform 0.3s, box-shadow 0.3s;";
    htmlPage += "  cursor: pointer;";
    htmlPage += "  box-shadow: 0 4px 8px rgba(0,0,0,0.2);";
    htmlPage += "}";
    
    htmlPage += ".nav-card:hover {";
    htmlPage += "  transform: translateY(-5px);";
    htmlPage += "  box-shadow: 0 8px 16px rgba(255,107,107,0.2);";
    htmlPage += "}";
    
    htmlPage += ".nav-card i {";
    htmlPage += "  font-size: 2em;";
    htmlPage += "  margin-bottom: 10px;";
    htmlPage += "  color: #4CAF50;";
    htmlPage += "}";
    
    htmlPage += "a {";
    htmlPage += "  color: white;";
    htmlPage += "  text-decoration: none;";
    htmlPage += "}";
    
    htmlPage += ".card-title {";
    htmlPage += "  color: #4CAF50;";
    htmlPage += "  font-size: 1.2em;";
    htmlPage += "  margin: 10px 0;";
    htmlPage += "}";
    
    htmlPage += ".card-description {";
    htmlPage += "  color: #888;";
    htmlPage += "  font-size: 0.8em;";
    htmlPage += "}";
    
    // Animated flame effect
    htmlPage += "@keyframes flicker {";
    htmlPage += "  0% { text-shadow: 0 0 10px #ff6b6b; }";
    htmlPage += "  50% { text-shadow: 0 0 20px #ff6b6b, 0 0 30px #ff4444; }";
    htmlPage += "  100% { text-shadow: 0 0 10px #ff6b6b; }";
    htmlPage += "}";
    
    htmlPage += ".flame { animation: flicker 2s infinite; }";
    htmlPage += "</style>";
    htmlPage += "</HEAD><BODY>";

    // Title with animated flames
    htmlPage += "<h1 class='title'>🔥 <span class='flame'>HighNoon</span> 🔥</h1>";

    // Navigation grid
    htmlPage += "<div class='nav-grid'>";
    
    // Fire Control Card
    htmlPage += "<a href='/fire'><div class='nav-card'>";
    htmlPage += "<div class='card-title'>🎯 Fire Control</div>";
    htmlPage += "<div class='card-description'>Trigger and monitor firing sequences</div>";
    htmlPage += "</div></a>";
    
    // Settings Card
    htmlPage += "<a href='/settings'><div class='nav-card'>";
    htmlPage += "<div class='card-title'>⚙️ Settings</div>";
    htmlPage += "<div class='card-description'>Configure system parameters</div>";
    htmlPage += "</div></a>";
    
    // Data Monitor Card
    htmlPage += "<a href='/data'><div class='nav-card'>";
    htmlPage += "<div class='card-title'>📊 Live Data</div>";
    htmlPage += "<div class='card-description'>Real-time system monitoring</div>";
    htmlPage += "</div></a>";
    
    // Statistics Card
    htmlPage += "<a href='/stats'><div class='nav-card'>";
    htmlPage += "<div class='card-title'>📈 Statistics</div>";
    htmlPage += "<div class='card-description'>System performance metrics</div>";
    htmlPage += "</div></a>";
    
    htmlPage += "</div>";  // Close nav-grid

    // Add version info or system status at bottom
    htmlPage += "<div style='margin-top: 40px; color: #666; font-size: 0.6em;'>";
    htmlPage += "HighNoon Control System v1.0";
    htmlPage += "</div>";

    htmlPage += "</BODY></HTML>";
    return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Data_Page() {
    String htmlPage;
    htmlPage.reserve(4096);  // Pre-allocate memory
    
    htmlPage += "<!DOCTYPE HTML><HTML><HEAD>";
    htmlPage += "<TITLE>Saloon Doors Data</TITLE>";
    htmlPage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    htmlPage += "<meta charset='UTF-8'>";
    htmlPage += "<style>";
    // Add consistent styling
    htmlPage += "body { font-size:200%; background-color:black; color:white; padding: 20px; }";
    htmlPage += ".data-box { background-color:#333; padding:15px; margin:10px; border-radius:5px; }";
    htmlPage += ".data-title { color:#4CAF50; margin-bottom:10px; font-size:120%; }";
    htmlPage += ".data-row { margin:8px 0; }";
    htmlPage += ".value { color:#FFA500; }";  // Orange color for values
    htmlPage += "a { color:#4CAF50; text-decoration:none; }";  // Green links
    htmlPage += "a:hover { color:#45a049; }";
    htmlPage += ".bool-true { color: #4CAF50; font-weight: bold; }";  // Green
    htmlPage += ".bool-false { color: #ff4444; font-weight: bold; }"; // Red
    htmlPage += ".state-firing { color: #ff4444; font-weight: bold; }";  // Red for firing
    htmlPage += ".state-idle { color: #4CAF50; font-weight: bold; }";    // Green for idle
    htmlPage += ".status-indicator {";  // Add the same indicator style
    htmlPage += "  display: inline-block;";
    htmlPage += "  width: 20px;";
    htmlPage += "  height: 20px;";
    htmlPage += "  border-radius: 50%;";
    htmlPage += "  margin: 0 10px;";
    htmlPage += "}";
    htmlPage += "</style>";
    
    htmlPage += "<script>";
    // Cache DOM elements on load for better performance
    htmlPage += "let elements = {};";
    htmlPage += "document.addEventListener('DOMContentLoaded', function() {";
    htmlPage += "  ['pin1', 'pin2', 'resetState', 'fireOn', 'remoteTriggerState',";
    htmlPage += "   'localTriggerState1', 'localTriggerState2', 'resetTimer',";
    htmlPage += "   'resetLimit', 'fireTimer', 'fireTimeLimit', 'accel1', 'accel2', 'aveGyro'";
    htmlPage += "  ].forEach(id => elements[id] = document.getElementById(id));";
    htmlPage += "});";

    // Optimize the update function
    htmlPage += "function updateElement(id, value, type) {";
    htmlPage += "  const elem = elements[id];";
    htmlPage += "  if (!elem) return;";
    htmlPage += "  if (elem._lastValue === value) return;"; // Skip if value hasn't changed
    htmlPage += "  elem._lastValue = value;";
    htmlPage += "  switch (type) {";
    htmlPage += "    case 'pin':";
    htmlPage += "      elem.className = value ? 'state-firing' : 'state-idle';";
    htmlPage += "      elem.textContent = value ? 'FIRE' : 'IDLE';";
    htmlPage += "      break;";
    htmlPage += "    case 'trigger':";
    htmlPage += "      elem.className = !value ? 'state-firing' : 'state-idle';";
    htmlPage += "      elem.textContent = !value ? 'ACTIVE' : 'INACTIVE';";
    htmlPage += "      break;";
    htmlPage += "    case 'reset':";
    htmlPage += "      elem.className = value ? 'bool-true' : 'bool-false';";
    htmlPage += "      elem.textContent = value ? 'READY' : 'WAITING';";
    htmlPage += "      break;";
    htmlPage += "    case 'fire':";
    htmlPage += "      elem.className = value ? 'state-firing' : 'state-idle';";
    htmlPage += "      elem.textContent = value ? 'FIRING' : 'IDLE';";
    htmlPage += "      break;";
    htmlPage += "    case 'number':";
    htmlPage += "      elem.textContent = Number(value).toFixed(2);";
    htmlPage += "      break;";
    htmlPage += "    default:";
    htmlPage += "      elem.textContent = value;";
    htmlPage += "  }";
    htmlPage += "}";

    // Main update function
    htmlPage += "function updateData() {";
    htmlPage += "  fetch('/data/status')";
    htmlPage += "    .then(response => response.json())";
    htmlPage += "    .then(data => {";
    htmlPage += "      updateElement('pin1', data.firePin1, 'pin');";
    htmlPage += "      updateElement('pin2', data.firePin2, 'pin');";
    htmlPage += "      updateElement('resetState', data.resetState, 'reset');";
    htmlPage += "      updateElement('fireOn', data.fireOn, 'fire');";
    htmlPage += "      updateElement('remoteTriggerState', data.remoteTriggerState, 'trigger');";
    htmlPage += "      updateElement('localTriggerState1', data.localTriggerState1, 'trigger');";
    htmlPage += "      updateElement('localTriggerState2', data.localTriggerState2, 'trigger');";
    htmlPage += "      updateElement('resetTimer', data.resetTimer, 'number');";
    htmlPage += "      updateElement('resetLimit', data.resetLimit, 'number');";
    htmlPage += "      updateElement('fireTimer', data.fireTimer, 'number');";
    htmlPage += "      updateElement('fireTimeLimit', data.fireTimeLimit, 'number');";
    htmlPage += "      updateElement('accel1', data.accel1, 'number');";
    htmlPage += "      updateElement('accel2', data.accel2, 'number');";
    htmlPage += "      updateElement('aveGyro', data.aveGyro, 'number');";
    htmlPage += "    })";
    htmlPage += "    .catch(error => console.error('Error:', error));";
    htmlPage += "}";

    // Initialize updates with requestAnimationFrame for better performance
    htmlPage += "function startUpdates() {";
    htmlPage += "  updateData();";
    htmlPage += "  requestAnimationFrame(startUpdates);";
    htmlPage += "}";

    htmlPage += "document.addEventListener('DOMContentLoaded', startUpdates);";
    htmlPage += "</script>";
    htmlPage += "</HEAD><BODY>";

    // Title
    htmlPage += "<h2>&#128293Live Data&#128293</h2>";

    // Fire Pin Status Section
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Fire Pin Status</div>";
    htmlPage += "<div class='data-row'>Fire Pin 1<br><span id='pin1' class='status-indicator'></span></div>";
    htmlPage += "<div class='data-row'>Fire Pin 2<br><span id='pin2' class='status-indicator'></span></div>";
    htmlPage += "</div>";

    // Timer Status Section
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Timer Status</div>";
    htmlPage += "<div class='data-row'>Reset Timer [sec]<br><span class='value' id='resetTimer'>0.00</span> / <span class='value' id='resetLimit'>0.00</span></div>";
    htmlPage += "<div class='data-row'>Fire Timer [sec]<br><span class='value' id='fireTimer'>0.00</span> / <span class='value' id='fireTimeLimit'>0.00</span></div>";
    htmlPage += "</div>";

    // System State Section
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>System State</div>";
    htmlPage += "<div class='data-row'>Reset State<br><span id='resetState'>0</span></div>";
    htmlPage += "<div class='data-row'>Fire State<br><span id='fireOn'>0</span></div>";
    htmlPage += "</div>";

    // Trigger States Section
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Trigger States</div>";
    htmlPage += "<div class='data-row'>Remote Trigger<br><span id='remoteTriggerState'>0</span></div>";
    htmlPage += "<div class='data-row'>Local Trigger 1<br><span id='localTriggerState1'>0</span></div>";
    htmlPage += "<div class='data-row'>Local Trigger 2<br><span id='localTriggerState2'>0</span></div>";
    htmlPage += "</div>";

    // Sensor Readings Section
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Sensor Readings</div>";
    htmlPage += "<div class='data-row'>Accelerometer 1 [g]<br><span class='value' id='accel1'>0.00</span></div>";
    htmlPage += "<div class='data-row'>Accelerometer 2 [g]<br><span class='value' id='accel2'>0.00</span></div>";
    htmlPage += "<div class='data-row'>Gyro [deg/sec]<br><span class='value' id='aveGyro'>0.00</span></div>";
    htmlPage += "</div>";

    // Navigation Links
    htmlPage += "<br>";
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-row'><a href='/'>Root Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/settings'>Settings Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/fire'>Fire Control Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/data'>Refresh Data Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/stats'>Statistics Page</a></div>";
    htmlPage += "</div>";

    htmlPage += "</BODY></HTML>";
    
    return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Fire_Control_Page() {
    String htmlPage;
    htmlPage.reserve(4096);
    
    htmlPage += "<!DOCTYPE HTML><HTML><HEAD>";
    htmlPage += "<TITLE>Fire Control Panel</TITLE>";
    htmlPage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    htmlPage += "<meta charset='UTF-8'>";
    htmlPage += "<style>";
    // Base styling
    htmlPage += "body { font-size:200%; background-color:black; color:white; padding: 20px; text-align: center; }";
    htmlPage += ".data-box { background-color:#333; padding:15px; margin:10px; border-radius:5px; }";
    htmlPage += ".data-title { color:#4CAF50; margin-bottom:10px; font-size:120%; }";
    htmlPage += ".data-row { margin:8px 0; }";
    htmlPage += ".value { color:#FFA500; }";
    htmlPage += "a { color:#4CAF50; text-decoration:none; }";
    htmlPage += "a:hover { color:#45a049; }";
    
    // Fire button styling
    htmlPage += ".fire-button {";
    htmlPage += "  width: 80vw;";
    htmlPage += "  height: 80vw;";
    htmlPage += "  max-width: 400px;";
    htmlPage += "  max-height: 400px;";
    htmlPage += "  border-radius: 50%;";
    htmlPage += "  border: none;";
    htmlPage += "  font-size: 48px;";
    htmlPage += "  font-weight: bold;";
    htmlPage += "  margin: 20px auto;";
    htmlPage += "  cursor: pointer;";
    htmlPage += "  transition: all 0.3s ease;";
    htmlPage += "  box-shadow: 0 0 20px rgba(255,0,0,0.5);";
    htmlPage += "}";
    
    // Status indicators
    htmlPage += ".status-indicator {";
    htmlPage += "  display: inline-block;";
    htmlPage += "  width: 20px;";
    htmlPage += "  height: 20px;";
    htmlPage += "  border-radius: 50%;";
    htmlPage += "  margin: 0 10px;";
    htmlPage += "}";
    
    // Progress bar
    htmlPage += ".progress-container {";
    htmlPage += "  width: 100%;";
    htmlPage += "  background-color: #444;";
    htmlPage += "  border-radius: 10px;";
    htmlPage += "  margin: 10px 0;";
    htmlPage += "}";
    htmlPage += ".progress-bar {";
    htmlPage += "  width: 0%;";
    htmlPage += "  height: 20px;";
    htmlPage += "  background-color: #4CAF50;";
    htmlPage += "  border-radius: 10px;";
    htmlPage += "  transition: width 0.3s ease;";
    htmlPage += "}";
    htmlPage += "</style>";

    // JavaScript for dynamic updates
    htmlPage += "<script>";
    htmlPage += "function updateStatus() {";
    htmlPage += "  fetch('/fire/status')";
    htmlPage += "  .then(response => response.json())";
    htmlPage += "  .then(data => {";
    htmlPage += "    const btn = document.getElementById('fireButton');";
    htmlPage += "    const resetProgress = (data.resetTimer / data.resetLimit) * 100;";
    htmlPage += "    const fireProgress = (data.fireTimer / data.fireTimeLimit) * 100;";
    
    // Update button state
    htmlPage += "    if (data.fireOn) {";
    htmlPage += "      btn.style.backgroundColor = '#ff4444';";
    htmlPage += "      btn.innerHTML = 'FIRING';";
    htmlPage += "      btn.disabled = true;";
    htmlPage += "    } else if (data.resetTimer < data.resetLimit) {";
    htmlPage += "      btn.style.backgroundColor = '#666';";
    htmlPage += "      btn.innerHTML = 'RESETTING';";
    htmlPage += "      btn.disabled = true;";
    htmlPage += "    } else {";
    htmlPage += "      btn.style.backgroundColor = '#4CAF50';";
    htmlPage += "      btn.innerHTML = 'FIRE!';";
    htmlPage += "      btn.disabled = false;";
    htmlPage += "    }";
    
    // Update timers and progress bars
    htmlPage += "    document.getElementById('resetTimer').textContent = data.resetTimer.toFixed(2);";
    htmlPage += "    document.getElementById('resetLimit').textContent = data.resetLimit.toFixed(2);";
    htmlPage += "    document.getElementById('fireTimer').textContent = data.fireTimer.toFixed(2);";
    htmlPage += "    document.getElementById('fireTimeLimit').textContent = data.fireTimeLimit.toFixed(2);";
    htmlPage += "    document.getElementById('resetProgress').style.width = resetProgress + '%';";
    htmlPage += "    document.getElementById('fireProgress').style.width = fireProgress + '%';";
    
    // Update pin status indicators
    htmlPage += "    document.getElementById('pin1').style.backgroundColor = data.firePin1 ? '#ff4444' : '#4CAF50';";
    htmlPage += "    document.getElementById('pin2').style.backgroundColor = data.firePin2 ? '#ff4444' : '#4CAF50';";
    htmlPage += "  });";
    htmlPage += "}";
    
    // Fire trigger function
    htmlPage += "function fireTrigger() {";
    htmlPage += "  const btn = document.getElementById('fireButton');";
    htmlPage += "  btn.disabled = true;";
    htmlPage += "  fetch('/fire/on')";
    htmlPage += "    .then(response => {";
    htmlPage += "      if (!response.ok) btn.disabled = false;";
    htmlPage += "    });";
    htmlPage += "  return false;";
    htmlPage += "}";
    
    // Initialize updates
    htmlPage += "document.addEventListener('DOMContentLoaded', function() {";
    htmlPage += "  updateStatus();";
    htmlPage += "  setInterval(updateStatus, 50);";  // Changed from 100 to 50ms for faster updates
    htmlPage += "});";
    htmlPage += "</script>";
    htmlPage += "</HEAD><BODY>";

    // Main fire control button
    htmlPage += "<button id='fireButton' class='fire-button' onclick='return fireTrigger();'>FIRE!</button>";

    // Status boxes
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>System Status</div>";
    htmlPage += "<div class='data-row'>Fire Pin 1 <span id='pin1' class='status-indicator'></span></div>";
    htmlPage += "<div class='data-row'>Fire Pin 2 <span id='pin2' class='status-indicator'></span></div>";
    htmlPage += "</div>";

    // Timer box with progress bars
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Timers</div>";
    htmlPage += "<div class='data-row'>Fire Progress: <span class='value'><span id='fireTimer'>0.00</span>/<span id='fireTimeLimit'>0.00</span></span></div>";
    htmlPage += "<div class='progress-container'><div id='fireProgress' class='progress-bar'></div></div>";
    htmlPage += "<div class='data-row'>Reset Progress: <span class='value'><span id='resetTimer'>0.00</span>/<span id='resetLimit'>0.00</span></span></div>";
    htmlPage += "<div class='progress-container'><div id='resetProgress' class='progress-bar'></div></div>";
    htmlPage += "</div>";

    // Navigation links
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-row'><a href='/'>Root Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/settings'>Settings Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/data'>Data Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/stats'>Statistics Page</a></div>";
    htmlPage += "</div>";

    htmlPage += "</BODY></HTML>";
    return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Fire_Settings_Page() {
    String htmlPage;
    htmlPage.reserve(4096);  // Pre-allocate memory
    
    htmlPage += "<!DOCTYPE HTML><HTML><HEAD>";
    htmlPage += "<TITLE>Saloon Doors Settings</TITLE>";
    htmlPage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    htmlPage += "<meta charset='UTF-8'>";
    htmlPage += "<style>";
    // Base styling to match other pages
    htmlPage += "body { font-size:200%; background-color:black; color:white; padding: 20px; }";
    htmlPage += ".data-box { background-color:#333; padding:15px; margin:10px; border-radius:5px; }";
    htmlPage += ".data-title { color:#4CAF50; margin-bottom:10px; font-size:120%; }";
    htmlPage += ".data-row { margin:8px 0; }";
    htmlPage += ".value { color:#FFA500; }";
    htmlPage += "a { color:#4CAF50; text-decoration:none; }";
    htmlPage += "a:hover { color:#45a049; }";
    
    // Input styling
    htmlPage += "input[type='number'] {";
    htmlPage += "  width: 120px;";
    htmlPage += "  padding: 8px;";
    htmlPage += "  margin: 5px 0;";
    htmlPage += "  border: 2px solid #4CAF50;";
    htmlPage += "  border-radius: 4px;";
    htmlPage += "  background-color: #222;";
    htmlPage += "  color: #FFA500;";
    htmlPage += "  font-size: 100%;";
    htmlPage += "}";
    
    // Button styling
    htmlPage += ".action-button {";
    htmlPage += "  background-color: #4CAF50;";
    htmlPage += "  color: white;";
    htmlPage += "  padding: 15px 32px;";
    htmlPage += "  border: none;";
    htmlPage += "  border-radius: 4px;";
    htmlPage += "  font-size: 100%;";
    htmlPage += "  cursor: pointer;";
    htmlPage += "  margin: 10px 0;";
    htmlPage += "  width: 100%;";
    htmlPage += "  transition: background-color 0.3s;";
    htmlPage += "}";
    htmlPage += ".reset-button { background-color: #ff4444; }";
    htmlPage += ".action-button:hover { filter: brightness(110%); }";
    htmlPage += ".unit { color: #888; font-size: 80%; }";
    
    // Form validation and submission
    htmlPage += "</style><script>";
    htmlPage += "function validateForm() {";
    htmlPage += "  const inputs = document.querySelectorAll('input[type=\"number\"]');";
    htmlPage += "  let valid = true;";
    htmlPage += "  inputs.forEach(input => {";
    htmlPage += "    const value = parseFloat(input.value);";
    htmlPage += "    if (isNaN(value) || value < 0) {";
    htmlPage += "      alert('All values must be positive numbers');";
    htmlPage += "      input.focus();";
    htmlPage += "      valid = false;";
    htmlPage += "    }";
    htmlPage += "  });";
    htmlPage += "  const minFire = parseFloat(document.getElementsByName('MIN_FIRE_TIME')[0].value);";
    htmlPage += "  const maxFire = parseFloat(document.getElementsByName('MAX_FIRE_TIME')[0].value);";
    htmlPage += "  const minGyro = parseFloat(document.getElementsByName('MIN_GYRO')[0].value);";
    htmlPage += "  const maxGyro = parseFloat(document.getElementsByName('MAX_GYRO')[0].value);";
    htmlPage += "  if (minFire >= maxFire) {";
    htmlPage += "    alert('Maximum fire time must be greater than minimum fire time');";
    htmlPage += "    valid = false;";
    htmlPage += "  }";
    htmlPage += "  if (minGyro >= maxGyro) {";
    htmlPage += "    alert('Maximum gyro must be greater than minimum gyro');";
    htmlPage += "    valid = false;";
    htmlPage += "  }";
    htmlPage += "  return valid;";
    htmlPage += "}";

    // Add this new function for submitting the form via AJAX
    htmlPage += "function submitForm(event) {";
    htmlPage += "  event.preventDefault();";  // Prevent form from submitting normally
    htmlPage += "  if (!validateForm()) return false;";
    htmlPage += "  const formData = new FormData(document.getElementById('settingsForm'));";
    htmlPage += "  fetch('/settings/action_page', {";
    htmlPage += "    method: 'POST',";
    htmlPage += "    body: formData";
    htmlPage += "  })";
    htmlPage += "  .then(response => {";
    htmlPage += "    if (!response.ok) throw new Error('Network response was not ok');";
    htmlPage += "    return response.text();";
    htmlPage += "  })";
    htmlPage += "  .then(data => {";
    htmlPage += "    alert('Settings updated successfully');";  // Show popup
    htmlPage += "  })";
    htmlPage += "  .catch(error => {";
    htmlPage += "    alert('Error updating settings: ' + error.message);";
    htmlPage += "  });";
    htmlPage += "  return false;";  // Prevent form submission
    htmlPage += "}";

    // Add resetDefaults function if not already present
    htmlPage += "function resetDefaults() {";
    htmlPage += "  if (confirm('Reset all settings to defaults?')) {";
    htmlPage += "    fetch('/settings/reset', { method: 'POST' })";
    htmlPage += "    .then(response => {";
    htmlPage += "      if (!response.ok) throw new Error('Network response was not ok');";
    htmlPage += "      return response.text();";
    htmlPage += "    })";
    htmlPage += "    .then(data => {";
    htmlPage += "      alert('Settings reset to defaults');";
    htmlPage += "      location.reload();";  // Reload page to show new values
    htmlPage += "    })";
    htmlPage += "    .catch(error => {";
    htmlPage += "      alert('Error resetting settings: ' + error.message);";
    htmlPage += "    });";
    htmlPage += "  }";
    htmlPage += "}";
    htmlPage += "</script></HEAD><BODY>";

    // Title
    htmlPage += "<h2>&#128293Settings&#128293</h2>";

    // Update the form tag to use onsubmit
    htmlPage += "<form id='settingsForm' onsubmit='return submitForm(event)'>";

    // Motion Detection Settings
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Motion Detection</div>";
    htmlPage += "<div class='data-row'>Minimum Gyro <span class='unit'>[deg/sec]</span><br>";
    htmlPage += "<input type='number' name='MIN_GYRO' value='" + String(state.minGyro, 2) + "' step='0.01' min='0' required></div>";
    htmlPage += "<div class='data-row'>Maximum Gyro <span class='unit'>[deg/sec]</span><br>";
    htmlPage += "<input type='number' name='MAX_GYRO' value='" + String(state.maxGyro, 2) + "' step='0.01' min='0' required></div>";
    htmlPage += "</div>";

    // Fire Duration Settings
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Fire Duration</div>";
    htmlPage += "<div class='data-row'>Minimum Fire Time <span class='unit'>[sec]</span><br>";
    htmlPage += "<input type='number' name='MIN_FIRE_TIME' value='" + String(state.minFireTime, 2) + "' step='0.01' min='0' required></div>";
    htmlPage += "<div class='data-row'>Maximum Fire Time <span class='unit'>[sec]</span><br>";
    htmlPage += "<input type='number' name='MAX_FIRE_TIME' value='" + String(state.maxFireTime, 2) + "' step='0.01' min='0' required></div>";
    htmlPage += "<div class='data-row'>Remote Fire Time <span class='unit'>[sec]</span><br>";
    htmlPage += "<input type='number' name='REMOTE_FIRE_TIME' value='" + String(state.remoteFireTime, 2) + "' step='0.01' min='0' required></div>";
    htmlPage += "</div>";

    // System Timing Settings
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>System Timing</div>";
    htmlPage += "<div class='data-row'>Reset Time Limit <span class='unit'>[sec]</span><br>";
    htmlPage += "<input type='number' name='RESET_LIMIT' value='" + String(state.resetLimit, 2) + "' step='0.01' min='0' required></div>";
    htmlPage += "<div class='data-row'>Fire Cycle Time <span class='unit'>[sec]</span><br>";
    htmlPage += "<input type='number' name='FIRE_CYCLE' value='" + String(state.fireCycle, 2) + "' step='0.01' min='0' required></div>";
    htmlPage += "</div>";

    // Action Buttons
    htmlPage += "<div class='data-box'>";
    htmlPage += "<input type='submit' class='action-button' value='Update Settings'>";
    htmlPage += "<button type='button' class='action-button reset-button' onclick='resetDefaults()'>Reset to Defaults</button>";
    htmlPage += "</div>";

    htmlPage += "</form>";

    // Navigation Links
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-row'><a href='/'>Root Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/fire'>Fire Control Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/data'>Data Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/stats'>Statistics Page</a></div>";
    htmlPage += "</div>";

    htmlPage += "</BODY></HTML>";
    return htmlPage;
}

// Add new page preparation function
String prepare_Stats_Page() {
    String htmlPage;
    htmlPage.reserve(2048);
    
    htmlPage += "<!DOCTYPE HTML><HTML><HEAD>";
    htmlPage += "<TITLE>System Statistics</TITLE>";
    htmlPage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    htmlPage += "<meta charset='UTF-8'>";
    htmlPage += "<style>";
    // Match the data page styling
    htmlPage += "body { font-size:200%; background-color:black; color:white; padding: 20px; }";
    htmlPage += ".data-box { background-color:#333; padding:15px; margin:10px; border-radius:5px; }";
    htmlPage += ".data-title { color:#4CAF50; margin-bottom:10px; font-size:120%; }";
    htmlPage += ".data-row { margin:8px 0; }";
    htmlPage += ".value { color:#FFA500; }";  // Orange color for values
    htmlPage += "a { color:#4CAF50; text-decoration:none; }";
    htmlPage += "a:hover { color:#45a049; }";
    htmlPage += ".reset-btn {";
    htmlPage += "  background-color:#ff4444;";
    htmlPage += "  color:white;";
    htmlPage += "  padding:15px 32px;";
    htmlPage += "  border:none;";
    htmlPage += "  border-radius:5px;";
    htmlPage += "  font-size:100%;";
    htmlPage += "  cursor:pointer;";
    htmlPage += "  margin:20px 0;";
    htmlPage += "  transition: background-color 0.3s;";
    htmlPage += "}";
    htmlPage += ".reset-btn:hover { background-color:#ff0000; }";
    htmlPage += "</style>";
    
    // Add JavaScript for confirmation dialog
    htmlPage += "<script>";
    htmlPage += "function resetStats() {";
    htmlPage += "  if(confirm('Reset all statistics to zero?')) {";
    htmlPage += "    fetch('/stats/reset', {method:'POST'})";
    htmlPage += "    .then(response => {";
    htmlPage += "      if(response.ok) {";
    htmlPage += "        alert('Statistics reset successfully');";
    htmlPage += "        location.reload();";
    htmlPage += "      }";
    htmlPage += "    });";
    htmlPage += "  }";
    htmlPage += "}";
    htmlPage += "</script>";
    htmlPage += "</HEAD><BODY>";
    
    htmlPage += "<h2>&#128293 Statistics &#128293</h2>";
    
    // Trigger Statistics Box
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Trigger Statistics</div>";
    htmlPage += "<div class='data-row'>Remote Triggers<br><span class='value'>" + String(state.currentStats.remoteTriggersCount) + "</span></div>";
    htmlPage += "<div class='data-row'>Accelerometer Triggers<br><span class='value'>" + String(state.currentStats.accelTriggersCount) + "</span></div>";
    htmlPage += "</div>";
    
    // Motion Data Box
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Motion Data</div>";
    htmlPage += "<div class='data-row'>Highest Angular Acceleration<br><span class='value'>" + String(state.currentStats.highestGyroReading, 1) + " deg/s</span></div>";
    htmlPage += "<div class='data-row'>Highest Linear Acceleration<br><span class='value'>" + String(state.currentStats.highestAccelReading, 2) + " g</span></div>";
    htmlPage += "<div class='data-row'>Average Angular Trigger<br><span class='value'>" + String(state.currentStats.averageAccelTrigger, 1) + " deg/s</span></div>";
    htmlPage += "</div>";
    
    // Fire Duration Box
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>Fire Duration</div>";
    
    // Convert total fire time to hours:minutes:seconds
    unsigned long totalSeconds = (unsigned long)state.currentStats.totalFireTime;
    unsigned long hours = totalSeconds / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;
    
    htmlPage += "<div class='data-row'>Total Fire Time<br><span class='value'>" + 
                String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s</span></div>";
    htmlPage += "<div class='data-row'>Longest Single Fire<br><span class='value'>" + 
                String(state.currentStats.longestFireDuration, 1) + " seconds</span></div>";
    htmlPage += "</div>";
    
    // System Resources Box
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-title'>System Resources</div>";
    htmlPage += "<div class='data-row'>Peak Memory Usage<br><span class='value'>" + 
                String(state.currentStats.peakMemoryUsage, 1) + "%</span></div>";
    htmlPage += "</div>";
    
    // Reset Button and Navigation
    htmlPage += "<button class='reset-btn' onclick='resetStats()'>Reset Statistics</button>";
    
    // Navigation Links Box
    htmlPage += "<div class='data-box'>";
    htmlPage += "<div class='data-row'><a href='/'>Root Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/settings'>Settings Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/fire'>Fire Control Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/data'>Data Page</a></div>";
    htmlPage += "<div class='data-row'><a href='/stats'>Refresh Statistics</a></div>";
    htmlPage += "</div>";
    
    htmlPage += "</BODY></HTML>";
    
    return htmlPage;
}

// Add these handler functions before they're used in server.on()

void handle_Stats_Reset() {
    loadDefaultStats();
    if (!saveStats()) {
        state.server.send(500, "text/plain", "Failed to reset statistics");
        return;
    }
    state.server.send(200, "text/plain", "Statistics reset successfully");
}

void handle_Stats_Page() {
    state.server.send(200, "text/html", prepare_Stats_Page());
}

//===============================================================
// This routine is executed when you open its IP in browser
//===============================================================
void handleRoot() {
    state.server.sendHeader("Cache-Control", "max-age=3600");
    state.server.send(200, "text/html", prepare_Root_Page());
}

//===============================================================
// This routine is executed when you open the fire control page
//===============================================================
void handle_Fire_Control_Page() {
  state.server.send(200, "text/html", prepare_Fire_Control_Page());
}

//===============================================================
// This routine is executed when you trigger the FIRE on the control page
//===============================================================
void handle_Fire_Control_ON_Page() {
    static unsigned long lastFireTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastFireTime < 1000) {
        state.server.send(429, "text/plain", "Too many requests");
        return;
    }
    
    startFire();
    state.fireTimeLimit = state.remoteFireTime;
    state.fireTimer = 0;
    state.resetTimer = 0;
    state.resetState = 0;
    
    lastFireTime = currentTime;
    state.server.send(200, "text/plain", "OK");
}

//===============================================================
// This routine is executed when you open the fire settings page
//===============================================================
void handle_Fire_Settings_Page() {
  state.server.send(200, "text/html", prepare_Fire_Settings_Page());
}

//===============================================================
// This routine is executed when you open the data page
//===============================================================
void handle_Data_Page() {
  state.server.send(200, "text/html", prepare_Data_Page());
}

//===============================================================
// This routine is executed when you press submit
//===============================================================
void handleForm() {
    Serial.println("Form handler called"); // Debug print
    
    // Print all received arguments for debugging
    for (int i = 0; i < state.server.args(); i++) {
        Serial.printf("Arg %d: %s = %s\n", i, state.server.argName(i).c_str(), state.server.arg(i).c_str());
    }

    // Remove the check for "plain" data since we're handling form data
    if (state.server.args() == 0) {
        state.server.send(400, "text/plain", "No data received");
        return;
    }

    // Rest of the function remains the same...
}

void handleResetDefaults() {
    loadDefaultSettings();
    saveSettings();
    state.server.send(200, "text/html", prepare_Fire_Settings_Page());
}

void handle_Data_Status() {
    static char json[512];  // Make static to avoid stack fragmentation
    snprintf(json, sizeof(json),
        "{"
        "\"resetTimer\":%.2f,"
        "\"resetLimit\":%.2f,"
        "\"resetState\":%d,"
        "\"fireTimer\":%.2f,"
        "\"fireTimeLimit\":%.2f,"
        "\"remoteTriggerState\":%d,"
        "\"localTriggerState1\":%d,"
        "\"localTriggerState2\":%d,"
        "\"fireOn\":%d,"
        "\"accel1\":%.3f,"
        "\"accel2\":%.3f,"
        "\"aveGyro\":%.2f,"
        "\"firePin1\":%d,"
        "\"firePin2\":%d"
        "}",
        state.resetTimer,
        state.resetLimit,
        state.resetState,
        state.fireTimer,
        state.fireTimeLimit,
        state.remoteTriggerState,
        state.localTriggerState1,
        state.localTriggerState2,
        state.fireOn,
        state.accel1[7],
        state.accel2[7],
        state.aveGyro,
        state.firePinState1,
        state.firePinState2
    );
    
    state.server.send(200, "application/json", json);
}

void handle_Settings_Update() {
    Serial.println("Form handler called"); // Keep debug print
    
    if (state.server.args() == 0) {
        Serial.println("No data received"); // Keep debug print
        state.server.send(400, "text/plain", "No data received");
        return;
    }

    // Print all received arguments for debugging
    for (int i = 0; i < state.server.args(); i++) {
        Serial.printf("Arg %d: %s = %s\n", i, state.server.argName(i).c_str(), state.server.arg(i).c_str());
    }

    // Parse form data directly from arguments
    float newMinFireTime = state.server.arg("MIN_FIRE_TIME").toFloat();
    float newMaxFireTime = state.server.arg("MAX_FIRE_TIME").toFloat();
    float newResetLimit = state.server.arg("RESET_LIMIT").toFloat();
    float newRemoteFireTime = state.server.arg("REMOTE_FIRE_TIME").toFloat();
    float newFireCycle = state.server.arg("FIRE_CYCLE").toFloat();
    float newMinGyro = state.server.arg("MIN_GYRO").toFloat();
    float newMaxGyro = state.server.arg("MAX_GYRO").toFloat();

    // Validate input values
    if (newMinFireTime <= 0 || newMaxFireTime <= 0 || 
        newResetLimit <= 0 || newRemoteFireTime <= 0 || 
        newFireCycle < 0 || newMinFireTime >= newMaxFireTime ||
        newMinGyro <= 0 || newMaxGyro <= 0 || 
        newMinGyro >= newMaxGyro || newMaxGyro > 2000) {
        Serial.println(F("Settings update rejected - Invalid values")); // Keep debug print
        state.server.send(400, "text/plain", "Invalid values provided");
        return;
    }
    
    // Log changes to serial
    Serial.println(F("\n=== Settings Update ==="));
    if (state.currentSettings.minFireTime != newMinFireTime)
        Serial.printf("Min Fire Time: %.2f -> %.2f\n", state.currentSettings.minFireTime, newMinFireTime);
    if (state.currentSettings.maxFireTime != newMaxFireTime)
        Serial.printf("Max Fire Time: %.2f -> %.2f\n", state.currentSettings.maxFireTime, newMaxFireTime);
    if (state.currentSettings.remoteFireTime != newRemoteFireTime)
        Serial.printf("Remote Fire Time: %.2f -> %.2f\n", state.currentSettings.remoteFireTime, newRemoteFireTime);
    if (state.currentSettings.resetLimit != newResetLimit)
        Serial.printf("Reset Limit: %.2f -> %.2f\n", state.currentSettings.resetLimit, newResetLimit);
    if (state.currentSettings.fireCycle != newFireCycle)
        Serial.printf("Fire Cycle: %.2f -> %.2f\n", state.currentSettings.fireCycle, newFireCycle);
    if (state.currentSettings.minGyro != newMinGyro)
        Serial.printf("Min Gyro: %.2f -> %.2f\n", state.currentSettings.minGyro, newMinGyro);
    if (state.currentSettings.maxGyro != newMaxGyro)
        Serial.printf("Max Gyro: %.2f -> %.2f\n", state.currentSettings.maxGyro, newMaxGyro);
    
    // Update settings
    state.currentSettings.minFireTime = newMinFireTime;
    state.currentSettings.maxFireTime = newMaxFireTime;
    state.currentSettings.remoteFireTime = newRemoteFireTime;
    state.currentSettings.resetLimit = newResetLimit;
    state.currentSettings.fireCycle = newFireCycle;
    state.currentSettings.minGyro = newMinGyro;
    state.currentSettings.maxGyro = newMaxGyro;
    
    // Update global variables
    state.minFireTime = newMinFireTime;
    state.maxFireTime = newMaxFireTime;
    state.resetLimit = newResetLimit;
    state.remoteFireTime = newRemoteFireTime;
    state.fireCycle = newFireCycle;
    state.minGyro = newMinGyro;
    state.maxGyro = newMaxGyro;
    
    // Save to EEPROM
    if (!saveSettings()) {
        Serial.println(F("Failed to save settings to EEPROM")); // Keep debug print
        state.server.send(500, "text/plain", "Failed to save settings");
        return;
    }
    
    Serial.println(F("Settings updated and saved successfully"));
    Serial.println(F("=====================\n"));
    
    // Send success response instead of redirecting
    state.server.send(200, "text/plain", "Settings updated successfully");
}

void handle_Settings_Reset() {
    // Store old settings for comparison
    FireSettings oldSettings = state.currentSettings;
    
    loadDefaultSettings();
    if (!saveSettings()) {
        state.server.send(500, "text/plain", "Failed to reset settings");
        return;
    }

    // Log changes to serial
    Serial.println(F("\n=== Settings Reset to Defaults ==="));
    if (oldSettings.minFireTime != state.currentSettings.minFireTime)
        Serial.printf("Min Fire Time: %.2f -> %.2f\n", oldSettings.minFireTime, state.currentSettings.minFireTime);
    if (oldSettings.maxFireTime != state.currentSettings.maxFireTime)
        Serial.printf("Max Fire Time: %.2f -> %.2f\n", oldSettings.maxFireTime, state.currentSettings.maxFireTime);
    if (oldSettings.remoteFireTime != state.currentSettings.remoteFireTime)
        Serial.printf("Remote Fire Time: %.2f -> %.2f\n", oldSettings.remoteFireTime, state.currentSettings.remoteFireTime);
    if (oldSettings.resetLimit != state.currentSettings.resetLimit)
        Serial.printf("Reset Limit: %.2f -> %.2f\n", oldSettings.resetLimit, state.currentSettings.resetLimit);
    if (oldSettings.fireCycle != state.currentSettings.fireCycle)
        Serial.printf("Fire Cycle: %.2f -> %.2f\n", oldSettings.fireCycle, state.currentSettings.fireCycle);
    if (oldSettings.minGyro != state.currentSettings.minGyro)
        Serial.printf("Min Gyro: %.2f -> %.2f\n", oldSettings.minGyro, state.currentSettings.minGyro);
    if (oldSettings.maxGyro != state.currentSettings.maxGyro)
        Serial.printf("Max Gyro: %.2f -> %.2f\n", oldSettings.maxGyro, state.currentSettings.maxGyro);
    Serial.println(F("=====================\n"));

    state.server.send(200, "text/plain", "Settings reset to defaults");
}

void handle_Fire_Status() {
    static char json[256];  // Make static to avoid stack fragmentation
    snprintf(json, sizeof(json),
        "{"
        "\"fireOn\":%d,"
        "\"resetTimer\":%.2f,"
        "\"resetLimit\":%.2f,"
        "\"fireTimer\":%.2f,"
        "\"fireTimeLimit\":%.2f,"
        "\"firePin1\":%d,"
        "\"firePin2\":%d"
        "}",
        state.fireOn,
        state.resetTimer,
        state.resetLimit,
        state.fireTimer,
        state.fireTimeLimit,
        state.firePinState1,
        state.firePinState2
    );
    
    state.server.send(200, "application/json", json);
}

// Add these constants with other defines
#define DNS_PORT 53


void handleNotFound() {
    String ip = WiFi.softAPIP().toString();
    String url = "http://" + ip;
    String html = String("") +
        "<!DOCTYPE html>" +
        "<html>" +
        "<head>" +
        "<meta name='viewport' content='width=device-width, initial-scale=1'>" +
        "<style>" +
        "body { font-family: Arial, sans-serif; text-align: center; padding: 20px; background-color: black; color: white; }" +
        ".button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px; " +
        "text-align: center; text-decoration: none; display: inline-block; font-size: 16px; " +
        "margin: 4px 2px; cursor: pointer; border-radius: 4px; }" +
        ".info { background-color: #333; padding: 15px; border-radius: 4px; margin: 20px auto; max-width: 600px; }" +
        "</style>" +
        "</head>" +
        "<body>" +
        "<h1>&#128293; HighNoon Saloon Doors &#128293;</h1>" +
        "<div class='info'>" +
        "<p>To access the control panel, either:</p>" +
        "<p>1. Click the button below</p>" +
        "<p>2. Or open your browser and go to: <br><strong>" + url + "</strong></p>" +
        "</div>" +
        "<a class='button' href='" + url + "'>Open Control Panel</a>" +
        "<p style='margin-top: 20px; color: #888;'><small>Bookmark the control panel for easy access!</small></p>" +
        "</body>" +
        "</html>";
    
    state.server.send(200, "text/html", html);
}

void start_wifi(){
    // Initialize WiFi in Access Point mode and create captive portal
    WiFi.mode(WIFI_AP);
    WiFi.softAP("HighNoon", "shaboinky", 1, 0, 8);
    
    // Configure DNS server to redirect all domains to our IP
    IPAddress apIP = WiFi.softAPIP();
    state.dnsServer.start(DNS_PORT, "*", apIP);
    
    // Add WiFi power management
    WiFi.setOutputPower(20.5); // Max WiFi power
    WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable WiFi sleep mode
    
    // Increase the TCP MSS for better performance
    WiFi.setPhyMode(WIFI_PHY_MODE_11N); // Use 802.11n mode
    
    delay(500);
    
    // Add CORS headers to your request handlers
    state.server.onNotFound([]() {
        state.server.sendHeader("Access-Control-Allow-Origin", "*");
        handleNotFound();
    });

    // Add special URLs that mobile devices check for captive portals
    state.server.on("/generate_204", handleNotFound);  // Android
    state.server.on("/fwlink", handleNotFound);        // Microsoft
    state.server.on("/redirect", handleNotFound);      // Apple
    state.server.on("/hotspot-detect.html", handleNotFound); // Apple
    state.server.on("/canonical.html", handleNotFound); // Apple
    state.server.on("/success.txt", handleNotFound);    // Apple
    
    // Your existing endpoints...
    state.server.on("/", HTTP_GET, []() {
        state.server.sendHeader("Access-Control-Allow-Origin", "*");
        handleRoot();
    });
    state.server.on("/fire", HTTP_GET, handle_Fire_Control_Page);
    state.server.on("/fire/on", HTTP_GET, handle_Fire_Control_ON_Page);
    state.server.on("/fire/status", HTTP_GET, handle_Fire_Status);
    state.server.on("/settings", HTTP_GET, handle_Fire_Settings_Page);
    state.server.on("/settings/action_page", HTTP_POST, handle_Settings_Update);
    state.server.on("/settings/reset", HTTP_POST, handle_Settings_Reset);
    state.server.on("/data", HTTP_GET, handle_Data_Page);
    state.server.on("/data/status", HTTP_GET, handle_Data_Status);
    state.server.on("/stats", HTTP_GET, []() {
        state.server.sendHeader("Access-Control-Allow-Origin", "*");
        handle_Stats_Page();
    });
    state.server.on("/stats/reset", HTTP_POST, []() {
        state.server.sendHeader("Access-Control-Allow-Origin", "*");
        handle_Stats_Reset();
    });

    state.server.begin();
    Serial.println("Web server started");
    Serial.print("IP: "); Serial.println(WiFi.softAPIP());
    Serial.print("MAC: "); Serial.println(WiFi.softAPmacAddress());
}

static_assert(sizeof(FireSettings) + sizeof(SystemStats) + EEPROMConfig::EEPROM_DATA_ADDR <= EEPROMConfig::EEPROM_SIZE, 
    "Combined settings and stats too large for EEPROM");

/**
 * This routine turns off the I2C bus and clears it
 * on return SCA and SCL pins are tri-state inputs.
 * You need to call Wire.begin() after this to re-enable I2C
 * This routine does NOT use the Wire library at all.
 *
 * returns 0 if bus cleared
 *         1 if SCL held low.
 *         2 if SDA held low by slave clock stretch for > 2sec
 *         3 if SDA held low after 20 clocks.
 */
int I2C_ClearBus() {
#if defined(TWCR) && defined(TWEN)
  TWCR &= ~(_BV(TWEN)); //Disable the Atmel 2-Wire interface so we can control the SDA and SCL pins directly
#endif

  pinMode(SDA, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
  pinMode(SCL, INPUT_PULLUP);

  delay(2500);  // Wait 2.5 secs. This is strictly only necessary on the first power
  // up of the DS3231 module to allow it to initialize properly,
  // but is also assists in reliable programming of FioV3 boards as it gives the
  // IDE a chance to start uploaded the program
  // before existing sketch confuses the IDE by sending Serial data.

  boolean SCL_LOW = (digitalRead(SCL) == LOW); // Check is SCL is Low.
  if (SCL_LOW) { //If it is held low Arduno cannot become the I2C master. 
    return 1; //I2C bus error. Could not clear SCL clock line held low
  }

  boolean SDA_LOW = (digitalRead(SDA) == LOW);  // vi. Check SDA input.
  int clockCount = 20; // > 2x9 clock

  while (SDA_LOW && (clockCount > 0)) { //  vii. If SDA is Low,
    clockCount--;
  // Note: I2C bus is open collector so do NOT drive SCL or SDA high.
    pinMode(SCL, INPUT); // release SCL pullup so that when made output it will be LOW
    pinMode(SCL, OUTPUT); // then clock SCL Low
    delayMicroseconds(10); //  for >5us
    pinMode(SCL, INPUT); // release SCL LOW
    pinMode(SCL, INPUT_PULLUP); // turn on pullup resistors again
    // do not force high as slave may be holding it low for clock stretching.
    delayMicroseconds(10); //  for >5us
    // The >5us is so that even the slowest I2C devices are handled.
    SCL_LOW = (digitalRead(SCL) == LOW); // Check if SCL is Low.
    int counter = 20;
    while (SCL_LOW && (counter > 0)) {  //  loop waiting for SCL to become High only wait 2sec.
      counter--;
      delay(100);
      SCL_LOW = (digitalRead(SCL) == LOW);
    }
    if (SCL_LOW) { // still low after 2 sec error
      return 2; // I2C bus error. Could not clear. SCL clock line held low by slave clock stretch for >2sec
    }
    SDA_LOW = (digitalRead(SDA) == LOW); //   and check SDA input again and loop
  }
  if (SDA_LOW) { // still low
    return 3; // I2C bus error. Could not clear. SDA data line held low
  }

  // else pull SDA line low for Start or Repeated Start
  pinMode(SDA, INPUT); // remove pullup.
  pinMode(SDA, OUTPUT);  // and then make it LOW i.e. send an I2C Start or Repeated start control.
  // When there is only one I2C master a Start or Repeat Start has the same function as a Stop and clears the bus.
  /// A Repeat Start is a Start occurring after a Start with no intervening Stop.
  delayMicroseconds(10); // wait >5us
  pinMode(SDA, INPUT); // remove output low
  pinMode(SDA, INPUT_PULLUP); // and make SDA high i.e. send I2C STOP control.
  delayMicroseconds(10); // x. wait >5us
  pinMode(SDA, INPUT); // and reset pins as tri-state inputs which is the default state on reset
  pinMode(SCL, INPUT);
  return 0; // all ok
}

void start_I2C_communication(int MPU) {
    int rtn = I2C_ClearBus(); // clear the I2C bus first before calling Wire.begin()
    if (rtn != 0) {
        Serial.println(F("I2C bus error. Could not clear"));
        if (rtn == 1) {
            Serial.println(F("SCL clock line held low"));
        } else if (rtn == 2) {
            Serial.println(F("SCL clock line held low by slave clock stretch"));
        } else if (rtn == 3) {
            Serial.println(F("SDA data line held low"));
        }
    } else { // bus clear
        Wire.begin();
    }
    Serial.println("setup finished");

    Wire.beginTransmission(MPU);
    Wire.write(0x6B); 
    Wire.write(0);
    Wire.endTransmission(true);

    // Turn on Low Pass Filter:
    Wire.beginTransmission(MPU);  
    Wire.write(0x1A); 
    Wire.write(B00000011); // Level 3
    Wire.endTransmission(true);

    // Set gyro full scale Range to ±2000 deg/s
    Wire.beginTransmission(MPU);  
    Wire.write(0x1B); 
    Wire.write(B00011000);  // 0x18 = ±2000 deg/s
    state.gyroFactor = 16.4;  // Correct factor for ±2000 deg/s
    Wire.endTransmission(true);

    // Set accel full scale Range - pick ONE of these ranges:
    Wire.beginTransmission(MPU);  
    Wire.write(0x1C); 
    Wire.write(B00000000);
    state.accelFactor = 16384;  // +/- 2g
    //Wire.write(B00001000);
    //state.accelFactor = 8192;   // +/- 4g
    //Wire.write(B00010000);
    //state.accelFactor = 4096;   // +/- 8g
    //Wire.write(B00011000);
    //state.accelFactor = 2048;   // +/- 16g
    Wire.endTransmission(true);
}

void get_accel_data(int MPU, float output_array[9]) {
    Wire.beginTransmission(MPU);
    Wire.write(0x3B);  
    Wire.endTransmission(false);
    Wire.requestFrom(MPU,14,1);

    // Read raw values into temporary int16_t variables
    int16_t rawAccX = Wire.read()<<8|Wire.read();  // AcX
    int16_t rawAccY = Wire.read()<<8|Wire.read();  // AcY
    int16_t rawAccZ = Wire.read()<<8|Wire.read();  // AcZ
    int16_t rawTemp = Wire.read()<<8|Wire.read();  // Tmp
    int16_t rawGyroX = Wire.read()<<8|Wire.read(); // GyX
    int16_t rawGyroY = Wire.read()<<8|Wire.read(); // GyY
    int16_t rawGyroZ = Wire.read()<<8|Wire.read(); // GyZ

    // Convert and store values directly as floats
    output_array[0] = (float)rawAccX / state.accelFactor;
    output_array[1] = (float)rawAccY / state.accelFactor;
    output_array[2] = (float)rawAccZ / state.accelFactor;
    output_array[3] = (float)rawTemp / 340.0 + 36.53;
    output_array[4] = (float)rawGyroX / state.gyroFactor;
    output_array[5] = (float)rawGyroY / state.gyroFactor;
    output_array[6] = (float)rawGyroZ / state.gyroFactor;

    // Calculate and store magnitudes
    output_array[7] = sqrt(sq(output_array[0]) + sq(output_array[1]) + sq(output_array[2]));  // accel magnitude
    output_array[8] = sqrt(sq(output_array[4]) + sq(output_array[5]) + sq(output_array[6]));  // gyro magnitude
}

void setup() {
    // Add watchdog timer
    ESP.wdtEnable(WDTO_8S);
  
    // Initialize the serial port
    Serial.begin(9600);

    start_I2C_communication(state.MPU_1);
    start_I2C_communication(state.MPU_2);
    start_wifi();
 
    // Configure pins as outputs/inputs
    pinMode(state.FIRE_PIN_1, OUTPUT);
    pinMode(state.FIRE_PIN_2, OUTPUT);
    pinMode(state.MANUAL_TRIGGER_1, INPUT_PULLUP);
    pinMode(state.MANUAL_TRIGGER_2, INPUT_PULLUP);

    // Initialize EEPROM
    EEPROM.begin(EEPROMConfig::EEPROM_SIZE);
    loadSettings();
    loadStats();
}

void loop() {
    // Add at start of loop
    ESP.wdtFeed();  // Reset watchdog timer
    
    // Process network requests frequently
    static unsigned long lastNetworkUpdate = 0;
    static unsigned long lastUpdate = 0;
    static unsigned long lastFireCycleUpdate = 0;
    const unsigned long networkInterval = 500;  // 500 microseconds between network processing
    const unsigned long updateInterval = state.loopRate * 1000000; // Convert to microseconds
    
    unsigned long currentMicros = micros();
    
    // Handle timer rollovers for intervals
    if (currentMicros < lastNetworkUpdate) lastNetworkUpdate = 0;
    if (currentMicros < lastUpdate) lastUpdate = 0;
    if (currentMicros < lastFireCycleUpdate) lastFireCycleUpdate = 0;
    
    // Handle network with minimum interval
    if (currentMicros - lastNetworkUpdate >= networkInterval) {
        state.dnsServer.processNextRequest();
        state.server.handleClient();
        lastNetworkUpdate = currentMicros;
    }
    
    // Update sensors and main control at specified interval
    if (currentMicros - lastUpdate >= updateInterval) {
        lastUpdate = currentMicros;
        
        // Get sensor data
        get_accel_data(state.MPU_1, state.accel1);
        get_accel_data(state.MPU_2, state.accel2);
        
        // Calculate average gyro reading
        state.aveGyro = (state.accel1[8] + state.accel2[8]) / 2.0;
        
        // Update motion statistics immediately after reading sensors
        updateMotionStats();
        
        // Check manual triggers
        state.localTriggerState1 = digitalRead(state.MANUAL_TRIGGER_1);
        state.localTriggerState2 = digitalRead(state.MANUAL_TRIGGER_2);
        
        // Determine if we should trigger based on acceleration
        if (state.aveGyro > state.minGyro && state.resetTimer >= state.resetLimit && !state.fireOn) {
            float scale = (state.aveGyro - state.minGyro) / (state.maxGyro - state.minGyro);
            scale = constrain(scale, 0.0, 1.0);
            state.fireTimeLimit = state.minFireTime + scale * (state.maxFireTime - state.minFireTime);
            
            startFire();
            state.currentStats.accelTriggersCount++;
            
            // Update acceleration statistics
            if (state.aveGyro > state.currentStats.highestGyroReading) {
                state.currentStats.highestGyroReading = state.aveGyro;
                saveStats();
            }
            state.currentStats.averageAccelTrigger = 
                (state.currentStats.averageAccelTrigger * (state.currentStats.accelTriggersCount - 1) + state.aveGyro) 
                / state.currentStats.accelTriggersCount;
            
            saveStats();
        }
        
        // Check manual or remote triggers
        if ((state.localTriggerState1 == LOW || state.localTriggerState2 == LOW || state.remoteTriggerState == 0) 
            && state.resetTimer >= state.resetLimit && !state.fireOn) {
            state.fireTimeLimit = state.remoteFireTime;
            startFire();
        }
        
        // Fire control logic
        if (state.fireOn) {
            // Only check fire cycle timing if cycle is enabled
            if (state.fireCycle == 0) {
                // When fireCycle is 0, fire both pins simultaneously
                digitalWrite(state.FIRE_PIN_1, HIGH);
                digitalWrite(state.FIRE_PIN_2, HIGH);
                state.firePinState1 = true;
                state.firePinState2 = true;
            } else {
                unsigned long cycleInterval = state.fireCycle * 1000000;
                if (currentMicros - lastFireCycleUpdate >= cycleInterval) {
                    // Update lastFireCycleUpdate more precisely
                    lastFireCycleUpdate += cycleInterval;  // Use += instead of = for better timing
                    state.fireCycleToggle = !state.fireCycleToggle;
                    
                    // Update pins based on cycle state
                    digitalWrite(state.FIRE_PIN_1, state.fireCycleToggle ? HIGH : LOW);
                    digitalWrite(state.FIRE_PIN_2, state.fireCycleToggle ? LOW : HIGH);
                    state.firePinState1 = state.fireCycleToggle;
                    state.firePinState2 = !state.fireCycleToggle;
                }
            }
            
            // Check fire duration
            float currentFireDuration = (millis() - state.fireStartTime) / 1000.0;
            state.fireTimer = currentFireDuration;  // Keep fireTimer in sync
            
            //logFireStatus(currentFireDuration);
            
            if (currentFireDuration >= state.fireTimeLimit) {
                stopFire();
                state.fireTimer = 0;
                state.fireCycleToggle = false;
                state.resetState = 0;
                state.resetTimer = 0;
                digitalWrite(state.FIRE_PIN_1, LOW);
                digitalWrite(state.FIRE_PIN_2, LOW);
                state.firePinState1 = false;
                state.firePinState2 = false;
            }
        } else {
            digitalWrite(state.FIRE_PIN_1, LOW);
            digitalWrite(state.FIRE_PIN_2, LOW);
            state.firePinState1 = false;
            state.firePinState2 = false;
            lastFireCycleUpdate = currentMicros;
        }
        
        // Update timers
        if (state.fireOn) {
            float currentFireDuration = (millis() - state.fireStartTime) / 1000.0;
            state.fireTimer = currentFireDuration;
        } else if (!state.resetState && state.resetTimer < state.resetLimit) {
            state.resetTimer += state.loopRate;
            //logFireStatus(0, "Reset");
            
            if (state.resetTimer >= state.resetLimit) {
                state.resetState = 1;
                Serial.println("Reset complete - system ready");
            }
        }
        
        // Memory usage statistics
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t maxFreeBlock = ESP.getMaxFreeBlockSize();
        uint32_t heapFragmentation = ESP.getHeapFragmentation();
        
        float currentMemUsage = 100.0 * (1.0 - ((float)maxFreeBlock / (float)freeHeap));
        currentMemUsage = constrain(currentMemUsage, 0.0, 100.0);
        
        if (currentMemUsage > state.currentStats.peakMemoryUsage && !isnan(currentMemUsage)) {
            state.currentStats.peakMemoryUsage = currentMemUsage;
            Serial.printf("New peak memory usage: %.2f%% (Free: %u bytes, Fragmentation: %u%%)\n", 
                         currentMemUsage, freeHeap, heapFragmentation);
            saveStats();
        }
    }
    
    yield();
}
// Add near other constants
static_assert(sizeof(FireSettings) + sizeof(SystemStats) + EEPROMConfig::EEPROM_DATA_ADDR <= EEPROMConfig::EEPROM_SIZE, 
    "Combined settings and stats too large for EEPROM");
    
void logFireStatus(float currentFireDuration, const char* eventType) {
    Serial.printf("%-8s Duration: %6.2f/%6.2f (%3d%%), FireTimer: %6.2f/%6.2f (%3d%%), ResetTimer: %6.2f/%6.2f (%3d%%), Pin1=%-4s, Pin2=%-4s\n",
        eventType,
        currentFireDuration,
        state.fireTimeLimit,
        (int)((currentFireDuration / state.fireTimeLimit) * 100),  // Fire duration progress
        state.fireTimer,
        state.fireTimeLimit,
        (int)((state.fireTimer / state.fireTimeLimit) * 100),      // Fire timer progress
        state.resetTimer,
        state.resetLimit,
        (int)((state.resetTimer / state.resetLimit) * 100),        // Reset progress
        state.firePinState1 ? "HIGH" : "LOW",
        state.firePinState2 ? "HIGH" : "LOW"
    );
}
    
void updateMotionStats() {
    // Track highest angular acceleration
    float currentGyro = abs(state.aveGyro);
    if (currentGyro > state.currentStats.highestGyroReading) {
        state.currentStats.highestGyroReading = currentGyro;
        saveStats();
    }

    // Track highest linear acceleration from either sensor
    float currentAccel = max(state.accel1[7], state.accel2[7]);
    if (currentAccel > state.currentStats.highestAccelReading) {
        state.currentStats.highestAccelReading = currentAccel;
        Serial.printf("New highest linear acceleration: %.2f g\n", currentAccel); // Debug print
        saveStats();
    }
}
    