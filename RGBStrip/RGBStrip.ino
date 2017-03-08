#include <ESPHelper.h>
#include <HSBColor.h>

#define TOPIC "/light/rgb"
#define STATUS TOPIC "/status"
#define NETWORK_HOSTNAME "rgblight"
#define OTA_PASSWORD "_____" //set you OTA password

#define RED_PIN D1    //set the r,g,b, pin for your esp8266
#define GREEN_PIN D2
#define BLUE_PIN D3
#define BLINK_PIN D4

typedef struct lightState{
  double hue;
  double saturation;
  double brightness;
  int red;
  int redRate;
  int green;
  int greenRate;
  int blue;
  int blueRate;
  int fadePeriod;
  int updateType;
};


lightState nextState;

boolean newCommand = false;


char* lightTopic = TOPIC;
char* statusTopic = STATUS;
char* hostnameStr = NETWORK_HOSTNAME;

const int redPin = RED_PIN;
const int greenPin = GREEN_PIN;
const int bluePin = BLUE_PIN;
const int blinkPin = BLINK_PIN;

char statusString[50];  //string containing the current setting for the light


//set this info for your own network
netInfo homeNet = {.name = "___", .mqtt = "___", .ssid = "___", .pass = "___"};

ESPHelper myESP(&homeNet);

void setup() {
  //initialize the light as an output and set to LOW (off)
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(greenPin, OUTPUT);

  //all off
  digitalWrite(redPin, LOW);  //all off
  digitalWrite(greenPin, LOW);
  digitalWrite(bluePin, LOW);

  delay(1000);

  
  //setup ota on esphelper
  myESP.OTA_enable();
  myESP.OTA_setPassword(OTA_PASSWORD);
  myESP.OTA_setHostnameWithVersion(hostnameStr);

  myESP.enableHeartbeat(blinkPin);
  //subscribe to the lighttopic
  myESP.addSubscription(lightTopic);
  myESP.begin();
  myESP.setCallback(callback);
}


void loop() {

  static bool connected = false; //keeps track of connection state


  if(myESP.loop() == FULL_CONNECTION){

    //if the light was previously not connected to wifi and mqtt, update the status topic with the light being off
    if(!connected){
      connected = true; //we have reconnected so now we dont need to flag the setting anymore
      myESP.publish(statusTopic, "h0.00,0.00,0.00", true);
    }

  lightHandler();
  }

  
  yield();
}

void lightHandler(){
  //new and current lightStates
  static lightState newState;
  static lightState currentState;

  lightUpdater(&newState, currentState);
  lightChanger(newState, &currentState);
}

void lightChanger(lightState newState, lightState *currentState){
  
  currentState->red =newState.red;
  currentState->green =newState.green;
  currentState->blue =newState.blue;
  analogWrite(redPin, currentState->red);
  analogWrite(greenPin, currentState->green);
  analogWrite(bluePin, currentState->blue);
}

void lightUpdater (lightState *newState, lightState currentState){

  //calculate new vars only if there is a new command
  if (newCommand){

      //convert from HSB to RGB
      int newRGB[3];
      H2R_HSBtoRGBfloat(nextState.hue, nextState.saturation, nextState.brightness, newRGB);
      newState->red = newRGB[0];
      newState->green = newRGB[1];
      newState->blue = newRGB[2];
    }
}

//MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {

  //convert topic to string to make it easier to work with
  String topicStr = topic; 

  char newPayload[40];
  memcpy(newPayload, payload, length);
  newPayload[length] = '\0';

  //handle HSB updates
  if(payload[0] == 'h'){
    nextState.hue = atof(&newPayload[1]);
    nextState.saturation = atof(&newPayload[7]);
    nextState.brightness = atof(&newPayload[13]);

    newCommand = true;
    
    }
//package up status message reply and send it back out to the status topic
  strcpy(statusString, newPayload);
  myESP.publish(statusTopic, statusString, true);
}
