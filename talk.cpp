#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <zmq.hpp>
#include <iostream>

//----------------------------------------------------------------------------------------

class Talk {
public:;
    
	Talk();
    	~Talk();
    
	int	Init();
    	int	Send(const char*);
	int	Get(const char*);
	void	Set(const char*, int v);
    	int	Reply();
     	void	Done();

	int	XCommand(const char *cmd);	
    	char 	reply[512]; 
    	char	buf[512];
	char	trace;
private:
    	zmq::context_t *ctx;	
	zmq::socket_t  *socket;	
};

//----------------------------------------------------------------------------------------

int Talk::XCommand(const char* cmd)
{
	Send(cmd);
	
	int result;
	
	result = atoi(reply);

	return result;
}


//----------------------------------------------------------------------------------------

int Talk::Send(const char *cmd)
{
	int	len = strlen(cmd) + 1;
	
	zmq::message_t request(len);
	memcpy(request.data(), cmd, len);
	if (trace) printf("sending command %s\n", cmd);

	socket->send(request);

	zmq::message_t msg_reply;
	socket->recv(&msg_reply);

	int	reply_len = msg_reply.size();

	reply[0] = 0;
	
	memcpy(reply, msg_reply.data(), reply_len); 
	if (trace) printf("reply %s\n", reply);	
	return 0;
}

//----------------------------------------------------------------------------------------

int	Talk::Get(const char*var)
{
	char tmp[512];
	sprintf(tmp, "=%s", var);
	Send(tmp);
	int result = atoi(reply);
	return result;
}

//----------------------------------------------------------------------------------------
	

void	Talk::Set(const char* var, int v)
{
	char tmp[512];

	sprintf(tmp, "+%s %d", var, v);
	Send(tmp);
}
 
//----------------------------------------------------------------------------------------


int Talk::Init()
{
    ctx = new zmq::context_t(1);
    socket = new zmq::socket_t(*ctx, ZMQ_REQ);
    socket->connect ("tcp://localhost:5555");

    return 0;
}

//----------------------------------------------------------------------------------------


Talk::Talk()
{
	Init();
}

//----------------------------------------------------------------------------------------

Talk::~Talk()
{
}



