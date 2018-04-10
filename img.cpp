#include "qsiapi.h"
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#include <iostream>
#include <cmath>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include <opencv2/imgproc/imgproc.hpp>
#include <time.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;

QSICamera cam;
float g_exp = 0.001;
int   g_filter = 1;
int   g_bin = 2;
int   g_count = 38;
char g_fn[256]="out";
volatile int  killp = 0;

#include "talk.cpp"


Talk	*talk;


//-----------------------------------------------------------------------
#define ushort unsigned short
//-----------------------------------------------------------------------

#include <sys/time.h>

double nanotime()
{
   timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);

   return (ts.tv_sec + ts.tv_nsec * 1e-9);
}



class Cam {
   
public:;
 
    Cam();
    ~Cam();
    void	Update(bool force);
    int		Take();
    int		Find();
    int		Focus();
    int	  	Corner();
    int         FocusOptimizer(bool sub);
    int		Dark(); 
    int         Flat();
    void	AutoLevel();
    ushort 	Pixel(int x, int y);
    void	Save();
    void 	WriteLine(FILE *file, int y);
    float	Temp();
    int         FocusJob(int move0, int step);
    int         FocusJob1(int move0, int step);

    float 	hfd();

public:
    Mat	cv_image;

    short binX;
    short binY;
    long xsize;
    long ysize;
    long startX;
    long startY;
    float min_v;
    float range_v;
    float avg;
};


//-----------------------------------------------------------------------

#include "fits_header.h"

//-----------------------------------------------------------------------

void Cam::WriteLine(FILE *file, int y)
{
    ushort  tmp[16384];      //large enough for my camera
    
    int     x;
    
    for (int x = 0; x < xsize; x++) {
        ushort  v;
        
        v = Pixel(x, y);
        //v = v + 32768;
        v >>= 1; 
        v = (v>>8) | ((v&0xff) << 8);
        
        tmp[x] = v;
    }
    fwrite(tmp, xsize, 2, file);
}

//-----------------------------------------------------------------------

void IntTo4(int v, char *p)
{
	*p++ = 0x30 + (v/10000);
	v = v % 10000;

       *p++ = 0x30 + (v/1000);
        v = v % 1000;
       
	*p++ = 0x30 + (v/100);
        v = v % 100;
       
	*p++ = 0x30 + (v/10);
        v = v % 10;

       *p = 0x30 + v;
}

//-----------------------------------------------------------------------


void Cam::Save()
{
    time_t result = time(NULL); 
    char   buf[512];

    printf("save0\n");
    sprintf(buf, "%s_%ld.fit", g_fn, result);
    FILE *file = fopen(buf, "wb");
    printf("save %s\n", buf);
    char  header_buf[0xb40];

    int i;



    for (i = 0; i < 0xb40; i++) header_buf[i] = ' ';

    i = 0;

    do {
        const char*   header_line;

        header_line = header[i];

        if (strlen(header_line) > 0) {
            memcpy(&header_buf[i*80], header_line, strlen(header_line));
        }
        else
        break;
	if (i == 3) {
		char *tmp = &header_buf[i*80 + 25];
		IntTo4(xsize, tmp);
	}
        if (i == 4) {
                char *tmp = &header_buf[i*80 + 25];
                IntTo4(ysize, tmp);
        }
 
	i++;
    } while(i < 40);

    fwrite(header_buf, 0xb40, 1, file);

    int     y;

    for (y = 0; y < ysize; y++) {
        WriteLine(file, y);
    }

    fclose(file);
}

//-----------------------------------------------------------------------


float Cam::Temp()
{
	double temp;
	int result;
	
	result = cam.get_CCDTemperature(&temp);
	printf("%d\n", result);	
	printf("temp = %f\n", (float)temp);
	return temp;
}

//-----------------------------------------------------------------------

