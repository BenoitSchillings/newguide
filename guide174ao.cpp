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

Talk	*talk;

tiptilt *tt;

//--------------------------------------------------------------------------------------

float get_value(const char *name);
void  set_value(const char *name, float value);

//--------------------------------------------------------------------------------------

using namespace cv;
using namespace std;

//--------------------------------------------------------------------------------------

#define ushort unsigned short
#define uchar unsigned char
#define PTYPE unsigned short
#define BOX_HALF 36 // Centroid box half width/height

//--------------------------------------------------------------------------------------


float  g_gain = 150.0;
float  g_mult = 2.0;
float  g_exp = 0.1;

float gain0 = -1;
float exp0 = -1;

//--------------------------------------------------------------------------------------

void Wait(float t)
{
    usleep(t * 1000000.0);
}

void blit(Mat from, Mat to, int x1, int y1, int w, int h, int dest_x, int dest_y)
{
    ushort *source = (ushort*)(from.data);
    ushort *dest = (ushort*)(to.data);

    int	rowbyte_source = from.step/2;
    int	rowbyte_dest = to.step/2;

    if ((dest_x + w) >= to.cols) {
        w = -1 + to.cols - dest_x;
    }

    if ((dest_y + h) >= to.rows) {
        h = -1 + to.rows - dest_y;
    }

    if ((x1 + w) >= from.cols) {
        w = -1 + from.cols - x1;
    }

    if ((y1 + h) >= from.rows) {
        h = -1 + from.rows - y1;
    }

    for (int y = y1; y < y1 + h; y++) {
        int	dest_yc = dest_y + y;

        int off_y_dst = rowbyte_dest * dest_yc + dest_x + x1;
        int src_y_dst = rowbyte_source * y + x1;

        memcpy(dest + off_y_dst, source + src_y_dst, w*2);
    }
}

//--------------------------------------------------------------------------------------

void cvText(Mat img, const char* text, int x, int y)
{
    putText(img, text, Point2f(x, y), FONT_HERSHEY_PLAIN, 1, CV_RGB(62000, 62000, 62000), 1, 8);
}

//--------------------------------------------------------------------------------------

void center(Mat img)
{
    rectangle(img, Point(700-10, 500-10), Point(700+10, 500+10), Scalar(9000, 9000, 9000), 1, 8);
}

void DrawVal(Mat img, const char* title, float value, int y, const char *units)
{
    char    buf[512];

    y *= 30;
    y += 35;
    int x = 60;

    rectangle(img,
              Point(x - 7, y - 16), Point(x + 300, y + 6),
              Scalar(9000, 9000, 9000),
              -1,
              8);

    rectangle(img,
              Point(x - 7, y - 16), Point(x + 300, y + 6),
              Scalar(20000, 20000, 20000),
              1,
              8);

    sprintf(buf, "%s=%3.3f %s", title, value, units);
    cvText(img, buf, x, y);
}

//--------------------------------------------------------------------------------------


class Guider
{
public:
    Guider();
    ~Guider();

    bool 	GetFrame();

    int 	width;
    int 	height;
    Mat 	image;
    Mat	temp_image;
    int	ref_x;
    int	ref_y;
    int	guide_box_size;
    int	frame;
    float	gain;
    float	exp;
    float	background;
    float	dev;
    float	mount_dx1;
    float	mount_dy1;
    float	mount_dx2;
    float	mount_dy2;
    float	gain_x;
    float	gain_y;
private:
    void 	InitCam(int cx, int cy, int width, int height);

public:
    float 	error_to_tx(float mx, float my);
    float 	error_to_ty(float mx, float my);


public:
    void 	Centroid(float*cx, float*cy, float*total_v);
    bool 	HasGuideStar();
    bool 	FindGuideStar();
    Mat	GuideCrop();
    void	MinDev();
    void	Move(float dx, float dy);
};

//--------------------------------------------------------------------------------------
// guide solving
// http://www.wolframalpha.com/input/?i=solve+m%3D+x*u+%2B+x*y*v;+n+%3D+x*w+%2B+y*z+for+x,y
//--------------------------------------------------------------------------------------


