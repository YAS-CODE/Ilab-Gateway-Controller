APPS = Gateways
LIB_SRCS = tools-utils.c ../paho.mqtt.embedded-c-master/MQTTPacket/src/MQTTPacket.c ../paho.mqtt.embedded-c-master/MQTTPacket/src/MQTTDeserializePublish.c ../paho.mqtt.embedded-c-master/MQTTPacket/src/MQTTConnectClient.c ../paho.mqtt.embedded-c-master/MQTTPacket/src/MQTTSubscribeClient.c ../paho.mqtt.embedded-c-master/MQTTPacket/src/MQTTSerializePublish.c ../paho.mqtt.embedded-c-master/MQTTPacket/src/MQTTUnsubscribeClient.c
DEPEND = tools-utils.h
INC = -I ../paho.mqtt.embedded-c-master/MQTTClient/src/ -I ../paho.mqtt.embedded-c-master/MQTTClient/src/linux -I ../paho.mqtt.embedded-c-master/MQTTPacket/src
all: $(APPS)

CC=g++
CFLAGS += 

$(APPS) : % : %.cpp $(LIB_SRCS) $(DEPEND)
	$(CC) $(INC) $(CFLAGS) $< $(LIB_SRCS) -o $@

clean:
	rm -f $(APPS)
