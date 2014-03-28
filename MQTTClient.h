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

class MQTTClient;

enum QoS { QOS0, QOS1, QOS2 };

class MQTTResult
{
    /* success or failure result data */
    MQTTClient* client;
};

struct MQTTMessage
{
    enum QoS qos;
    bool retained;
    bool dup;
    unsigned short msgid;
    void *payload;
    size_t payloadlen;
};

struct MQTTConnectOptions
{
    unsigned short keepAliveInterval;
    bool cleansession;
    char* username;
    char* password;
    int timeout;
    std::vector<char*> serverURIs;
};

  
class MQTTClient
{
    
public:    

    static FP<void, MQTTResult*> None;   // default argument of no result handler to indicate call should be blocking
    
    MQTTClient(char* serverURI, char* clientId = "", const int buffer_size = 100); 
       
    int connect(MQTTConnectOptions* options = 0, FP<void, MQTTResult*> resultHandler = None);
        
    int publish(char* topic, MQTTMessage* message, FP<void, MQTTResult*> resultHandler = None);
    
    int subscribe(char* topicFilter, int qos, FP<void, MQTTMessage*> messageHandler, FP<void, MQTTResult*> resultHandler = None);
    
    int unsubscribe(char* topicFilter, FP<void, MQTTResult*> resultHandler = None);
    
    int disconnect(int timeout, FP<void, MQTTResult*> resultHandler = None);
    
private:

    int sendPacket(char* buf, int buflen);

    char* clientId;
    char* serverURI;
    char* buf;
    int buflen;
    
};

#endif
