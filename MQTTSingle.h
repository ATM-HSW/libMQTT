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

#if !defined(MQTTSINGLE_H)
#define MQTTSINGLE_H

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
    unsigned short id;
    void *payload;
    size_t payloadlen;
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

typedef void (*messageHandler)(Message*);

typedef struct limits
{
    int MAX_MQTT_PACKET_SIZE; // 
    int MAX_MESSAGE_HANDLERS;  // each subscription requires a message handler
    int command_timeout;
        
    limits()
    {
        MAX_MQTT_PACKET_SIZE = 100;
        MAX_MESSAGE_HANDLERS = 5;
        command_timeout = 30;
    }
} Limits;
  
  
template<class Network, class Timer> class Client
{
    
public:    
 
    Client(Network* network, Limits* limits = 0);
       
    int connect(MQTTPacket_connectData* options = 0);
        
    int publish(const char* topic, Message* message);
    
    int subscribe(const char* topicFilter, enum QoS qos, messageHandler mh);
    
    int unsubscribe(const char* topicFilter);
    
    int disconnect(int timeout);
    
private:

    int cycle(int timeout);
    int keepalive();

    int decodePacket(int* value, int timeout);
    int readPacket(int timeout = -1);
    int sendPacket(int length, int timeout = -1);
    int deliverMessage(MQTTString* topic, Message* message);
    
    Network* ipstack;
    Timer command_timer, ping_timer;
    
    char* buf;
    int buflen;
    
    char* readbuf;
    int readbuflen;

    unsigned int keepAliveInterval;
    bool ping_outstanding;
    
    int command_timeout; // max time to wait for any MQTT command to complete, in seconds
    PacketId packetid;
    
    typedef FP<void, Message*> messageHandlerFP;
    struct MessageHandlers
    {
        char* topic;
        messageHandlerFP fp;
    } *messageHandlers;  // Message handlers are linked to a subscription topic
    int messageHandlerCount;
    
};

}

template<class Network, class Timer, class Thread, class Mutex> MQTT::Client<Network, Timer, Thread, Mutex>::Client(Network* network, Limits* limits)  : packetid()
{
    Limits default_limits = Limits();
   
    if (limits == 0)
        limits = &default_limits;
   
    this->command_timeout = limits->command_timeout;
    this->ipstack = network;
    this->command_timer = Timer();
    this->ping_timer = Timer();
    this->ping_outstanding = 0;
       
    // How to make these memory allocations portable?  I was hoping to avoid the heap
    buflen = readbuflen = limits->MAX_MQTT_PACKET_SIZE;
    buf = new char[limits->MAX_MQTT_PACKET_SIZE];
    readbuf = new char[limits->MAX_MQTT_PACKET_SIZE];
    this->messageHandlers = new struct MessageHandlers[limits->MAX_MESSAGE_HANDLERS];
}


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::sendPacket(int length, int timeout)
{
    int sent = 0;
    
    while (sent < length)
        sent += ipstack->write(&buf[sent], length, -1);
    if (sent == length)
        ping_timer.reset(); // record the fact that we have successfully sent the packet    
    return sent;
}


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::decodePacket(int* value, int timeout)
{
    char c;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

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
template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::readPacket(int timeout) 
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


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::deliverMessage(MQTTString* topic, Message* message)
{
    // we have to find the right message handler - indexed by topic
}


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::cycle(int timeout)
{
    /* get one piece of work off the wire and one pass through */
    
    // read the socket, see what work is due
    int packet_type = readPacket(timeout);
    
    int len, rc;
    switch (packet_type)
    {
        case CONNACK:
        case PUBACK:
        case SUBACK:
            break;
        case PUBLISH:
            MQTTString topicName;
            Message msg;
            rc = MQTTDeserialize_publish((int*)&msg.dup, (int*)&msg.qos, (int*)&msg.retained, (int*)&msg.id, &topicName,
                                 (char**)&msg.payload, (int*)&msg.payloadlen, readbuf, readbuflen);
            if (msg.qos == QOS0)
                deliverMessage(&topicName, &msg);
            break;
        case PUBREC:
            int type, dup, mypacketid;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, readbuflen) == 1)
                ; 
            // must lock this access against the application thread, if we are multi-threaded
            len = MQTTSerialize_ack(buf, buflen, PUBREL, 0, mypacketid);
            rc = sendPacket(len); // send the subscribe packet
            if (rc != len) 
                goto exit; // there was a problem

            break;
        case PUBCOMP:
            break;
        case PINGRESP:
            ping_outstanding = false;
            break;
    }
    keepalive();
