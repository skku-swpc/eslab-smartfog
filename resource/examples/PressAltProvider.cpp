//Oduino serial reader
#include <stdio.h>    // Standard input/output definitions 
#include <unistd.h>   // UNIX standard function definitions 
#include <fcntl.h>    // File control definitions 
#include <errno.h>    // Error number definitions 
#include <termios.h>  // POSIX terminal control definitions 
#include <string.h>   // String function definitions 
#include <sys/ioctl.h>
///////////////////////
#include <functional>
#include <sstream>

#include <pthread.h>
#include <mutex>
#include <condition_variable>
#include <math.h>//for model driven calculation

#include "OCPlatform.h"
#include "OCApi.h"

#include "phy_interface.h"

#define ODU_DEV_PATH1 "/dev/ttyUSB0"
#define ODU_DEV_PATH2 "/dev/ttyACM1"
#define ODU_DEV_PATH3 "/dev/ttyACM2"

#define ODU_BD_RT B115200
#define ODU_BUF_SIZ 256
//#define SERIALPORTDEBUG

using namespace OC;
using namespace std;
namespace PH = std::placeholders;

int gObservation = 0;
void * ChangeOduinoRepresentation (void *param);
void * ReadOduinoData (void *param);
void * handleSlowResponse (void *param, std::shared_ptr<OCResourceRequest> pRequest);

// Specifies where to notify all observers or list of observers
// false: notifies all observers
// true: notifies list of observers
bool isListOfObservers = false;

// Specifies secure or non-secure
// false: non-secure resource
// true: secure resource
bool isSecure = false;

/// Specifies whether Entity handler is going to do slow response or not
bool isSlowResponse = false;

// Forward declaring the entityHandler

/// This class represents a single resource named 'lightResource'. This resource has
/// two simple properties named 'state' and 'power'

class SerialReader
{
	private:
		int fd,c,res;
		int press, alt;
		char buf[ODU_BUF_SIZ];
	public:
		SerialReader(){
			struct termios toptions;

			//fd = open(serialport, O_RDWR | O_NOCTTY | O_NDELAY);
			fd = open(ODU_DEV_PATH1, O_RDWR | O_NONBLOCK );

			if (fd == -1)  {
				printf("serialport_init: Unable to open por\n ");
			}

			//int iflags = TIOCM_DTR;
			//ioctl(fd, TIOCMBIS, &iflags);     // turn on DTR
			//ioctl(fd, TIOCMBIC, &iflags);    // turn off DTR

			if (tcgetattr(fd, &toptions) < 0) {
				perror("serialport_init: Couldn't get term attributes");
			}
			int baud = 115200;
			speed_t brate = baud; // let you override switch below if needed
			switch(baud) {
				case 4800:   brate=B4800;   break;
				case 9600:   brate=B9600;   break;
#ifdef B14400
				case 14400:  brate=B14400;  break;
#endif
				case 19200:  brate=B19200;  break;
#ifdef B28800
				case 28800:  brate=B28800;  break;
#endif
				case 38400:  brate=B38400;  break;
				case 57600:  brate=B57600;  break;
				case 115200: brate=B115200; break;
			}
			cfsetispeed(&toptions, brate);
			cfsetospeed(&toptions, brate);

			// 8N1
			toptions.c_cflag &= ~PARENB;
			toptions.c_cflag &= ~CSTOPB;
			toptions.c_cflag &= ~CSIZE;
			toptions.c_cflag |= CS8;
			// no flow control
			toptions.c_cflag &= ~CRTSCTS;

			//toptions.c_cflag &= ~HUPCL; // disable hang-up-on-close to avoid reset

			toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
			toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

			toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
			toptions.c_oflag &= ~OPOST; // make raw

			// see: http://unixwiz.net/techtips/termios-vmin-vtime.html
			toptions.c_cc[VMIN]  = 0;
			toptions.c_cc[VTIME] = 0;
			//toptions.c_cc[VTIME] = 20;

			tcsetattr(fd, TCSANOW, &toptions);
			if( tcsetattr(fd, TCSAFLUSH, &toptions) < 0) {
				perror("init_serialport: Couldn't set term attributes");
			}
		};
		~SerialReader(){
			close(fd);
		};
		int isCreated(){	//1:Success, 0:Failed
			if(fd<=0)
				return 0;
			else
				return 1;
		}

