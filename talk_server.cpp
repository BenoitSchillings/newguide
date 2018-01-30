#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zmq.hpp>
#include <string>
#include <map>

//----------------------------------------------------------------------------------------

class TalkServer {
public:;
    
	TalkServer();
    	~TalkServer();
    
	int				Init();
        
        std::map<std::string, int> 	values;
        int				HandleSetCommands(const char *);
	int 				HandleGetCommands(const char *);
	
	char 				reply[512]; 
    	char				buf[512];
	char				trace;
private:
};

//----------------------------------------------------------------------------------------

int TalkServer::Init()
{
    return 0;
}

//----------------------------------------------------------------------------------------


TalkServer::TalkServer()
{
    trace = 0;
}

//----------------------------------------------------------------------------------------

TalkServer::~TalkServer()
{
}


//----------------------------------------------------------------------------------------


int TalkServer::HandleSetCommands(const char * s)
{
    char tmp[512];
    int  v;

    int cnt = sscanf(s, "+%s %d", tmp, &v);
     if (strlen(tmp)>0) {
	values[std::string(tmp)] = v;
    }

    return 1234;
}

//----------------------------------------------------------------------------------------


int TalkServer::HandleGetCommands(const char * s)
{
    char tmp[512];
    int  v = -1;

    int cnt = sscanf(s, "=%s", tmp);
    if (strlen(tmp)>0) {
	v = values[std::string(tmp)];
    }

    return v;
}

//----------------------------------------------------------------------------------------


int main()
{
    TalkServer  *talker;
    char      command[1024];
 
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind ("tcp://*:5555");

    int timeout = 20;
    socket.setsockopt (ZMQ_RCVTIMEO, &timeout, sizeof (int));
 
    talker = new TalkServer();
  
    int error = 0;
 
    do { 
	error = talker->Init();
	if (error != 0) {
		usleep(1000*1000);		//sleep for 1 second 
	}
    } while(error != 0);


    int check = 0;

     while(1) {
        check++; 
       	zmq::message_t request;
        int result = socket.recv (&request);
	if (request.size() > 0) {
		memcpy(command, request.data(), request.size());
		if (command[0] == '+') {
			int result = talker->HandleSetCommands(command);
			sprintf(talker->reply, "%d", result);	
		}
		if (command[0] == '=') {
			int result = talker->HandleGetCommands(command);
			sprintf(talker->reply, "%d", result);	
		}
		if (talker->trace) {
			printf("server:: in command %s\n", command);
			printf("server:: reply %s\n", talker->reply);
		}	
		zmq::message_t msg_reply(strlen(talker->reply) + 1);
        	memcpy (msg_reply.data (), talker->reply, strlen(talker->reply) + 1);
        	socket.send(msg_reply);
	}
    }
    
    return 0;
}