exit:
    return packet_type;
}


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::keepalive()
{
    int rc = 0;

    if (keepAliveInterval == 0)
        goto exit;

    if (ping_timer.read_ms() >= (keepAliveInterval * 1000))
    {
        if (ping_outstanding)
            rc = -1;
        else
        {
            int len = MQTTSerialize_pingreq(buf, buflen);
            rc = sendPacket(len); // send the connect packet
            if (rc != len) 
                rc = -1; // indicate there's a problem
            else
                ping_outstanding = true;
        }
    }

exit:
    return rc;
}


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::connect(MQTTPacket_connectData* options, resultHandler resultHandler)
{
    command_timer.start();

    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    if (options == 0)
        options = &default_options; // set default options if none were supplied
    
    this->keepAliveInterval = options->keepAliveInterval;
    ping_timer.start();
    int len = MQTTSerialize_connect(buf, buflen, options);
    int rc = sendPacket(len); // send the connect packet
    if (rc != len) 
        goto exit; // there was a problem
    
    // this will be a blocking call, wait for the connack
    do
    {
        if (command_timer.read_ms() > (command_timeout * 1000)) 
             goto exit; // we timed out
    }
    while (cycle(command_timeout - command_timer.read_ms()) != CONNACK);
    int connack_rc = -1;
    if (MQTTDeserialize_connack(&connack_rc, readbuf, readbuflen) == 1)
        rc = connack_rc;
    
exit:
    command_timer.stop();
    command_timer.reset();
    return rc;
}


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::subscribe(const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{
    command_timer.start();

    MQTTString topic = {(char*)topicFilter, 0, 0};
    
    int len = MQTTSerialize_subscribe(buf, buflen, 0, packetid.getNext(), 1, &topic, (int*)&qos);
    int rc = sendPacket(len); // send the subscribe packet
    if (rc != len) 
        goto exit; // there was a problem
    
    // wait for suback - this will block
    if (cycle(command_timeout - command_timer.read_ms()) == SUBACK)
    {
        int count = 0, grantedQoS = -1, mypacketid;
        if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, readbuf, readbuflen) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80 
        if (rc != 0x80)
    }

exit:
    command_timer.stop();
    command_timer.reset();
    return rc;
}


template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::unsubscribe(const char* topicFilter, resultHandler resultHandler)
{
    command_timer.start();

    MQTTString topic = {(char*)topicFilter, 0, 0};
    
    int len = MQTTSerialize_unsubscribe(buf, buflen, 0, packetid.getNext(), 1, &topic);
    int rc = sendPacket(len); // send the subscribe packet
    if (rc != len) 
        goto exit; // there was a problem
    
    // this will block
    if (cycle(command_timeout - command_timer.read_ms()) == UNSUBACK)
    {
        int mypacketid;
        if (MQTTDeserialize_unsuback(&mypacketid, readbuf, readbuflen) == 1)
            rc = 0; 
    }
    
exit:
    command_timer.stop();
    command_timer.reset();
    return rc;
}


   
template<class Network, class Timer, class Thread, class Mutex> int MQTT::Client<Network, Timer, Thread, Mutex>::publish(const char* topicName, Message* message, resultHandler resultHandler)
{
    command_timer.start();

    MQTTString topic = {(char*)topicName, 0, 0};

    message->id = packetid.getNext();
    
    int len = MQTTSerialize_publish(buf, buflen, 0, message->qos, message->retained, message->id, topic, message->payload, message->payloadlen);
    int rc = sendPacket(len); // send the subscribe packet
    if (rc != len) 
        goto exit; // there was a problem
    
    /* wait for acks */
    if (resultHandler == 0)
    {
        if (message->qos == QOS1)
        {
            if (cycle(command_timeout - command_timer.read_ms()) == PUBACK)
            {
                int type, dup, mypacketid;
                if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, readbuflen) == 1)
                    rc = 0; 
            }
        }
        else if (message->qos == QOS2)
        {
            if (cycle(command_timeout - command_timer.read_ms()) == PUBCOMP)
            {
                int type, dup, mypacketid;
                if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, readbuflen) == 1)
                    rc = 0; 
            }

        }
    }
    
exit:
    command_timer.stop();
    command_timer.reset();
    return rc;
}


#endif
