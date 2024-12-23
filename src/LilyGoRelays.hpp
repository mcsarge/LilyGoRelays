/**
 *
 * @license MIT License
 *
 * Copyright (c) 2024 Matthew Sargent
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      LilyGoRelays.hpp
 * @author    Matthew Sargent (matthew.c.sargent@gmail.com)
 * @date      2024-03-12
 *
 */


#include <Arduino.h>
#include <ArduinoJson.h>
#include <ShiftRegister74HC595_NonTemplate.h>
#include <esp_log.h>
#include "LilyGoRelaysConstants.h"


#pragma once

static const char* LILYGOTAG = "LilyGoRelays";

class LilygoRelays
{
public:
    typedef enum {
        Lilygo4Relays,
        Lilygo8Relays,
        Lilygo6Relays
    } RelayType;

    typedef struct { 
        long on_duration;
        long off_duration;
        unsigned long last_set_time;
    } RelayLed;

        /*
        The Class for a single relay
        */
        class lilygoRelay
        {
        public:

            String relayName;
            int32_t momentaryDuration = -1 ;

            lilygoRelay(){
                index = 0;
                relayName = "";
                momentaryDuration = -1;
                outer = NULL;
                userData = "";
            }

            int getRelayStatus(){
                //Check for bad index or not initialized
                if (outer == NULL){
                    ESP_LOGE(LILYGOTAG, "Outer not defined");
                    return -1;
                }

                if (outer->__relayType==Lilygo6Relays){
                    return outer->__control->get(relayAddress);
                } else {
                    return digitalRead(relayAddress);
                }
            }

            void setRelayStatus(int status){
                //Check for bad index or not initialized
                if (outer == NULL){
                    ESP_LOGE(LILYGOTAG, "Outer not defined");
                    return;
                }

                if (getRelayStatus() != status){
                    if (outer->__relayType==Lilygo6Relays){
                        outer->__control->set(relayAddress, status==1?HIGH:LOW);
                    } else {
                        digitalWrite(relayAddress, status==1?HIGH:LOW);
                    }

                    if(momentaryDuration>0){
                        lastSetMillis = millis();
                    }
                    if(outer->__relayUpdatedCB != NULL){
                        (*outer->__relayUpdatedCB)(index, status==1?HIGH:LOW); //let them know it changed
                    }
                }

            }
            String getRelayFixedShortName(){
                char str[5];
                snprintf(str, 5, "r%d", index+1);
                return str;
            }
            String getRelayFixedName(){
                char str[10];
                snprintf(str, 10, "Relay %d", index+1);
                return str;
            }

            String getUserData(){
                return userData;
            }

            void setUserData(String data){
                if (data.length() > LILYGORELAY_USER_DATA_MAX){
                    userData = data.substring(0, LILYGORELAY_USER_DATA_MAX-1);
                    ESP_LOGW(LILYGOTAG, "userData too long, truncated to %d", LILYGORELAY_USER_DATA_MAX);
                } else {
                    userData = data;
                }
            }

        private:
        protected:
            int index;
            int relayAddress;
            unsigned long lastSetMillis = 0;
            LilygoRelays* outer;
            String userData = "";

            void setOuter(LilygoRelays* _outer){
                outer = _outer;
            }

            bool loop(){
                if((lastSetMillis+(momentaryDuration*1000)) < millis()){
                    setRelayStatus(LOW);
                    return true;
                }
                return false;
            }
        
            friend class LilygoRelays;
        };

