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

//   "sketch": "Saloon_Doors_Main_Board\\Saloon_Doors_V191201.ino",

// Define WiFi Settings
  //WiFiServer server(80);
  ESP8266WebServer server(80);
  //IPAddress local_ip(100,100,1,10);
  //IPAddress gateway(100,100,1,1);
  //IPAddress mask(255,255,0,0);
  // IPAddress = 192.168.4.1  <--------------------

//Define Pin assignments:
  //#define          LED_PIN    2   //Labeled ??
  //                   SDA    4   //Labeled "D2"
  //                   SCL    5   //Labeled "D1" 
  #define       FIRE_PIN_1   14   //Labeled "D5"
  #define       FIRE_PIN_2   12   //Labeled "D6"
  #define MANUAL_TRIGGER_1   0   //Labeled "D3"      
  #define MANUAL_TRIGGER_2   2   //Labeled "D4"      

// Define & Initialize System Settigns:
  float LOOP_RATE = 0.05; //Define the amount of seconds per loop
  int REMOTE_TRIGGER_STATE = 1; // Inititalize the REMOTE trigger state to open
  int LOCAL_TRIGGER_STATE_1 = 1; // Inititalize the LOCAL 1 trigger state to open
  int LOCAL_TRIGGER_STATE_2 = 1; // Inititalize the LOCAL 2 trigger state to open
  int TRIGGER_STATE = 0; // Inititalize the trigger state to open
  int FIRE_ON = 0; // Define switch for when fire is active
  float FIRE_TIMER;  //declare a timer variable for the duration of fire output
  String request = "null";
  int RESET_STATE = 1;

// Define Initial Accerometer Settings:
  const int MPU_1=0x68; //Need to change the I2C address
  const int MPU_2=0x69; //Need to change the I2C address
  int16_t ACCEL_1[9] = {0,0,0,0,0,0,0,0,0}; // Global declare accel 1 and Initialize
  int16_t ACCEL_2[9] = {0,0,0,0,0,0,0,0,0}; // Global declare accel 2 and Initialize
  float AVE_GYRO = 0;
  float MIN_GYRO;  // Degrees per second
  float MAX_GYRO;  // Degrees per second
  float GYRO_FACTOR;
  float ACCEL_FACTOR;

//  Define Fire control Settings:
  float MIN_FIRE_TIME;  // Number of fire seconds corresponding to the MIN_GYRO value
  float MAX_FIRE_TIME;  // Number of fire seconds corresponding to the MIN_GYRO value
  float FIRE_TIME_LIMIT = 0.0;  //Initialize fire time
  float REMOTE_FIRE_TIME;  //Define fire time for remote trigger
  float RESET_LIMIT;
  float RESET_TIMER;
  float FIRE_CYCLE;  //Define fire time to cycle through both sides
  float FIRE_CYCLE_COUNTER = 0.0;  //Define fire time to cycle through both sides

// Add these constants for EEPROM management
#define EEPROM_SIZE 512
#define EEPROM_MAGIC_NUMBER 0xAB  // Used to verify if EEPROM has been initialized
#define EEPROM_MAGIC_ADDR 0
#define EEPROM_DATA_ADDR 1

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

// Global settings variable
FireSettings currentSettings;

// Add after FireSettings struct
struct SystemStats {
    // Core stats
    unsigned long remoteTriggersCount;
    unsigned long accelTriggersCount;
    float highestAccelReading;
    float totalFireTime;  // in seconds
    
    // Additional stats
    float longestFireDuration;      // longest single fire event in seconds
    float averageAccelTrigger;      // running average of trigger acceleration
    float peakMemoryUsage;          // percentage of memory used (0-100%)
};

// Add with other constants
const SystemStats DEFAULT_STATS = {
    .remoteTriggersCount = 0,
    .accelTriggersCount = 0,
    .highestAccelReading = 0.0,
    .totalFireTime = 0.0,
    .longestFireDuration = 0.0,
    .averageAccelTrigger = 0.0,
    .peakMemoryUsage = 0.0
};

// Add with other globals
SystemStats currentStats;

// Add these globals near the top with other globals
unsigned long fireStartTime = 0;  // To track duration of current fire event

// Add these forward declarations after the struct definitions but before any function implementations
bool saveStats();
void loadDefaultStats();
bool loadStats();
void startFire();
void stopFire();

// Modify the function that starts the fire (where FIRE_ON is set to true)
void startFire() {
    FIRE_ON = true;
    fireStartTime = millis();  // Record start time when fire begins
    currentStats.remoteTriggersCount++;  // If this is a remote trigger
    saveStats();
}

// Modify the function that stops the fire
void stopFire() {
    if (FIRE_ON) {
        FIRE_ON = false;
        // Calculate fire duration and update stats
        float fireDuration = (millis() - fireStartTime) / 1000.0;  // Convert to seconds
        
        // Update total fire time
        currentStats.totalFireTime += fireDuration;
        
        // Update longest fire duration if applicable
        if (fireDuration > currentStats.longestFireDuration) {
            currentStats.longestFireDuration = fireDuration;
        }
        
        saveStats();
    }
}

// Add these functions after the FireSettings functions
bool saveStats() {
    EEPROM.put(EEPROM_DATA_ADDR + sizeof(FireSettings), currentStats);
    if (!EEPROM.commit()) {
        Serial.println("Error: Failed to commit stats to EEPROM");
        return false;
    }
    return true;
}

void loadDefaultStats() {
    currentStats = DEFAULT_STATS;
}