Cam::Cam()
{
    int x,y,z;
    bool canSetTemp;
    bool hasFilters;
    int iNumFound;
  

    min_v = cvGetTrackbarPos("min", "img");
    range_v = cvGetTrackbarPos("range", "img");

 
    std::string serial("");
    std::string desc("");
    std::string info = "";
    std::string modelNumber("");


 
    cam.get_DriverInfo(info);
    std::string camSerial[QSICamera::MAXCAMERAS];
    std::string camDesc[QSICamera::MAXCAMERAS];
    cam.get_AvailableCameras(camSerial, camDesc, iNumFound);
    
    if (iNumFound < 1) {
        std::cout << "No cameras found\n";
        exit(-1);
    }
    
    for (int i = 0; i < iNumFound; i++) {
        std::cout << camSerial[i] << ":" << camDesc[i] << "\n";
    }
    
    cam.put_SelectCamera(camSerial[0]);
    
    cam.put_IsMainCamera(true);
    cam.put_Connected(true);
    cam.get_ModelNumber(modelNumber);
    std::cout << modelNumber << "\n";
    cam.get_Description(desc);
    cam.put_SoundEnabled(true);
    cam.put_FanMode(QSICamera::fanQuiet);
    
    // Query if the camera can control the CCD temp
    cam.get_CanSetCCDTemperature(&canSetTemp);
    if (canSetTemp) {
        // Set the CCD temp setpoint to 10.0C
        cam.put_SetCCDTemperature(-21);
        // Enable the cooler
        cam.put_CoolerOn(true);
    }
    cam.put_CameraGain(QSICamera::CameraGainHigh); 
    if (modelNumber.substr(0,1) == "6") {
        cam.put_ReadoutSpeed(QSICamera::FastReadout); //HighImageQuality
    }
    
    cam.get_HasFilterWheel(&hasFilters);
    if ( hasFilters) {
        // Set the filter wheel to position 1 (0 based position)
        cam.put_Position(g_filter);
    } 
    //cam.put_ShutterPriority(QSICamera::ShutterPriorityElectronic);
 
    Temp();
 
    cam.put_BinX(g_bin);
    cam.put_BinY(g_bin);
    cam.get_CameraXSize(&xsize);
    cam.get_CameraYSize(&ysize);
    xsize /= g_bin;
    ysize /= g_bin;	
    printf("%ld %ld\n", xsize, ysize);
    
    // Set the exposure to a full frame
    cam.put_StartX(0);
    cam.put_StartY(0);
    cam.put_NumX(xsize);
    cam.put_NumY(ysize);
    
    cv_image = Mat(Size(xsize, ysize), CV_16UC1);
}

//-----------------------------------------------------------------------


Cam::~Cam()
{
}

//-----------------------------------------------------------------------

ushort Cam::Pixel(int x, int y)
{
	return cv_image.at<unsigned short>(y, x);
}

//-----------------------------------------------------------------------



void Cam::Update(bool force)
{
    
    long sum = *cv_image.ptr<unsigned short>(0);
    sum -= 1000;
    float min = cvGetTrackbarPos("min", "img");
    if (min == 0)
        min = sum;
    float range = cvGetTrackbarPos("range", "img");
   
    if (range != range_v || min != min_v || force) { 
    	float mult = (32768.0/range);
	
	cv::imshow("img", mult*(cv_image - min));
   	min_v = min;
	range_v = range; 
    } 

    if (force) {
	//for (int y = 0; y < ysize; y += (ysize/8)) {
		//for (int x = 0; x < xsize; x+= (xsize/8)) {
			//printf("%d ", Pixel(x, y));	
		//}
		//printf("\n");
	//} 
    }
}


//-----------------------------------------------------------------------

