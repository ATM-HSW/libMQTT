#if !defined(MQTTSOCKET_H)
#define MQTTSOCKET_H

#include "MQTT_mbed.h"
#include "TCPSocketConnection.h"

class MQTTSocket
{
public:    
    int connect(char* hostname, int port, int timeout=1000)
    {
        mysock.set_blocking(false, timeout);    // 1 second Timeout 
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

    TCPSocketConnection mysock; 
    
};



#endif
