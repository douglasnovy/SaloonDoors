// Base ESP8266
#include <ESP8266WiFi.h>
//#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
//#include <math.h>


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
  float FIRE_TIMER = 0.0;  //declare a timer variable for the duration of fire output
  String request = "null";
  int RESET_STATE = 1;

// Define Initial Accerometer Settings:
  const int MPU_1=0x68; //Need to change the I2C address
  const int MPU_2=0x69; //Need to change the I2C address
  int16_t ACCEL_1[9] = {0,0,0,0,0,0,0,0,0}; // Global declare accel 1 and Initialize
  int16_t ACCEL_2[9] = {0,0,0,0,0,0,0,0,0}; // Global declare accel 2 and Initialize
  float AVE_GYRO = 0;
  float MIN_GYRO = 50; // Degrees per second
  float MAX_GYRO = 750;  // Degrees per second
  float GYRO_FACTOR;
  float ACCEL_FACTOR;

//  Define Fire control Settings:
  float MIN_FIRE_TIME = 1.0; // Number of fire seconds corresponding to the MIN_GYRO value
  float MAX_FIRE_TIME = 6.0; // Number of fire seconds corresponding to the MIN_GYRO value
  float FIRE_TIME_LIMIT = 0.0;  //Initialize fire time
  float REMOTE_FIRE_TIME = 2.5;  //Define fire time for remote trigger
  float RESET_LIMIT = 3.0;
  float RESET_TIMER = RESET_LIMIT;
  float FIRE_CYCLE = 0.5;  //Define fire time to cycle through both sides
  float FIRE_CYCLE_COUNTER = 0.0;  //Define fire time to cycle through both sides

// Forward declare the functions:
 // void start_I2C_communication(int);
//  void start_wifi();
//  void get_accel_data(int, int16_t);
//  void handleRoot();

