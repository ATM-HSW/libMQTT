/**
 * @file    MQTT.h
 * @brief   API - for MQTT
 * @author  
 * @version 1.0
 * @see     
 *
 * Copyright (c) 2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MQTT_H
#define MQTT_H

/** Example using the MQTT API class
 * @code
 *  #include "mbed.h"
 *  #include "MQTT.h"
 *  
 *  DigitalOut myled(LED1);
 *  
 *  int main()
 *  {
 *      while(1) 
 *      {
 *          myled = 1;
 *          wait(0.2);
 *          myled = 0;
 *          wait(0.2);
 *      }
 *  }
 * @endcode
 */

struct TopicPayload
{
    char *topic;
    char *payload;
};

#include "FP.h"
#include "MQTTPubSub.h"
#include "mbed.h"

class MQTT : public MQTTPubSub
{
public:
    MQTT(){}
    
    char *mqttStream(void){return 0;}
    int   mqttStreamLength(void){return 0;}
    
    FP <void,char*>callback;
};

#endif
