#include <ESP8266WiFi.h>
#include <ThingerESP8266.h>
#include <Servo.h>

#define ALARMSPEAKER 1
#define ALARMON      LOW
#define ALARMOFF     HIGH
#define BUZZER       D1 
#define REDLED       D2
#define YELLOWLED    D3
#define GREENLED     D4

// Configuration
#define ssid     "YourWiFiSSID"
#define password "YourWiFiPassword"
#define LEVEL2DEGREE   1.8
#define MILLISTOWAIT   250    // Controls rate of drain
#define MILLISTOREPORT 6000
#define MUTEALARM      0      // 1 = Mute On, 0 = Mute Off

const char* thingerUsername         = "YourThingerUsername";         // Thinger.io account username
const char* thingerDeviceCredential = "YourThingerDeviceCredential"; // NOT Thinger.io account password

// Define Thinger
ThingerESP8266 *thing;
String myDeviceBaseName = "esp8266_sn_iot";  
char deviceName[64];

Servo myservo;  // create servo object to control a servo

// States to control
bool resetPressed = false;  // Button
bool alert        = false;
bool outage       = false;
bool draining     = true;
bool step1        = false;
int fuelLevel     = 100;
unsigned long tMoved;

// Runtime parameters
unsigned int millis_to_wait     = MILLISTOWAIT;
float        level_to_degree    = LEVEL2DEGREE;
unsigned int warning_fuel_level = 20;
unsigned int mute_alarm         = MUTEALARM;

void connectToWiFi() {
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  myservo.attach(D8);
  tMoved = millis();

  pinMode(YELLOWLED, OUTPUT);
  pinMode(GREENLED,  OUTPUT);
  pinMode(REDLED,    OUTPUT);
  pinMode(BUZZER,    OUTPUT); 
  pinMode(D6,        OUTPUT);
  pinMode(D7,        INPUT_PULLUP); //Reset Button
  myservo.attach(D8);  // attaches the servo on pin D8 to the servo object

  pinMode(BUILTIN_LED, OUTPUT); 

  connectToWiFi();
  
  myDeviceBaseName.toCharArray( deviceName, myDeviceBaseName.length() + 1 );
  Serial.print( "ThingerWifi |" );
  Serial.print( deviceName );
  Serial.println( "|" );
 
  thing =  new ThingerESP8266(thingerUsername, deviceName, thingerDeviceCredential);

  (*thing)["led"] << digitalPin(BUILTIN_LED);

  (*thing)["draining"] << [](pson & in) {
    if (in.is_empty()) {
      in = true;
    }
    else {
      if (in) {
        draining = true;
      } else {
        draining = false;
      }
    }
  };

  (*thing)["refill"] << [](pson & in) {
    if (in.is_empty()) {
      in = false;
    }
    else {
      if (in) {
        doRefill();
      }
    }
  };

  (*thing)["mute"] << [](pson & in) {
    if (in.is_empty()) {
      in = (bool) mute_alarm;
    }
    else {
      if (in) {
        Serial.println("Mute");
        mute_alarm = 1;
      } else {
        Serial.println("UnMute");
        mute_alarm = 0;
      }
    }
  };
}

void doRefill() {
  Serial.println("Fuel set to 100");
  fuelLevel = 100;
  tMoved    = millis();
  alert     = false;
  outage    = false;

  // Message to Instance
  pson myMessage;
  myMessage["u_message"]        = "Coolant was refilled!";
  myMessage["u_device_id"]      = myDeviceBaseName;
  myMessage["u_sensor"]         = "Coolant level";
  myMessage["u_sensor_reading"] = fuelLevel;
  (*thing).call_endpoint(deviceName, myMessage);

}

void doLED() {
  if (fuelLevel > warning_fuel_level) {
    digitalWrite(GREENLED,  1);
    digitalWrite(YELLOWLED, 0);
    digitalWrite(REDLED,    0);
    digitalWrite(D1,        0);

  }
  else {
    if (fuelLevel > 0) {
      digitalWrite(GREENLED,  0);
      digitalWrite(YELLOWLED, 1);
    } else {
      digitalWrite(REDLED,    1);
      digitalWrite(YELLOWLED, 0);
      digitalWrite(D1,        !mute_alarm);
    }
  }
}

void loop() {
  (*thing).handle();

  if (digitalRead(D7) && !resetPressed) {
    Serial.print("PotiRead: ");
    Serial.println(analogRead(A0));
    resetPressed = true;

    if (step1 && analogRead(A0) < 50 ) {
      doRefill();
      step1 = false;
    }
    if (analogRead(A0) > 800 && !step1) {
      step1 = true;
    } else {
      step1 = false;
    }
  }

  if (!digitalRead(D7) && resetPressed) {
    resetPressed = false;
    Serial.println("Setting resetPressed to 'false'"); 
  }

  if (draining && fuelLevel > 0 && (millis() - tMoved) > millis_to_wait) {
    fuelLevel--;
    tMoved = millis();
  }
  doLED();

  myservo.write((int) fuelLevel * level_to_degree);

  if (fuelLevel < warning_fuel_level && !alert) {
    Serial.println("Coolant warning threshold"); 
    alert = true;
    pson myMessage;
    myMessage["u_message"] = "Coolant level has reached a warning threshhold - Please refill!";
    myMessage["u_device_id"] = myDeviceBaseName;
    myMessage["u_sensor"] = "Coolant level";
    myMessage["u_sensor_reading"] = fuelLevel;
    (*thing).call_endpoint(deviceName, myMessage);
  }

  if (fuelLevel == 0 && !outage) {
    Serial.println("Coolant empty");
    outage = true;
    pson myMessage;
    myMessage["u_message"] = "Coolant is empty! Shutting down device!";
    myMessage["u_device_id"] = myDeviceBaseName;
    myMessage["u_sensor"] = "Coolant level";
    myMessage["u_sensor_reading"] = fuelLevel;
    (*thing).call_endpoint(deviceName, myMessage);
  }
}
