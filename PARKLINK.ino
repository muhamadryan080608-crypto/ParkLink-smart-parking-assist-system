// ============================================================
// 1. LIBRARIES
// ============================================================
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <UniversalTelegramBot.h>
  #include <SPI.h>
  #include <MFRC522.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>           
  #include <Adafruit_SSD1306.h>
  #include <ESP32Servo.h>

// ============================================================
// 2. PIN DEFINITIONS
// ============================================================

// ---------- Parking Vacancy System ----------

//Parking 1
  const int TRIG_PIN_1 = 32;       //Ultrasonic TRIG pin 
  const int ECHO_PIN_1 = 35;       //Ultrasonic ECHO pin
  const int RED_LED_1 = 13;        //Red Tricolor RGB LED pin
  const int GREEN_LED_1 = 12;      //Green Tricolor RGB LED pin
  const int BLUE_LED_1 = 14;        //Blue Tricolor RGB LED pin

//Parking 2
  const int TRIG_PIN_2 = 33;       //Ultrasonic TRIG pin
  const int ECHO_PIN_2 = 25;       //Ultrasonic ECHO pin
  const int RED_LED_2 = 17;        //Red Tricolor RGB LED pin
  const int GREEN_LED_2 = 16;      //Green Tricolor RGB LED pin
  const int BLUE_LED_2 = 0;        //Blue Tricolor RGB LED pin

// ---------- Fire Alarm ----------

  const int THERMISTOR_PIN = 34;   //Thermistor Sensor
  const int BUZZER_PIN = 4;        //Buzzer

// ---------- Smart Light ----------

  const int LDR_PIN = 36;          //Light Dependent Resistor Module //SVP Pin is Pin 36
  const int WHITE_LED = 2;        //White LED 

// ---------- Gate Entrance ----------

  #define SCREEN_WIDTH 128         //OLED Display Dimension
  #define SCREEN_HEIGHT 64
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

  #define SS_PIN 5 //SDA           //RFID Module
  #define RST_PIN 15 //RST
  // MISO = 19
  // MOSI = 23
  // SCK = 18
  MFRC522 rfid(SS_PIN, RST_PIN);


  #define IR_PIN 26               //Infrared Sensor

  #define SERVO_PIN 27            //Servo Motor

  Servo gateServo;

// ============================================================
// 3. SYSTEM CONSTANTS
// ============================================================

// ---------- Parking Vacancy System ----------

  const float PARKING_THRESHOLD = 4.0;
  const float PARKING_MOMENT = 8.0;

// ---------- Fire Alarm ----------

  const int FIRE_THRESHOLD = 1000;
  const int FIRE_RESET_THRESHOLD = 1500;

// ---------- Smart Light ----------

  int LDR_THRESHOLD = 700;

// ---------- Gate Entrance ----------

  const int CLOSE_POS = 90;
  const int OPEN_POS = 0;

  bool waitingForRFID = false;
  bool showingWelcome = false;
  unsigned long welcomeStartTime = 0;
  const unsigned long WELCOME_TIME = 2500;
  bool gateOpened = false; 
  unsigned long noVehicleTimer = 0;
  unsigned long vehicleLeftTimer = 0;

// ---------- Timing ----------

  const unsigned long SENSOR_INTERVAL = 500;
  const unsigned long TELEGRAM_INTERVAL = 1000;

