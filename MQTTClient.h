/**
 * @file    MQTTPubSub.h
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

#if !defined(MQTTCLIENT_H)
#define MQTTCLIENT_H

#include <vector>

#include "mbed.h"
#include "FP.h"
#include "MQTTPacket.h"
#include "include_me.h"

namespace MQTT
{

class Client;

enum QoS { QOS0, QOS1, QOS2 };

class Result
{
    /* success or failure result data */
    Client* client;
};

struct Message
{
    enum QoS qos;
    bool retained;
    bool dup;
    unsigned short msgid;
    void *payload;
    size_t payloadlen;
};

  
class Client
{
    
public:    

    static FP<void, Result*> None;   // default argument of no result handler to indicate call should be blocking
    
    Client(IPStack* ipstack, const int buffer_size = 100); 
       
    int connect(MQTTPacket_connectData* options = 0, FP<void, Result*> resultHandler = None);
        
    int publish(char* topic, Message* message, FP<void, Result*> resultHandler = None);
    
    int subscribe(char* topicFilter, int qos, FP<void, Message*> messageHandler, FP<void, Result*> resultHandler = None);
    
    int unsubscribe(char* topicFilter, FP<void, Result*> resultHandler = None);
    
    int disconnect(int timeout, FP<void, Result*> resultHandler = None);
    
private:

    void cycle();

    int decodePacket(int* value, int timeout);
    int readPacket(char* buf, int buflen, int timeout = -1);
    int sendPacket(int length);
    
    IPStack* ipstack;
    char* buf;
    int buflen;
    
};

}

#endif