bool loadStats() {
    EEPROM.get(EEPROM_DATA_ADDR + sizeof(FireSettings), currentStats);
    
    // Basic validation
    bool valid = true;
    valid &= currentStats.remoteTriggersCount < 1000000;  // Reasonable maximum
    valid &= currentStats.accelTriggersCount < 1000000;
    valid &= currentStats.highestAccelReading >= 0 && currentStats.highestAccelReading < 10000;
    valid &= currentStats.totalFireTime >= 0 && currentStats.totalFireTime < 1000000;
    
    if (!valid) {
        loadDefaultStats();
        return false;
    }
    return true;
}

// Add these functions for EEPROM management
bool saveSettings() {
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_NUMBER);
    EEPROM.put(EEPROM_DATA_ADDR, currentSettings);
    if (!EEPROM.commit()) {
        Serial.println("Error: Failed to commit settings to EEPROM");
        return false;
    }
    return true;
}

void loadDefaultSettings() {
    currentSettings = DEFAULT_SETTINGS;
    // Update global variables
    MIN_FIRE_TIME = currentSettings.minFireTime;
    MAX_FIRE_TIME = currentSettings.maxFireTime;
    REMOTE_FIRE_TIME = currentSettings.remoteFireTime;
    RESET_LIMIT = currentSettings.resetLimit;
    FIRE_CYCLE = currentSettings.fireCycle;
    MIN_GYRO = currentSettings.minGyro;
    MAX_GYRO = currentSettings.maxGyro;
}

bool loadSettings() {
    if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_NUMBER) {
        loadDefaultSettings();
        return false;
    }
    
    EEPROM.get(EEPROM_DATA_ADDR, currentSettings);
    
    // Validate loaded values
    bool valid = true;
    valid &= currentSettings.minFireTime > 0 && currentSettings.minFireTime < 60;
    valid &= currentSettings.maxFireTime > 0 && currentSettings.maxFireTime < 60;
    valid &= currentSettings.remoteFireTime > 0 && currentSettings.remoteFireTime < 60;
    valid &= currentSettings.resetLimit > 0 && currentSettings.resetLimit < 60;
    valid &= currentSettings.fireCycle >= 0 && currentSettings.fireCycle < 60;
    valid &= currentSettings.minGyro > 0 && currentSettings.minGyro < 1000;
    valid &= currentSettings.maxGyro > 0 && currentSettings.maxGyro < 2000;
    valid &= currentSettings.minGyro < currentSettings.maxGyro;
    
    if (!valid) {
        loadDefaultSettings();
        return false;
    }
    
    // Update global variables
    MIN_FIRE_TIME = currentSettings.minFireTime;
    MAX_FIRE_TIME = currentSettings.maxFireTime;
    REMOTE_FIRE_TIME = currentSettings.remoteFireTime;
    RESET_LIMIT = currentSettings.resetLimit;
    FIRE_CYCLE = currentSettings.fireCycle;
    MIN_GYRO = currentSettings.minGyro;
    MAX_GYRO = currentSettings.maxGyro;
    RESET_TIMER = RESET_LIMIT;
    return true;
}