float Guider::error_to_tx(float mx, float my)
{
    float num = (mount_dy2 * mx) - (mount_dx2*my);
    float den = (mount_dx1*mount_dy2)-(mount_dx2*mount_dy1);

    return(num/den);
}

//--------------------------------------------------------------------------------------

float Guider::error_to_ty(float mx, float my)
{
    float num = (mount_dy1*mx - mount_dx1*my);
    float den = (mount_dx2*mount_dy1 - mount_dx1*mount_dy2);

    return(num/den);
}

//--------------------------------------------------------------------------------------

void Guider::MinDev()
{
    float	count = 0;
    float	sum = 0;

    background = 1e9;

    for (int y = 1; y < (height-1); y += 20) {
        for (int x = 1; x < (width-1); x += 20) {
            float v = image.at<unsigned short>(y, x);
            float v1 = image.at<unsigned short>(y, x + 1);

            if (v < background) background = v;
            count += 1;
            float v2 = (v1-v);
            sum += (v2*v2);
        }
    }
    sum = sum / count;
    sum /= 2.0;
    dev = sqrt(sum);
 }

//--------------------------------------------------------------------------------------

Guider::Guider()
{
    width = 1600/2;
    height = 1100/2; 
    frame = 0;
    background = 0;
    dev = 100;

    image = Mat(Size(width, height), CV_16UC1);
    temp_image = Mat(Size(width, height), CV_16UC1);

    guide_box_size = 32;

    ref_x = -1;
    ref_y = -1;

    if (!sim) InitCam(0, 0, width, height);


    mount_dx1 = get_value("mount_dx1")/500.0;
    mount_dx2 = get_value("mount_dx2")/500.0;
    mount_dy1 = get_value("mount_dy1")/500.0;
    mount_dy2 = get_value("mount_dy2")/500.0;
    gain_x = 1.0;
    gain_y = 1.0;
}

//--------------------------------------------------------------------------------------


void	Guider::Move(float dx, float dy)
{
    //while(fabs(dx) > 15 || fabs(dy) > 15) { dx/=2.0;dy/=2.0;};
    tt->Move(dx, dy);
}

//--------------------------------------------------------------------------------------

bool Guider::HasGuideStar()
{
    return (ref_x > 0);
}

//--------------------------------------------------------------------------------------

bool Guider::FindGuideStar()
{
    MinDev();
    GaussianBlur(image, temp_image, Point(7, 7), 5);
    
    ref_x = -1;
    int	x, y;
    long	max = 0;

    int	cx = width / 2;
    int	cy = height / 2;

    for (y = 30; y < height - 30; y++) {
        for (x = 30; x < width - 30; x++) {
            int v = temp_image.at<unsigned short>(y, x) +
                    temp_image.at<unsigned short>(y, x + 1) +
                    temp_image.at<unsigned short>(y + 1, x) +
                    temp_image.at<unsigned short>(y + 1, x + 1);
            if (v > max) {
                max = v;
                ref_x = x;
                ref_y = y;
            }
        }
    }

    if (max < 0) {
        ref_x = -1;
        ref_y = -1;
        return false;
    }
    printf("max %ld, %d %d\n", max, ref_x, ref_y);
    return true;
}


//--------------------------------------------------------------------------------------


void Guider::InitCam(int cx, int cy, int width, int height)
{
    int CamNum=0;
    bool bresult;


    int numDevices = getNumberOfConnectedCameras();
    if(numDevices <= 0) {
        printf("no device\n");
        return;
        exit(-1);
    }

    CamNum = 0;

    bresult = openCamera(CamNum);
    if(!bresult) {
        printf("could not open camera\n");
        exit(-1);
    }


    initCamera(); //this must be called before camera operation. and it only need init once
    printf("resolution %d %d\n", getMaxWidth(), getMaxHeight());
 
    //pulseGuide(guideNorth, 250);
    setImageFormat(width, height, 2, IMG_RAW16);
    setValue(CONTROL_BRIGHTNESS, 250, false);
    setValue(CONTROL_GAIN, 0, false);
    printf("max %d\n", getMax(CONTROL_BANDWIDTHOVERLOAD));

    setValue(CONTROL_BANDWIDTHOVERLOAD, 64, false); //lowest transfer speed
    setValue(CONTROL_EXPOSURE, 10, false);
    setValue(CONTROL_HIGHSPEED, 1, false);
    setValue(CONTROL_HARDWAREBIN, 1, false); 
    bool foo; 
}

