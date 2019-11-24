#define MQTTCLIENT_QOS2 1

#include <memory.h>

#include "MQTTClient.h"

#define DEFAULT_STACK_SIZE -1

#include "linux.cpp"

/*---------------------------------------------------------------------------*/
#include "tools-utils.h"

#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
/*---------------------------------------------------------------------------*/
#define BAUDRATE B115200
#define BAUDRATE_S "115200"

speed_t b_rate = BAUDRATE;
/*---------------------------------------------------------------------------*/
#ifdef linux
#define MODEMDEVICE (char *)"/dev/ttyUSB0"
#else
#define MODEMDEVICE "/dev/com1"
#endif /* linux */
/*---------------------------------------------------------------------------*/
#define SLIP_END      0300
#define SLIP_ESC      0333
#define SLIP_ESC_END  0334
#define SLIP_ESC_ESC  0335

#define CSNA_INIT     0x01

#define BUFSIZE         40
#define HCOLS           20
#define ICOLS           18

#define MODE_START_DATE  0
#define MODE_DATE        1
#define MODE_START_TEXT  2
#define MODE_TEXT        3
#define MODE_INT         4
#define MODE_HEX         5
#define MODE_SLIP_AUTO   6
#define MODE_SLIP        7
#define MODE_SLIP_HIDE   8
/*---------------------------------------------------------------------------*/
#ifndef O_SYNC
#define O_SYNC 0
#endif

#define OPEN_FLAGS (O_RDWR | O_NOCTTY | O_NDELAY | O_SYNC)
static unsigned char rxbuf[2048];
/*---------------------------------------------------------------------------*/
	static void
intHandler(int sig)
{
	exit(0);
}
/*---------------------------------------------------------------------------*/

fd_set mask, smask;
int fd;

unsigned char serialbuf[BUFSIZE];
int nfound;

bool InitSerialPort(){
	signal(SIGINT, intHandler);

	struct termios options;
	//fd_set mask, smask;
	//int fd;
	int baudrate = BUNKNOWN;
	char *device = MODEMDEVICE;
	char *timeformat = NULL;
	//unsigned char buf[BUFSIZE];
	char outbuf[HCOLS];
	unsigned char mode = MODE_START_TEXT;
	int flags = 0;
	unsigned char lastc = '\0';

	int index = 1;

	if(baudrate != BUNKNOWN) {
		b_rate = select_baudrate(baudrate);
		if(b_rate == 0) {
			fprintf(stderr, "unknown baudrate %d\n", baudrate);
			return false;//exit(-1);
		}
	}

	fprintf(stderr, "connecting to %s", device);

	fd = open(device, OPEN_FLAGS);

	if(fd < 0) {
		fprintf(stderr, "\n");
		perror("open");
		//exit(-1);
		return false;
	}
	fprintf(stderr, " [OK]\n");

	if(fcntl(fd, F_SETFL, 0) < 0) {
		perror("could not set fcntl");
		return false;//exit(-1);
	}

	if(tcgetattr(fd, &options) < 0) {
		perror("could not get options");
		return false;//exit(-1);
	}

	cfsetispeed(&options, b_rate);
	cfsetospeed(&options, b_rate);

	/* Enable the receiver and set local mode */
	options.c_cflag |= (CLOCAL | CREAD);
	/* Mask the character size bits and turn off (odd) parity */
	options.c_cflag &= ~(CSIZE | PARENB | PARODD);
	/* Select 8 data bits */
	options.c_cflag |= CS8;

	/* Raw input */
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
			| INLCR | IGNCR | ICRNL | IXON);
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	/* Raw output */
	options.c_oflag &= ~OPOST;

	if(tcsetattr(fd, TCSANOW, &options) < 0) {
		perror("could not set options");
		exit(-1);
	}

	FD_ZERO(&mask);
	FD_SET(fd, &mask);
	FD_SET(fileno(stdin), &mask);

	index = 0;
	return true;
}

int arrivedcount = 0;
IPStack ipstack = IPStack();
MQTT::Client<IPStack, Countdown> client = MQTT::Client<IPStack, Countdown>(ipstack);

int messagePublish(const char* topic,const char* messagedata){
	MQTT::Message message;
	char buf[100];
	sprintf(buf, "%s", messagedata);
	message.qos = MQTT::QOS1;
	message.retained = false;
	message.dup = false;
	message.payload = (void*)buf;
	message.payloadlen = strlen(buf)+1;

	client.publish(topic, message);
}