// prepare a web page to be send to a client (web browser)
String prepare_Root_Page()
{
  String htmlPage =
            String("") +
            "<!DOCTYPE HTML>" +
            "<HTML>" +
            "<HEAD>" +
            "<meta name='viewport' content='width=device-width, initial-scale=1'>" +
            "<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'>" +
            "<meta http-equiv='Pragma' content='no-cache'>" +
            "<meta http-equiv='Expires' content='0'>" +
            "<TITLE>Saloon Doors Root</TITLE>" +
            "</HEAD>" +
            "<BODY style='font-size:300%;background-color:black;color:white'>" +
            "<h2>&#128293 <u>HighNoon</u> &#128293</h2>" +
            "<br>" +
            "<p><a href='/'>Root Page</a></p>" +
            "<p><a href='/settings'>Settings Control Page</a></p>" +
            "<p><a href='/fire'>Fire Control Page</a></p>" +
            "<p><a href='/data'>Data Page</a></p>" +
            "<p><a href='/stats'>Statistics Page</a></p>" +
            "</BODY>" +
            "</HTML>";
          
  return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Data_Page()
{
  String htmlPage =
            String("") +
            "<!DOCTYPE HTML>" +
            "<HTML>" +
            "<HEAD>" +
            "<TITLE>Saloon Doors Data</TITLE>" +
            "<script>" +
            "function updateData() {" +
            "  fetch('/data/status')" +
            "  .then(response => response.json())" +
            "  .then(data => {" +
            "    document.getElementById('resetTimer').innerHTML = data.resetTimer.toFixed(2);" +
            "    document.getElementById('resetLimit').innerHTML = data.resetLimit.toFixed(2);" +
            "    document.getElementById('resetState').innerHTML = data.resetState;" +
            "    document.getElementById('fireTimer').innerHTML = data.fireTimer.toFixed(2);" +
            "    document.getElementById('fireTimeLimit').innerHTML = data.fireTimeLimit.toFixed(2);" +
            "    document.getElementById('remoteTrigger').innerHTML = data.remoteTriggerState;" +
            "    document.getElementById('localTrigger1').innerHTML = data.localTriggerState1;" +
            "    document.getElementById('localTrigger2').innerHTML = data.localTriggerState2;" +
            "    document.getElementById('fireOn').innerHTML = data.fireOn;" +
            "    document.getElementById('accel1').innerHTML = data.accel1.toFixed(2);" +
            "    document.getElementById('accel2').innerHTML = data.accel2.toFixed(2);" +
            "    document.getElementById('aveGyro').innerHTML = data.aveGyro.toFixed(2);" +
            "  })" +
            "  .catch(error => console.error('Error:', error));" +
            "}" +
            "document.addEventListener('DOMContentLoaded', function() {" +
            "  updateData();" +  // Initial update
            "  setInterval(updateData, 100);" +  // Update every 100ms
            "});" +
            "</script>" +
            "</HEAD>" +
            "<BODY style='font-size:300%;background-color:black;color:white'>" +
            "<h3>Reset Timer [sec]: <span id='resetTimer'>" + String(RESET_TIMER, 2) + "</span></h3>" +
            "<h3>Reset Limit [sec]: <span id='resetLimit'>" + String(RESET_LIMIT, 2) + "</span></h3>" +
            "<h3>Reset State: <span id='resetState'>" + RESET_STATE + "</span></h3>" +
            "<h3>Fire Timer [sec]: <span id='fireTimer'>" + String(FIRE_TIMER, 2) + "</span></h3>" +
            "<h3>Fire Time Limit [sec]: <span id='fireTimeLimit'>" + String(FIRE_TIME_LIMIT, 2) + "</span></h3>" +
            "<h3>REMOTE_TRIGGER_STATE: <span id='remoteTrigger'>" + REMOTE_TRIGGER_STATE + "</span></h3>" +
            "<h3>LOCAL_TRIGGER_STATE_1: <span id='localTrigger1'>" + digitalRead(MANUAL_TRIGGER_1) + "</span></h3>" +
            "<h3>LOCAL_TRIGGER_STATE_2: <span id='localTrigger2'>" + digitalRead(MANUAL_TRIGGER_2) + "</span></h3>" +
            "<h3>FIRE_ON: <span id='fireOn'>" + FIRE_ON + "</span></h3>" +
            "<h3>ACCEL_1: <span id='accel1'>" + String(ACCEL_1[8], 2) + "</span></h3>" +
            "<h3>ACCEL_2: <span id='accel2'>" + String(ACCEL_2[8], 2) + "</span></h3>" +
            "<h3>AVE_GYRO: <span id='aveGyro'>" + String(AVE_GYRO, 2) + "</span></h3>" +
            "<br>" +
            "<p><a href='/'>Root Page</a></p>" +
            "<p><a href='/settings'>Settings Control Page</a></p>" +
            "<p><a href='/fire'>Fire Control Page</a></p>" +
            "<p><a href='/data'>Data Page</a></p>" +
            "</BODY>" +
            "</HTML>";
            
  return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Fire_Control_Page() {
    String htmlPage;
    htmlPage.reserve(4096);
    
    htmlPage += "<!DOCTYPE HTML>";
    htmlPage += "<HTML>";
    htmlPage += "<HEAD>";
    htmlPage += "<TITLE>Saloon Doors Controls</TITLE>";
    htmlPage += "<meta charset='UTF-8'>";
    htmlPage += "<style>";
    htmlPage += "  .button {";
    htmlPage += "    border: none;";
    htmlPage += "    color: white;";
    htmlPage += "    padding: 15px 32px;";
    htmlPage += "    text-align: center;";
    htmlPage += "    text-decoration: none;";
    htmlPage += "    display: inline-block;";
    htmlPage += "    font-size: 150px;";
    htmlPage += "    margin: 4px 2px;";
    htmlPage += "    cursor: pointer;";
    htmlPage += "    transition: background-color 0.3s;";
    htmlPage += "  }";
    htmlPage += "  .status-left {";
    htmlPage += "    position: fixed;";
    htmlPage += "    top: 10px;";
    htmlPage += "    left: 10px;";
    htmlPage += "    font-size: 144px;";
    htmlPage += "  }";
    htmlPage += "  .status-right {";
    htmlPage += "    position: fixed;";
    htmlPage += "    top: 10px;";
    htmlPage += "    right: 10px;";
    htmlPage += "    font-size: 144px;";
    htmlPage += "  }";
    htmlPage += "  .main-content {";
    htmlPage += "    margin-top: 200px;";
    htmlPage += "  }";
    htmlPage += "</style>";
    htmlPage += "<script>";
    // Update status function
    htmlPage += "function updateStatus() {";
    htmlPage += "  fetch('/fire/status')";
    htmlPage += "    .then(response => response.json())";
    htmlPage += "    .then(data => {";
    htmlPage += "      document.getElementById('resetTimer').textContent = data.resetTimer.toFixed(2);";
    htmlPage += "      document.getElementById('fireTimer').textContent = data.fireTimer.toFixed(2);";
    htmlPage += "      document.getElementById('fireTimeLimit').textContent = data.fireTimeLimit.toFixed(2);";
    htmlPage += "      const btn = document.getElementById('fireButton');";
    htmlPage += "      if (data.fireOn) {";
    htmlPage += "        btn.style.backgroundColor = 'red';";
    htmlPage += "        btn.disabled = true;";
    htmlPage += "      } else if (data.resetTimer < data.resetLimit) {";
    htmlPage += "        btn.style.backgroundColor = 'grey';";
    htmlPage += "        btn.disabled = true;";
    htmlPage += "      } else {";
    htmlPage += "        btn.style.backgroundColor = 'green';";
    htmlPage += "        btn.disabled = false;";
    htmlPage += "      }";
    htmlPage += "      document.getElementById('pin1Status').innerHTML = !data.firePin1 ? '🟢' : '🔴';";
    htmlPage += "      document.getElementById('pin2Status').innerHTML = !data.firePin2 ? '🟢' : '🔴';";
    htmlPage += "    })";
    htmlPage += "    .catch(error => console.error('Error:', error));";
    htmlPage += "}";
    // Fire trigger function
    htmlPage += "function fireTrigger() {";
    htmlPage += "  const btn = document.getElementById('fireButton');";
    htmlPage += "  btn.disabled = true;";  // Disable immediately
    htmlPage += "  btn.style.backgroundColor = 'grey';";  // Visual feedback
    htmlPage += "  fetch('/fire/on')";
    htmlPage += "    .then(response => {";
    htmlPage += "      if (response.ok) {";
    htmlPage += "        updateStatus();";
    htmlPage += "      } else {";
    htmlPage += "        btn.disabled = false;";
    htmlPage += "        btn.style.backgroundColor = 'green';";
    htmlPage += "      }";
    htmlPage += "    })";
    htmlPage += "    .catch(error => {";
    htmlPage += "      console.error('Error:', error);";
    htmlPage += "      btn.disabled = false;";
    htmlPage += "      btn.style.backgroundColor = 'green';";
    htmlPage += "    });";
    htmlPage += "  return false;";
    htmlPage += "}";
    // Start periodic updates
    htmlPage += "document.addEventListener('DOMContentLoaded', function() {";
    htmlPage += "  updateStatus();";  // Initial update
    htmlPage += "  setInterval(updateStatus, 100);";  // Update every 100ms
    htmlPage += "});";
    htmlPage += "</script>";
    htmlPage += "</HEAD>";
    htmlPage += "<BODY style='font-size:300%;background-color:black;color:white'>";
    htmlPage += "<div class='status-left' id='pin1Status'>🟢</div>";
    htmlPage += "<div class='status-right' id='pin2Status'>🟢</div>";
    htmlPage += "<div class='main-content'>";
    htmlPage += "<h3>Reset Timer [sec]: <span id='resetTimer'>" + String(RESET_TIMER, 2) + "</span> / " + String(RESET_LIMIT, 2) + "</h3>";
    htmlPage += "<h3>Fire Timer [sec]: <span id='fireTimer'>" + String(FIRE_TIMER, 2) + "</span> / <span id='fireTimeLimit'>" + String(FIRE_TIME_LIMIT, 2) + "</span></h3>";
    htmlPage += "<br>";
    // Initial button state based on current conditions
    String initialColor = FIRE_ON ? "red" : (RESET_TIMER < RESET_LIMIT ? "grey" : "green");
    String initialDisabled = (FIRE_ON || RESET_TIMER < RESET_LIMIT) ? "disabled" : "";
    htmlPage += "<button id='fireButton' class='button' onclick='return fireTrigger();' style='background-color: " + initialColor + "' " + initialDisabled + ">FIRE!</button>";
    htmlPage += "<br>";
    htmlPage += "<p><a href='/'>Root Page</a></p>";
    htmlPage += "<p><a href='/settings'>Settings Control Page</a></p>";
    htmlPage += "<p><a href='/fire'>Fire Control Page</a></p>";
    htmlPage += "<p><a href='/data'>Data Page</a></p>";
    htmlPage += "</div>";
    htmlPage += "</BODY>";
    htmlPage += "</HTML>";
    
    return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Fire_Settings_Page()
{
    String htmlPage =
            String("") +
            "<!DOCTYPE HTML>" +
            "<HTML>" +
            "<HEAD>" +
            "<TITLE>Saloon Doors Settings</TITLE>" +
            "<script>" +
            "function submitSettings(event) {" +
            "  event.preventDefault();" +
            "  const form = document.getElementById('settingsForm');" +
            "  const formData = new URLSearchParams(new FormData(form));" +
            "  fetch('/settings/action_page', {" +
            "    method: 'POST'," +
            "    headers: {" +
            "      'Content-Type': 'application/x-www-form-urlencoded'" +
            "    }," +
            "    body: formData.toString()" +
            "  })" +
            "  .then(response => {" +
            "    if(response.ok) {" +
            "      alert('Settings updated successfully!');" +
            "      location.reload();" +
            "    } else {" +
            "      response.text().then(text => alert('Error: ' + text));" +
            "    }" +
            "  })" +
            "  .catch(error => {" +
            "    console.error('Error:', error);" +
            "    alert('Error updating settings: ' + error);" +
            "  });" +
            "}" +
            "function resetDefaults() {" +
            "  if(confirm('Reset to default settings?')) {" +
            "    fetch('/settings/reset', {method: 'POST'})" +
            "    .then(response => {" +
            "      if(response.ok) {" +
            "        alert('Settings reset to defaults');" +
            "        location.reload();" +
            "      }" +
            "    });" +
            "  }" +
            "}" +
            "</script>" +
            "</HEAD>" +
            "<BODY style='font-size:300%;background-color:black;color:white'>" +
            "<form id='settingsForm' onsubmit='submitSettings(event)' method='POST' enctype='application/x-www-form-urlencoded'>" +  // Added method and enctype
            "MIN Gyro [degree/sec]:<br><input style='font-size:150%' type='number' name='MIN_GYRO' value='" + MIN_GYRO + "'><br>" +
            "MAX Gyro [degree/sec]:<br><input style='font-size:150%' type='number' name='MAX_GYRO' value='" + MAX_GYRO + "'><br>" +
            "MIN Fire Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='MIN_FIRE_TIME' value='" + MIN_FIRE_TIME + "'><br>" +
            "MAX Fire Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='MAX_FIRE_TIME' value='" + MAX_FIRE_TIME + "'><br>" +
            "RESET Time Limit [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='RESET_LIMIT' value='" + RESET_LIMIT + "'><br>" +
            "Fire Cycle Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='FIRE_CYCLE' value='" + FIRE_CYCLE + "'><br>" +
            "Remote Fire Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='REMOTE_FIRE_TIME' value='" + REMOTE_FIRE_TIME + "'><br><br><br>" +
            "<input type='submit' style='font-size:300%;color:red' value='UPDATE'>" +
            "</form>" +
            "<br>" +
            "<button onclick='resetDefaults()' style='font-size:300%;color:orange'>Reset to Defaults</button>" +
            // ... navigation links ...
            "</BODY>" +
            "</HTML>";
            
    return htmlPage;
}

// Add new page preparation function
String prepare_Stats_Page() {
    String htmlPage;
    htmlPage.reserve(2048);
    
    htmlPage += "<!DOCTYPE HTML><HTML><HEAD>";
    htmlPage += "<TITLE>System Statistics</TITLE>";
    htmlPage += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    htmlPage += "<style>";
    htmlPage += "body { font-size:300%; background-color:black; color:white; }";
    htmlPage += ".stat-box { background-color:#333; padding:15px; margin:10px; border-radius:5px; }";
    htmlPage += ".reset-btn { background-color:#ff4444; color:white; padding:15px; border:none; border-radius:5px; font-size:100%; cursor:pointer; }";
    htmlPage += "</style>";
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
    
    htmlPage += "<h2>System Statistics</h2>";
    htmlPage += "<div class='stat-box'>";
    htmlPage += "<h3>Trigger Statistics</h3>";
    htmlPage += "<p>Remote Triggers: " + String(currentStats.remoteTriggersCount) + "</p>";
    htmlPage += "<p>Accelerometer Triggers: " + String(currentStats.accelTriggersCount) + "</p>";
    
    htmlPage += "<h3>Acceleration Data</h3>";
    htmlPage += "<p>Highest Acceleration: " + String(currentStats.highestAccelReading, 2) + " g</p>";
    htmlPage += "<p>Average Trigger Acceleration: " + String(currentStats.averageAccelTrigger, 2) + " g</p>";
    
    htmlPage += "<h3>Fire Duration</h3>";
    // Convert total fire time to hours:minutes:seconds
    unsigned long totalSeconds = (unsigned long)currentStats.totalFireTime;
    unsigned long hours = totalSeconds / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;
    
    htmlPage += "<p>Total Fire Time: " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s</p>";
    htmlPage += "<p>Longest Single Fire: " + String(currentStats.longestFireDuration, 1) + " seconds</p>";
    
    htmlPage += "<h3>System Resources</h3>";
    htmlPage += "<p>Peak Memory Usage: " + String(currentStats.peakMemoryUsage, 1) + "%</p>";
    htmlPage += "</div>";
    
    htmlPage += "<button class='reset-btn' onclick='resetStats()'>Reset Statistics</button>";
    htmlPage += "<br><br>";
    htmlPage += "<p><a href='/'>Root Page</a></p>";
    htmlPage += "</BODY></HTML>";
    
    return htmlPage;
}

// Add these handler functions before they're used in server.on()

void handle_Stats_Reset() {
    loadDefaultStats();
    if (!saveStats()) {
        server.send(500, "text/plain", "Failed to reset statistics");
        return;
    }
    server.send(200, "text/plain", "Statistics reset successfully");
}

void handle_Stats_Page() {
    server.send(200, "text/html", prepare_Stats_Page());
}

//===============================================================
// This routine is executed when you open its IP in browser
//===============================================================
void handleRoot() {
  server.sendHeader("Cache-Control", "max-age=3600"); // Cache for 1 hour
  server.send(200, "text/html", prepare_Root_Page());
}

//===============================================================
// This routine is executed when you open the fire control page
//===============================================================
void handle_Fire_Control_Page() {
  server.send(200, "text/html", prepare_Fire_Control_Page());
}

//===============================================================
// This routine is executed when you trigger the FIRE on the control page
//===============================================================
void handle_Fire_Control_ON_Page() {
    static unsigned long lastFireTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastFireTime < 1000) {
        server.send(429, "text/plain", "Too many requests");
        return;
    }
    
    startFire();
    FIRE_TIME_LIMIT = REMOTE_FIRE_TIME;
    FIRE_TIMER = 0;
    RESET_TIMER = 0;
    RESET_STATE = 0;
    
    lastFireTime = currentTime;
    server.send(200, "text/plain", "OK");
}

//===============================================================
// This routine is executed when you open the fire settings page
//===============================================================
void handle_Fire_Settings_Page() {
  server.send(200, "text/html", prepare_Fire_Settings_Page());
}

//===============================================================
// This routine is executed when you open the data page
//===============================================================
void handle_Data_Page() {
  server.send(200, "text/html", prepare_Data_Page());
}

//===============================================================
// This routine is executed when you press submit
//===============================================================
void handleForm() {
    Serial.println("Form handler called"); // Debug print
    
    // Print all received arguments for debugging
    for (int i = 0; i < server.args(); i++) {
        Serial.printf("Arg %d: %s = %s\n", i, server.argName(i).c_str(), server.arg(i).c_str());
    }

    // Remove the check for "plain" data since we're handling form data
    if (server.args() == 0) {
        server.send(400, "text/plain", "No data received");
        return;
    }

    // Rest of the function remains the same...
}

void handleResetDefaults() {
    loadDefaultSettings();
    saveSettings();
    server.send(200, "text/html", prepare_Fire_Settings_Page());
}

void handle_Data_Status() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  // Pre-allocate string space
  String json;
  json.reserve(512); // Adjust size as needed
  
  json += "{";
  json += "\"resetTimer\":" + String(RESET_TIMER, 2) + ",";
  json += "\"resetLimit\":" + String(RESET_LIMIT, 2) + ",";
  json += "\"resetState\":" + String(RESET_STATE) + ",";
  json += "\"fireTimer\":" + String(FIRE_TIMER, 2) + ",";
  json += "\"fireTimeLimit\":" + String(FIRE_TIME_LIMIT, 2) + ",";
  json += "\"remoteTriggerState\":" + String(REMOTE_TRIGGER_STATE) + ",";
  json += "\"localTriggerState1\":" + String(digitalRead(MANUAL_TRIGGER_1)) + ",";
  json += "\"localTriggerState2\":" + String(digitalRead(MANUAL_TRIGGER_2)) + ",";
  json += "\"fireOn\":" + String(FIRE_ON) + ",";
  json += "\"accel1\":" + String(ACCEL_1[8], 2) + ",";
  json += "\"accel2\":" + String(ACCEL_2[8], 2) + ",";
  json += "\"aveGyro\":" + String(AVE_GYRO, 2);
  json += "}";
  server.send(200, "application/json", json);
}

void handle_Settings_Update() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "No data received");
        return;
    }

    String data = server.arg("plain");
    
    // Parse JSON data
    // Note: In production code, you should use a proper JSON parser
    float newMinFireTime = server.arg("MIN_FIRE_TIME").toFloat();
    float newMaxFireTime = server.arg("MAX_FIRE_TIME").toFloat();
    float newResetLimit = server.arg("RESET_LIMIT").toFloat();
    float newRemoteFireTime = server.arg("REMOTE_FIRE_TIME").toFloat();
    float newFireCycle = server.arg("FIRE_CYCLE").toFloat();
    float newMinGyro = server.arg("MIN_GYRO").toFloat();
    float newMaxGyro = server.arg("MAX_GYRO").toFloat();
    
    // Validate input values
    if (newMinFireTime <= 0 || newMaxFireTime <= 0 || 
        newResetLimit <= 0 || newRemoteFireTime <= 0 || 
        newFireCycle < 0 || newMinFireTime >= newMaxFireTime ||
        newMinGyro <= 0 || newMaxGyro <= 0 || 
        newMinGyro >= newMaxGyro || newMaxGyro > 2000) {
        server.send(400, "text/plain", "Invalid values provided");
        return;
    }
    
    // Update settings
    currentSettings.minFireTime = newMinFireTime;
    currentSettings.maxFireTime = newMaxFireTime;
    currentSettings.remoteFireTime = newRemoteFireTime;
    currentSettings.resetLimit = newResetLimit;
    currentSettings.fireCycle = newFireCycle;
    currentSettings.minGyro = newMinGyro;
    currentSettings.maxGyro = newMaxGyro;
    
    // Update global variables
    MIN_FIRE_TIME = newMinFireTime;
    MAX_FIRE_TIME = newMaxFireTime;
    RESET_LIMIT = newResetLimit;
    REMOTE_FIRE_TIME = newRemoteFireTime;
    FIRE_CYCLE = newFireCycle;
    MIN_GYRO = newMinGyro;
    MAX_GYRO = newMaxGyro;
    
    // Save to EEPROM
    if (!saveSettings()) {
        server.send(500, "text/plain", "Failed to save settings");
        return;
    }
    
    server.send(200, "text/plain", "Settings updated successfully");
}