void Cam::AutoLevel()
{
	float sum = 0;
	float dev = 0;
	float cnt = 0;

        for (int y = 0; y < ysize; y += 50) {
                for (int x = 0; x < xsize; x+= 50) {
                        sum = sum + Pixel(x, y);
			cnt = cnt + 1.0;	
		}
	}
	sum /= cnt;
	if (sum < 0) sum = 0;
        avg = sum;
        
	cnt = 0.0;

        for (int y = 0; y < ysize; y += 50) {
                for (int x = 0; x < xsize; x+= 50) {
                        dev =  dev + (sum - Pixel(x, y)) * (sum - Pixel(x,y));
                        cnt = cnt + 1.0;
                }
        }
	dev = dev / cnt;
	dev = sqrt(dev);
	if (dev < 10) dev = 10;
	if (dev > 5000) dev = 5000;

	sum -= dev;
 
   	setTrackbarPos("min", "img", sum);
    	setTrackbarPos("range", "img", dev * 4.0);
	Update(true);
}


//-----------------------------------------------------------------------

void center(Mat img)
{
        int     cx, cy;

        cx = img.cols;
        cy = img.rows;
        cx /= 2.0;
        cy /= 2.0;
        rectangle(img, Point(cx-10, cy-10), Point(cx+10, cy+10), Scalar(32000, 32000, 32000), 1, 8);
        rectangle(img, Point(cx-13, cy-13), Point(cx+13, cy+13), Scalar(9000, 9000, 9000), 1, 8);
        rectangle(img, Point(cx-12, cy-12), Point(cx+12, cy+12), Scalar(9000, 9000, 9000), 1, 8);
        rectangle(img, Point(cx-11, cy-11), Point(cx+11, cy+11), Scalar(9000, 9000, 9000), 1, 8);
}



int Cam::Find()
{
    int x,y,z;
    
   
    while(1) {	
        bool imageReady = false;
	cam.StartExposure(g_exp, true);
	cam.get_ImageReady(&imageReady);
	
	while(!imageReady) {
            Update(false); 
	    char c = cvWaitKey(1);
            
            if (killp || c == 27) { 
                goto exit;
            }
            if (c == 'a' || c == 'A') {
                AutoLevel();
            }
            
            usleep(100);
            cam.get_ImageReady(&imageReady);
        }
	
        // Get the image dimensions to allocate an image array
        cam.get_ImageArraySize(x, y, z);
 
        cam.get_ImageArray(cv_image.ptr<unsigned short>(0));	
       	center(cv_image); 
	Update(true);
	char c = cvWaitKey(1);	
        
        if (killp || c == 27) {
            goto exit;	
        }
        if (c == 'a' || c == 'A') {
        	AutoLevel();
        }

	//Save();

    }
    cam.put_Connected(false);
    return 0;
    
exit:;
    cam.put_Connected(false);
    killp = 0; 
    return 0;
}

//-----------------------------------------------------------------------


int Cam::Corner()
{
    int x,y,z;
    
    cam.put_BinX(1);
    cam.put_BinY(1);

    cam.get_CameraXSize(&xsize);
    cam.get_CameraYSize(&ysize);
    printf("Focus\n"); 
    // Set the exposure to a full frame
    xsize /= 2;
    ysize /= 2;
    
    cam.put_StartX(xsize/3);
    cam.put_StartY(ysize);
    cam.put_NumX(xsize);
    cam.put_NumY(ysize);
    
    cv_image = Mat(Size(xsize, ysize), CV_16UC1);
    while(1) {	
        bool imageReady = false;
	cam.StartExposure(g_exp, true);
	cam.get_ImageReady(&imageReady);
	
	while(!imageReady) {
            Update(false); 
	    char c = cvWaitKey(1);
            
            if (killp || c == 27) { 
                goto exit;
            }
            if (c == 'a' || c == 'A') {
                AutoLevel();
            }
            if (c == '+') {
		//scope->XCommand("xfocus8\n");	
	   	printf("move\n"); 
	    }
	    if (c == '-') {
                //scope->XCommand("xfocus-8\n"); 
           	printf("move in\n"); 
	    } 
            usleep(100);
            cam.get_ImageReady(&imageReady);
        }
	
        // Get the image dimensions to allocate an image array
        cam.get_ImageArraySize(x, y, z);
 
        cam.get_ImageArray(cv_image.ptr<unsigned short>(0));
        
        double minVal;
        double maxVal;
        Point  minLoc;
        Point  maxLoc;
        
        minMaxLoc(cv_image, &minVal, &maxVal, &minLoc, &maxLoc);
        printf("max %f, %f\n", maxVal, hfd());
	Update(true);
	char c = cvWaitKey(1);	
        
        if (killp || c == 27) {
            goto exit;	
        }
        if (c == 'a' || c == 'A') {
        	AutoLevel();
        }
            if (c == '+') {
                //scope->XCommand("xfocus8\n");
                printf("move\n");
            }
            if (c == '-') {
                //scope->XCommand("xfocus-8\n");
                printf("move in\n");
            }



    }
    cam.put_Connected(false);
    std::cout << "Camera disconnected.\nTest complete.\n";
    std::cout.flush();
    return 0;
    
exit:;
    cam.put_Connected(false);
    killp = 0; 
    return 0;
}

