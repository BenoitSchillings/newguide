#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include <libusb-1.0/libusb.h>


using namespace std;

Talk	*tt_talk = 0;

//--------------------------------------------------------------------

#define USB_VENDOR_ID         0x0104d
#define USB_PRODUCT_ID        0x4000                       /* USB product ID used by the device */
#define USB_ENDPOINT_IN       (LIBUSB_ENDPOINT_IN  | 1)    /* endpoint address */
#define USB_ENDPOINT_OUT      (LIBUSB_ENDPOINT_OUT | 2)    /* endpoint address */
#define USB_TIMEOUT            3000                        /* Connection timeout (in ms) */

//--------------------------------------------------------------------

libusb_context *ctx = NULL;
libusb_device_handle *handle;
uint8_t receiveBuf[64];
uint8_t transferBuf[64];


//--------------------------------------------------------------------


int usb_write(const char *txt)
{
    int n, ret;
    
    if (handle == 0) return 0;

    n = sprintf((char*)transferBuf, "%s\r", txt);
    ret = libusb_bulk_transfer(handle, USB_ENDPOINT_OUT, transferBuf, n, &n, USB_TIMEOUT);
    
    if (ret != 0) {
        printf("Error in USB write %d\n", ret);
        return -1;
    }
    return ret;
}

//--------------------------------------------------------------------

int usb_read(void)
{
    int nread, ret;

    if (handle == 0) return 0;
    
    ret = libusb_bulk_transfer(handle, USB_ENDPOINT_IN, receiveBuf, sizeof(receiveBuf), &nread, USB_TIMEOUT);
    if (ret) {
        printf("ERROR in bulk read: %d\n", ret);
        return -1;
    } else {
         return 0;
    }
}

//--------------------------------------------------------------------

int wait_complete(int motor)
{
    int nread, ret;
    
    do {
        char buf[256];
        
        sprintf(buf, "%dMD?", motor);
        usb_write(buf);
        
        ret = libusb_bulk_transfer(handle, USB_ENDPOINT_IN, receiveBuf, sizeof(receiveBuf), &nread, USB_TIMEOUT);
        if (ret) {
            printf("ERROR in bulk read: %d\n", ret);
            return -1;
        }
        //        printf("[%d %c %c]\n", nread, receiveBuf[0], receiveBuf[1]);
        if (receiveBuf[0] == '1') return 0;
        usleep(500);
    } while(1);
}

//--------------------------------------------------------------------

int init_usb_connection()
{
    libusb_init(&ctx);
    libusb_set_debug(ctx, 3);
    
    //Open Device with VendorID and ProductID
    handle = libusb_open_device_with_vid_pid(ctx, USB_VENDOR_ID, USB_PRODUCT_ID);
    
    if (!handle) {
        perror("device not found");
        return -1;
        //exit(-1);
    }
    
    libusb_reset_device(handle);
    
    int r = 1;
    
    r = libusb_claim_interface(handle, 0);
    if (r < 0) {
        fprintf(stderr, "usb_claim_interface error %d\n", r);
        return 2;
    }
    return 0;
}

//--------------------------------------------------------------------

void move(int motor, int delta)
{
    char    buf[256];
    
    //printf("%d %d\n", motor, delta);
    sprintf(buf, "%dPR%d", motor, delta);
    usb_write(buf);
}

//--------------------------------------------------------------------


void tt_sighandler(int signum)
{
    if (handle) {
        libusb_release_interface(handle, 0);
        libusb_close(handle);
     }
    libusb_exit(NULL);
    exit(0);
}

//--------------------------------------------------------------------

// x = (2/3)*a
// y = (-1/3) * a + tan(30)*b;
// z = (-1/3) * a - tan(30)*b;


//--------------------------------------------------------------------


class	tiptilt {
public:;

		tiptilt();
		~tiptilt();

	void	Move(float d_alpha, float d_beta);
	void	MoveTo(float alpha, float beta);	
	void	reset_pos();

	void	MoveFocus(float delta);
	void	MoveToFocus(float value);	
   	void    setxyz(float x, float y, float z);

private:
	int     InitUSB();
	void	CloseUSB();

	int	    WriteCommand(string cmd);
	string	ReadResult();
    

	int	    map_distance_step_x(float distance);
	int	    map_distance_step_y(float distance);
	int	    map_distance_step_z(float distance);

    float    map_step_distance_x(int steps);
    float    map_step_distance_y(int steps);
    float    map_step_distance_z(int steps);

	void    upd_tilt();
    
    	 
    float   c_x;
    float   c_y;
    float   c_z;
    
    float   backward_step_x;
    float   backward_step_y;
    float   backward_step_z;
    
    float   forward_step_x;
    float   forward_step_y;
    float   forward_step_z;

    float   c_a;
    float   c_b;
    float   d_focus;
};


//--------------------------------------------------------------------

void tiptilt::reset_pos()
{
	return;
	printf("reset tt\n");
	move(1, 15000);
    	wait_complete(1);
	move(1, -7000);
    	wait_complete(1);

	move(2, 15000);
    	wait_complete(2);
	move(2, -7000);
    	wait_complete(2);

	move(3, 15000);
    	wait_complete(3);
	move(3, -7000);
    	wait_complete(3);

	c_x = 0;
	c_y = 0;
	c_z = 0;

    	tt_talk->Set("c_x", c_x);
    	tt_talk->Set("c_y", c_y);
    	tt_talk->Set("c_z", c_z);

	MoveToFocus(tt_talk->Get("d_focus"));
}