void zolertiaMessageArrived(MQTT::MessageData& md)
{
	MQTT::Message &message = md.message;

	printf("Message %d arrived: qos %d, retained %d, dup %d, packetid %d\n", 
			++arrivedcount, message.qos, message.retained, message.dup, message.id);
	printf("Payload %.*s\n", (int)message.payloadlen, (char*)message.payload);

	if(strncmp("on",(char *)message.payload,(int)message.payloadlen)==0){
		system("sudo /share/yepkit-USB-hub/ykushcmd -u 1");
		sleep(1);
		messagePublish("/iot/zolertia/reply","on");
	}
	if(strncmp("off",(char *)message.payload,(int)message.payloadlen)==0){
		system("sudo /share/yepkit-USB-hub/ykushcmd -d 1");
		sleep(1);
		messagePublish("/iot/zolertia/reply","off");
	}    
}

void sensorMessageArrived(MQTT::MessageData& md)
{
	MQTT::Message &message = md.message;

	printf("Message %d arrived: qos %d, retained %d, dup %d, packetid %d\n", 
			++arrivedcount, message.qos, message.retained, message.dup, message.id);
	printf("Payload %.*s\n", (int)message.payloadlen, (char*)message.payload);    
	if(InitSerialPort()){
		smask = mask;

		if(message.payloadlen > 0) {

			int i;
			char c;
			memset(serialbuf,0,sizeof(BUFSIZE));
			strcpy((char*)serialbuf,(char*)(message.payload));
			/*    fprintf(stderr, "SEND %d bytes\n", n);*/
			/* write slowly */
			for(i = 0; i < message.payloadlen; i++) {
				c=serialbuf[i];
				if(write(fd, &c, 1) <= 0) {
					perror("write");
					//exit(1);
				} else {
					fflush(NULL);
					usleep(6000);
				}
			}          
			// for of line
			c='\n';
			if(write(fd, &c, 1) <= 0) {
				perror("write");
				//exit(1);
			} else {
				fflush(NULL);
				usleep(6000);
			}
		}

		memset(serialbuf,0,sizeof(BUFSIZE));
		if(1/*FD_ISSET(fd, &smask)*/) {
			int i, n = read(fd, serialbuf, sizeof(serialbuf));
			if(n < 0) {
				perror("could not read");
				//exit(-1);
			}
			if(n == 0) {
				errno = EBADF;
				perror("serial device disconnected");
				//exit(-1);
			}


			printf("output: %s\n",serialbuf);
			messagePublish("/iot/sensor/reply",(char*)serialbuf);
		}
		close(fd);
	}
	else{
		messagePublish("/iot/sensor/reply","Serial_Have_Issues");
	}
}


int main(int argc, char* argv[])
{


	float version = 0.3;

	printf("Version is %f\n", version);

	const char* hostname = "52.14.169.245";
	int port = 1883;
	printf("Connecting to %s:%d\n", hostname, port);
	int rc = ipstack.connect(hostname, port);
	if (rc != 0)
		printf("rc from TCP connect is %d\n", rc);

	printf("MQTT connecting\n");
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
	data.MQTTVersion = 3;
	data.clientID.cstring = (char*)"mbed-icraggs";
	rc = client.connect(data);
	if (rc != 0)
		printf("rc from MQTT connect is %d\n", rc);
	printf("MQTT connected\n");

	rc = client.subscribe("/iot/sensor/cmd", MQTT::QOS1, sensorMessageArrived);   
	if (rc != 0)
		printf("rc from MQTT subscribe is %d\n", rc);
	rc = client.subscribe("/iot/zolertia/cmd", MQTT::QOS1, zolertiaMessageArrived);   
	if (rc != 0)
		printf("rc from MQTT subscribe is %d\n", rc);


	while(1){
		client.yield(100);
	}

	rc = client.unsubscribe("/iot/zolertia/cmd");
	if (rc != 0)
		printf("rc from unsubscribe was %d\n", rc);
	rc = client.unsubscribe("/iot/sensor/cmd");
	if (rc != 0)
		printf("rc from unsubscribe was %d\n", rc);

	rc = client.disconnect();
	if (rc != 0)
		printf("rc from disconnect was %d\n", rc);

	ipstack.disconnect();

	printf("Finishing with %d messages received\n", arrivedcount);

	return 0;
}