//-----------------------------------------------------------------------

int Cam::Focus()
{
    int x,y,z;
    
    cam.put_BinX(1);
    cam.put_BinY(1);

    cam.get_CameraXSize(&xsize);
    cam.get_CameraYSize(&ysize);
    printf("Focus\n"); 
    // Set the exposure to a full frame
    


    xsize /= 3;
    ysize /= 3;
    
    cam.put_StartX(xsize);
    cam.put_StartY(ysize);
    cam.put_NumX(xsize);
    cam.put_NumY(ysize);
    
    cv_image = Mat(Size(xsize, ysize), CV_16UC1);
    while(1) {	
        bool imageReady = false;
	cam.StartExposure(g_exp, true);
	cam.get_ImageReady(&imageReady);
	
	while(!imageReady) {
            Update(false); 
	    char c = cvWaitKey(1);
            
            if (killp || c == 27) { 
                goto exit;
            }
            if (c == 'a' || c == 'A') {
                AutoLevel();
            }
            if (c == '+') {
		//scope->XCommand("xfocus8\n");	
	   	printf("move\n"); 
	    }
	    if (c == '-') {
               //scope->XCommand("xfocus-8\n"); 
           	printf("move in\n"); 
	    } 
            usleep(100);
            cam.get_ImageReady(&imageReady);
        }
	
        // Get the image dimensions to allocate an image array
        cam.get_ImageArraySize(x, y, z);
 
        cam.get_ImageArray(cv_image.ptr<unsigned short>(0));
        
        double minVal;
        double maxVal;
        Point  minLoc;
        Point  maxLoc;
        
        minMaxLoc(cv_image, &minVal, &maxVal, &minLoc, &maxLoc);
        printf("max %f, %f\n", maxVal, hfd());
	Update(true);
	char c = cvWaitKey(1);	
        
        if (killp || c == 27) {
            goto exit;	
        }
        if (c == 'a' || c == 'A') {
        	AutoLevel();
        }
            if (c == '+') {
                //scope->XCommand("xfocus8\n");
                printf("move\n");
            }
            if (c == '-') {
               //scope->XCommand("xfocus-8\n");
                printf("move in\n");
            }



    }
    cam.put_Connected(false);
    std::cout << "Camera disconnected.\nTest complete.\n";
    std::cout.flush();
    return 0;
    
exit:;
    cam.put_Connected(false);
    killp = 0; 
    return 0;
}



//-----------------------------------------------------------------------
       
typedef struct {
       float   value;
       float   distance;
} hdf_entry;

//-----------------------------------------------------------------------


