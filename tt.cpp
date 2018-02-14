#include "ASICamera.h"
#include <sys/time.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <time.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "./tiny/tinyxml.h"
#include <signal.h>
#include <zmq.hpp>
#include <iostream>
#include "/home/benoit/skyx_tcp/skyx.h"

#include "talk.cpp"
#include "./tiptilt/motor.cpp"


bool sim = false;

tiptilt *tt;
Talk	*talk;


void intHandler(int signum)
{
    tt_sighandler(signum);
    closeCamera();
    printf("emergency close\n");
    exit(0);
}

//--------------------------------------------------------------------------------------


int main(int argc, char **argv)
{
    signal(SIGINT, intHandler);


    tt = new tiptilt();
 
    int i = 0;

    if (argc == 2) {

	if (strcmp(argv[1], "reset") == 0) {
		tt->reset_pos();
		goto exit;
	}

        float focus_move = atof(argv[1]);
	if (fabs(focus_move > 12500.0)) {
		printf("max move %f\n", focus_move);
	}
	else {
		tt->MoveFocus(focus_move);
	}
    }

    if (argc == 3) {
        float dx = atof(argv[1]);
	float dy = atof(argv[2]);

	if (fabs(dx > 12500.0) || fabs(dy > 12500)) {
		printf("max tilt %f %f\n", dx, dy);
	}
	else {
		tt->Move(dx, dy);
	}
    }

exit:;

    delete tt;

}