//--------------------------------------------------------------------------------------

void Guider::Centroid(float*cx, float*cy, float*total_v)
{
    float bias = 0;
    float cnt;

    MinDev();
    cnt = 0.0;

    for (int j = 0; j < 4; j++)
        for (int i = 0; i < guide_box_size; i++) {
            bias +=  image.at<unsigned short>(ref_y + i - guide_box_size/2, ref_x - guide_box_size/2 - j);
            cnt+= 1.0;
        }
    bias /= cnt;
    bias += dev * 4;
    //printf("%f\n", bias);

    int vx, vy;
    float sum_x = 0;
    float sum_y = 0;
    float total = 0;
    int pcnt = 0;

    for (vy = ref_y - guide_box_size/2; vy <= ref_y + guide_box_size/2; vy++) {
        for (vx = ref_x - guide_box_size/2; vx <= ref_x + guide_box_size/2; vx++) {
            float v = image.at<unsigned short>(vy, vx);
            v -= bias;
            if (v > 0) {
                sum_x = sum_x + (float)vx * v;
                sum_y = sum_y + (float)vy * v;
                total = total + v;
                pcnt++;
            }
        }
    }
    sum_x = sum_x / total;
    sum_y = sum_y / total;
    *cx = sum_x;
    *cy = sum_y;
    //printf("%f %d\n", total, pcnt);
    if (pcnt < 4) total = 0;
    *total_v = total;
}


//--------------------------------------------------------------------------------------

bool Guider::GetFrame()
{
    frame++;
     bool got_it;
    int total = 0;

    got_it = getImageData(image.ptr<uchar>(0), width * height * sizeof(PTYPE), -1);


    if (!got_it) {
        printf("bad cam\n");
    }
    return got_it;
}

//--------------------------------------------------------------------------------------

Mat Guider::GuideCrop()
{
    Mat tmp;

    tmp = Mat(image, Rect(ref_x - guide_box_size, ref_y - guide_box_size, guide_box_size*2, guide_box_size * 2));
    resize(tmp, tmp, Size(0, 0), 3, 3, INTER_NEAREST);
    return tmp;
}

//--------------------------------------------------------------------------------------

void hack_gain_upd(Guider *aguide)
{
    float gain = cvGetTrackbarPos("gain", "video");
    float exp = cvGetTrackbarPos("exp", "video");
    exp = exp / 1000.0;
    g_exp = exp;
    g_gain = gain;
    g_mult = cvGetTrackbarPos("mult", "video")/10.0;
    if (exp0 != exp || gain0 != gain) {

        if (sim == 0) {
            setValue(CONTROL_GAIN, gain, false);
            setValue(CONTROL_EXPOSURE, exp*1000000, false);
            setValue(CONTROL_BRIGHTNESS, 200, false);
        }

        gain0 = gain;
        exp0 = exp;
        aguide->gain = gain;
        aguide->exp = exp;
    }
}

//--------------------------------------------------------------------------------------

void ui_setup()
{
    namedWindow("video", 1);
    createTrackbar("gain", "video", 0, 600, 0);
    createTrackbar("exp", "video", 0, 1000, 0);
    createTrackbar("mult", "video", 0, 100, 0);
    createTrackbar("Sub", "video", 0, 15500, 0);

    setTrackbarPos("gain", "video", g_gain);
    setTrackbarPos("exp", "video", 1000.0 * g_exp);
    setTrackbarPos("mult", "video", 10.0 *g_mult);
}

//--------------------------------------------------------------------------------------