		int listen(){
			if(isCreated() == 0){
				cout << "SerialReader has not been created" <<endl;
				return 0;
			}

			return parse()==1;
		};
		int parse(){
		
			char b[1];  // read expects an array, so we give it a 1-byte array
			int i=0;
			int timeout = 2;
			do { 
				int n = read(fd, b, 1);  // read a char at a time
				if( n==-1) return -1;    // couldn't read
				if( n==0 ) {
					usleep( 1 * 1000 );  // wait 1 msec try again
					timeout--;
					continue;
				}
#ifdef SERIALPORTDEBUG  
				printf("serialport_read_until: i=%d, n=%d b='%c'\n",i,n,b[0]); // debug
#endif
				buf[i] = b[0]; 
				i++;
			} while( b[0] != '\n' && i < ODU_BUF_SIZ && timeout>0 );

			buf[i] = 0;  // null terminate the string

#ifdef SERIALPORTDEBUG
			cout<<i<<endl;
#endif

			char *cur, *next;
			int len, index;
			char value_string[ODU_BUF_SIZ];
			float value[3];

			cur = buf;
			for(index = 0;;index++){
				memset(value_string, 0, sizeof(char) * 10);
				next = strstr(cur, "/");
				if (next == NULL)
					break;
				len = next - cur;
				next++;
				strncpy(value_string, cur, len);
				cur = next;
				value[index] = atof(value_string);

				//printf("%.2f\n", value[index]);
			}
			if(strlen(buf) < 12){
			    return 0;
			}
			press = (int)value[0];
			alt = (int)value[1];
			
			//printf("%d %d\n", press, alt);

#ifdef SERIALPORTDEBUG
			cout <<press<<' '<<humi<<endl;
#endif
//			sleep(2);
			tcflush(fd, TCIFLUSH);

			return 1;
		};
		int get_Press(){
			return press;
		}
		int get_Alt(){
			return alt;
		}

};



class OduinoResource
{

private:
    /// Access this property from a TB client
    std::string m_name;
    int m_press;
    int m_alt;
    int m_availablePolicy;
    std::string m_oduinoUri;
    static OduinoResource *instance;
    SerialReader *sr;
    /// Constructor
    OduinoResource()
    {
        // Initialize representation

        m_name = "ESLab Press/Alt Sensor";
        m_press = 0;
        m_alt = 0;
        m_oduinoUri = "/a/pa";
        //PHY_Interface::SetNetPolicy(DT_ACQ_POLLING | DT_ACQ_BATCHING | DT_ACQ_MODEL, 0, m_availablePolicy);
        //PHY_Interface::SetRawPolicy(DT_ACQ_POLLING, 0, m_availablePolicy);
	PHY_Interface::SetPolicy(DT_ACQ_POLLING, 0, DT_ACQ_POLLING | DT_ACQ_BATCHING | DT_ACQ_MODEL, 0, m_availablePolicy);
        sr = new SerialReader();
    }

public:
    static OduinoResource *getInstance();

    std::string getUri()
    {
        return m_oduinoUri;
    }
    int getPress()
    {
        return m_press;
    }
    int getAlt()
    {
        return m_alt;
    } 

    void put(OCRepresentation& rep)
    {
        try {
	    std::string tempString;
	    if (rep.getValue("press", tempString))
            {
		m_press = atoi(tempString.c_str());
                cout << "\t\t\t\t" << "pressure: " << m_press << endl;
            }
            else
            {
                cout << "\t\t\t\t" << "press not found in the representation" << endl;
            }

            if (rep.getValue("alt", tempString))
            {
		m_alt = atoi(tempString.c_str());
                cout << "\t\t\t\t" << "altitude: " << m_alt << endl;
            }
            else
            {
                cout << "\t\t\t\t" << "alt not found in the representation" << endl;
            }
        }
        catch (exception& e)
        {
            cout << e.what() << endl;
        }

    }
    void get(OCRepresentation &rep)
    {
       if(sr->listen()){
    		m_press = sr->get_Press();
    		m_alt = sr->get_Alt();
		printf("OduinoResource::get(), %d, %d\n", m_press, m_alt);
        }
        
#ifdef SERIALPORTDEBUG
		cout << "PRESS:" <<oduinoPtr->m_press<<"ALT:"<<oduinoPtr->m_alt<<endl;
#endif
        std::stringstream ss;
        std::stringstream ss1;
        ss << m_press;
        rep.setValue("press", ss.str());
        ss1 << m_alt;
        rep.setValue("alt", ss1.str());
    }
    int getAvailablePolicy()
    {
        return m_availablePolicy;
    }

};

OduinoResource *OduinoResource::instance = NULL;
OduinoResource *OduinoResource::getInstance()
{
    if(instance == NULL){
        instance = new OduinoResource();
//	printf("New instance\n");
    }

    return instance;
}
// ChangeLightRepresentaion is an observation function,
// which notifies any changes to the resource to stack
// via notifyObservers

class MY_Callback : public PHY_Basic_Callback
{
    public:
	int num_notify;
	int net_level, raw_level;
	
	MY_Callback()
	{
	    net_level = 0;
	    raw_level = 0;
	    num_notify = 0;
	}