void handle_Settings_Reset() {
    loadDefaultSettings();
    if (!saveSettings()) {
        server.send(500, "text/plain", "Failed to reset settings");
        return;
    }
    server.send(200, "text/plain", "Settings reset to defaults");
}

void handle_Fire_Status() {
    String json;
    json.reserve(256);
    
    json = "{";
    json += "\"fireOn\":" + String(FIRE_ON) + ",";
    json += "\"resetTimer\":" + String(RESET_TIMER, 2) + ",";
    json += "\"resetLimit\":" + String(RESET_LIMIT, 2) + ",";
    json += "\"fireTimer\":" + String(FIRE_TIMER, 2) + ",";
    json += "\"fireTimeLimit\":" + String(FIRE_TIME_LIMIT, 2) + ",";
    json += "\"firePin1\":" + String(digitalRead(FIRE_PIN_1)) + ",";
    json += "\"firePin2\":" + String(digitalRead(FIRE_PIN_2));
    json += "}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", json);
}

// Add these constants with other defines
#define DNS_PORT 53

// Add this global variable with other globals
DNSServer dnsServer;

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
    
    server.send(200, "text/html", html);
}

void start_wifi(){
  WiFi.mode(WIFI_AP);
  WiFi.softAP("HighNoon", "shaboinky", 1, 0, 8);
  
  // Configure DNS server to redirect all domains to our IP
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);
  
  // Add WiFi power management
  WiFi.setOutputPower(20.5); // Max WiFi power
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable WiFi sleep mode
  
  // Increase the TCP MSS for better performance
  WiFi.setPhyMode(WIFI_PHY_MODE_11N); // Use 802.11n mode
  
  delay(500);
  
  // Remove this line that caused the error:
  // server.enableCrossOrigin(true);
  
  // Instead, add CORS headers to your request handlers
  server.onNotFound([]() {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      handleNotFound();
  });

  // Add CORS headers to your existing handlers
  server.on("/", HTTP_GET, []() {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      handleRoot();
  });

  // Add special URLs that mobile devices check for captive portals
  server.on("/generate_204", handleNotFound);  // Android
  server.on("/fwlink", handleNotFound);        // Microsoft
  server.on("/redirect", handleNotFound);      // Apple
  server.on("/hotspot-detect.html", handleNotFound); // Apple
  server.on("/canonical.html", handleNotFound); // Apple
  server.on("/success.txt", handleNotFound);    // Apple
  
  // Your existing endpoints...
  server.on("/fire", HTTP_GET, handle_Fire_Control_Page);
  server.on("/fire/on", HTTP_GET, handle_Fire_Control_ON_Page);
  server.on("/fire/status", HTTP_GET, handle_Fire_Status);
  server.on("/settings", HTTP_GET, handle_Fire_Settings_Page);
  server.on("/settings/action_page", HTTP_POST, handle_Settings_Update);
  server.on("/settings/reset", HTTP_POST, handle_Settings_Reset);
  server.on("/data", HTTP_GET, handle_Data_Page);
  server.on("/data/status", HTTP_GET, handle_Data_Status);
  server.on("/stats", HTTP_GET, []() {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      handle_Stats_Page();
  });
  server.on("/stats/reset", HTTP_POST, []() {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      handle_Stats_Reset();
  });

  server.begin();
  Serial.println("Web server started");
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());
  Serial.print("MAC: "); Serial.println(WiFi.softAPmacAddress());
}