int find_guide()
{
    Guider *g;

    g = new Guider();
    ui_setup();
    hack_gain_upd(g);

    startCapture();

    int cnt = 0;

    while(1) {
        g->GetFrame();

        cnt++;
        if (g->frame % 1 == 0) {
            g->MinDev();
            center(g->image);
            DrawVal(g->image, "exp ", g->exp, 0, "sec");
            DrawVal(g->image, "gain", g->gain, 1, "");
            DrawVal(g->image, "frame", g->frame*1.0, 2, "");

	    g->image = g->image - (cvGetTrackbarPos("Sub", "video") - 1000);
            g->image = g->image * (0.1 * cvGetTrackbarPos("mult", "video"));
 
	    cv::imshow("video", g->image);
            char c = cvWaitKey(1);
            hack_gain_upd(g);

            if (c == 27) {
                stopCapture();
                closeCamera();
                return 0;
            }
        }
    }
}

//--------------------------------------------------------------------------------------

int guide()
{
    float   sum_x;
    float   sum_y;
    int     frame_per_correction = 1;
    int     frame_count;
    int	drizzle_dx = 0;
    int	drizzle_dy = 0;
    int	err = 0;

    Guider *g = new Guider();

    char buf[512];


    ui_setup();
    hack_gain_upd(g);
    Mat uibm = Mat(Size(1200, 800), CV_16UC1);

    startCapture();

    Mat zoom;



    frame_count = 0;
    sum_x = 0;
    sum_y = 0;

    int logger = 0;
    int bad = 0;

    int nn = 0;

    float ftx = 0; float fty = 0;

    while(1) {
restart:
again:
	bad = 0;

        g->GetFrame();
        if (!g->HasGuideStar()) {
            blit(g->image   * (0.1 * cvGetTrackbarPos("mult", "video")), uibm, 0, 0, 2300, 2300, 0, 0);
            if (g->FindGuideStar()) {
                uibm = Mat(Size(400, 400), CV_16UC1, cv::Scalar(0));
            }
        }

        ushort *src;

        src = (ushort*)g->image.ptr<uchar>(0);


        hack_gain_upd(g);


        if (g->HasGuideStar()) {
	    nn++;
            float cx;
            float cy;
            float total_v;
            g->Centroid(&cx, &cy, &total_v);

	    if (total_v == 0) {
		bad++;
	    }
	    else 
		bad = 0;


	    if (bad == 10) {
		printf("no guide star. restart\n");
		goto restart;
	    }

 	    if (total_v > 0) {
                float dx = cx-g->ref_x + drizzle_dx;
                float dy = cy-g->ref_y + drizzle_dy;
                sum_x += dx;
                sum_y += dy;
                frame_count++;

                if (frame_count == frame_per_correction) {
                    sum_x = sum_x / frame_per_correction;
                    sum_y = sum_y / frame_per_correction;
                    float tx = g->error_to_tx(sum_x, sum_y);
                    float ty = g->error_to_ty(sum_x, sum_y);
                    //printf("error %f %f\n", sum_x, sum_y); 
		    sum_x = 0;
                    sum_y = 0;
                    frame_count = 0;
		    printf("%d %f %f/ %f %f\n", nn, -tx*0.2, -ty*0.2, ftx*0.2, fty*0.2);
		    ftx += tx;fty += ty;
		    g->Move(-tx * 0.2, -ty * 0.2);
                }
            }

            logger++;

            float mult = 0.1 * cvGetTrackbarPos("mult", "video");
            blit(mult * g->GuideCrop(), uibm, 0, 0, 1000, 1000, 150, 150);
            DrawVal(uibm, "tot ", total_v, 2, "adu");
            DrawVal(uibm, "cx ", cx, 3, "");
            DrawVal(uibm, "cy ", cy, 4, "");
        }

        DrawVal(uibm, "exp ", g->exp, 0, "sec");
        DrawVal(uibm, "gain", g->gain, 1, "");


	if (nn % 5 == 0) {
		if (talk->Get("resetguide") == 1) {
			//talk->Set("resetpt", 1);
			tt->MoveTo(0, 0); 
			usleep(2000*1000);
			talk->Set("resetguide", 0);
			g->ref_x = -1;
			g->GetFrame();
			g->GetFrame();
			g->GetFrame();
			printf("reset guide\n");
			goto again;
		}

        	cv::imshow("video", uibm);

        	char c = cvWaitKey(1);
        	if (c == 27) {

            		stopCapture();
            		closeCamera();
            		return 0;
        	}
	}
    }
}