        void get(OUT OCRepresentation &rep)
        {
            OduinoResource::getInstance()->get(rep);
        }
        void put(IN OCRepresentation &rep)
        {
            OduinoResource::getInstance()->put(rep);
        }
        bool defaultAction(int polType)
        {
	    //return pollingAction(polType, 10);
	    //return modelAction(polType, 10);
	    /*
	    if(polType == RAW_POL_TYPE){
		return pollingAction(polType, 2);
	    }
	    else if(polType == NET_POL_TYPE){
		return batchingAction(polType, 20);
	    }
	    */

	    if(polType == RAW_POL_TYPE){
		sleep(1);
		num_notify++;
    	    }
	    else if(polType == NET_POL_TYPE){
	        num_notify = 0;
	    }
	    return true;
        }

        bool pollingAction(int polType, int level)
        {
	    static int raw_sleep;
	    static int net_sleep;

            if(polType == RAW_POL_TYPE){
		if(raw_sleep >= level){
		    raw_sleep = 0;
		    num_notify++;
		    printf("polling raw num_notify = %d\n", num_notify);
                    return true;		 
		}
		else{
		    int sleep_time;
		    int raw_expect = raw_level - raw_sleep;
		    int net_expect = net_level - net_sleep;

		    if(net_expect>0){
			sleep_time = raw_expect < net_expect ? raw_expect:net_expect;
		    }
		    else{
			sleep_time = raw_expect;
		    }
		    if(sleep_time > 0){
			sleep(sleep_time);
			raw_sleep+=sleep_time;
			net_sleep+=sleep_time;
		    }
		    return false;
		}
	    }
	    else if(polType == NET_POL_TYPE){
		if(net_sleep >= level){
		    net_sleep = 0;
		    //num_notify = 0;
		    //printf("polling net num_notify = %d\n", num_notify);
		    return true;
		}
		else{
		    int sleep_time;
		    int raw_expect = raw_level - raw_sleep;
		    int net_expect = net_level - net_sleep;
		    if(raw_expect>0){
			sleep_time = raw_expect < net_expect ? raw_expect:net_expect;
		    }
		    else{
			sleep_time = net_expect;
		    }
		    if(sleep_time > 0){
			sleep(sleep_time);
			raw_sleep+=sleep_time;
			net_sleep+=sleep_time;
		    }
		    return false;
		}
	    }
	    return true;
	}

        bool modelAction(int polType, int level)
        {  
	    static int prevPress;
	    static int prevAlt;
            if(polType == RAW_POL_TYPE){
		raw_level = level;
                return pollingAction(polType, level);
	    }
	    else if(polType == NET_POL_TYPE){
		net_level = level;
		int curPress, curAlt;
		curPress = OduinoResource::getInstance()->getPress();
		curAlt = OduinoResource::getInstance()->getAlt();
		//printf("press:%d/%d\talt:%d/%d\n", curPress, prevPress, curAlt, prevAlt);
		if((double)abs(curPress-prevPress) > (double)prevPress / 1000 * (level * 100 / 255)){
		    prevPress = curPress;
		    prevAlt = curAlt;
		    return true;
		}
		else if((double)abs(curAlt-prevAlt) > (double)prevAlt / 1000 * (level * 100 / 255)){
		    prevPress = curPress;
		    prevAlt = curAlt;
		    return true;
		}
		else{
		    return false;
		}
	    }
	    return true;
        }

        bool batchingAction(int polType, int level)
        {
	    static int net_flag;
	    if(polType == RAW_POL_TYPE){
		raw_level = level;
		return pollingAction(polType, level);;
	    }
	    else if(polType == NET_POL_TYPE){
		net_level = level;
		int ret = false;

		if(net_flag != true){
		    ret = pollingAction(polType, level);
		}
		if(ret == true){
		    net_flag = true;
		}
		if(net_flag == true && num_notify > 0){
		    num_notify--;
		    printf("num_notify = %d\n", num_notify);
		    return true;
		}
		else if(net_flag == true && num_notify == 0){
		    net_flag = false;
		    return false;
		}
		return false;
	    }
	    return false;
        }
};

int main(int argc, char* argv[])
{
    try
    {
        PHY_Interface::getInstance()->createResource( OduinoResource::getInstance()->getUri(), "core.provider", DEFAULT_INTERFACE, 
            OduinoResource::getInstance()->getAvailablePolicy(), false, std::make_shared<MY_Callback>());

        // A condition variable will free the mutex it is given, then do a non-
        // intensive block until 'notify' is called on it.  In this case, since we
        // don't ever call cv.notify, this should be a non-processor intensive version
        // of while(true);
        std::mutex blocker;
        std::condition_variable cv;
        std::unique_lock<std::mutex> lock(blocker);
        cv.wait(lock);
    }
    catch(OCException e)
    {
        //log(e.what());
    }

    // No explicit call to stop the platform.
    // When OCPlatform::destructor is invoked, internally we do platform cleanup

    return 0;
}