// prepare a web page to be send to a client (web browser)
String prepare_Root_Page()
{
  String htmlPage =
            String("") +
            "<!DOCTYPE HTML>" +
            " <HTML>" +
            "   <HEAD>" +
            "    <TITLE>Saloon Doors Root</TITLE>" +
            "  </HEAD>" +
            "<BODY style='font-size:400%;background-color:black;color:white'>" +
            "<h2> &#128293 <u>HighNoon</u> &#128293 <h2>" + 
            "</BODY>" +
            "<br>"+
            "<br>"+
            "<h3><a href='/'>Root Page</a></h3>"+
            "<h3><a href='/settings'>Settings Control Page</a></h3>"+
            "<h3><a href='/fire'>Fire Control Page</a></h3>"+
            "<h3><a href='/data'>Data Page</a></h3>"+
 
            "</HTML>" +
            "\r\n";
          
  return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Data_Page()
{
  String htmlPage =
            String("") +
            "<!DOCTYPE HTML>" +
            " <HTML>" +
            "   <HEAD>" +
            "    <TITLE>Saloon Doors Data</TITLE>" +
            "    <meta http-equiv='refresh' content='1; url=/data' >"+
            "  </HEAD>" +
            "<BODY style='font-size:200%;background-color:black;color:white'>" +
                    "<h3> Reset Timer [sec]: " + RESET_TIMER + "</h3>" +
                    "<h3> Reset Limit [sec]: " + RESET_LIMIT + "</h3>" +
                    "<h3> Reset State: " + RESET_STATE + "</h3>" +
                    "<h3> Fire Timer [sec]: " + FIRE_TIMER + "</h3>" +
                    "<h3> Fire Time Limit [sec]: " + FIRE_TIME_LIMIT + "</h3>" +
                    "<h3> REMOTE_TRIGGER_STATE: " + String(REMOTE_TRIGGER_STATE) + "</h3>" +
                    "<h3> LOCAL_TRIGGER_STATE_1: " + digitalRead(MANUAL_TRIGGER_1) + "</h3>" +
                    "<h3> LOCAL_TRIGGER_STATE_2: " + digitalRead(MANUAL_TRIGGER_2) + "</h3>" +
                    "<h3> FIRE_ON: " + FIRE_ON + "</h3>" +
                    "<h3> ACCEL_1: " + ACCEL_1[8] +"</h3>" +
                    "<h3> ACCEL_2: " + ACCEL_2[8] +"</h3>" +
                    "<h3> AVE_GYRO: " + AVE_GYRO +"</h3>" +
            "<br>"
            "<p><a href='/'>Root Page</a></p>"+
            "<p><a href='/settings'>Settings Control Page</a></p>"+
            "<p><a href='/fire'>Fire Control Page</a></p>"+
            "<p><a href='/data'>Data Page</a></p>"+          
            "</BODY>" +
            "</HTML>" +
            "\r\n";       
  return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Fire_Control_Page()
{
  String button_style = String("");

  if (FIRE_ON ==1 ){
    button_style = String("") +
                      "  .button {"+
                      "    background-color: red;"+
                      "    border: none;"+
                      "    color: white;"+
                      "    padding: 15px 32px;"+
                      "    text-align: center;"+
                      "    text-decoration: none;"+
                      "    display: inline-block;"+
                      "    font-size: 200px;"+
                      "    margin: 4px 2px;"+
                      "    cursor: pointer;"
                      "  }";
  }
  else if (RESET_TIMER < RESET_LIMIT){
      button_style = String("") +
                      "  .button {"+
                      "    background-color: grey;"+
                      "    border: none;"+
                      "    color: white;"+
                      "    padding: 15px 32px;"+
                      "    text-align: center;"+
                      "    text-decoration: none;"+
                      "    display: inline-block;"+
                      "    font-size: 200px;"+
                      "    margin: 4px 2px;"+
                      "    cursor: pointer;"
                      "  }";
  }
  else
  {
    button_style = String("") +
                      "  .button {"+
                      "    background-color: green;"+
                      "    border: none;"+
                      "    color: white;"+
                      "    padding: 15px 32px;"+
                      "    text-align: center;"+
                      "    text-decoration: none;"+
                      "    display: inline-block;"+
                      "    font-size: 200px;"+
                      "    margin: 4px 2px;"+
                      "    cursor: pointer;"
                      "  }";
  }

    String htmlPage =
              String("") +
              "<!DOCTYPE HTML>" +
              " <HTML>" +
              "   <HEAD>" +
              "    <TITLE>Saloon Doors Controls</TITLE>" +
              "    <meta http-equiv='refresh' content='1; url=/fire' >"
              "  </HEAD>" +
                      "  <style>"+
                      button_style +
                      " </style>"+
              "<BODY style='font-size:400%;background-color:black;color:white'>" +
              "<h3> Reset Timer [sec]:</h3>" +
              "<h3>" + RESET_TIMER + " / " + RESET_LIMIT + "</h3>" +
              "<h3> Fire Timer [sec]:</h3>" +
              "<h3>" + FIRE_TIMER + " / " + FIRE_TIME_LIMIT + "</h3>" +
              "<br>"+
              "<input type=button class='button' onClick=\"location.href='/fire/on'\" value='FIRE!'>" +
              "  </form>" +
              "<br>"
              "<p><a href='/'>Root Page</a></p>"+
              "<p><a href='/settings'>Settings Control Page</a></p>"+
              "<p><a href='/fire'>Fire Control Page</a></p>"+ 
              "<p><a href='/data'>Data Page</a></p>"+           
              "</BODY>" +
              "</HTML>" +
              "\r\n";     
    return htmlPage;
}

// prepare a web page to be send to a client (web browser)
String prepare_Fire_Settings_Page()
{
  String htmlPage =
            String("") +
            "<!DOCTYPE HTML>" +
            " <HTML>" +
            "   <HEAD>" +
            "    <TITLE>Saloon Doors Settings</TITLE>" +
            "  </HEAD>" +
            "<BODY style='font-size:300%;background-color:black;color:white'>" +
            "  <form action='/settings/action_page'>" +
            "MIN Gyro [degree/sec]:<br><input style='font-size:150%' type='number'  name='MIN_GYRO'  value= " + MIN_GYRO + "><br>" +
            "MAX Gyro [degree/sec]:<br><input style='font-size:150%' type='number'  name='MAX_GYRO'  value= " + MAX_GYRO + "><br>" +
            "MIN Fire Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='MIN_FIRE_TIME'  value= " + MIN_FIRE_TIME + "><br>" +
            "MAX Fire Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='MAX_FIRE_TIME'  value= " + MAX_FIRE_TIME + "><br>" + 
            "RESET Time Limit [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='RESET_LIMIT'  value= " + RESET_LIMIT + "><br>" + 
            "Fire Cylce Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='FIRE_CYCLE'  value= " + FIRE_CYCLE + "><br>" + 
            "Remote Fire Time [sec]:<br><input style='font-size:150%' type='number' step='0.01' name='REMOTE_FIRE_TIME'  value= " + REMOTE_FIRE_TIME + "><br><br><br>	 " + 
            "    <input type='submit' style='font-size:300%;color:red' value='UPDATE'>" +
            "  </form>" +
            "<br>"
            "<p><a href='/'>Root Page</a></p>"+
            "<p><a href='/settings'>Settings Control Page</a></p>"+
            "<p><a href='/fire'>Fire Control Page</a></p>"+
            "<p><a href='/data'>Data Page</a></p>"+
            "</BODY>" +
            "</HTML>" +
            "\r\n";
          
  return htmlPage;
}

//===============================================================
// This routine is executed when you open its IP in browser
//===============================================================
void handleRoot() {
 //String s = MAIN_page; //Read HTML contents
 String s = prepare_Root_Page();
 server.send(200, "text/html", s); //Send web page
}

//===============================================================
// This routine is executed when you open the fire control page
//===============================================================
void handle_Fire_Control_Page() {
 server.send(200, "text/html", prepare_Fire_Control_Page()); //Send web page
}

//===============================================================
// This routine is executed when you trigger the FIRE on the control page
//===============================================================
void handle_Fire_Control_ON_Page() {
 REMOTE_TRIGGER_STATE  = 0;
 server.send(200, "text/html", prepare_Fire_Control_Page()); //Send web page
}

//===============================================================
// This routine is executed when you open the fire settings page
//===============================================================
void handle_Fire_Settings_Page() {
 String s = prepare_Fire_Settings_Page();
 server.send(200, "text/html", s); //Send web page
}

//===============================================================
// This routine is executed when you open the data page
//===============================================================
void handle_Data_Page() {
 String s = prepare_Data_Page();
 server.send(200, "text/html", s); //Send web page
}

//===============================================================
// This routine is executed when you press submit
//===============================================================
void handleForm() {
 MIN_GYRO = server.arg("MIN_GYRO").toFloat(); 
 MAX_GYRO = server.arg("MAX_GYRO").toFloat(); 
 MIN_FIRE_TIME = server.arg("MIN_FIRE_TIME").toFloat(); 
 MAX_FIRE_TIME = server.arg("MAX_FIRE_TIME").toFloat();
 RESET_LIMIT = server.arg("RESET_LIMIT").toFloat(); 
 REMOTE_FIRE_TIME = server.arg("REMOTE_FIRE_TIME").toFloat(); 
 FIRE_CYCLE = server.arg("FIRE_CYCLE").toFloat(); 
 RESET_TIMER = 0;

 Serial.print("MIN_GYRO:");
 Serial.println(MIN_GYRO);
 
 Serial.print("MAX_GYRO:");
 Serial.println(MAX_GYRO);
 
 Serial.print("MIN_FIRE_TIME:");
 Serial.println(MIN_FIRE_TIME);
 
 Serial.print("MAX_FIRE_TIME:");
 Serial.println(MAX_FIRE_TIME);

 handle_Fire_Settings_Page();
}

void start_wifi(){
 WiFi.mode(WIFI_AP);
 WiFi.softAP("HighNoon","shaboinky",1,0,8);
 //WiFi.softAPConfig(local_ip, gateway, mask);
 delay(100);

  server.on("/", handleRoot);      //Which routine to handle at root location

  server.on("/settings", handle_Fire_Settings_Page);      //
  server.on("/settings/action_page", handleForm);         //form action is handled here

  server.on("/fire", handle_Fire_Control_Page);           //
  server.on("/fire/on", handle_Fire_Control_ON_Page);     //

  server.on("/data", handle_Data_Page);      //

  server.begin();
  Serial.println();
  Serial.printf("Web server started, open %s in a web browser\n", WiFi.localIP().toString().c_str());
  Serial.println();
  Serial.println("Server started.");
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());
  Serial.print("MAC:"); Serial.println(WiFi.softAPmacAddress()); 
}


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
}