    LilygoRelays()
    {
        __relayType = Lilygo4Relays;
        __banks = 1;
        __numberOfRelays = 4;
        __numberOfLEDs = 1;
        __relaysPerBank = 4;
        this->__relayUpdatedCB = NULL;

        //__relays = (relayArray*)malloc(sizeof(singleRelay)*__numberOfRelays);
        __relays = new lilygoRelay[__numberOfRelays];
        __relays[0].relayAddress = LILYGORELAY4_RELAY1_PIN;
        __relays[1].relayAddress = LILYGORELAY4_RELAY2_PIN;
        __relays[2].relayAddress = LILYGORELAY4_RELAY3_PIN;
        __relays[3].relayAddress = LILYGORELAY4_RELAY4_PIN;
        
        //Set other default values.
        for (int relay=0;relay<__numberOfRelays; relay++){
            __relays[relay].relayName = "Relay " + String(relay+1); //Names start at 1.
            __relays[relay].momentaryDuration = -1;
            //__relays[relay].setLastSetMillis = 0;
        }
    }

    LilygoRelays(RelayType relayType, int banks=1)
    {
        this->__relayUpdatedCB = NULL;
        __relayType = relayType;
        __banks = 1;

        if (relayType == Lilygo6Relays){
            if ((banks <= LILYGORELAY6_BANKS_MAX) && (banks >= LILYGORELAY6_BANKS_MIN)){
                __banks = banks;
            }
            __relaysPerBank = 6;
            __control = new ShiftRegister74HC595_NonTemplate(8*__banks, LILYGORELAY6_SHIFT_DATA_PIN, LILYGORELAY6_SHIFT_CLOCK_PIN, LILYGORELAY6_SHIFT_LATCH_PIN);
            __control->setAllLow();

            __numberOfRelays = __relaysPerBank*__banks;
            __relays = new lilygoRelay[__numberOfRelays];
            //set the relay address
            //each bank has 8 spots, the last 2 are for the LEDs in bank 0 and unused in the other banks. need to skip them
            // so they go like this:
            // Relay Number: 0  1  2  3  4  5        6  7  8  9 10 11       12 13 14 15 16 17       18 19 20 21 22 23    
            // Bank          0  0  0  0  0  0  0  0  1  1  1  1  1  1  1  1  2  2  2  2  2  2  2  2  3  3  3  3  3  3  3  3  
            // Bank addr     0  1  2  3  4  5  6  7  0  1  2  3  4  5  6  7  0  1  2  3  4  5  6  7  0  1  2  3  4  5  6  7  
            // Shift R Addr  0  1  2  3  4  5  L  L  8  9 10 11 12 13  B  B 16 17 18 19 20 21  B  B 24 25 26 26 28 29  B  B
            for (int bank=0;bank<__banks; bank++){
                for (int relay=0; relay<__relaysPerBank;relay++){
                    int bankRelayAddress = (bank*8) + relay;
                    int relayNumber = (bank*__relaysPerBank) + relay; 
                    __relays[relayNumber].relayAddress = bankRelayAddress;
                }
            }

        } else if (relayType == Lilygo4Relays) {
            __numberOfRelays = 4;
            __relaysPerBank = 4;
            __relays = new lilygoRelay[__numberOfRelays];
            //set the relay address
            __relays[0].relayAddress = LILYGORELAY4_RELAY1_PIN;
            __relays[1].relayAddress = LILYGORELAY4_RELAY2_PIN;
            __relays[2].relayAddress = LILYGORELAY4_RELAY3_PIN;
            __relays[3].relayAddress = LILYGORELAY4_RELAY4_PIN;

        } else if (relayType == Lilygo8Relays){
            __numberOfRelays = 8;
            __relaysPerBank = 8;
            __relays = new lilygoRelay[__numberOfRelays];
            //set the relay address
            __relays[0].relayAddress = LILYGORELAY8_RELAY1_PIN;
            __relays[1].relayAddress = LILYGORELAY8_RELAY2_PIN;
            __relays[2].relayAddress = LILYGORELAY8_RELAY3_PIN;
            __relays[3].relayAddress = LILYGORELAY8_RELAY4_PIN;
            __relays[4].relayAddress = LILYGORELAY8_RELAY5_PIN;
            __relays[5].relayAddress = LILYGORELAY8_RELAY6_PIN;
            __relays[6].relayAddress = LILYGORELAY8_RELAY7_PIN;
            __relays[7].relayAddress = LILYGORELAY8_RELAY8_PIN;
        }

        //Set other default values.
        for (int relay=0;relay<__numberOfRelays; relay++){
            __relays[relay].relayName = "Relay " + String(relay+1); //Names start at 1.
            __relays[relay].momentaryDuration = -1;
            __relays[relay].setOuter(this);
            __relays[relay].index=relay;
        }
        __initialized = true;
    }