static_assert(sizeof(FireSettings) + sizeof(SystemStats) + EEPROM_DATA_ADDR <= EEPROM_SIZE, 
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

void start_I2C_communication(int MPU){
  
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
    // re-enable Wire
    // now can start Wire Arduino master
    Wire.begin();
  }
  Serial.println("setup finished");

  Wire.beginTransmission(MPU);
  Wire.write(0x6B); 
  Wire.write(0);
  Wire.endTransmission(true);

  // Turn on Low Pass Fiter:
    Wire.beginTransmission(MPU);  
    Wire.write(0x1A); 
    //Wire.write(B00000000); // Level 0
    //Wire.write(B00000001); // Level 1
    //Wire.write(B00000010); // Level 2
    Wire.write(B00000011); // Level 3
    //Wire.write(B00000100); // Level 4
    //Wire.write(B00000101); // Level 5
    //Wire.write(B00000110); // Level 6
    Wire.endTransmission(true);

  // Set gyro full scale Range:
    Wire.beginTransmission(MPU);  
    Wire.write(0x1B); 
    //Wire.write(B00000000);GYRO_FACTOR=131.0;  // +/- 250 deg/s
    //Wire.write(B00001000);GYRO_FACTOR=65.5;  // +/- 500 deg/s
    //Wire.write(B00010000);GYRO_FACTOR=32.8;  // +/- 1000 deg/s
    Wire.write(B00011000);GYRO_FACTOR=16.4;  // +/- 2000 deg/s
    Wire.endTransmission(true);

  // Set accel full scale Range:
    Wire.beginTransmission(MPU);  
    Wire.write(0x1C); 
    Wire.write(B00000000);ACCEL_FACTOR=16384;  // +/- 2g
    Wire.write(B00001000);ACCEL_FACTOR=8192;  // +/- 4g
    Wire.write(B00010000);ACCEL_FACTOR=4096;  // +/- 8g
    Wire.write(B00011000);ACCEL_FACTOR=2048;  // +/- 16g
    Wire.endTransmission(true);
}

