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

#ifndef MQTTPUBSUB_H
#define MQTTPUBSUB_H

#include "MQTTConnection.h"

class MQTTTopic
{
    MQTTString topic;
    char *msg_buffer;
    int   msg_size;
};

class MQTTPubSub : private MQTTConnect
{
private:
    MQTTTopic topic;

public:    
    enum {
        TOPIC_PUBLISH, TOPIC_SUBSCRIBE
    }Message;
    
    MQTTPubSub(){}
    
    void publish(){}
    void subscribe(){}    
};

#endif