//--------------------------------------------------------------------------------------

float calc_d(float x1, float y1, float x2, float y2)
{
	float dx = (x2-x1);
	float dy = (y2-y1);

	return(sqrt(dx*dx+dy*dy));
}

//--------------------------------------------------------------------------------------

int ao_calibrate(int m)
{
    Guider *g = new Guider();
    float  x1, y1;
    float  x2, y2;
    float  x3, y3;

    float  d1, d2;

    ui_setup();
    hack_gain_upd(g);
    Mat uibm = Mat(Size(1200, 800), CV_16UC1);
    startCapture();


    for (int i = 0; i < 3; i++) g->GetFrame();
    
    g->FindGuideStar();

    x1 = g->ref_x;
    y1 = g->ref_y;

    printf("v1 %f %f\n", x1, y1);


	    tt->setxyz(0, 0, 4000);
    
    usleep(2000*1000);
    for (int i = 0; i < 3; i++) g->GetFrame();
    
    g->FindGuideStar();

    x2 = g->ref_x;
    y2 = g->ref_y;

    printf("v2 %f %f\n", x2, y2);


    tt->setxyz(0, 0, 0);
    usleep(2000*1000);
 
    for (int i = 0; i < 3; i++) g->GetFrame();
    
    g->FindGuideStar();

    x3 = g->ref_x;
    y3 = g->ref_y;

    printf("v3 %f %f\n", x3, y3);


    d1 = calc_d(x1, y1, x2, y2);
    d2 = calc_d(x2, y2, x3, y3);

    printf("%d -- d-> [%f, %f]\n", m, d1, d2);
    stopCapture();
    closeCamera();

    return 0;
}

//--------------------------------------------------------------------------------------


int calibrate()
{
    Guider *g = new Guider();
    float  x1, y1;
    float  x2, y2;
    float  x3, y3;

    ui_setup();
    hack_gain_upd(g);
    Mat uibm = Mat(Size(1200, 800), CV_16UC1);
    startCapture();
//-----------------------------
//   x1,y1 ------->x(5)-->x2,y2
//                         |
//                         |
//                         |
//                         |
//                        y(5)
//                         |
//                         |
//                        x3,y3
//     then move back x(-5), y(-5)


    {
        for (int i = 0; i < 8; i++) g->GetFrame();
        g->FindGuideStar();

        x1 = g->ref_x;
 	y1 = g->ref_y;
 	printf("v1 %f %f\n", x1, y1);


	g->Move(500, 0);

	cv::imshow("video", g->image);
        char c = cvWaitKey(1);
        for (int i =0; i < 8; i++)
            g->GetFrame();

        g->FindGuideStar();
        x2 = g->ref_x;
        y2 = g->ref_y;
        printf("v2 %f %f\n", x2, y2);

	g->Move(0, 500);

	cv::imshow("video", g->image);
        c = cvWaitKey(1);

        for (int i = 0; i < 8; i++) g->GetFrame();
        g->FindGuideStar();
        x3 = g->ref_x;
        y3 = g->ref_y;
        printf("v3 %f %f\n", x3, y3);

        cv::imshow("video", g->image);
        c = cvWaitKey(1);

	g->Move(-500, -500);
      
        for (int i = 0; i < 8; i++) g->GetFrame(); 
        g->FindGuideStar();
        float x4 = g->ref_x;
        float y4 = g->ref_y;
	 
	printf("v4 %f %f\n", x4, y4);
        stopCapture();
        closeCamera();
    }
    if (x1 < 0 || x2 < 0 || x3 < 0) {
        printf("no reference star\n");
        exit(-1);
    }

    g->mount_dx1 = (x2-x1);
    g->mount_dy1 = (y2-y1);
    g->mount_dx2 = (x3-x2);
    g->mount_dy2 = (y3-y2);

    set_value("mount_dx1", g->mount_dx1);
    set_value("mount_dx2", g->mount_dx2);
    set_value("mount_dy1", g->mount_dy1);
    set_value("mount_dy2", g->mount_dy2);

    g->mount_dx1 = (x2-x1)/500.0;
    g->mount_dy1 = (y2-y1)/500.0;
    g->mount_dx2 = (x3-x2)/500.0;
    g->mount_dy2 = (y3-y2)/500.0;

    return 0;
}