int sort_hdf(const void * a, const void * b)
{
	float d_a = ((hdf_entry*)a)->distance;
	float d_b = ((hdf_entry*)b)->distance;

	if (d_a == d_b) return 0;
	if (d_a < d_b) return -1;
	return 1;

}

//-----------------------------------------------------------------------

float Cam::hfd()
{
	Mat	tmp;
	float	down_scale = 0.1;
	int	x, y;

	typedef struct {
		float	value;
		float   distance;
	} hdf_entry;


	resize(cv_image, tmp, Size(0, 0), down_scale, down_scale, INTER_LINEAR);
	//printf("convert and resize done\n");	

	double maxval;
	double minval;
        
	Point  minLoc;
        Point  maxLoc;

	minMaxLoc(tmp, &minval, &maxval, &minLoc, &maxLoc);


	maxLoc.x *= 10.0;
	maxLoc.y *= 10.0;

	//printf("real pos %f %f\n", (float)maxLoc.x, (float)maxLoc.y); 
	int box = 20;

	if (maxLoc.x <= box)
		return 32;
	if (maxLoc.y <= box)
		return 32;

	if (maxLoc.x >= cv_image.cols - box)
		return 32;

        if (maxLoc.y >= cv_image.rows - box)
                return 32;


	//printf("inside box\n");
	float bias = 0;

	for (int y = 5; y < 10; y++) {
		for (int x = 5; x < 10; x++) {	
			bias += cv_image.at<unsigned short>(10 + maxLoc.y + y, 10 + maxLoc.x + x);
		}
	}
	
	bias /= 25.0;
	bias += 15.0;

	//printf("bias cut %f\n", bias);

	float total = 0;	
	float tx = 0;
	float ty = 0;	
	
	for (y = maxLoc.y - box; y <= maxLoc.y + box; y++) { 
		for (x = maxLoc.x - box; x  <= maxLoc.x + box; x++) {
			float v = cv_image.at<unsigned short>(y, x);
			v = v - bias;
			if (v < 0) v = 0;
			total = total + v;	
			tx = tx + v * x;
			ty = ty + v * y;	
		}
	}	
 	//printf("total %f\n", total);	
        if (total < 500) {
		return 128;
	}

	tx /= total;
	ty /= total;
	//printf("real center %f %f\n", tx, ty);
	
	int count =0;

        for (y = ty - box; y <= ty + box; y++) {
                for (x = tx - box; x  <= tx + box; x++) {
                        float v = cv_image.at<unsigned short>(y, x); 
                        v = v - bias;
                        if (v < 0) v = 0;
			if (v > 0) 
				count++;
                }
        }

	//printf("count entry %d\n", count);

	hdf_entry *list;
	
	list = (hdf_entry*)malloc(count * sizeof(hdf_entry));


        count = 0;
	total = 0;

        for (y = ty - box; y <= ty + box; y++) {
                for (x = tx - box; x  <= tx + box; x++) {
                        float v = cv_image.at<unsigned short>(y, x);
                        v = v - bias;
                        if (v < 0) v = 0;
                        if (v > 0) {
				float dx = x - tx;
				float dy = y - ty;
				float dist = (dx*dx)+(dy*dy);
				dist = sqrt(dist);
                               	list[count].distance = dist;
				list[count].value = v;
				total += v; 
				count++;
			}
                }
        }
	qsort(list, count-1, sizeof(hdf_entry), sort_hdf);

	float half_total = 0;

	for (int i = 0; i < count; i++) {
		if (half_total > (total/2.0)) {
			//printf("found it %f\n", list[i].distance);
			return list[i].distance;
		}
		half_total += list[i].value; 	
	}
	return 32;
}


//-----------------------------------------------------------------------



