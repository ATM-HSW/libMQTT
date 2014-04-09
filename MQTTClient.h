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


class PacketId
{
public:
    PacketId();
    
    int getNext();
    
private:
    static const int MAX_PACKET_ID = 65535;
    int next;
};

typedef void (*resultHandler)(Result*);
typedef void (*messageHandler)(Message*);
  
template<class Network, class Timer, class Thread> class Client
{
    
public:    
   
    Client(Network* network, Timer* timer, const int buffer_size = 100, const int command_timeout = 30); 
       
    int connect(MQTTPacket_connectData* options = 0, resultHandler fn = 0);
    
     template<class T>
    int connect(MQTTPacket_connectData* options = 0, T *item = 0, void(T::*method)(Result *) = 0);  // alternative to pass in pointer to member function
        
    int publish(const char* topic, Message* message, resultHandler rh = 0);
    
    int subscribe(const char* topicFilter, enum QoS qos, messageHandler mh, resultHandler rh = 0);
    
    int unsubscribe(char* topicFilter, resultHandler rh = 0);
    
    int disconnect(int timeout, resultHandler rh = 0);
    
    void run(void const *argument);
    
private:

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
    PacketId packetid;
    
    typedef FP<void, Result*> resultHandlerFP;    
    // how many concurrent operations should we allow?  Each one will require a function pointer
    resultHandlerFP connectHandler; 
    
    #define MAX_MESSAGE_HANDLERS 5
    typedef FP<void, Message*> messageHandlerFP;
    messageHandlerFP messageHandlers[MAX_MESSAGE_HANDLERS];  // Linked list, or constructor parameter to limit array size?
    
};

void threadfn(void* arg);

}

template<class Network, class Timer, class Thread> MQTT::Client<Network, Timer, Thread>::Client(Network* network, Timer* timer, const int buffer_size, const int command_timeout)  : packetid()
{
    
   buf = new char[buffer_size];
   readbuf = new char[buffer_size];
   buflen = readbuflen = buffer_size;
   this->command_timeout = command_timeout;
   this->thread = 0;
   this->ipstack = network;
   this->timer = timer;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::sendPacket(int length, int timeout)
{
    int sent = 0;
    
    while (sent < length)
        sent += ipstack->write(&buf[sent], length, -1);
        
    return sent;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::decodePacket(int* value, int timeout)
{
    char c;
    int multiplier = 1;
    int len = 0;
#define MAX_NO_OF_REMAINING_LENGTH_BYTES 4

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = ipstack->read(&c, 1, timeout);
        if (rc != 1)
            goto exit;
        *value += (c & 127) * multiplier;
        multiplier *= 128;
    } while ((c & 128) != 0);
exit:
    return len;
}


/**
 * If any read fails in this method, then we should disconnect from the network, as on reconnect
 * the packets can be retried. 
 * @param timeout the max time to wait for the packet read to complete, in milliseconds
 * @return the MQTT packet type, or -1 if none
 */
template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::readPacket(int timeout) 
{
    int rc = -1;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    if (ipstack->read(readbuf, 1, timeout) != 1)
        goto exit;

    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(&rem_len, timeout);
    len += MQTTPacket_encode(readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (ipstack->read(readbuf + len, rem_len, timeout) != rem_len)
        goto exit;

    header.byte = readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::cycle()
{
    int timeout = -1;
    /* get one piece of work off the wire and one pass through */
    
    // 1. read the socket, see what work is due. 
    int packet_type = readPacket(timeout);

    printf("packet type %d\n", packet_type);
    
    switch (packet_type)
    {
        case CONNACK:
            printf("connack received\n");
            break;
        case PUBLISH:
            break;
        case PUBACK:
            break;
        case SUBACK:
            break;
        case PUBREC:
            break;
        case PUBCOMP:
            break;
        case PINGRESP:
            break;
        case -1:
            break;
    }
    return packet_type;
}


template<class Network, class Timer, class Thread> void MQTT::Client<Network, Timer, Thread>::run(void const *argument)
{
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::connect(MQTTPacket_connectData* options, resultHandler resultHandler)
{
    int len = 0;
    int rc = -99;
    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;

    /* 2. if the connect was successful, send the MQTT connect packet */   
    if (options == 0)
    {
        default_options.clientID.cstring = "me";
        options = &default_options;
    }
    
    this->keepalive = options->keepAliveInterval;
    len = MQTTSerialize_connect(buf, buflen, options);
    printf("len from send is %d %d\n", len, buflen);
    rc = sendPacket(len); // send the connect packet
    printf("rc from send is %d\n", rc);
    
    /* 3. wait until the connack is received */
    if (resultHandler == 0)
    {
        // this will be a blocking call, wait for the connack
        if (cycle() == CONNACK)
        {
            int connack_rc = -1;
            if (MQTTDeserialize_connack(&connack_rc, readbuf, readbuflen) == 1)
                rc = connack_rc;
        }
    }
    else
    {
        // set connect response callback function
        connectHandler.attach(resultHandler);
        
        // start background thread
            
        this->thread = new Thread((void (*)(void const *argument))&MQTT::threadfn, (void*)this);
    }
    
    return rc;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::subscribe(const char* topicFilter, enum QoS qos, 
    messageHandler messageHandler, resultHandler resultHandler)
{
    int rc = -1, 
        len = 0;
    MQTTString topic = {(char*)topicFilter, 0, 0};
    
    len = MQTTSerialize_subscribe(buf, buflen, 0, packetid.getNext(), 1, &topic, (int*)&qos);
    rc = sendPacket(len); // send the subscribe packet
    
    /* wait for suback */
    if (resultHandler == 0)
    {
        // this will block
        if (cycle() == SUBACK)
        {
            int count = 0, grantedQoS = -1, mypacketid;
            if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, readbuf, readbuflen) == 1)
                rc = grantedQoS; // 0, 1, 2 or 0x80 
        }
    }
    else
    {
        // set subscribe response callback function
        
    }
    
    return rc;
}


#endif
