#include <string>
#include <cstdlib>
#include <pthread.h>
#include <mutex>
#include <condition_variable>
#include <typeinfo>

#include "OCPlatform.h"
#include "OCApi.h"

using namespace OC;

std::shared_ptr<OCResource> curResource;
static ObserveType OBSERVE_TYPE_TO_USE = ObserveType::Observe;

class Oduino
{
public:

    int m_temp;
    int m_humi;
    std::string m_name;

    Oduino() : m_temp(0), m_humi(0), m_name("")
    {
    }
};

static std::string curDiscomfort;
static int discomfortDirty;
static int discomfortSeq;

static Oduino myOduino1;
static Oduino myOduino2;


void ss_initialize()
{
	/* Virtual Sensor Test Code */

	curDiscomfort = "cozy";
    discomfortDirty = 0;
    discomfortSeq = 0;
}
int fibonacci(int n)
{
	if (n == 1 || n == 0)
	{
		return 1;
	}
	
	return fibonacci(n-1) + fibonacci(n-2);
}
void Discomfort_Update()
{
	static std::string prev_level = "cosy";
	std::string curr_level;
	int ret;
	int tmp, hum;
	int fibo = fibonacci(20);
    std::cout << "fibo : " << fibo << std::endl;
    

	tmp = (myOduino1.m_temp + myOduino2.m_temp) >> 1;
	hum = (myOduino1.m_humi + myOduino2.m_humi) >> 1;
	
	
	if(22 < tmp && tmp <= 28 && 39 < hum && hum < 61)
	{
		curr_level = "cosy";
	}
	else if(tmp > 28)
	{
		if(hum > 61)
		{
			curr_level = "hot and humid";
		} 
		else if(hum < 40)
		{
			curr_level = "hot and dry";
		}
		else
		{
			curr_level = "hot";
		}
		
	}
	else if(tmp < 20)
	{
		if(hum >= 61)
		{
			curr_level = "cold and humid";
		}
		else if(hum < 40)
		{
			curr_level = "cold and dry";
		}
		else
		{
			curr_level = "cold";
		}
	}
	else
	{
		if(hum >= 61)
		{
			curr_level = "humid";
		}
		else if(hum < 40)
		{
			curr_level = "dry";
		}
		else
		{
			curr_level = "cosy";
		}
	}

	if(curDiscomfort != curr_level)
	{
		discomfortDirty = 1;
		curDiscomfort = curr_level;
	}

}

void onObserve(const HeaderOptions headerOptions, const OCRepresentation& rep,
                    const int& eCode, const int& sequenceNumber)
{
    if(eCode == OC_STACK_OK)
    {
        if(rep.getUri() == "/a/oduino")
        {
            std::string temp;
            rep.getValue("temp", temp);
            myOduino1.m_temp = atoi(temp.c_str());
            rep.getValue("humi", temp);
            myOduino1.m_humi = atoi(temp.c_str());
            Discomfort_Update();
        }else if(rep.getUri() == "/a/th")
        {
            std::string temp;
            rep.getValue("temp", temp);
            myOduino2.m_temp = atoi(temp.c_str());
            rep.getValue("humi", temp);
            myOduino2.m_humi = atoi(temp.c_str());
            Discomfort_Update();
        }
        

        if(discomfortDirty)
        {
            std::cout << "OBSERVE RESULT:"<<std::endl;
            std::cout << "\tSequenceNumber: "<< discomfortSeq++ << endl;
            std::cout << "URI:/ss/discomfort \n attribute: "<< curDiscomfort << std::endl;
            
            discomfortDirty = 0;
        }

    }
    else
    {
        std::cout << "onObserve Response error: " << eCode << std::endl;
        std::exit(-1);
    }
}

// Local function to get representation of light resource

// Callback to found resources
void foundResource(std::shared_ptr<OCResource> resource)
{
    if(curResource)
    {
        std::cout << "Found another resource, ignoring"<<std::endl;
    }

    std::string resourceURI;
    std::string hostAddress;
    try
    {
        // Do some operations with resource object.
        if(resource)
        {
            std::cout<<"DISCOVERED Resource:"<<std::endl;
            // Get the resource URI
            resourceURI = resource->uri();
            std::cout << "\tURI of the resource: " << resourceURI << std::endl;
            if(resourceURI == "/a/oduino" || resourceURI == "/a/th")
            {
                static int od, th;
                if(resourceURI == "/a/oduino")
                {
                    od = 1;
                     resource->observe(OBSERVE_TYPE_TO_USE, QueryParamsMap(), &onObserve);
                }
                if (resourceURI == "/a/th")
                {
                    th = 1;
                    resource->observe(OBSERVE_TYPE_TO_USE, QueryParamsMap(), &onObserve);
                }
                if(od && th)
                    ss_initialize();
            }
        }
        else
        {
            // Resource is invalid
            std::cout << "Resource is invalid" << std::endl;
        }

    }
    catch(std::exception& e)
    {
        //log(e.what());
    }
}

void PrintUsage()
{
    std::cout << std::endl;
    std::cout << "Usage : simpleclient <ObserveType>" << std::endl;
    std::cout << "   ObserveType : 1 - Observe" << std::endl;
    std::cout << "   ObserveType : 2 - ObserveAll" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 1)
    {
        OBSERVE_TYPE_TO_USE = ObserveType::Observe;
    }
    else if (argc == 2)
    {
        int value = atoi(argv[1]);
        if (value == 1)
            OBSERVE_TYPE_TO_USE = ObserveType::Observe;
        else if (value == 2)
            OBSERVE_TYPE_TO_USE = ObserveType::ObserveAll;
        else
            OBSERVE_TYPE_TO_USE = ObserveType::Observe;
    }
    else
    {
        PrintUsage();
        return -1;
    }

    // Create PlatformConfig object
    PlatformConfig cfg {
        OC::ServiceType::InProc,
        OC::ModeType::Client,
        "0.0.0.0",
        0,
        OC::QualityOfService::LowQos
    };

    OCPlatform::Configure(cfg);
    try
    {
        // makes it so that all boolean values are printed as 'true/false' in this stream
        std::cout.setf(std::ios::boolalpha);
        // Find all resources
        OCPlatform::findResource("", "coap://224.0.1.187/oc/core?rt=core.provider", &foundResource);
        std::cout<< "Finding Resource... " <<std::endl;

        // A condition variable will free the mutex it is given, then do a non-
        // intensive block until 'notify' is called on it.  In this case, since we
        // don't ever call cv.notify, this should be a non-processor intensive version
        // of while(true);
        std::mutex blocker;
        std::condition_variable cv;
        std::unique_lock<std::mutex> lock(blocker);
        cv.wait(lock);

    }catch(OCException& e)
    {
        //log(e.what());
    }

    return 0;
}

