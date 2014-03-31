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

MQTT::Client::Client(IPStack* ipstack, const int buffer_size)
{
    
   buf = new char[buffer_size];
   this->ipstack = ipstack;
}


int MQTT::Client::sendPacket(int length)
{
    int sent = 0;
    
    while (sent < length)
        sent += ipstack->write(&buf[sent], length);
        
    return sent;
}


int MQTT::Client::decodePacket(int* value, int timeout)
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


int MQTT::Client::readPacket(int timeout) 
{
    int rc = -1;
    MQTTHeader header;
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

    header.byte = buf[0];
    rc = header.bits.type;
exit:
    return rc;
}


void MQTT::Client::cycle()
{
    int timeout = 1000L;
    /* get one piece of work off the wire and one pass through */
    
    // 1. read the socket, see what work is due. 
    int packet_type = readPacket(buf, buflen, -1);
    
}


int MQTT::Client::connect(MQTTPacket_connectData* options, FP<void, MQTT::Result*> resultHandler)
{
    /* 1. connect to the server with the desired transport */
    if (!ipstack->connect())
        return -99;
    
    /* 2. if the connect was successful, send the MQTT connect packet */        
    int len = MQTTSerialize_connect(buf, buflen, options);
    sendPacket(len); // send the connect packet
    
    /* 3. wait until the connack is received */
    /* how to make this check work?
    if (resultHandler == None)
    {
        // this will be a blocking call, wait for the connack
        
    }*/
    
    return len;
}