// ============================================================
// 4. USER'S CREDENTIALS
// ============================================================
  String getCardOwner() {  

  // Card 1 (White Card)
  if (rfid.uid.uidByte[0] == 0xBB &&
      rfid.uid.uidByte[1] == 0xAC &&
      rfid.uid.uidByte[2] == 0xF1 &&
      rfid.uid.uidByte[3] == 0x06)
  {
    return "USER 1";
  }

  // Card 2 (Blue Keychain)
  if (rfid.uid.uidByte[0] == 0xFA &&
      rfid.uid.uidByte[1] == 0xF0 &&
      rfid.uid.uidByte[2] == 0xC7 &&
      rfid.uid.uidByte[3] == 0x06)
  {
    return "USER 2";
  }

  // Card 3 (Student Card)
  if (rfid.uid.uidByte[0] == 0x2F &&
      rfid.uid.uidByte[1] == 0xD2 &&
      rfid.uid.uidByte[2] == 0x7F &&
      rfid.uid.uidByte[3] == 0xBA)
  {
    return "USER 3";
  }

  // Card 4 (Student Card)
  if (rfid.uid.uidByte[0] == 0x6F &&
      rfid.uid.uidByte[1] == 0xDF &&
      rfid.uid.uidByte[2] == 0xB8 &&
      rfid.uid.uidByte[3] == 0x84)
  {
    return "USER 4";
  }

  // Card 5 (Student Card)
  if (rfid.uid.uidByte[0] == 0xAF &&
      rfid.uid.uidByte[1] == 0x98 &&
      rfid.uid.uidByte[2] == 0x6D &&
      rfid.uid.uidByte[3] == 0xAA)
  {
    return "USER 5";
  }

   // Card 6 (Hotel Keycard)
  if (rfid.uid.uidByte[0] == 0x4A &&
      rfid.uid.uidByte[1] == 0x5 &&
      rfid.uid.uidByte[2] == 0x95 &&
      rfid.uid.uidByte[3] == 0xAA)
  {
    return "USER 6";
  }

 return "";
  
  }

// ============================================================
// 5. WIFI & TELEGRAM (CONFIDENTIAL)
// ============================================================

  const char* ssid = "YOUR WIFI SSID NAME";
  const char* password = "YOUR WIFI SSID PASSWORD";

  const char* botToken = "YOUR TELEGRAM BOT TOKEN";
  String chatId = "YOUR TELEGRAM CHAT ID";

  WiFiClientSecure client;
  UniversalTelegramBot bot(botToken, client);

// ============================================================
// 6. GLOBAL VARIABLES
// ============================================================

// ---------- Parking Vacancy System ----------

  float parkingDistance1 = -1;
  float parkingDistance2 = -1;

  unsigned long parkingStartTime1 = 0;
  bool towingWarningSent1 = false;
  unsigned long parkingStartTime2 = 0;
  bool towingWarningSent2 = false;
  const unsigned long IMPROPER_PARKING_TIME = 5000; // 5 seconds

// ---------- Fire Alarm ----------

  int thermistorValue = analogRead(THERMISTOR_PIN);
  bool fireAlarm = false;


// ---------- Timers ----------

  unsigned long lastSensorUpdate = 0;
  unsigned long lastTelegramCheck = 0;

// ============================================================
// 7. ULTRASONIC SENSOR FUNCTIONS
// ============================================================

  float getDistance1() {      //Create function for receiving distance using ultrasonic sensor for parking 1

    digitalWrite(TRIG_PIN_1, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN_1, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN_1, LOW);

    long duration = pulseIn(
        ECHO_PIN_1,
        HIGH,
        30000
    );

    if (duration == 0) {
        return -1;
    }

    return duration * 0.0343 / 2.0;
  }


  float getDistance2() {      //Create function for receiving distance using ultrasonic sensor for parking 2

    digitalWrite(TRIG_PIN_2, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN_2, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN_2, LOW);

    long duration = pulseIn(
        ECHO_PIN_2,
        HIGH,
        30000
    );

    if (duration == 0) {
        return -1;
    }

    return duration * 0.0343 / 2.0;
  }

// ============================================================
// 8. SENSOR UPDATE
// ============================================================

  void updateSensors() {      //Create function for Updating distance using ultrasonic sensor for parking 1 and parking 2

    if (millis() - lastSensorUpdate >= SENSOR_INTERVAL) {

        parkingDistance1 = getDistance1();

       // delay(50);

        parkingDistance2 = getDistance2();

        thermistorValue = analogRead(THERMISTOR_PIN);

        lastSensorUpdate = millis();
    }
  }

