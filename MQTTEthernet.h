
#if !defined(MQTTETHERNET_H)
#define MQTTETHERNET_H

#include "MQTT_mbed.h"
#include "EthernetInterface.h"

class MQTTEthernet
{
public:    
    MQTTEthernet()
    {
        eth.init();                          // Use DHCP
        eth.connect();
        mysock.set_blocking(false, 1000);    // 1 second Timeout 
    }
    
    int connect(char* hostname, int port)
    {
        return mysock.connect(hostname, port);
    }

    int read(char* buffer, int len, int timeout)
    {
        mysock.set_blocking(false, timeout);  
        return mysock.receive(buffer, len);
    }
    
    int write(char* buffer, int len, int timeout)
    {
        mysock.set_blocking(false, timeout);  
        return mysock.send(buffer, len);
    }
    
    int disconnect()
    {
        return mysock.close();
    }
    
private:

    EthernetInterface eth;
    TCPSocketConnection mysock; 
    
};



#endif
