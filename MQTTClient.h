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

#include "FP.h"
#include "MQTTPacket.h"

namespace MQTT
{


enum QoS { QOS0, QOS1, QOS2 };


struct Message
{
    enum QoS qos;
    bool retained;
    bool dup;
    unsigned short msgid;
    void *payload;
    size_t payloadlen;
};

template<class Network, class Thread> class Client;

class Result
{
    /* success or failure result data */
    Client<class Network, class Thread>* client;
};

  
template<class Network, class Thread> class Client
{
    
public:    
   
    Client(Network* network, const int buffer_size = 100, const int command_timeout = 30);  
       
    int connect(MQTTPacket_connectData* options = 0, FP<void, Result*> *resultHandler = 0);
        
    int publish(char* topic, Message* message, FP<void, Result*> *resultHandler = 0);
    
    int subscribe(char* topicFilter, int qos, FP<void, Message*> messageHandler, FP<void, Result*> *resultHandler = 0);
    
    int unsubscribe(char* topicFilter, FP<void, Result*> *resultHandler = 0);
    
    int disconnect(int timeout, FP<void, Result*> *resultHandler = 0);
    
private:

    int cycle();

    int decodePacket(int* value, int timeout);
    int readPacket(int timeout = -1);
    int sendPacket(int length);
    
    Thread* thread;
    Network* ipstack;
    
    char* buf;
    int buflen;
    
    char* readbuf;
    int readbuflen;
    
    int command_timeout;
    
};

}

#endif