// ============================================================
// 9. PARKING SYSTEM
// ============================================================

  String getParkingStatus(float distance) {      //Create situations where different distances create different output for parking 1 and parking 2

    if (distance < 0) {
        return "SENSOR ERROR⚠️";
    }

    if (distance > PARKING_MOMENT) {
        return "VACANT🟢";
    }

    if (distance > PARKING_THRESHOLD) {
        return "A VEHICLE IS PARKING🔵";
    }

    return "OCCUPIED🔴";
  }


  void updateParkingSystem() {      //Create function for Updating parking vacancy status parking 1 and parking 2

    String parking1Status =
        getParkingStatus(parkingDistance1);

    String parking2Status =
        getParkingStatus(parkingDistance2);
  }

// ============================================================
// 10. TELEGRAM COMMANDS
// ============================================================

  void handleCommand(String command) {   //Create telegram /start command for users

    command.toLowerCase();
    command.trim();


    // ---------- START ----------

    if (command == "/start") {

        String message =
            "WELCOME TO NIGHT CITY MALL "
            "SMART PARKING SYSTEM😁\n\n"
            "*Empowered by ParkLink⚡️\n\n"
            " \n\n"
            "Available commands📝:\n"
            "/status - Parking status🔴🔵🟢";

        bot.sendMessage(
            chatId,
            message,
            ""
        );
    }


    // ---------- STATUS ----------

    else if (command == "/status") {   //Create /status command to acknowledge the vacancy of a parking spot

        String message =
            "NIGHT CITY MALL PARKING STATUS🌌\n\n";

        message += "PARKING 1 : ";
        message += getParkingStatus(
            parkingDistance1
        );

        message += "\nDistance: ";

        if (parkingDistance1 >= 0) {
            message += String(
                parkingDistance1,
                1
            );
            message += " cm";
        }
        else {
            message += "N/A";
        }


        message += "\n\nPARKING 2 : ";
        message += getParkingStatus(
            parkingDistance2
        );

        message += "\nDistance: ";

        if (parkingDistance2 >= 0) {
            message += String(
                parkingDistance2,
                1
            );
            message += " cm";
        }
        else {
            message += "N/A";
        }


        bot.sendMessage(
            chatId,
            message,
            ""
        );
    }


    // ---------- UNKNOWN ----------

    else {

        bot.sendMessage(
            chatId,
            "Unknown command.\nUse /start",
            ""
        );
    }
  }


  void checkTelegram() {          //Allow Telegram to check recent messages for overlapped updates using message counts

    if (
        millis() - lastTelegramCheck
        >= TELEGRAM_INTERVAL
    ) {

        int messageCount =
            bot.getUpdates(
                bot.last_message_received + 1
            );


        while (messageCount) {

            for (int i = 0; i < messageCount; i++) {

                String incomingChatId =
                    bot.messages[i].chat_id;

                String command =
                    bot.messages[i].text;


                if (incomingChatId == chatId) {

                    handleCommand(command);
                }
            }


            messageCount =
                bot.getUpdates(
                    bot.last_message_received + 1
                );
        }


        lastTelegramCheck = millis();
    }
  }

