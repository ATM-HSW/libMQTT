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
    unsigned short id;
    void *payload;
    size_t payloadlen;
};

template<class Network, class Timer, class Thread> class Client;

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
  
template<class Network, class Timer, class Thread> class Client
{
    
public:    

	struct Result
	{
    	/* success or failure result data */
    	Client<Network, Timer, Thread>* client;
		int connack_rc;
	};

	typedef void (*resultHandler)(Result*);
   
    Client(Network* network, const int MAX_MQTT_PACKET_SIZE = 100, const int command_timeout = 30); 
       
    int connect(MQTTPacket_connectData* options = 0, resultHandler fn = 0);
    
     template<class T>
    int connect(MQTTPacket_connectData* options = 0, T *item = 0, void(T::*method)(Result *) = 0);  // alternative to pass in pointer to member function
        
    int publish(const char* topic, Message* message, resultHandler rh = 0);
    
    int subscribe(const char* topicFilter, enum QoS qos, messageHandler mh, resultHandler rh = 0);
    
    int unsubscribe(const char* topicFilter, resultHandler rh = 0);
    
    int disconnect(int timeout, resultHandler rh = 0);
    
    void run(void const *argument);
    
private:

    int cycle(int timeout);
	int keepalive();

    int decodePacket(int* value, int timeout);
    int readPacket(int timeout = -1);
    int sendPacket(int length, int timeout = -1);
    
    Thread* thread;
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
    
    typedef FP<void, Result*> resultHandlerFP;    
    // how many concurrent operations should we allow?  Each one will require a function pointer
    resultHandlerFP connectHandler; 
    
    #define MAX_MESSAGE_HANDLERS 5
    typedef FP<void, Message*> messageHandlerFP;
    messageHandlerFP messageHandlers[MAX_MESSAGE_HANDLERS];  // Linked list, or constructor parameter to limit array size?

	static void threadfn(void* arg);
    
};

}


template<class Network, class Timer, class Thread> void MQTT::Client<Network, Timer, Thread>::threadfn(void* arg)
{
    ((Client<Network, Timer, Thread>*) arg)->run(NULL);
}


