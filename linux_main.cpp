
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <pthread.h>

#include "MQTTClient.h"
#include "MQTTClient.cpp"
#include "FP.cpp"

#define DEFAULT_STACK_SIZE -1

class Thread {
public:

    Thread(void* (*fn)(void *parameter), void *parameter=NULL,
           int priority=0,
           uint32_t stack_size=DEFAULT_STACK_SIZE,
           unsigned char *stack_pointer=NULL)
	{
		thread = 0;
		pthread_attr_t attr;

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&thread, &attr, fn, parameter) != 0)
			thread = 0;
		pthread_attr_destroy(&attr);
	}
    
private:
	pthread_t thread;
};


class IPStack 
{
public:    
    IPStack()
    {

    }
    
	int Socket_error(const char* aString)
	{

		if (errno != EINTR && errno != EAGAIN && errno != EINPROGRESS && errno != EWOULDBLOCK)
		{
			if (strcmp(aString, "shutdown") != 0 || (errno != ENOTCONN && errno != ECONNRESET))
				printf("Socket error %s in %s for socket %d\n", strerror(errno), aString, mysock);
		}
		return errno;
	}

    int connect(const char* hostname, int port)
    {
		int type = SOCK_STREAM;
		struct sockaddr_in address;
		int rc = -1;
		sa_family_t family = AF_INET;
		struct addrinfo *result = NULL;
		struct addrinfo hints = {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0, NULL, NULL, NULL};

		if ((rc = getaddrinfo(hostname, NULL, &hints, &result)) == 0)
		{
			struct addrinfo* res = result;

			/* prefer ip4 addresses */
			while (res)
			{
				if (res->ai_family == AF_INET)
				{
					result = res;
					break;
				}
				res = res->ai_next;
			}

			if (result->ai_family == AF_INET)
			{
				address.sin_port = htons(port);
				address.sin_family = family = AF_INET;
				address.sin_addr = ((struct sockaddr_in*)(result->ai_addr))->sin_addr;
			}
			else
				rc = -1;

			freeaddrinfo(result);
		}

		if (rc == 0)
		{
			mysock = socket(family, type, 0);
			if (mysock != -1)
			{
				int opt = 1;

				//if (setsockopt(mysock, SOL_SOCKET, SO_NOSIGPIPE, (void*)&opt, sizeof(opt)) != 0)
				//	printf("Could not set SO_NOSIGPIPE for socket %d", mysock);
				
				rc = ::connect(mysock, (struct sockaddr*)&address, sizeof(address));
			}
		}

        return rc;
    }

    int read(char* buffer, int len, int timeout)
    {
		printf("reading %d bytes\n", len);
		int rc = ::recv(mysock, buffer, (size_t)len, 0);
		if (rc == -1)
			Socket_error("read");
		printf("read %d bytes\n", rc);
		return rc;
    }
    
    int write(char* buffer, int len, int timeout)
    {
		return ::write(mysock, buffer, len);
    }
    
private:

    int mysock; 
    
};


class Timer
{
public:

	Timer()
	{
		reset();
	}

	void start()
	{
		if (running)
			return;
		if (!timerisset(&stop_time))
			gettimeofday(&start_time, NULL);
		else 
		{
			struct timeval now, res;

			gettimeofday(&now, NULL);

			timersub(&now, &stop_time, &res); // interval to be added to start time
			timeradd(&start_time, &res, &stop_time);
			stop_time = start_time;
			timerclear(&stop_time);
		}
		running = true;
	}

	void stop()
	{
		if (running)
		{
			gettimeofday(&stop_time, NULL);
			running = false;
		}
	}

	void reset()
	{
		timerclear(&start_time);
		timerclear(&stop_time);
		running = false;
	}

	int read_ms() // get the time passed in milli-seconds
	{
		struct timeval now, res;

		gettimeofday(&now, NULL);
		timersub(&now, &start_time, &res);
		return (res.tv_sec)*1000 + (res.tv_usec)/1000;
	}

private:

	struct timeval start_time, stop_time;
	bool running;

};


void messageArrived(MQTT::Message* message)
{
}


int main(int argc, char* argv[])
{   
    IPStack ipstack = IPStack();
	Timer t;
	FP<void, MQTT::Message*> messageArrivedPointer;

	messageArrivedPointer.attach(messageArrived);
      
    int rc = ipstack.connect("127.0.0.1", 1883);
	printf("rc from TCP connect is %d\n", rc);
        
    MQTT::Client<IPStack, Timer, Thread> client = MQTT::Client<IPStack, Timer, Thread>(&ipstack, &t);

	printf("constructed\n");
    
    rc = client.connect();
	printf("rc from connect is %d\n", rc);

	rc = client.subscribe("topic", MQTT::QOS2, messageArrivedPointer);
	sleep(1);
}