void setup() {                                //This Function setup will only run once
  // put your setup code here, to run once:
  
  Serial.begin(115200);                       //Allow Serial communication to run (115200 Baudrate)

// ---------- Parking Vacancy System ----------

  pinMode(TRIG_PIN_1,OUTPUT);
  pinMode(ECHO_PIN_1,INPUT);
  pinMode(RED_LED_1,OUTPUT);
  pinMode(GREEN_LED_1,OUTPUT);
  pinMode(BLUE_LED_1,OUTPUT);

  pinMode(TRIG_PIN_2,OUTPUT);
  pinMode(ECHO_PIN_2,INPUT);
  pinMode(RED_LED_2,OUTPUT);
  pinMode(GREEN_LED_2,OUTPUT);
  pinMode(BLUE_LED_2,OUTPUT);

// ---------- Fire Alarm ----------

  pinMode(BUZZER_PIN,OUTPUT);

// ---------- Smart Light ----------

  pinMode(WHITE_LED,OUTPUT);

// ---------- Gate Entrance ----------

  pinMode(IR_PIN,INPUT);

  gateServo.attach(SERVO_PIN);
  gateServo.write(CLOSE_POS);

  Wire.begin(21,22);           //Pin definitions for OLED Display SCL=22,SDA=21

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("OLED Failed");
    while(1);
  }

  display.clearDisplay();
  display.display();

  SPI.begin();
  rfid.PCD_Init(); 

  showIdle();

    // ---------- WiFi ----------

    Serial.println();
    Serial.println("Connecting to WiFi...");

    WiFi.begin(
        ssid,
        password
    );

    while (
        WiFi.status() != WL_CONNECTED
    ) {

        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi Connected!");


    // ---------- Telegram ----------

    client.setInsecure();

    bot.sendMessage(
        chatId,
        "NIGHT CITY MALL SMART PARKING SYSTEM ONLINE\n\n"
        "SMART PARKING SYSTEM😁\n\n"
        "*Empowered by ParkLink⚡️\n\n"
        " \n\n"
        "Available commands📝:\n"
         "/status - Parking status🔴🔵🟢",
        ""
    );

// ---------- Serial Monitor Warm-up ----------

  Serial.println("SMART PARKING SYSTEM LOADING.....");
  delay(100);
  Serial.println("SMART PARKING SYSTEM LOAD COMPLETE!");  
  delay(100);
  Serial.println("RUNNING....."); 
  delay(100);

  Serial.println("--------------------------------------------------------------------------------");
  Serial.printf("%-12s %-12s %-18s %-20s\n",
              "PARKING 1",
              "PARKING 2",
              "BRIGHTNESS",     
              "FIRE HAZARD");
  Serial.println("--------------------------------------------------------------------------------");

  }

