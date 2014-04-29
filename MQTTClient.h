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
 
 /*
 
 TODO: 
 
 log messages - use macros
 define return code constants
 
 */

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
    unsigned short id;
    void *payload;
    size_t payloadlen;
};


struct MessageData
{
    struct Message message;
    char* topicName;
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
    long command_timeout_ms;
        
    limits()
    {
        MAX_MQTT_PACKET_SIZE = 100;
        MAX_MESSAGE_HANDLERS = 5;
        command_timeout_ms = 30000;
    }
} Limits;
  
  
/**
 * @class Client
 * @brief blocking, non-threaded MQTT Client API
 * @param Network a network class which supports send, receive
 * @param Timer a timer class with the methods: 
 */ 
template<class Network, class Timer> class Client
{
    
public:    

    /** Construct the client
     *  @param network - pointer to an instance of the Network class - must be connected to the endpoint
     *      before calling MQTT connect
     *  @param limits an instance of the Limit class - to alter limits as required
     */
    Client(Network* network, const Limits limits = Limits()); 
    
    typedef struct
    {
        Client* client;
        Network* network;
    } connectionLostInfo;
    
    typedef int (*connectionLostHandlers)(connectionLostInfo*);
    
    /** Set the connection lost callback - called whenever the connection is lost and we should be connected
     *  @param clh - pointer to the callback function
     */
    void setConnectionLostHandler(connectionLostHandlers clh)
    {
        connectionLostHandler.attach(clh);
    }
    
    /** Set the default message handling callback - used for any message which does not match a subscription message handler
     *  @param mh - pointer to the callback function
     */
    void setDefaultMessageHandler(messageHandler mh)
    {
        defaultMessageHandler.attach(mh);
    }
    
    /** MQTT Connect - send an MQTT connect packet down the network and wait for a Connack
     *  The nework object must be connected to the network endpoint before calling this 
     *  @param options - connect options
     *  @return success code -  
     */       
    int connect(MQTTPacket_connectData* options = 0);
      
    /** MQTT Publish - send an MQTT publish packet and wait for all acks to complete for all QoSs
     *  @param topic - the topic to publish to
     *  @param message - the message to send
     *  @return success code -  
     */      
    int publish(const char* topicName, Message* message);
   
    /** MQTT Subscribe - send an MQTT subscribe packet and wait for the suback
     *  @param topicFilter - a topic pattern which can include wildcards
     *  @param qos - the MQTT QoS to subscribe at
     *  @param mh - the callback function to be invoked when a message is received for this subscription
     *  @return success code -  
     */   
    int subscribe(const char* topicFilter, enum QoS qos, messageHandler mh);
    
    /** MQTT Unsubscribe - send an MQTT unsubscribe packet and wait for the unsuback
     *  @param topicFilter - a topic pattern which can include wildcards
     *  @return success code -  
     */   
    int unsubscribe(const char* topicFilter);
    
    /** MQTT Disconnect - send an MQTT disconnect packet 
     *  @return success code -  
     */
    int disconnect();
    
    /** A call to this API must be made within the keepAlive interval to keep the MQTT connection alive
     *  yield can be called if no other MQTT operation is needed.  This will also allow messages to be 
     *  received.
     */
    void yield(int timeout);
    
private:

    int cycle(Timer& timer);
    int waitfor(int packet_type, Timer& timer);
    int keepalive();

    int decodePacket(int* value, int timeout);
    int readPacket(Timer& timer);
    int sendPacket(int length, Timer& timer);
    int deliverMessage(MQTTString* topic, Message* message);
    
    Network* ipstack;
    
    Limits limits;
    
    char* buf;  
    char* readbuf;

    Timer ping_timer;
    unsigned int keepAliveInterval;
    bool ping_outstanding;
    
    PacketId packetid;
    
    typedef FP<void, Message*> messageHandlerFP;
    struct MessageHandlers
    {
        const char* topic;
        messageHandlerFP fp;
    } *messageHandlers;      // Message handlers are indexed by subscription topic
    
    messageHandlerFP defaultMessageHandler;
    
    typedef FP<int, connectionLostInfo*> connectionLostFP;
    
    connectionLostFP connectionLostHandler;
    
};

}


