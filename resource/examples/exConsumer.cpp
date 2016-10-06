#include "vs_basic.h"

void printDeviceList(std::shared_ptr<VS_DeviceList> deviceList);
void printCollection(std::shared_ptr<VS_Collection> collection);
void printCollection(std::shared_ptr<VS_Collection> collection, int tabs);
void printResource(std::shared_ptr<VS_Resource> resource);
void printResource(std::shared_ptr<VS_Resource> resource, int tabs);

class ResourceFinder
{
	private:
		int i;
	public:
		ResourceFinder()
		 : i(0) {
		}

		std::shared_ptr<VS_Resource> getNthResource(IN std::shared_ptr<VS_Collection> collection, int n) {
			if(collection->isResource()) {
				std::vector<std::shared_ptr<VS_Resource>> resources = collection->getResources();
				for(std::shared_ptr<VS_Resource> resource : resources) {
					i++;
					if(i == n) return resource;
				}
				return NULL;
			}
			else {
				std::vector<std::shared_ptr<VS_Collection>> children = collection->getChildren();
				for(std::shared_ptr<VS_Collection> child: children) {
					std::shared_ptr<VS_Resource> nthResource = getNthResource(child, n);
					if(nthResource != NULL) {
						return nthResource;
					}
				}
				return NULL;
			}
		}

        std::string getNthResourceUri(IN std::shared_ptr<VS_DeviceList> deviceList, unsigned int n)
        {
#ifndef BIG_NETWORK_SIZE_SUPPORTED
            std::shared_ptr<std::vector<std::string>> resourcesVector = deviceList->getResourcesVector();

            if(n < 1 || n > resourcesVector->size())
                return std::string();

            return resourcesVector->at(n-1);
#else
            std::shared_ptr<VS_Resource> vs_res = getNthResource(deviceList->getRoot(), n);
            if(NULL == vs_res)
                return std::string();
            else
                return vs_res->getUri();
#endif
        }
};

class MyCallbacks : public VS_Basic_Callbacks 
{
	public:
		void onConnected() {
			vs_log("onConnected----------------------------");
			vs_log("==================================================\n\n");

			vs_log("==================================================");
			vs_log("Request: GW_CMD_GET_DEVICE_LIST");

			if(VSF_S_OK != VS_Basic::get()->GetDeviceList())
			{
				vs_log("No connection with GW");
			}
		}


		void onGetDeviceList(IN std::shared_ptr<VS_DeviceList> deviceList, const int eCode) {
			vs_log("onGetDeviceList-----------------------------------");
			//vs_log("DeviceList(XML) = %s", deviceList->getDeviceListXML().c_str());
			vs_log("Parsed:");
			printDeviceList(deviceList);
			vs_log("==================================================\n\n");

			vs_log("==================================================");
			std::string firstResourceUri = ResourceFinder().getNthResourceUri(deviceList, 1);
			if(firstResourceUri.size() <= 0)
			{
   				vs_log("WARNING: There is no first sensor to read. It exits now.");
                return;
			}
			VS_Basic::get()->GetSensorInfo(firstResourceUri);
			std::string secondResourceUri = ResourceFinder().getNthResourceUri(deviceList, 2);
			if(secondResourceUri.size() <= 0)
			{
   				vs_log("WARNING: There is no second sensor to read. It exits now.");
                return;
			}
			VS_Basic::get()->GetSensorInfo(secondResourceUri);

            VS_Basic::get()->SetPolicy(firstResourceUri, DT_ACQ_POLLING, 100, DT_ACQ_MODEL, 100);

             OCRepresentation oc_rep;
            oc_rep.setValue("temp", std::string("50"));
            oc_rep.setValue("humi", std::string("40"));
            VS_Basic::get()->PutActuatorData(firstResourceUri, oc_rep);

		}

		void onGetSensorInfo(IN std::shared_ptr<VS_Resource> resource, const int eCode) {
			vs_log("onGetSensorInfo----------------------------------");
//			vs_log("Resource(XML) = %s", resource->getResourceXML().c_str());
//			vs_log("Parsed:");
			printResource(resource);
			vs_log("==================================================\n\n");

			vs_log("==================================================");
			vs_log("Request: Observe - %s", resource->getUri().c_str());

			VS_Basic::get()->StartObservingSensorData(resource->getUri());
		}

        void onGetSensorData(IN std::shared_ptr<VS_Resource> resource, const int eCode)
            //TODO: Implementation of XML parser for sensor data
        {            
        }
		void onObservingSensorData(IN std::shared_ptr<VS_Resource> resource, const int& eCode, const int& sequenceNumber) {
			vs_log("onObserveSensorData-------------------------------");
//			vs_log("Resource(XML) = %s", resource->getResourceXML().c_str());
//			vs_log("Parsed:");
			printResource(resource);
			vs_log("==================================================");
		}

        void onPutActuatorData(IN std::shared_ptr<VS_Resource> resource, const int& eCode)
        {
            vs_log("onPutActuaotrData----------------------------------");
//			vs_log("Parsed:");
			printResource(resource);
			vs_log("==================================================\n\n");
        }
};