int Cam::FocusJob1(int move0, int step)
{
    int 	direction = step;
    int 	total_move = 0;
    double	maxVal; 
    double	minVal; 
    char 	buf[256];
    int 	x, y, z;
    int 	mdelta;
    int		total = 0;
    float	exp = 2.8;
    float	max1 = 0;
    char	first_move = true;
 
    float min_size = 1e8;
 
    for (int iter = 0; iter < 15; iter++) {
        int move;

        do {
                move = rand() % 41;
                move = move - 20;
        } while(move < 4 && move > -4);

	if (first_move)
		move = 0;

	first_move = false;

//       FocusMove(move);

        sleep(1);


	bool imageReady = false;
	cam.StartExposure(g_exp, true);
	cam.get_ImageReady(&imageReady);
	
	while(!imageReady) {
            Update(false); 
            usleep(500);
            cam.get_ImageReady(&imageReady);
        }
        cam.get_ImageArraySize(x, y, z);
        cam.get_ImageArray(cv_image.ptr<unsigned short>(0));
        
        Update(false);
        AutoLevel();

         
	float hf_size0 = hfd();
        
	Point  minLoc;
        Point  maxLoc;

	Mat tmp;
	
	GaussianBlur(cv_image, tmp, Size( 3, 3), 0, 0 );
        minMaxLoc(tmp, &minVal, &maxVal, &minLoc, &maxLoc);

	float max0 = maxVal;
       
	printf("move by %d, new_max = %f, old_max = %f\n", move, max0, max1); 
	if (max0 < max1) { 	//reduce maximum brightness. just go back
            //FocusMove(-move);
            sleep(1);
        }
       	else {
	    total = total + move;	
	    max1 = max0;	
	}
 
	char c = cvWaitKey(1);	
        
        if (killp || c == 27) {
            goto eexit0;	
        }
    }
   
eexit0:;
   
eexit:; 
}

//-----------------------------------------------------------------------

int Cam::FocusJob(int move0, int step)
{
    int 	direction = step;
    int 	total_move = 0;
    float 	max0 = 0;
    double	maxVal; 
    double	minVal; 
    char 	buf[256];
    int 	x, y, z;
    int 	mdelta;

 
    //FocusMove(-move0/2);
 
    printf("sleep0\n"); 
    sleep(4); 
    printf("sleep1\n"); 
    int best = 0; 
    float min_size = 1e8;
 
    while(total_move < move0) {
        bool imageReady = false;
	//cam.StartExposure(0.2, true);
	cam.StartExposure(g_exp, true);	
	cam.get_ImageReady(&imageReady);
	
	while(!imageReady) {
            Update(false); 
            usleep(500);
            cam.get_ImageReady(&imageReady);
        }
	
        // Get the image dimensions to allocate an image array
        cam.get_ImageArraySize(x, y, z);
 
        cam.get_ImageArray(cv_image.ptr<unsigned short>(0));
        
        Point  minLoc;
        Point  maxLoc;
        
        minMaxLoc(cv_image, &minVal, &maxVal, &minLoc, &maxLoc);
        
	float hf_size = hfd();

	if (maxVal > max0) {
		best = total_move;
		max0 = maxVal;
	}
 
        total_move += direction;
       
	//FocusMove(direction);
        printf("old_max %f, new_max %f. direction = %d. totalmove = %d\n", max0, maxVal, direction, total_move);
     
	//wait 3 second for focus move to complete 
        sleep(3); 
        Update(false);
        AutoLevel();
	char c = cvWaitKey(1);	
        
        if (killp || c == 27) {
            goto eexit0;	
        }
    }
   
eexit0:;

    if (max0 < 900.0) { //no star found really. go back to original point
	best = 0;
   	mdelta = -total_move/2; 
    }
    else { 
    	mdelta = best - total_move;
    } 
    //FocusMove(mdelta);
    sleep(3); 
    printf("final move back by %d\n", mdelta); 
   
eexit:; 
}



