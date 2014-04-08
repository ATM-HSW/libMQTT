/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#if !defined(MQTTCLIENT_H)
#define MQTTCLIENT_H

#include "FP.h"
#include "MQTTPacket.h"
#include "stdio.h"

namespace MQTT
{
    
const int MAX_PACKET_ID = 65535;


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

template<class Network, class Timer, class Thread> class Client;

class Result
{
    /* success or failure result data */
    Client<class Network, class Timer, class Thread>* client;
};

  
template<class Network, class Timer, class Thread> class Client
{
    
public:    
   
    Client(Network* network, Timer* timer, const int buffer_size = 100, const int command_timeout = 30);  
       
    int connect(MQTTPacket_connectData* options = 0, FP<void, Result*> *resultHandler = 0);
        
    int publish(const char* topic, Message* message, FP<void, Result*> *resultHandler = 0);
    
    int subscribe(const char* topicFilter, enum QoS qos, FP<void, Message*> messageHandler, FP<void, Result*> *resultHandler = 0);
    
    int unsubscribe(char* topicFilter, FP<void, Result*> *resultHandler = 0);
    
    int disconnect(int timeout, FP<void, Result*> *resultHandler = 0);
    
private:

    int getPacketId();
    int cycle();

    int decodePacket(int* value, int timeout);
    int readPacket(int timeout = -1);
    int sendPacket(int length, int timeout = -1);
    
    Thread* thread;
    Network* ipstack;
    Timer* timer;
    
    char* buf;
    int buflen;
    
    char* readbuf;
    int readbuflen;
    
    int command_timeout; // max time to wait for any MQTT command to complete, in seconds
    int keepalive;
    int packetid;
    
};

}

#endif