void loop() {
  // put your main code here, to run repeatedly:

// ---------- Parking Vacancy System ----------

  
  parkingDistance1 = getDistance1();
  parkingDistance2 = getDistance2();


// =========================
//--> PARKING 1 LED
// =========================

// PARKING SPOT 1 LED + IMPROPER PARKING WARNING

digitalWrite(GREEN_LED_1, LOW);
digitalWrite(BLUE_LED_1, LOW);
digitalWrite(RED_LED_1, LOW);

if (parkingDistance1 > PARKING_MOMENT) {

  // VACANT
  digitalWrite(GREEN_LED_1, HIGH);

  parkingStartTime1 = 0;
  towingWarningSent1 = false;

}

else if (parkingDistance1 > PARKING_THRESHOLD) {

  // A VEHICLE IS PARKING
  digitalWrite(BLUE_LED_1, HIGH);

  // Start timer when vehicle enters parking process
  if (parkingStartTime1 == 0) {
    parkingStartTime1 = millis();
  }

  // Vehicle has been stuck in parking process too long
  if (!towingWarningSent1 &&
      millis() - parkingStartTime1 >= IMPROPER_PARKING_TIME) {

    String message =
      "⚠️TOWING WARNING⚠️\n"
      "❌Parking Spot 1: Vehicle may be improperly parked.\n"
      "❌Your vehicle congest the one way road\n"
      "⭕️Please park your vehicle properly or it will be towed away"
      "💸Violators will be fined for up to RM200";
      
      bot.sendMessage(
            chatId,
            message,
            ""

    );
    
    towingWarningSent1 = true;
  }

}

else {

  // OCCUPIED
  digitalWrite(RED_LED_1, HIGH);

  parkingStartTime1 = 0;
  towingWarningSent1 = false;
}

// =========================
//--> PARKING 2 LED
// =========================

// PARKING SPOT 2 LED + IMPROPER PARKING WARNING

digitalWrite(GREEN_LED_2, LOW);
digitalWrite(BLUE_LED_2, LOW);
digitalWrite(RED_LED_2, LOW);

if (parkingDistance2 > PARKING_MOMENT) {

  // VACANT
  digitalWrite(GREEN_LED_2, HIGH);

  parkingStartTime2 = 0;
  towingWarningSent2 = false;

}

else if (parkingDistance2 > PARKING_THRESHOLD) {

  // A VEHICLE IS PARKING
  digitalWrite(BLUE_LED_2, HIGH);

  // Start timer when vehicle enters parking process
  if (parkingStartTime2 == 0) {
    parkingStartTime2 = millis();
  }

  // Vehicle has been stuck in parking process too long
  if (!towingWarningSent2 &&
      millis() - parkingStartTime2 >= IMPROPER_PARKING_TIME) {

    String message =
      "⚠️TOWING WARNING⚠️\n"
      "❌Parking Spot 2: Vehicle may be improperly parked.\n"
      "❌Your vehicle congest the one way road\n"
      "⭕️Please park your vehicle properly or it will be towed away\n\n"
      "💸Violators will be fined for up to RM200";
      
      bot.sendMessage(
            chatId,
            message,
            ""
    );
    
    towingWarningSent2 = true;
  }

}

else {

  // OCCUPIED
  digitalWrite(RED_LED_2, HIGH);

  parkingStartTime2 = 0;
  towingWarningSent2 = false;
}
// ============================================================
//---> FIRE DETECTION
// ============================================================

  int thermistorValue = analogRead(THERMISTOR_PIN);
    if (thermistorValue <= FIRE_THRESHOLD) {

    digitalWrite(BUZZER_PIN, HIGH);

        if (!fireAlarm) {              //Set situation where fire is detected by the sudden increase temperature in thermistor

            String message =
                "🔥 FIRE WARNING\n\n"
                " \n\n"
                "High temperature detected "
                "in the parking area.\n\n"
                " \n\n"
                "⚠️ DO NOT USE ELEVATOR\n\n"
                "⚠️ USE EMERGENCY STAIRCASE PROVIDED\n\n"
                "⚠️ EVERYONE MUST GATHER AT ASSEMBLY POINT\n\n"
                " \n\n"
                "Thermistor reading: " +
                String(thermistorValue);

            bot.sendMessage(
                chatId,
                message,
                ""
            );

            fireAlarm = true;
        }
    }

    else if (                          //Set situation where fire is cleared by the drop in temperature in thermistor
        fireAlarm &&
        thermistorValue >= FIRE_RESET_THRESHOLD
    ) {

        digitalWrite(BUZZER_PIN, LOW);

        bot.sendMessage(
            chatId,
            "✅ FIRE ALERT CLEARED\n\n"
            " \n\n"
            "⚠️ DO NOT RETURN INTO THE BUILDING\n\n"
            "⚠️ WAIT FOR FURTHER INSTRUCTIONS BY THE AUTHORITIES\n\n"
            " \n\n"
            "Temperature has returned "
            "to a safe level.\n\n"
            "\n\n"
            "Thermistor reading: " +
                String(thermistorValue),
            ""
        );

        fireAlarm = false;
    }

    else if (!fireAlarm) {

        digitalWrite(BUZZER_PIN, LOW);
    }

  checkTelegram();

// ============================================================
//---> SMART LIGHT
// ============================================================

  int LDR_VALUE;                           
  LDR_VALUE = analogRead(LDR_PIN);
  float brightness = map(LDR_VALUE,4095,0,1000,0);

  if(brightness >= LDR_THRESHOLD)                    //Set situation where the white LED will turn on when the garage is too dark
  {
  digitalWrite(WHITE_LED,HIGH);
  }
  else                                   
  {
  digitalWrite(WHITE_LED,LOW);
  }

// ============================================================
//---> SERIAL MONITOR DATA
// ============================================================

  Serial.printf("%-12.1f %-12.1f %-18d %-20.1f\n",
              parkingDistance1,
              parkingDistance2,
              analogRead(LDR_PIN),        
              analogRead(THERMISTOR_PIN));
              delay(100);
// ============================================================
//---> GATE ENTRANCE
// ============================================================

 bool vehiclePresent = digitalRead(IR_PIN) == LOW;  //Pull the Infrared sensor input pin LOW


// ============================================================
// VEHICLE ARRIVES
// ============================================================

  //Set situation where vehicle is detected by Infrared sensor so welcome screen will be displayed

  if (vehiclePresent && !waitingForRFID && !showingWelcome && !gateOpened)
  {
    welcomeScreen();

    showingWelcome = true;
    welcomeStartTime = millis();
  }


// ============================================================
// WELCOME SCREEN → REQUEST CARD
// ============================================================

  //Set situation where request card screen will be displayed after welcome screen

  if (showingWelcome)
  {
    if (millis() - welcomeStartTime >= WELCOME_TIME)
    {
      requestCard();

      showingWelcome = false;
      waitingForRFID = true;
    }
  }


// ============================================================
// GATE ALREADY OPEN
// ============================================================

  //Set situation where the gate will closed if theres no vehicle detected by the Infrared module sensor after some time

  if (gateOpened)
  {
    if (!vehiclePresent)
    {
      if (noVehicleTimer == 0)
      {
        noVehicleTimer = millis();
      }

      if (millis() - noVehicleTimer >= 2000)
      {
        gateServo.write(CLOSE_POS);

        gateOpened = false;
        noVehicleTimer = 0;

        showIdle();
      }
    }
    else
    {
      noVehicleTimer = 0;
    }
  }


// ============================================================
// RFID
// ============================================================

  //Set situation where vehicle with authorised keycard detected by RFID module will open the gate and vice versa

  if (waitingForRFID)
  {
    if (rfid.PICC_IsNewCardPresent() &&
        rfid.PICC_ReadCardSerial())
    {
      String owner = getCardOwner();

      if (owner != "")
      {
        // AUTHORIZED
        thankYouScreen(owner);

        gateServo.write(OPEN_POS);

        gateOpened = true;
        waitingForRFID = false;
        vehicleLeftTimer = 0;
      }
      else
      {
        // UNAUTHORIZED
        accessdenied();
        //delay(1000);

        waitingForRFID = false;
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }


// ============================================================
// VEHICLE LEAVES BEFORE RFID
// ============================================================

  //Set situation where vehicle leaves before tapping their keycards

    if (!vehiclePresent)
    {
      if (vehicleLeftTimer == 0)
      {
        vehicleLeftTimer = millis();
      }

      if (millis() - vehicleLeftTimer >= 3000)
      {
        waitingForRFID = false;
        vehicleLeftTimer = 0;

        showIdle();
      }
    }
    else
    {
      vehicleLeftTimer = 0;
    }
  }
  

}

// ============================================================
//--> OLED Display Settings
// ============================================================

void welcomeScreen()
  {

  //Welcome text
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(34,20);
  display.println("WELCOME TO");     

  display.setCursor(19,30);
  display.println("NIGHT CITY MALL");

  // Smiley
  display.drawCircle(64,52,10,WHITE);
  display.fillCircle(60,49,1,WHITE);
  display.fillCircle(68,49,1,WHITE);
  display.drawLine(60,55,62,57,WHITE);
  display.drawLine(62,57,66,57,WHITE);
  display.drawLine(66,57,68,55,WHITE);

  display.display();
  }

void requestCard()
  {

  //Request card text
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(28,20);
  display.println("PLEASE TOUCH");  
  display.setCursor(28,35);
  display.println("YOUR KEYCARD");

  display.drawLine(64, 40, 64, 56, SSD1306_WHITE);
  display.drawLine(60, 52, 64, 56, SSD1306_WHITE);
  display.drawLine(68, 52, 64, 56, SSD1306_WHITE);

  display.display();
  }

void thankYouScreen(String owner)
  {

  //Thank you text
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(37,20);               
  display.println("THANK YOU");

  // Center the RFID owner's name
  int nameX = (128 - owner.length() * 6) / 2;
  display.setCursor(nameX,30);
  display.println(owner);

  display.setCursor(19,40);
  display.println("HAVE A NICE DAY");

  display.display();
  }

void showIdle()
  {

  //Idle screen
  display.clearDisplay();

  display.setTextSize(1);            
  display.setCursor(25,25);
  display.println("SYSTEM READY");
  display.setCursor(1,40);
  display.println("POWERED BY PARKLINK");

  display.display();
  }

void accessdenied()
{

  //Access denied screen
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(13, 25);
  display.println("UNAUTHORISED USER");

  display.display();

}