int Cam::FocusOptimizer(bool sub)
{
    cam.put_BinX(1);
    cam.put_BinY(1);

    cam.get_CameraXSize(&xsize);
    cam.get_CameraYSize(&ysize);
    printf("Focus\n"); 
    xsize /= 1;
    ysize /= 1;
    
    cam.put_StartX(xsize*0.0);
    cam.put_StartY(ysize*0.0);
    
 
    cam.put_NumX(xsize);
    cam.put_NumY(ysize);
    
    int iter = 235;
    
    cv_image = Mat(Size(xsize, ysize), CV_16UC1);
  
    printf("job\n"); 
    FocusJob1(88, 8);
    if (!sub) {
        cam.put_Connected(false);
    	std::cout << "Camera disconnected.\nTest complete.\n";
    	std::cout.flush();
    } 
    return 0;
    
eexit:;
    if (!sub) cam.put_Connected(false);
    killp = 0; 
    return 0;
}

//-----------------------------------------------------------------------

int Cam::Take()
{
    int x,y,z;
    int fc = 0; 
    long xsize;
    long ysize; 
    bool imageReady = false;
  
    fc = 0;
    int cnt = 0;
 
    while(fc!=g_count) { 
    printf("%d out of %d\n", fc, g_count); 
    fc++; 
    double start = nanotime();
    cam.put_ReadoutSpeed(QSICamera::HighImageQuality); //HighImageQuality

    cam.StartExposure(g_exp, true);
    cam.get_ImageReady(&imageReady);
	
    while(!imageReady) {
        char c = cvWaitKey(1);
        if (killp || c == 27) { 
            goto exit;
        }
       	cnt++;
	if (cnt % 60 == 0) printf("%f\n", (nanotime() - start)); 

        usleep(500);
        cam.get_ImageReady(&imageReady);
    }
   
    talk->Set("resetguide", 1);
    talk->Set("driz", 1);

    cam.get_ImageArraySize(x, y, z);
    cam.get_ImageArray(cv_image.ptr<unsigned short>(0));	
    Save(); 
    usleep(5000*1000);
    } 
exit:; 
    cam.put_Connected(false);
    killp = 0; 
    return 0;
}

//-----------------------------------------------------------------------

int Cam::Dark()
{
    int x,y,z;


    bool imageReady = false;
    
    cam.put_UseStructuredExceptions(false);
    cam.put_ReadoutSpeed(QSICamera::HighImageQuality); //HighImageQuality

    double start = nanotime();
    int err = cam.StartExposure(g_exp, false);
    printf("err %d\n", err);
 
    cam.get_ImageReady(&imageReady);
       
    int cnt = 0;
 
    while(!imageReady) {
        char c = cvWaitKey(1);
        if (killp || c == 27) {
            printf("exit\n"); 
	    goto exit;
        }
       	cnt++;
	if (cnt % 30 == 0) printf("%f\n", (nanotime() - start)); 
	usleep(1000);
        cam.get_ImageReady(&imageReady);
    }
    cam.get_ImageArraySize(x, y, z);
    cam.get_ImageArray(cv_image.ptr<unsigned short>(0));
    Save(); 
 exit:; 
    cam.put_Connected(false);
    killp = 0; 
    return 0;
}

//-----------------------------------------------------------------------

int Cam::Flat()
{
    int x,y,z;
    int frame_taken = 0;

    bool imageReady = false;
    
    cam.put_UseStructuredExceptions(false);
    cam.put_ReadoutSpeed(QSICamera::HighImageQuality); //HighImageQuality

    while(frame_taken < 15) {
    int err = cam.StartExposure(g_exp, true);
 
    cam.get_ImageReady(&imageReady);
       
    int cnt = 0; 
    while(!imageReady) {
        
	char c = cvWaitKey(1);
        if (killp || c == 27) {
            goto exit;
        }
        usleep(10000);
        cam.get_ImageReady(&imageReady);
	cnt++;
    }
    Update(true); 
    char c = cvWaitKey(1);
    if (killp || c == 27) {
    	goto exit;
    }
  
    cam.get_ImageArraySize(x, y, z);
    cam.get_ImageArray(cv_image.ptr<unsigned short>(0));
    AutoLevel();
    printf("level %f\n", avg); 
   if (avg > 3000 && avg < 18000) {
        frame_taken++; 
	Save();
    }
    
 }  
exit:; 
    printf("forced exit\n"); 
    usleep(1000000); 
    cam.put_Connected(false);
    killp = 0; 
    return 0;
}

