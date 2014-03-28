/**
 * @file    MQTTPubSub.cpp
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
 
#include "MQTTClient.h"
#include "MQTTPacket.h"

MQTTClient::MQTTClient(char* serverURI, char* clientId, const int buffer_size)
{
    
   buf = new char[buffer_size];
   this->clientId = clientId;
   this->serverURI = serverURI;
}


int MQTTClient::connect(MQTTConnectOptions* options, FP<void, MQTTResult*> resultHandler)
{
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
     
    data.clientID.cstring = clientId;
    if (options)
    {
        data.keepAliveInterval = options->keepAliveInterval;
        data.cleansession = options->cleansession;
        data.username.cstring = options->username;
        data.password.cstring = options->password;
    }
     
    int len = MQTTSerialize_connect(buf, buflen, &data);
    
    sendPacket(buf, buflen); // send the connect packet
    
    /* how to make this check work?
    if (resultHandler == None)
    {
        // this will be a blocking call, wait for the connack
        
    }*/
    
    return len;
}

