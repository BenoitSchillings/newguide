ver = release 
platform = x64 

CC = g++
TINY = ./tinyobj/tinyxmlerror.cpp.o ./tinyobj/tinystr.cpp.o ./tinyobj/tinyxmlparser.cpp.o ./tinyobj/tinyxml.cpp.o
OPENCV0 = -I/usr/local/include/opencv -I/usr/local/include -L/usr/local/lib -lopencv_calib3d -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_ts -lopencv_video -lopencv_videostab -L/usr/local/lib -lzmq 
OPENCV = -I/usr/local/include/opencv  -lopencv_calib3d -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_imgcodecs -lopencv_highgui -lopencv_imgproc -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_superres  -lopencv_video -lopencv_videostab  -lzmq 

USB =  -I./libusb/include  -L./libusb/$(platform) -lusb-1.0  



LIBSPATH = -L./lib/$(platform) -I./include



ifeq ($(ver), debug)
DEFS = -D_LIN -D_DEBUG 
CFLAGS = -g  -I $(INCLIB) -L $(LDLIB) $(DEFS) $(COMMON) $(LIBSPATH)  -lpthread  $(USB) -DGLIBC_20
else
DEFS = -D_LIN 
CFLAGS =  -O3 -I $(INCLIB) -L $(LDLIB) $(DEFS) $(COMMON) $(LIBSPATH)  -lpthread  $(USB) -DGLIBC_20
endif

ifeq ($(platform), mac32)
CC = g++
CFLAGS += -D_MAC -m32
OPENCV = -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_ts -lopencv_video -lopencv_videostab -I/usr/local/include/opencv `pkg-config --libs opencv`
endif

ifeq ($(platform), mac64)
CC = g++
CFLAGS += -D_MAC -m64 -O3
OPENCV = -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_ts -lopencv_video -lopencv_videostab -I/usr/local/include/opencv
endif

ifeq ($(platform), x86)
CFLAGS += -m32
CFLAGS += -lrt
endif


ifeq ($(platform), x64)
CFLAGS += -m64
CFLAGS += -lrt
endif


all: guide174ao talk_server
	
guide174ao: talk.cpp guide174ao.cpp util.cpp ./tinyobj/tinystr.cpp.o ./tinyobj/tinyxmlparser.cpp.o ./tinyobj/tinyxml.cpp.o ./tinyobj/tinyxmlerror.cpp.o
	$(CC) -march=native -O3 guide174ao.cpp  ao.cpp util.cpp ~/skyx_tcp/skyx.cpp ~/skyx_tcp/sky_ip.cpp $(TINY) -o guide174ao $(CFLAGS)  $(OPENCV) -lASICamera

talk_server:  talk_server.cpp util.cpp ./tinyobj/tinystr.cpp.o ./tinyobj/tinyxmlparser.cpp.o ./tinyobj/tinyxml.cpp.o ./tinyobj/tinyxmlerror.cpp.o
	$(CC) -march=native -O3 -lzmq talk_server.cpp util.cpp $(TINY) -lzmq -o talk_server $(CFLAGS)


./tinyobj/tinystr.cpp.o: ./tiny/tinystr.cpp
	$(CC)  -c ./tiny/tinystr.cpp -o ./tinyobj/tinystr.cpp.o $(CFLAGS)

./tinyobj/tinyxml.cpp.o: ./tiny/tinyxml.cpp
	$(CC)  -c ./tiny/tinyxml.cpp -o ./tinyobj/tinyxml.cpp.o $(CFLAGS)

./tinyobj/tinyxmlparser.cpp.o: ./tiny/tinyxmlparser.cpp
	$(CC)  -c ./tiny/tinyxmlparser.cpp -o ./tinyobj/tinyxmlparser.cpp.o $(CFLAGS)

./tinyobj/tinyxmlerror.cpp.o: ./tiny/tinyxmlerror.cpp
	$(CC)  -c ./tiny/tinyxmlerror.cpp -o ./tinyobj/tinyxmlerror.cpp.o $(CFLAGS)

clean:
	rm -fguide174ao talk_server
#pkg-config libusb-1.0 --cflags --libs
#pkg-config opencv --cflags --libs