//-----------------------------------------------------------------------

void help(char **argv)
{
                printf("%s -h        print this help\n", argv[0]);
                printf("%s -take   take one frame\n", argv[0]);
                printf("%s -dark   take one dark frame\n", argv[0]);
                printf("%s -focus   focus loop\n", argv[0]);
                printf("%s -flat (flat loop)\n", argv[0]);
 
		printf("%s -find   continous find mode\n", argv[0]); 
		printf("exta args\n");
                printf("-exp=value (in sec)\n");
		printf("-filter=value (0-5)\n");
		printf("-bin=value (1-9)\n");
		printf("-o=filename\n");	
		printf("sudo ./img -exp=0.001 -filter=3 -find -bin=4\n");
		printf("sudo ./img -exp=0.0 -filter=0 -take -bin=2 -o=data\n");
}


//-----------------------------------------------------------------------


bool match(char *s1, const char *s2)
{
        return(strncmp(s1, s2, strlen(s2)) == 0);
}

//-----------------------------------------------------------------------

void intHandler(int dummy=0) {
       	killp = 1;
	printf("sig kill\n");
        return;
}

//-----------------------------------------------------------------------


int main(int argc, char** argv)
{
    talk = new Talk();


    signal(SIGINT, intHandler);

    if (argc == 1 || strcmp(argv[1], "-h") == 0) {
            help(argv);
            return 0;
     }

     int pos = 1;

     g_count = 20;

     while(pos < argc) {
     	if (match(argv[pos], "-exp=")) {sscanf(strchr(argv[pos], '=') , "=%f",  &g_exp); argv[pos][0] = 0;}
        if (match(argv[pos], "-filter=")) {sscanf(strchr(argv[pos], '=') , "=%d",  &g_filter); argv[pos][0] = 0;}
	if (match(argv[pos], "-bin=")) {sscanf(strchr(argv[pos], '=') , "=%d",  &g_bin); argv[pos][0] = 0;}
	if (match(argv[pos], "-count=")) {sscanf(strchr(argv[pos], '=') , "=%d",  &g_count); argv[pos][0] = 0;}	
	if (match(argv[pos], "-o="))  {sscanf(strchr(argv[pos], '=') , "=%s",  (char*)g_fn); argv[pos][0] = 0;}	
	pos++;
     } 
 
    printf("exp    = %f\n", g_exp);
    printf("bin    = %d\n", g_bin);
    printf("filter = %d\n", g_filter);
    printf("file   = %s\n", g_fn);
 
    namedWindow("img", 1);
    
    createTrackbar("min", "img", 0, 64000, 0);
    createTrackbar("range", "img", 0, 32000, 0);
    
    setTrackbarPos("min", "img", 2000); 
    setTrackbarPos("range", "img", 32000);
    
    char c = cvWaitKey(1);
   
    Cam *a_cam;

    a_cam = new Cam(); 
   
    pos = 1;

    while(pos < argc) { 
    	if (match(argv[pos], "-find")) a_cam->Find();
   	if (match(argv[pos], "-focus")) a_cam->Focus();
	if (match(argv[pos], "-corner")) a_cam->Corner();	
	if (match(argv[pos], "-take")) a_cam->Take(); 
  	if (match(argv[pos], "-dark")) a_cam->Dark();	
	if (match(argv[pos], "-flat")) a_cam->Flat();	
	if (match(argv[pos], "-xfocus")) a_cam->FocusOptimizer(false);	
	pos++;
   }
}