void testParsingResource() {

	char inputXML[] =\
"<resource>\n\
	<uri>/odroid/thermometer</uri>\n\
	<name>thermometer</name>\n\
	<AvailablePolicy>000000ff</AvailablePolicy>\n\
	<attributes>\n\
		<attribute>\n\
			<name>temperature</name>\n\
			<value>28</value>\n\
		</attribute>\n\
		<attribute>\n\
			<name>humidity</name>\n\
			<value>40</value>\n\
		</attribute>\n\
	</attributes>\n\
	<policies>\n\
		<policy>0a0a0a0a</policy>\n\
		<policy>0abababa</policy>\n\
		<policy>0cfafafa</policy>\n\
	</policies>\n\
</resource>";
	vs_log("XML Input: %s", inputXML);
		std::string xmlString(inputXML);
		shared_ptr<VS_Resource> resource = VS_Resource::create(xmlString);
		printResource(resource);
    return;
}

void testParsingDeviceList() {
	char inputXML[] =\
"<collection><name>root</name>\n\
	<collection><name>a</name>\n\
		<resource><uri>root/a/oduino</uri><name>oduino</name><availablepolicy>00000000</availablepolicy></resource>\n\
	</collection>\n\
</collection>";
	vs_log("xml input: %s", inputXML);
	std::string xmlString(inputXML);
	shared_ptr<VS_DeviceList> deviceList = VS_DeviceList::create(xmlString);
	printDeviceList(deviceList);
	return;
}

void testParsingDeviceList2() {
	char inputXML[] =\
"<collection><name>root</name>\
	<collection><name>a</name>\
		<resource><uri>root/a/tme</uri><name>tme</name><AvailablePolicy>00000000</AvailablePolicy></resource>\
		<resource><uri>root/a/pmp</uri><name>pmp</name><AvailablePolicy>00000000</AvailablePolicy></resource>\
		<resource><uri>root/a/oduino</uri><name>oduino</name><AvailablePolicy>00000000</AvailablePolicy></resource>\
	</collection>\
	<resource><uri>root/tat</uri><name>tat</name><AvailablePolicy>00000000</AvailablePolicy></resource>\
	<collection><name>ss</name>\
		<resource><uri>root/ss/kakaka</uri><name>kakaka</name><AvailablePolicy>00000000</AvailablePolicy></resource>\
		<resource><uri>root/ss/discomfort</uri><name>discomfort</name><attributes><attribute><name>level</name><value>cosy</value></attribute></attributes><AvailablePolicy>00000000</AvailablePolicy></resource>\
	</collection>\
</collection>";
	vs_log("xml input: %s", inputXML);
	std::string xmlString(inputXML);
	shared_ptr<VS_DeviceList> DeviceList = VS_DeviceList::create(xmlString);
	printDeviceList(DeviceList);
	return;
}

int main()
{
//	testParsingResource();
//	testParsingDeviceList();
//	testParsingDeviceList2();

	std::shared_ptr<VS_Basic_Callbacks> callbacks(new MyCallbacks());
	VS_Basic::get()->setCallbacks(callbacks);
	VS_Basic::get()->ConnectGW();
}

inline void printfWithTabs(int tabs, const char* format, ...) {
	for(int i=0; i<tabs; i++) printf("  ");
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

void printDeviceList(std::shared_ptr<VS_DeviceList> deviceList) {
	if(deviceList == NULL) return;
#ifdef BIG_NETWORK_SIZE_SUPPORTED
	printCollection(deviceList->getRoot());
#else
   std::shared_ptr<std::vector<std::string>> resourcesVector = deviceList->getResourcesVector();
    for(std::vector<std::string>::iterator it = resourcesVector->begin(); it != resourcesVector->end(); it++)
    {
        printfWithTabs(0, "[Resource uri = \"%s\"]\n", it->c_str());
    }
#endif
}

void printCollection(std::shared_ptr<VS_Collection> collection) {
	printCollection(collection, 0);
}

void printCollection(std::shared_ptr<VS_Collection> collection, int tabs) {
	printfWithTabs(tabs, "[Collection name=\"%s\"]\n", collection->getName().c_str());
	if(collection->isResource()) {
		std::vector<std::shared_ptr<VS_Resource>> resources;
		resources = collection->getResources();
		for(std::shared_ptr<VS_Resource> resource: resources) {
			printResource(resource, tabs + 1);
		}
	}
	std::vector<std::shared_ptr<VS_Collection>> children = collection->getChildren();
	for(std::shared_ptr<VS_Collection> child : children) {
		printCollection(child, tabs + 1);
	}
}

void printResource(std::shared_ptr<VS_Resource> resource) {
	if(resource == NULL) return;
	printResource(resource, 0);
}

void printResource(std::shared_ptr<VS_Resource> resource, int tabs) {
	printfWithTabs(tabs, "[Resource uri=\"%s\", name: \"%s\"]\n", resource->getUri().c_str(), resource->getName().c_str());
	printfWithTabs(tabs + 1, "AvailablePolicy: %08x\n",
			resource->getAvailablePolicy());
	std::vector<int> policies = resource->getPolicies();
	printfWithTabs(tabs + 1, "Policy: ");
	if(policies.size() == 0)
		printf("No policy");
	else {
		for(auto policy: policies) {
			printf("%08x ", policy);
		}
	}
	printf("\n");
	printfWithTabs(tabs + 1, "Attributes: ");
	std::map<std::string, std::string> attributes = resource->getAttributes();
	if(attributes.size() == 0) 
		printf("No attributes\n");
	else {
		for(auto attribute: attributes) {
			printf("%s=\"%s\" ", attribute.first.c_str(), attribute.second.c_str());
		}
	}
	printf("\n");
	return;
}