void loop() {

  server.handleClient();          //Handle client requests

  //Get Big Red button and local button trigger state
    LOCAL_TRIGGER_STATE_1 = digitalRead(MANUAL_TRIGGER_1);
    LOCAL_TRIGGER_STATE_2 = digitalRead(MANUAL_TRIGGER_2);

  // Get Accerometer data:
    get_accel_data(MPU_1, ACCEL_1);
    get_accel_data(MPU_2, ACCEL_2);
    AVE_GYRO = (ACCEL_1[8] + ACCEL_2[8])/2;

    if (AVE_GYRO == 0){
        // Reset the IC2 comms is the thing craps out...
        start_I2C_communication(MPU_1);
        start_I2C_communication(MPU_2);
    }

  // Calculate fire time FIRE_TIME_LIMIT:
    FIRE_TIME_LIMIT = max(FIRE_TIME_LIMIT,MIN_FIRE_TIME+(AVE_GYRO-MIN_GYRO)*(MAX_FIRE_TIME-MIN_FIRE_TIME)/(MAX_GYRO-MIN_GYRO));

    if (REMOTE_TRIGGER_STATE == 0){
      FIRE_TIME_LIMIT = REMOTE_FIRE_TIME;
      }
    // If only one of the manual triggers is energized, use LOW fire time limit
      if (LOCAL_TRIGGER_STATE_1 == 1 && LOCAL_TRIGGER_STATE_2 == 0){
        FIRE_TIME_LIMIT = MIN_FIRE_TIME;
        }
      if (LOCAL_TRIGGER_STATE_1 == 0 && LOCAL_TRIGGER_STATE_2 == 1){
        FIRE_TIME_LIMIT = MIN_FIRE_TIME;
        }
    // If both of the manual triggers are energized, is HIGH fire time limit
      if (LOCAL_TRIGGER_STATE_1 == 0 && LOCAL_TRIGGER_STATE_2 == 0){
        FIRE_TIME_LIMIT = MAX_FIRE_TIME;
        }

    //If either the remote or the local button is pressed, trigger the fire
    //Oboard LED indicated when a trigger signal is being recieved
    if (REMOTE_TRIGGER_STATE == 0 || LOCAL_TRIGGER_STATE_1==0 || LOCAL_TRIGGER_STATE_2==0 || AVE_GYRO > MIN_GYRO){
    TRIGGER_STATE = 0;

    //Reset triggers when the TRIGGER_STATE is true
    REMOTE_TRIGGER_STATE  = 1;
    LOCAL_TRIGGER_STATE_1 = 1;
    LOCAL_TRIGGER_STATE_2 = 1;
    }
    else {
    TRIGGER_STATE = 1;
    }

  if (TRIGGER_STATE == 0 && RESET_TIMER >= RESET_LIMIT){
    FIRE_ON = 1.0;  // Turn on the Fire!!
    RESET_STATE = 0;
    RESET_TIMER = 0.0;
  }

  if (FIRE_TIMER > FIRE_TIME_LIMIT){
    FIRE_TIMER = 0.0;  // Reset the Fire timer
    FIRE_TIME_LIMIT = 0.0; // Reset the Fire time limit (will be restored from Gryo Data)
    FIRE_ON    = 0.0;  // Turn off the fire  :(
    RESET_STATE = 1;
  }
  

//  Control fire valves
  if (FIRE_ON == 1){
    // If the FIRE_CYCLE setting is set to zero - trigger both valves at the same time
      if (FIRE_CYCLE == 0.0){
        // Turn the LED on (HIGH is the voltage level)
        digitalWrite(FIRE_PIN_1, HIGH);
        digitalWrite(FIRE_PIN_2, HIGH);
      }
      else {
          FIRE_CYCLE_COUNTER = sin ( 2 * 3.14159 * FIRE_TIMER / FIRE_CYCLE);
            // If the FIRE_CYCLE_COUNTER setting is >= zero, then trigger valve #1
            if (FIRE_CYCLE_COUNTER >= 0){
              // Turn Valve #1 ON and Valve #2 OFF
              digitalWrite(FIRE_PIN_1, HIGH);
              digitalWrite(FIRE_PIN_2, LOW);
            }
            // If the FIRE_CYCLE_COUNTER setting is <= zero, then trigger valve #2
            if (FIRE_CYCLE_COUNTER <= 0){
              // Turn Valve #1 OFF and Valve #2 ON
              digitalWrite(FIRE_PIN_1, LOW);
              digitalWrite(FIRE_PIN_2, HIGH);
            }
      }
    }
    else {
      // Turn the fire off by making the voltage LOW
      digitalWrite(FIRE_PIN_1, LOW);
      digitalWrite(FIRE_PIN_2, LOW);
     }


  FIRE_TIMER = FIRE_TIMER + LOOP_RATE * FIRE_ON; // Add time to the Fire Timer
  RESET_TIMER = RESET_TIMER + LOOP_RATE * RESET_STATE; // Add time to the Fire Timer

  // Wait for next loop
  delay(1000 * LOOP_RATE);  //
}