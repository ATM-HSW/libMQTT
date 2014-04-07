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

template<class Network, class Thread> MQTT::Client<Network, Thread>::Client(Network* network, const int buffer_size, const int command_timeout)
{
    
   buf = new char[buffer_size];
   readbuf = new char[buffer_size];
   this->ipstack = ipstack;
   this->command_timeout = command_timeout;
   //this->thread = new Thread(0); // only need a background thread for non-blocking mode
   this->ipstack = network;
}


template<class Network, class Thread> int MQTT::Client<Network, Thread>::sendPacket(int length)
{
    int sent = 0;
    
    while (sent < length)
        sent += ipstack->write(&buf[sent], length, -1);
        
    return sent;
}


template<class Network, class Thread> int MQTT::Client<Network, Thread>::decodePacket(int* value, int timeout)
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


template<class Network, class Thread> int MQTT::Client<Network,Thread>::readPacket(int timeout) 
{
    int rc = -1;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    if ((rc = ipstack->read(readbuf, 1, timeout)) != 1)
        goto exit;

    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(&rem_len, timeout);
    len += MQTTPacket_encode(readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if ((rc = ipstack->read(readbuf + len, rem_len, timeout)) != rem_len)
        goto exit;

    header.byte = readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}


template<class Network, class Thread> int MQTT::Client<Network, Thread>::cycle()
{
    int timeout = 1000L;
    /* get one piece of work off the wire and one pass through */
    
    // 1. read the socket, see what work is due. 
    int packet_type = readPacket(-1);

	printf("packet type %d\n", packet_type);
    
    switch (packet_type)
    {
        case CONNACK:
			printf("connack received\n");
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
    }
    return packet_type;
}


template<class Network, class Thread> int MQTT::Client<Network, Thread>::connect(MQTTPacket_connectData* options, FP<void, MQTT::Result*> *resultHandler)
{
	int len = 0;
	int rc = -99;

    /* 2. if the connect was successful, send the MQTT connect packet */   
	if (options == 0)
	{
		MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
		default_options.clientID.cstring = "me";
		len = MQTTSerialize_connect(buf, buflen, &default_options);
	}
	else
		len = MQTTSerialize_connect(buf, buflen, options);
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
    }
    
    return rc;
}
