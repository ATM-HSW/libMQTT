
#ifndef ETHERNETINTERFACEIPSTACK_H
#define ETHERNETINTERFACEIPSTACK_H

#include "EthernetInterface.h"

class IPStack 
{
public:    
    IPStack()
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


class Countdown
{
public:
    Countdown()
    {
        t = Timer();   
    }
    
    Countdown(int ms)
    {
        t = Timer();
        countdown_ms(ms);   
    }
    
    
    bool expired()
    {
        return t.read_ms() >= interval_end_ms;
    }
    
    void countdown_ms(int ms)  
    {
        t.stop();
        interval_end_ms = ms;
        t.reset();
        t.start();
    }
    
    void countdown(int seconds)
    {
        countdown_ms(seconds * 1000);
    }
    
    int left_ms()
    {
        return interval_end_ms - t.read_ms();
    }
    
private:
    Timer t;
    int interval_end_ms; 
};

#endif