void get_accel_data(int MPU, int16_t output_array[9]){
  // Note: output_array must be a GLOBAL array variable

  Wire.beginTransmission(MPU);
  Wire.write(0x3B);  
  Wire.endTransmission(false);
  Wire.requestFrom(MPU,14,1);
  //Wire.requestFrom(MPU,14,true);

  int i = 0;
  output_array[i]=Wire.read()<<8|Wire.read();i+=1;  // AcX
  output_array[i]=Wire.read()<<8|Wire.read();i+=1;  // AcY
  output_array[i]=Wire.read()<<8|Wire.read();i+=1;  // AcZ
  output_array[i]=Wire.read()<<8|Wire.read();i+=1;  // Tmp
  output_array[i]=Wire.read()<<8|Wire.read();i+=1;  // GyX
  output_array[i]=Wire.read()<<8|Wire.read();i+=1;  // GyY
  output_array[i]=Wire.read()<<8|Wire.read();i+=1;  // GyZ

  output_array[0] = output_array[0] / ACCEL_FACTOR;  //Convert accel to units of g
  output_array[1] = output_array[1] / ACCEL_FACTOR;  //Convert accel to units of g
  output_array[2] = output_array[2] / ACCEL_FACTOR;  //Convert accel to units of g
  output_array[3] = output_array[3] / 340;           //Convert Temp to units of degrees C
  output_array[4] = output_array[4] / GYRO_FACTOR;   //Convert gyro to units of degree / sec
  output_array[5] = output_array[5] / GYRO_FACTOR;   //Convert gyro to units of degree / sec
  output_array[6] = output_array[6] / GYRO_FACTOR;   //Convert gyro to units of degree / sec

  output_array[7] = sqrt(sq(output_array[0])+sq(output_array[1])+sq(output_array[2])); // Mag. of acceleration
  output_array[8] = sqrt(sq(output_array[4])+sq(output_array[5])+sq(output_array[6])); // Mag. of rotation
  output_array[7] = abs(output_array[7]);
  output_array[8] = abs(output_array[8]);
}

