#if !defined(MQTT_MBED_H)
#define MQTT_MBED_H

#include "mbed.h"

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