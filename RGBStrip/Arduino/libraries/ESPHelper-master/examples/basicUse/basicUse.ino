/*    
basicUse.ino
Copyright (c) 2016 ItKindaWorks All right reserved.
github.com/ItKindaWorks

This file is part of ESPHelper

ESPHelper is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ESPHelper is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ESPHelper.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ESPHelper.h"

netInfo homeNet = {.name = "NETWORK NICKNAME", .mqtt = "YOUR MQTT-IP", .ssid = "YOUR SSID", .pass = "YOUR NETWORK PASS"};

ESPHelper myESP(&homeNet);

void setup() {
	
	Serial.begin(115200);	//start the serial line
	delay(500);

	Serial.println("Starting Up, Please Wait...");

	myESP.addSubscription("/test");

	myESP.begin();
	myESP.setCallback(callback);   //can only set callback after begin method. Calling before begin() will not set the callback (return false)
	
	Serial.println("Initialization Finished.");
}

void loop(){
	myESP.loop();  //run the loop() method as often as possible - this keeps the network services running

	//Put application code here

	yield();
}

void callback(char* topic, uint8_t* payload, unsigned int length) {
	//put mqtt callback code here
}