void setup() {
  // Add watchdog timer
  ESP.wdtEnable(WDTO_8S);
  
  // Initialize the serial port
    Serial.begin(9600);

  start_I2C_communication(MPU_1);
  start_I2C_communication(MPU_2);
  start_wifi();
 

  // Configure pin as an output
    pinMode(FIRE_PIN_1, OUTPUT);
    pinMode(FIRE_PIN_2, OUTPUT);
  // Configure BUTTON pin as an input with a pullup
    pinMode(MANUAL_TRIGGER_1, INPUT_PULLUP);
    pinMode(MANUAL_TRIGGER_2, INPUT_PULLUP);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();
  loadStats();
}

void loop() {
    dnsServer.processNextRequest();
    
    static unsigned long lastUpdate = 0;
    const unsigned long updateInterval = LOOP_RATE * 1000; // Convert to milliseconds
    
    unsigned long currentMillis = millis();
    
    // Handle client requests as frequently as possible
    server.handleClient();
    
    // Only update sensors and fire control at the specified LOOP_RATE
    if (currentMillis - lastUpdate >= updateInterval) {
        lastUpdate = currentMillis;
        
        // Get sensor data
        get_accel_data(MPU_1, ACCEL_1);
        get_accel_data(MPU_2, ACCEL_2);
        
        // Calculate average gyro reading
        AVE_GYRO = (ACCEL_1[8] + ACCEL_2[8]) / 2.0;
        
        // Check manual triggers
        LOCAL_TRIGGER_STATE_1 = digitalRead(MANUAL_TRIGGER_1);
        LOCAL_TRIGGER_STATE_2 = digitalRead(MANUAL_TRIGGER_2);
        
        // Determine if we should trigger based on acceleration
        if (AVE_GYRO > MIN_GYRO && RESET_TIMER >= RESET_LIMIT && !FIRE_ON) {
            // Calculate fire duration based on gyro reading
            float scale = (AVE_GYRO - MIN_GYRO) / (MAX_GYRO - MIN_GYRO);
            scale = constrain(scale, 0.0, 1.0);
            FIRE_TIME_LIMIT = MIN_FIRE_TIME + scale * (MAX_FIRE_TIME - MIN_FIRE_TIME);
            
            startFire();
            currentStats.accelTriggersCount++;
            
            // Update acceleration statistics
            if (AVE_GYRO > currentStats.highestAccelReading) {
                currentStats.highestAccelReading = AVE_GYRO;
            }
            currentStats.averageAccelTrigger = 
                (currentStats.averageAccelTrigger * (currentStats.accelTriggersCount - 1) + AVE_GYRO) 
                / currentStats.accelTriggersCount;
            
            saveStats();
        }
        
        // Check manual or remote triggers
        if ((LOCAL_TRIGGER_STATE_1 == LOW || LOCAL_TRIGGER_STATE_2 == LOW || REMOTE_TRIGGER_STATE == 0) 
            && RESET_TIMER >= RESET_LIMIT && !FIRE_ON) {
            FIRE_TIME_LIMIT = REMOTE_FIRE_TIME;
            startFire();
        }
        
        // Fire control logic
        if (FIRE_ON) {
            // Handle alternating fire pattern
            if (FIRE_CYCLE_COUNTER <= FIRE_CYCLE) {
                digitalWrite(FIRE_PIN_1, HIGH);
                digitalWrite(FIRE_PIN_2, LOW);
            } else {
                digitalWrite(FIRE_PIN_1, LOW);
                digitalWrite(FIRE_PIN_2, HIGH);
            }
            
            FIRE_CYCLE_COUNTER += LOOP_RATE;
            if (FIRE_CYCLE_COUNTER >= FIRE_CYCLE * 2) {
                FIRE_CYCLE_COUNTER = 0;
            }
            
            if (FIRE_TIMER >= FIRE_TIME_LIMIT) {
                stopFire();
                FIRE_TIMER = 0;
                FIRE_CYCLE_COUNTER = 0;
                RESET_STATE = 0;
                RESET_TIMER = 0;
                digitalWrite(FIRE_PIN_1, LOW);
                digitalWrite(FIRE_PIN_2, LOW);
            }
        } else {
            digitalWrite(FIRE_PIN_1, LOW);
            digitalWrite(FIRE_PIN_2, LOW);
        }
        
        // Update timers
        if (FIRE_ON) {
            FIRE_TIMER += LOOP_RATE;
        } else if (!RESET_STATE && RESET_TIMER < RESET_LIMIT) {  // Only increment reset timer when not firing and below limit
            RESET_TIMER += LOOP_RATE;
        }
        
        // Update memory usage statistics
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t maxFreeBlock = ESP.getMaxFreeBlockSize();
        float currentMemUsage = 100.0 * (1.0 - ((float)freeHeap / (float)maxFreeBlock));
        
        // Ensure the value is within reasonable bounds (0-100%)
        currentMemUsage = constrain(currentMemUsage, 0.0, 100.0);
        
        if (currentMemUsage > currentStats.peakMemoryUsage && !isnan(currentMemUsage)) {
            currentStats.peakMemoryUsage = currentMemUsage;
            saveStats();
        }
    }
    
    // Small delay to prevent WiFi issues
    delay(1);
}

// Add near other constants
static_assert(sizeof(FireSettings) + sizeof(SystemStats) + EEPROM_DATA_ADDR <= EEPROM_SIZE, 
    "Combined settings and stats too large for EEPROM");