//--------------------------------------------------------------------------------------

void test_guide()
{
    Guider *g;

    g = new Guider();


    g->mount_dx1 = 10.0;
    g->mount_dy1 = 0.001;
    g->mount_dx2 = 0.001;
    g->mount_dy2 = 10.0;


    printf("error is -10 pixel in x  %f %f\n", g->error_to_tx(-10, 0), g->error_to_ty(-10, 0));
    printf("error is 10 pixel in y  %f %f\n", g->error_to_tx(0, 10), g->error_to_ty(0, 10));
    printf("error is 10 pixel in x&y  %f %f\n", g->error_to_tx(10, 10), g->error_to_ty(10, 10));
    printf("error is 10 pixel in x and -10 y  %f %f\n", g->error_to_tx(10, -10), g->error_to_ty(10, -10));

}



//--------------------------------------------------------------------------------------

bool match(char *s1, const char *s2)
{
    return(strncmp(s1, s2, strlen(s2)) == 0);
}

//--------------------------------------------------------------------------------------

void help(char **argv)
{
    printf("%s -h        print this help\n", argv[0]);
    printf("%s -f        full field find star\n", argv[0]);
    printf("%s -g        acquire guide star and guide\n", argv[0]);
    printf("%s -cal      calibrate mount\n", argv[0]);
    printf("%s -t        test guide logic\n", argv[0]);
    printf("exta args\n");
    printf("-gain=value\n");
    printf("-exp=value (in sec)\n");
    printf("-mult=value\n");
    printf("complex example\n");
    printf("./guider -exp=0.5 -gain=300 -mult=4 -cal\n");
}

//--------------------------------------------------------------------------------------


void intHandler(int signum)
{
    talk->Set("guide", 0);
    tt->MoveTo(0, 0);
    tt_sighandler(signum);
    closeCamera();
    printf("emergency close\n");
    exit(0);
}

//--------------------------------------------------------------------------------------


int main(int argc, char **argv)
{
    signal(SIGINT, intHandler);

    talk = new Talk();

    //for (int i = 0; i < 100000; i++) { 	   
    printf("not found %d\n", talk->Get("xx"));
    //}

    talk->Set("guide", 1);

    tt = new tiptilt();
 
    int i = 0;

    if (argc == 1 || strcmp(argv[1], "-h") == 0) {
        help(argv);
        return 0;
    }

    int pos = 1;

    g_exp = get_value("exp");
    g_gain = get_value("gain");
    g_mult = get_value("mult");

    while(pos < argc) {
        if (match(argv[pos], "-gain=")) {
            sscanf(strchr(argv[pos], '=') , "=%f",  &g_gain);
            argv[pos][0] = 0;
        }
        if (match(argv[pos], "-exp="))  {
            sscanf(strchr(argv[pos], '=') , "=%f",  &g_exp);
            argv[pos][0] = 0;
        }
        if (match(argv[pos], "-mult=")) {
            sscanf(strchr(argv[pos], '=') , "=%f",  &g_mult);
            argv[pos][0] = 0;
        }
        pos++;
    }
    pos = 1;
    set_value("exp", g_exp);
    set_value("gain", g_gain);
    set_value("mult", g_mult);

    while(pos < argc) {
        if (match(argv[pos], "-t")) test_guide();
        if (match(argv[pos], "-f")) find_guide();
        if (match(argv[pos], "-guide")) { guide(); tt->MoveTo(0, 0); }

        if (match(argv[pos], "-cal")) {calibrate(); tt->MoveTo(0, 0); }
        if (match(argv[pos], "-zcal")) {ao_calibrate(1);tt->MoveTo(0, 0); }

        pos++;
    }

    set_value("exp", g_exp);
    set_value("gain", g_gain);
    set_value("mult", g_mult);
    delete tt;
    talk->Set("guide", 0);

    delete talk;
}