template<class Network, class Timer, class Thread> MQTT::Client<Network, Timer, Thread>::Client(Network* network, const int MAX_MQTT_PACKET_SIZE, const int command_timeout)  : packetid()
{
    
   buf = new char[MAX_MQTT_PACKET_SIZE];
   readbuf = new char[MAX_MQTT_PACKET_SIZE];
   buflen = readbuflen = MAX_MQTT_PACKET_SIZE;
   this->command_timeout = command_timeout;
   this->thread = 0;
   this->ipstack = network;
   this->command_timer = Timer();
   this->ping_timer = Timer();
   this->ping_outstanding = 0;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::sendPacket(int length, int timeout)
{
    int sent = 0;
    
    while (sent < length)
        sent += ipstack->write(&buf[sent], length, -1);
	if (sent == length)
	    ping_timer.reset(); // record the fact that we have successfully sent the packet    
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


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::cycle(int timeout)
{
    /* get one piece of work off the wire and one pass through */
    
    // read the socket, see what work is due
    int packet_type = readPacket(timeout);

    printf("packet type %d\n", packet_type);
    
	int len, rc;
    switch (packet_type)
    {
        case CONNACK:
			if (this->thread)
			{
				Result res = {this, 0};
            	int connack_rc = -1;
            	if (MQTTDeserialize_connack(&res.connack_rc, readbuf, readbuflen) == 1)
                	;
				connectHandler(&res);
			}
        case PUBACK:
        case SUBACK:
            break;
        case PUBLISH:
			MQTTString topicName;
			Message msg;
			rc = MQTTDeserialize_publish((int*)&msg.dup, (int*)&msg.qos, (int*)&msg.retained, (int*)&msg.id, &topicName,
								 (char**)&msg.payload, (int*)&msg.payloadlen, readbuf, readbuflen);
			if (msg.qos == QOS0)
				messageHandlers[0](&msg);
            break;
        case PUBREC:
   	        int type, dup, mypacketid;
   	        if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, readbuflen) == 1)
   	            ; 
			len = MQTTSerialize_ack(buf, buflen, PUBREL, 0, mypacketid);
		    rc = sendPacket(len); // send the subscribe packet
			if (rc != len) 
				goto exit; // there was a problem

            break;
        case PUBCOMP:
            break;
        case PINGRESP:
			if (ping_outstanding)
				ping_outstanding = false;
			//else disconnect();
            break;
        case -1:
            break;
    }
	keepalive();
exit:
    return packet_type;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::keepalive()
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


template<class Network, class Timer, class Thread> void MQTT::Client<Network, Timer, Thread>::run(void const *argument)
{
	while (true)
		cycle((keepAliveInterval * 1000) - ping_timer.read_ms());
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::connect(MQTTPacket_connectData* options, resultHandler resultHandler)
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
    
    if (resultHandler == 0)     // wait until the connack is received 
    {
		if (command_timer.read_ms() > (command_timeout * 1000)) 
			goto exit; // we timed out
        // this will be a blocking call, wait for the connack
        if (cycle(command_timeout - command_timer.read_ms()) == CONNACK)
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
        this->thread = new Thread((void (*)(void const *argument))&MQTT::Client<Network, Timer, Thread>::threadfn, (void*)this);
    }
    
exit:
	command_timer.stop();
	command_timer.reset();
    return rc;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::subscribe(const char* topicFilter, enum QoS qos, messageHandler messageHandler, resultHandler resultHandler)
{
	command_timer.start();

    MQTTString topic = {(char*)topicFilter, 0, 0};
    
    int len = MQTTSerialize_subscribe(buf, buflen, 0, packetid.getNext(), 1, &topic, (int*)&qos);
    int rc = sendPacket(len); // send the subscribe packet
	if (rc != len) 
		goto exit; // there was a problem
    
    /* wait for suback */
    if (resultHandler == 0)
    {
        // this will block
        if (cycle(command_timeout - command_timer.read_ms()) == SUBACK)
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
    
exit:
	command_timer.stop();
	command_timer.reset();
    return rc;
}


template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::unsubscribe(const char* topicFilter, resultHandler resultHandler)
{
	command_timer.start();

    MQTTString topic = {(char*)topicFilter, 0, 0};
    
    int len = MQTTSerialize_unsubscribe(buf, buflen, 0, packetid.getNext(), 1, &topic);
    int rc = sendPacket(len); // send the subscribe packet
	if (rc != len) 
		goto exit; // there was a problem
    
    /* wait for suback */
    if (resultHandler == 0)
    {
        // this will block
        if (cycle(command_timeout - command_timer.read_ms()) == UNSUBACK)
        {
            int mypacketid;
            if (MQTTDeserialize_unsuback(&mypacketid, readbuf, readbuflen) == 1)
                rc = 0; 
        }
    }
    else
    {
        // set unsubscribe response callback function
        
    }
    
exit:
	command_timer.stop();
	command_timer.reset();
    return rc;
}


   
template<class Network, class Timer, class Thread> int MQTT::Client<Network, Timer, Thread>::publish(const char* topicName, Message* message, resultHandler resultHandler)
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
	        if (cycle(command_timeout - command_timer.read_ms()) == PUBREC)
    	    {
    	        int type, dup, mypacketid;
    	        if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, readbuflen) == 1)
    	            rc = 0; 
				len = MQTTSerialize_ack(buf, buflen, PUBREL, 0, message->id);
			    rc = sendPacket(len); // send the subscribe packet
				if (rc != len) 
					goto exit; // there was a problem
		        if (cycle(command_timeout - command_timer.read_ms()) == PUBCOMP)
	    	    {
    	        	if (MQTTDeserialize_ack(&type, &dup, &mypacketid, readbuf, readbuflen) == 1)
    	            	rc = 0; 
				}
    	    }
		}
    }
    else
    {
        // set publish response callback function
        
    }
    
exit:
	command_timer.stop();
	command_timer.reset();
    return rc;
}


#endif