    void setRelayUpdateCallback(void (*relayUpdatedFunc)(int, int)){
        this->__relayUpdatedCB = relayUpdatedFunc;
    }

    bool initialize(){
        if (__relayType == Lilygo6Relays){
            setLEDStatus(LOW, &greenLed);
        }

        for (int relay=0;relay<__numberOfRelays; relay++){
            __relays[relay].setRelayStatus(LOW);
        }

        setLEDStatus(LOW);
        return true;
    }

    bool initialize(String rawJson){
        //Call the basic initializer, then set the relays and stuff based on the passed in Json String.
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, rawJson);
        if (err) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(err.c_str());
            return false;
        } else {

            if (!doc.containsKey("numberofRelays")){
                Serial.println("Error, required value numberOfRelays not present aborting.");
                return false;
            }


            if (doc.containsKey("rled")){
                JsonObject rled = doc["rled"];
                redLed.on_duration = rled["on"]; // -1
                redLed.off_duration = rled["off"]; // -1
                setLEDStatus(rled["state"].as<int>(), &redLed);
            }

            if (__relayType==Lilygo6Relays){
                if (doc.containsKey("gled")){
                    JsonObject gled = doc["gled"];
                    greenLed.on_duration = gled["on"]; // -1
                    greenLed.off_duration = gled["off"]; // -1
                    setLEDStatus(gled["state"].as<int>(), &greenLed);
                }
            }

            //Only read in what is in the doc or what can be configured, whichever is less.
            int jsonCount = doc["numberofRelays"];
            int workingCount = min(__numberOfRelays, jsonCount);
            Serial.println(workingCount);

            int index = 0;
            for (JsonObject relay : doc["relays"].as<JsonArray>()) {
                if (index >= workingCount){
                    Serial.println("Break");
                    break;
                }
                __relays[index].relayName = relay["name"].as<String>();
                __relays[index].momentaryDuration = relay["duration"].as<int32_t>();
                __relays[index].setRelayStatus(relay["state"].as<int>());
                __relays[index].setUserData(relay["ud"]);
                index++;
            }
        }
        return true;
    }

    lilygoRelay& operator[](String key) {
        for (int i = 0; i<__numberOfRelays; i++){
            if (__relays[i].relayName==key){
                return __relays[i];    
            }
        }
        return __relays[0];    
    }

    lilygoRelay& operator[](const int index) {
        if (index < 0 or index > __numberOfRelays){
            return __relays[0];
        }
        return __relays[index];    
    }

    void loop(){
        for (int relay=0; relay <__numberOfRelays; relay++){
            if (__relays[relay].momentaryDuration>0 && __relays[relay].getRelayStatus()==1){
                __relays[relay].loop();
            }
        }
        ledLoop();
    }

    int numberOfRelays(){
        return __numberOfRelays;
    }

    String asRawJson(){
        JsonDocument doc;
        doc["numberofRelays"] = __numberOfRelays;

        JsonArray data = doc["relays"].to<JsonArray>();

        for (int relay=0; relay <__numberOfRelays; relay++){
            JsonObject r = data.add<JsonObject>();
            r["name"]=__relays[relay].relayName;
            r["state"]=__relays[relay].getRelayStatus();
            r["duration"]=__relays[relay].momentaryDuration;
            r["ud"] = __relays[relay].getUserData();
        }

        JsonObject rLed = doc["rled"].to<JsonObject>();
        rLed["on"] = redLed.on_duration;
        rLed["off"] = redLed.off_duration;
        rLed["state"] = getLEDStatus(&redLed);

        if (__relayType==Lilygo6Relays){
            JsonObject gLed = doc["gled"].to<JsonObject>();
            gLed["on"] = greenLed.on_duration;
            gLed["off"] = greenLed.off_duration;
            gLed["state"] = getLEDStatus(&greenLed);
        }

        String returnString;
        doc.shrinkToFit(); 
        serializeJson(doc,returnString);
        return returnString;
    }


    bool hasGreenLed(){
        if (__relayType==Lilygo6Relays)
            return true;
        else 
            return false;
    }

    int getRedLedStatus(RelayLed *whichLED){
        return getLEDStatus(&redLed);
    }

    int getGreenLedStatus(RelayLed *whichLED){
        return getLEDStatus(&greenLed);
    }

    void setRedLedStatus( int status, unsigned long onTime, unsigned long offTime){
        setLEDStatus(status, &redLed, onTime, offTime);
    }
    void setGreenLedStatus( int status, unsigned long onTime, unsigned long offTime){
        setLEDStatus(status, &greenLed, onTime, offTime);
    }