template<class Network, class Timer> MQTT::Client<Network, Timer>::Client(Network* network, Limits limits)  : limits(limits), packetid()
{
    this->ipstack = network;
    this->ping_timer = Timer();
    this->ping_outstanding = 0;
       
    // How to make these memory allocations portable?  I was hoping to avoid the heap
    buf = new char[limits.MAX_MQTT_PACKET_SIZE];
    readbuf = new char[limits.MAX_MQTT_PACKET_SIZE];
    this->messageHandlers = new struct MessageHandlers[limits.MAX_MESSAGE_HANDLERS];
    for (int i = 0; i < limits.MAX_MESSAGE_HANDLERS; ++i)
        messageHandlers[i].topic = 0;
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::sendPacket(int length, Timer& timer)
{
    int sent = 0;
    
    while (sent < length)
        sent += ipstack->write(&buf[sent], length, timer.left_ms());
    if (sent == length)
        ping_timer.countdown(this->keepAliveInterval); // record the fact that we have successfully sent the packet    
    return sent;
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::decodePacket(int* value, int timeout)
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
template<class Network, class Timer> int MQTT::Client<Network, Timer>::readPacket(Timer& timer) 
{
    int rc = -1;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    if (ipstack->read(readbuf, 1, timer.left_ms()) != 1)
        goto exit;

    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(&rem_len, timer.left_ms());
    len += MQTTPacket_encode(readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (ipstack->read(readbuf + len, rem_len, timer.left_ms()) != rem_len)
        goto exit;

    header.byte = readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::deliverMessage(MQTTString* topic, Message* message)
{
    int rc = -1;

    // we have to find the right message handler - indexed by topic
    for (int i = 0; i < limits.MAX_MESSAGE_HANDLERS; ++i)
    {
        if (messageHandlers[i].topic != 0 && MQTTPacket_equals(topic, (char*)messageHandlers[i].topic))
        {
            messageHandlers[i].fp(message);
            rc = 0;
            break;
        }
    }
    if (rc == -1)
        defaultMessageHandler(message);
    
    return rc;
}



template<class Network, class Timer> void MQTT::Client<Network, Timer>::yield(int timeout)
{
    Timer timer = Timer();
    
    timer.countdown_ms(timeout);
    while (!timer.expired())
        cycle(timer);
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::cycle(Timer& timer)
{
    /* get one piece of work off the wire and one pass through */

    // read the socket, see what work is due
    int packet_type = readPacket(timer);
    
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
                                 (char**)&msg.payload, (int*)&msg.payloadlen, readbuf, limits.MAX_MQTT_PACKET_SIZE);;
            deliverMessage(&topicName, &msg);
            if (msg.qos != QOS0)
            {
                if (msg.qos == QOS1)
                    len = MQTTSerialize_ack(buf, limits.MAX_MQTT_PACKET_SIZE, PUBACK, 0, msg.id);
                else if (msg.qos == QOS2)
                    len = MQTTSerialize_ack(buf, limits.MAX_MQTT_PACKET_SIZE, PUBREC, 0, msg.id);
                rc = sendPacket(len, timer); 
                if (rc != len) 
                    goto exit; // there was a problem
            }
            break;
        case PUBREC:
            int type, dup, mypacketid;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, limits.MAX_MQTT_PACKET_SIZE) == 1)
                ; 
            len = MQTTSerialize_ack(buf, limits.MAX_MQTT_PACKET_SIZE, PUBREL, 0, mypacketid);
            rc = sendPacket(len, timer); // send the PUBREL packet
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


template<class Network, class Timer> int MQTT::Client<Network, Timer>::keepalive()
{
    int rc = 0;

    if (keepAliveInterval == 0)
        goto exit;

    if (ping_timer.expired())
    {
        if (ping_outstanding)
            rc = -1;
        else
        {
            Timer timer = Timer(1000);
            int len = MQTTSerialize_pingreq(buf, limits.MAX_MQTT_PACKET_SIZE);
            rc = sendPacket(len, timer); // send the ping packet
            if (rc != len) 
                rc = -1; // indicate there's a problem
            else
                ping_outstanding = true;
        }
    }

exit:
    return rc;
}


// only used in single-threaded mode where one command at a time is in process
template<class Network, class Timer> int MQTT::Client<Network, Timer>::waitfor(int packet_type, Timer& timer)
{
    int rc = -1;
    
    do
    {
        if (timer.expired()) 
            break; // we timed out
    }
    while ((rc = cycle(timer)) != packet_type);  
    
    return rc;
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::connect(MQTTPacket_connectData* options)
{
    Timer connect_timer = Timer(limits.command_timeout_ms);

    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    if (options == 0)
        options = &default_options; // set default options if none were supplied
    
    this->keepAliveInterval = options->keepAliveInterval;
    ping_timer.countdown(this->keepAliveInterval);
    int len = MQTTSerialize_connect(buf, limits.MAX_MQTT_PACKET_SIZE, options);
    int rc = sendPacket(len, connect_timer); // send the connect packet
    if (rc != len) 
        goto exit; // there was a problem
    
    // this will be a blocking call, wait for the connack
    if (waitfor(CONNACK, connect_timer) == CONNACK)
    {
        int connack_rc = -1;
        if (MQTTDeserialize_connack(&connack_rc, readbuf, limits.MAX_MQTT_PACKET_SIZE) == 1)
            rc = connack_rc;
    }
    
exit:
    return rc;
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::subscribe(const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{ 
    int len = -1;
    Timer timer = Timer(limits.command_timeout_ms);
    
    MQTTString topic = {(char*)topicFilter, 0, 0};
    
    int rc = MQTTSerialize_subscribe(buf, limits.MAX_MQTT_PACKET_SIZE, 0, packetid.getNext(), 1, &topic, (int*)&qos);
    if (rc <= 0)
        goto exit;
    len = rc;
    if ((rc = sendPacket(len, timer)) != len) // send the subscribe packet
        goto exit; // there was a problem
    
    if (waitfor(SUBACK, timer) == SUBACK)      // wait for suback 
    {
        int count = 0, grantedQoS = -1, mypacketid;
        if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, readbuf, limits.MAX_MQTT_PACKET_SIZE) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80 
        if (rc != 0x80)
        {
            for (int i = 0; i < limits.MAX_MESSAGE_HANDLERS; ++i)
            {
                if (messageHandlers[i].topic == 0)
                {
                    messageHandlers[i].topic = topicFilter;
                    messageHandlers[i].fp.attach(messageHandler);
                    rc = 0;
                    break;
                }
            }
        }
    }
    
exit:
    return rc;
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::unsubscribe(const char* topicFilter)
{   
    int len = -1;
    Timer timer = Timer(limits.command_timeout_ms);
    
    MQTTString topic = {(char*)topicFilter, 0, 0};
    
    int rc = MQTTSerialize_unsubscribe(buf, limits.MAX_MQTT_PACKET_SIZE, 0, packetid.getNext(), 1, &topic);
    if (rc <= 0)
        goto exit;
    len = rc;
    if ((rc = sendPacket(len, timer)) != len) // send the subscribe packet
        goto exit; // there was a problem
    
    if (waitfor(UNSUBACK, timer) == UNSUBACK)
    {
        int mypacketid;  // should be the same as the packetid above
        if (MQTTDeserialize_unsuback(&mypacketid, readbuf, limits.MAX_MQTT_PACKET_SIZE) == 1)
            rc = 0; 
    }
    
exit:
    return rc;
}


   
template<class Network, class Timer> int MQTT::Client<Network, Timer>::publish(const char* topicName, Message* message)
{
    Timer timer = Timer(limits.command_timeout_ms);
    
    MQTTString topicString = {(char*)topicName, 0, 0};

    if (message->qos == QOS1 || message->qos == QOS2)
        message->id = packetid.getNext();
    
    int len = MQTTSerialize_publish(buf, limits.MAX_MQTT_PACKET_SIZE, 0, message->qos, message->retained, message->id, 
              topicString, (char*)message->payload, message->payloadlen);
    int rc = sendPacket(len, timer); // send the subscribe packet
    if (rc != len) 
        goto exit; // there was a problem
    
    if (message->qos == QOS1)
    {
        if (waitfor(PUBACK, timer) == PUBACK)
        {
            int type, dup, mypacketid;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, limits.MAX_MQTT_PACKET_SIZE) == 1)
                rc = 0; 
        }
    }
    else if (message->qos == QOS2)
    {
        if (waitfor(PUBCOMP, timer) == PUBCOMP)
        {
            int type, dup, mypacketid;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, limits.MAX_MQTT_PACKET_SIZE) == 1)
                rc = 0; 
        }
    }
    
exit:
    return rc;
}


template<class Network, class Timer> int MQTT::Client<Network, Timer>::disconnect()
{  
    Timer timer = Timer(limits.command_timeout_ms);     // we might wait for incomplete incoming publishes to complete
    int len = MQTTSerialize_disconnect(buf, limits.MAX_MQTT_PACKET_SIZE);
    int rc = sendPacket(len, timer);   // send the disconnect packet
    
    return (rc == len) ? 0 : -1;
}


#endif