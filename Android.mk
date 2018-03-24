LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
PAHO_C_FILES:= paho/Clients.c paho/MQTTAsync.c paho/MQTTPersistence.c \
	 						 paho/Socket.c paho/Tree.c paho/Heap.c \
               paho/Messages.c paho/MQTTPersistenceDefault.c \
							 paho/SocketBuffer.c paho/utf-8.c paho/LinkedList.c \
							 paho/MQTTPacket.c paho/MQTTProtocolClient.c \
							 paho/StackTrace.c paho/Log.c paho/MQTTPacketOut.c \
							 paho/MQTTProtocolOut.c paho/Thread.c \

INIH_C_FILES:= inih/ini.c

SCHEDULER_FILES:= TaskScheduler/TaskScheduler.cpp
LOCAL_SRC_FILES:= wink_manager.cpp $(SCHEDULER_FILES)  $(PAHO_C_FILES) $(INIH_C_FILES)
LOCAL_CPPFLAGS:= -Wall -std=c++14 -Ipaho/ -ITaskScheduler/ -Iinih/
LOCAL_MODULE:= wink_manager
include $(BUILD_EXECUTABLE) # Tell ndk-build that we want to build a native executable.