private:

    bool                                __initialized = false;
    int                                 __relaysPerBank = 4;
    int                                 __numberOfRelays = 4;
    int                                 __numberOfLEDs = 1;
    RelayType                           __relayType = Lilygo4Relays;
    int                                 __banks = 1;
    lilygoRelay                         *__relays;
    ShiftRegister74HC595_NonTemplate    *__control;
    void (*__relayUpdatedCB)(int, int) = NULL;

    RelayLed redLed {-1,-1,0};
    RelayLed greenLed {-1,-1,0}; //only used for Lilygo 6 Relays units.

    friend class lilygoRelay;

    int getLEDStatus(RelayLed *whichLED){
        //Check for bad index or not initialized
        if (__relayType==Lilygo6Relays){
            return (__control->get(whichLED==&redLed?LILYGORELAY6_RLED_POS:LILYGORELAY6_GLED_POS));
        } else {
            return digitalRead(LILYGORELAY4OR8_RLED_PIN); //even if you ask for blue, you get red because there is only 1.
        }
    }

    void setLEDStatus( int status, RelayLed *whichLED, unsigned long onTime, unsigned long offTime){
        //Check for bad index or not initialized
        if (__relayType!=Lilygo6Relays){
            whichLED=&redLed;//If this is not a 6, we just set the single LED.
        }
        if (onTime != whichLED->on_duration || offTime != whichLED->off_duration){
            whichLED->on_duration = onTime;
            whichLED->off_duration = offTime;
        }

        if (getLEDStatus(whichLED) != status) {
            whichLED->last_set_time = millis();
            if (__relayType==Lilygo6Relays){
                __control->set(whichLED==&redLed?LILYGORELAY6_RLED_POS:LILYGORELAY6_GLED_POS, status==1?HIGH:LOW);
            } else {
                digitalWrite(LILYGORELAY4OR8_RLED_PIN, status==1?HIGH:LOW);
            }
        }
    }

    void setLEDStatus(int status, RelayLed *whichLED){
        setLEDStatus(status, whichLED, whichLED->on_duration, whichLED->off_duration);
    }

    void setLEDStatus(int status){
        setLEDStatus(status, &redLed, redLed.on_duration, redLed.off_duration);
    }




    void checkLED(RelayLed *theLed){
        //Is this a blinker?
        if (theLed->on_duration > 0){
            int status = getLEDStatus(theLed);
            if (status==HIGH){
                if((theLed->last_set_time+theLed->on_duration) < millis() ){
                    setLEDStatus(LOW,theLed); //turn it off
                }
            } else {
                if(theLed->last_set_time+theLed->off_duration<millis() ){
                    setLEDStatus(HIGH,theLed); //turn it on
                }
            }
        }
    }

    void ledLoop(){
        if (__relayType==Lilygo6Relays){
            checkLED(&greenLed);
        }
        checkLED(&redLed);
    }


protected:


};