//--------------------------------------------------------------------


   tiptilt::tiptilt()
{
    c_x = 0;
    c_y = 0;
    c_z = 0;
    d_focus = 0;
    c_a = 0;
    c_b = 0;
    
    tt_talk = new Talk();
/*
    c_x = tt_talk->Get("c_x");
    c_y = tt_talk->Get("c_y");
    c_z = tt_talk->Get("c_z");
*/
    backward_step_x = 20.10;
    backward_step_y = 19.94*1.11;
    backward_step_z = 22.44/1.11;

    forward_step_x = 23.28;
    forward_step_y = 23.32*1.11;
    forward_step_z = 26.02/1.11;


    InitUSB();
    if (handle == 0) return;

    usb_write("1VA1500");
    usb_write("1AC22000");
    usb_write("2VA1500");
    usb_write("2AC22000");
    usb_write("3VA1500");
    usb_write("3AC22000");

}


//--------------------------------------------------------------------

	tiptilt::~tiptilt()
{
    	if (handle == 0) return;
   
	CloseUSB();
}

//--------------------------------------------------------------------

int tiptilt::map_distance_step_x(float d)
{
    int steps;
    
    if (d < 0)
        steps = round(d / backward_step_x);
    else
        steps = round(d / forward_step_x);
    
    return steps;
}

//--------------------------------------------------------------------

int tiptilt::map_distance_step_y(float d)
{
    int steps;
    
    if (d < 0)
        steps = round(d / backward_step_y);
    else
        steps = round(d / forward_step_y);
    
    return steps;
}

//--------------------------------------------------------------------

int tiptilt::map_distance_step_z(float d)
{
    int steps;
    
    if (d < 0)
        steps = round(d / backward_step_z);
    else
        steps = round(d / forward_step_z);
    
    return steps;
}

//--------------------------------------------------------------------

float tiptilt::map_step_distance_x(int steps)
{
    float d;
    
    if (steps < 0)
        d = steps * backward_step_x;
    else
        d = steps * forward_step_x;
    
    return d;
}

//--------------------------------------------------------------------

float tiptilt::map_step_distance_y(int steps)
{
    float d;
    
    if (steps < 0)
        d = steps * backward_step_y;
    else
        d = steps * forward_step_y;
    
    return d;
}

//--------------------------------------------------------------------

float tiptilt::map_step_distance_z(int steps)
{
    float d;
    
    if (steps < 0)
        d = steps * backward_step_z;
    else
        d = steps * forward_step_z;

    return d;
}

//--------------------------------------------------------------------

void    tiptilt::setxyz(float x, float y, float z)
{
    //printf("%f %f %f\n", x, y, z); 
    x *= 20.0;
    y *= 20.0;
    z *= 20.0;
    
    float ddx = x - c_x;
    float ddy = y - c_y;
    float ddz = z - c_z;
   

    int step_x = map_distance_step_x(ddx);
    int step_y = map_distance_step_y(ddy);
    int step_z = map_distance_step_z(ddz);
 
    if (step_x != 0) {
	move(1, step_x);
    	wait_complete(1);
    }

    if (step_y != 0) {
	move(2, step_y);
    	wait_complete(2);
    }

    if (step_z != 0) {
    	move(3, step_z);
    	wait_complete(3);
    }
    
    c_x += map_step_distance_x(step_x);
    c_y += map_step_distance_y(step_y);
    c_z += map_step_distance_z(step_z);

    tt_talk->Set("c_x", c_x);
    tt_talk->Set("c_y", c_y);
    tt_talk->Set("c_z", c_z);

}

//--------------------------------------------------------------------

void    tiptilt::upd_tilt()
{
    float x,y,z;
    
    if (handle == 0) return;

    tt_talk->Set("d_focus", d_focus);
    float tan30 = 0.57735026;
    
    x = (2.0 / 3.0) * c_a;
    y = (-1.0 / 3.0) * c_a + tan30 * c_b;
    z = (-1.0 / 3.0) * c_a - tan30 * c_b;

    x += d_focus;
    y += d_focus;
    z += d_focus;
    
    setxyz(x, y, z);
}

//--------------------------------------------------------------------

void	tiptilt::Move(float delta_alpha, float delta_beta)
{
    c_a += delta_alpha;
    c_b += delta_beta;
    upd_tilt();
}

//--------------------------------------------------------------------

void	tiptilt::MoveTo(float alpha, float beta)
{
    c_a = alpha;
    c_b = beta;
    upd_tilt();
}


//--------------------------------------------------------------------


void	tiptilt::MoveFocus(float delta)
{
    d_focus += delta;
    upd_tilt();
}

//--------------------------------------------------------------------

void	tiptilt::MoveToFocus(float value)
{
    d_focus = value;
    upd_tilt();
}

//--------------------------------------------------------------------

int	tiptilt::InitUSB()
{
	return init_usb_connection();
}

//--------------------------------------------------------------------

void	tiptilt::CloseUSB()
{
}

//--------------------------------------------------------------------

int	tiptilt::WriteCommand(string cmd)
{
    usb_write(cmd.c_str());
	return 0;
}


//--------------------------------------------------------------------

string	tiptilt::ReadResult()
{
    usb_read();
    
	return string((char*)receiveBuf);
}

//--------------------------------------------------------------------

/*
tiptilt *tt;


int main()
{
    signal(SIGINT, tt_sighandler);

    tt = new tiptilt();




    for (int i = 0; i < 5000; i++) {
	    tt->MoveFocus(1);
	    tt->MoveFocus(1);
    }

    printf("u\n");
    delete tt;
    
    return 0;
}
*/
