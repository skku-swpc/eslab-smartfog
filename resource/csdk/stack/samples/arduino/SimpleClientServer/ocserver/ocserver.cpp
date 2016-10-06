//******************************************************************
//
// Copyright 2014 Intel Mobile Communications GmbH All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Do not remove the include below
#include "Arduino.h"

#include "logger.h"
#include "ocstack.h"
#include <string.h>
#include "cJSON.h"
// Arduino WiFi Shield
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define dht_dpin A0
byte bGlobalErr;
byte dht_dat[5];

#define POLICY_POLLING 0
#define POLICY_BATCHING 2
#define POLICY_MODEL 4

const char *getResult(OCStackResult result);

PROGMEM const char TAG[] = "Arduino";

int gDHTUnderObservation = 0;
void createDHTResource();

/* Structure to represent a Light resource */
typedef struct DHTRESOURCE{
    OCResourceHandle handle;
    bool state;
    float temp;
    float humid;
} DHTResource;

static DHTResource dht, old;

//static char responsePayloadGet[256] = "0";
//static char responsePayloadPut[] = "{\"href\":\"/a/dht\",\"rep\":{\"state\":\"on\",\"policy\":1}}";
static char responseA[] = "{\"href\":\"/a/dht\",\"rep\":{\"state\":\"";
static char responseB[] = "\",\"temp\":";
static char responseC[] = ",\"humid\":";
static char responseD[] = "}}";

/// This is the port which Arduino Server will use for all unicast communication with it's peers
static uint16_t OC_WELL_KNOWN_PORT = 5683;

/// WiFi Shield firmware with Intel patches
//static const char INTEL_WIFI_SHIELD_FW_VER[] = "1.1.0";

/// WiFi network info and credentials
//char ssid[] = "iotivity";
//char pass[] = "iotivity123";

int dev_policy = POLICY_POLLING;
int dev_level = 127;
int net_policy = POLICY_POLLING;
int net_level = 127;
int delayed_time=0;

void InitDHT(){
    pinMode(dht_dpin,OUTPUT);
    digitalWrite(dht_dpin,HIGH);
}

byte read_dht_dat(){
    byte i = 0;
    byte result=0;
    for(i=0; i< 8; i++){
	while(digitalRead(dht_dpin)==LOW);
	delayMicroseconds(30);
	if (digitalRead(dht_dpin)==HIGH)
		result |=(1<<(7-i));
	while (digitalRead(dht_dpin)==HIGH);
    }
    return result;
}


void ReadDHT(){
	bGlobalErr=0;
	byte dht_in;
	byte i;
	digitalWrite(dht_dpin,LOW);
	delay(20);

	digitalWrite(dht_dpin,HIGH);
	delayMicroseconds(40);
	pinMode(dht_dpin,INPUT);
	//delayMicroseconds(40);
	dht_in=digitalRead(dht_dpin);

	if(dht_in){
		bGlobalErr=1;
		return;
	}
	delayMicroseconds(80);
	dht_in=digitalRead(dht_dpin);

	if(!dht_in){
		bGlobalErr=2;
		return;
	}
	delayMicroseconds(80);
	for (i=0; i<5; i++)
		dht_dat[i] = read_dht_dat();
	pinMode(dht_dpin,OUTPUT);
	digitalWrite(dht_dpin,HIGH);
	byte dht_check_sum = dht_dat[0]+dht_dat[1]+dht_dat[2]+dht_dat[3];
	if(dht_dat[4]!= dht_check_sum)
		bGlobalErr=3;
}

int ConnectToNetwork()
{
    char *fwVersion;
    int status = WL_IDLE_STATUS;
    IPAddress s_ip(192,168,0,6);
    IPAddress s_dns(115,145,129,11);
    IPAddress s_gw(192,168,0,1);
    IPAddress s_sn(255,255,255,0);

    // check for the presence of the shield:
    if (WiFi.status() == WL_NO_SHIELD)
    {
        OC_LOG(ERROR, TAG, PCF("WiFi shield not present"));
        return -1;
    }

    // Verify that WiFi Shield is running the firmware with all UDP fixes

//    fwVersion = WiFi.firmwareVersion();
//    OC_LOG_V(INFO, TAG, "WiFi Shield Firmware version %s", fwVersion);
/*
    if ( strncmp(fwVersion, INTEL_WIFI_SHIELD_FW_VER, sizeof(INTEL_WIFI_SHIELD_FW_VER)) !=0 )
    {
        OC_LOG(DEBUG, TAG, PCF("!!!!! Upgrade WiFi Shield Firmware version !!!!!!"));
        return -1;
    }
*/

    // attempt to connect to Wifi network:
    while (status != WL_CONNECTED)
    {
        //OC_LOG_V(INFO, TAG, "Attempting to connect to SSID: %s", "eslab");
        //status = WiFi.begin(ssid,pass);
	WiFi.config(s_ip,s_dns,s_gw,s_sn);
	status = WiFi.begin("eslab_sub", "eslab100");

        // wait 10 seconds for connection:
        delay(5000);
    }
    OC_LOG(DEBUG, TAG, PCF("Connected to wifi"));

    IPAddress ip = WiFi.localIP();
    OC_LOG_V(INFO, TAG, "IP Address:  %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    s_gw = WiFi.gatewayIP();
    OC_LOG_V(INFO, TAG, "GW Address:  %d.%d.%d.%d", s_gw[0], s_gw[1], s_gw[2], s_gw[3]);
    s_sn = WiFi.subnetMask();
    OC_LOG_V(INFO, TAG, "SN Address:  %d.%d.%d.%d", s_sn[0], s_sn[1], s_sn[2], s_sn[3]);

    return 0;
}

// for setting 'policy' and 'acquisition level'
OCEntityHandlerResult ProcessPutRequest (OCEntityHandlerRequest *ehRequest, char *payload, OCEntityHandlerResponse *response){
	OCEntityHandlerResult ehRet = OC_EH_OK;
	cJSON *putJson = cJSON_Parse((char *)ehRequest->reqJSONPayload);;
	char *policy;
	int level = 0;
	char value[10] = {0};
	
	policy = cJSON_GetObjectItem(putJson, "NET_POLICY")->valuestring;
	if(strcmp(policy, "polling") == 0){
		net_policy = POLICY_POLLING;
		OC_LOG(INFO, TAG, PCF("Network Policy = polling"));
	}
	else if(strcmp(policy, "batch") == 0){
		net_policy = POLICY_BATCHING;
		OC_LOG(INFO, TAG, PCF("Network Policy = batch"));
	}
	else if(strcmp(policy, "model") == 0){
		net_policy = POLICY_MODEL;
		OC_LOG(INFO, TAG, PCF("Network Policy = model)"));
	}
	else{
		OC_LOG_V(WARNING, TAG, "Unkown Network Policy = %s", policy);
		ehRet = OC_EH_ERROR;
	}
	
	if(ehRet == OC_EH_OK){
		level = cJSON_GetObjectItem(putJson, "NET_LEVEL")->valueint;
		if(level < 256 && level >= 0){
			OC_LOG_V(INFO, TAG, "Set Policy Level = %d", level);
			net_level = level;
		}
		else{
			OC_LOG_V(INFO, TAG, "Assertion Fail, Current Policy Level = %d, Requested Level = %d", dev_level, level);
		}
	}


	policy = cJSON_GetObjectItem(putJson, "RAW_POLICY")->valuestring;
	if(strcmp(policy, "polling") == 0){
		dev_policy = POLICY_POLLING;
		OC_LOG(INFO, TAG, PCF("Device Policy = polling"));
	}
	else if(strcmp(policy, "batch") == 0){
		dev_policy = POLICY_BATCHING;
		OC_LOG(INFO, TAG, PCF("Device Policy = batch"));
	}
	else if(strcmp(policy, "model") == 0){
		dev_policy = POLICY_MODEL;
		OC_LOG(INFO, TAG, PCF("Device Policy = model"));
	}
	else{
		OC_LOG_V(WARNING, TAG, "Unkown Device Policy = %s, set to POLLING", policy);
		dev_policy = POLICY_POLLING;
		ehRet = OC_EH_ERROR;
	}

	if(ehRet == OC_EH_OK){
		level = cJSON_GetObjectItem(putJson, "RAW_LEVEL")->valueint;
		if(level < 256 && level >= 0){
			OC_LOG_V(INFO, TAG, "Set Policy Level = %d", level);
			dev_level = level;
		}
		else{
			OC_LOG_V(INFO, TAG, "Assertion Fail, Current Policy Level = %d, Requested Level = %d", dev_level, level);
		}
	}

	cJSON_Delete(putJson);

	strcat(payload, responseA);
        if(dht.state == 0)
            strcat(payload, "off");
        else
            strcat(payload, "on");
	strcat(payload, responseB);
	sprintf(value, "%.1f", dht.temp);
	strcat(payload, value);
	strcat(payload, responseC);
	sprintf(value, "%.1f", dht.humid);
	strcat(payload, value);
	strcat(payload, responseD);

	//OC_LOG_V(DEBUG, TAG, "Created Payload = %s", payload);

	if(sizeof(payload) >= 256)
	    ehRet = OC_EH_ERROR;


	return ehRet;
}

OCEntityHandlerResult ProcessGetRequest (OCEntityHandlerRequest *ehRequest, char *payload, OCEntityHandlerResponse *response){
    OCEntityHandlerResult ehRet = OC_EH_OK;
    char value[10] = {0};
    
    strcat(payload, responseA);
    if(dht.state == 0)
        strcat(payload, "off");
    else
        strcat(payload, "on");
    strcat(payload, responseB);
    sprintf(value, "%.1f", dht.temp);
    strcat(payload, value);
    strcat(payload, responseC);
    sprintf(value, "%.1f", dht.humid);
    strcat(payload, value);
    strcat(payload, responseD);
    
   // OC_LOG_V(DEBUG, TAG, "Created Payload = %s", payload);

    if(sizeof(payload) > MAX_RESPONSE_LENGTH)
	ehRet = OC_EH_ERROR;
    
    return ehRet;
}

// This is the entity handler for the registered resource.
// This is invoked by OCStack whenever it recevies a request for this resource.
OCEntityHandlerResult OCEntityHandlerCb(OCEntityHandlerFlag flag, OCEntityHandlerRequest * entityHandlerRequest )
{
    OC_LOG(INFO, TAG, PCF("OCEntityHandlerCb"));
    OCEntityHandlerResult ehRet = OC_EH_OK;

    OCEntityHandlerResponse response = {0};
    char payload[MAX_RESPONSE_LENGTH] = {0};
    char value[10] = {0};
    if(entityHandlerRequest && (flag & OC_REQUEST_FLAG))
    {
        OC_LOG (INFO, TAG, PCF("Flag includes OC_REQUEST_FLAG"));

        if(OC_REST_GET == entityHandlerRequest->method)
        {
	    ehRet = ProcessGetRequest(entityHandlerRequest, payload, &response);
            /*
	    size_t responsePayloadGetLength = strlen(responsePayloadGet);
            if (responsePayloadGetLength < (sizeof(payload) - 1))
            {
                strncpy(payload, responsePayloadGet, responsePayloadGetLength);
            }
            else
            {
                ehRet = OC_EH_ERROR;
            }
	    */
        }
        else if(OC_REST_PUT == entityHandlerRequest->method)
        {
            //Do something with the 'put' payload
	    ehRet = ProcessPutRequest(entityHandlerRequest, payload, &response);
	    /*
            size_t responsePayloadPutLength = strlen(responsePayloadPut);
            if (responsePayloadPutLength < (sizeof(payload) - 1))
            {
                strncpy((char *)payload, responsePayloadPut, responsePayloadPutLength);
            }
            else
            {
                ehRet = OC_EH_ERROR;
            }*/
        }

        if (ehRet == OC_EH_OK)
        {
/*	    strcat(payload, responseA);
	    if(dht.state == 0)
		strcat(payload, "on");
	    else
		strcat(payload, "off");
	    strcat(payload, responseB);
	    sprintf(value, "%.2f", dht.temp);
	    strcat(payload, value);
	    sprintf(payload, responseC);
	    sprintf(value, "%.2f", dht.temp);
	    strcat(payload, value);
	    strcat(payload, responseD);*/
	    OC_LOG_V(DEBUG, TAG, "Created Payload = %s", payload);
            
            // Format the response.  Note this requires some info about the request
            response.requestHandle = entityHandlerRequest->requestHandle;
            response.resourceHandle = entityHandlerRequest->resource;
            response.ehResult = ehRet;
            response.numSendVendorSpecificHeaderOptions = 0;
            memset(response.sendVendorSpecificHeaderOptions, 0, sizeof response.sendVendorSpecificHeaderOptions);
            memset(response.resourceUri, 0, sizeof response.resourceUri);
            // Indicate that response is NOT in a persistent buffer
            response.persistentBufferFlag = 0;

            // Send the response
            if (OCDoResponse(&response) != OC_STACK_OK)
            {
                OC_LOG(ERROR, TAG, "Error sending response");
                ehRet = OC_EH_ERROR;
            }
        }
    }
    if (entityHandlerRequest && (flag & OC_OBSERVE_FLAG))
    {
        if (OC_OBSERVE_REGISTER == entityHandlerRequest->obsInfo.action)
        {
            OC_LOG (INFO, TAG, PCF("Received OC_OBSERVE_REGISTER from client"));
            gDHTUnderObservation = 1;
        }
        else if (OC_OBSERVE_DEREGISTER == entityHandlerRequest->obsInfo.action)
        {
            OC_LOG (INFO, TAG, PCF("Received OC_OBSERVE_DEREGISTER from client"));
        }
    }
    return ehRet;
}

int model_check(void){
//    float threshold = (net_level * 10 / 255);
    float threshold = 5; // 5% diff
    float diff;
    diff = (dht.temp - old.temp) / dht.temp;
    if(diff > threshold)
	return 1;
    diff = (old.temp - dht.temp) / dht.temp;
    if(diff > threshold)
	return 1;
    diff = (dht.humid - old.humid) / dht.humid;
    if(diff > threshold)
	return 1;
    diff = (old.humid - dht.humid) / dht.humid;
    if(diff > threshold)
	return 1;
    return 0;
}

// This method is used to display 'Observe' functionality of OC Stack.
void *ChangeDHTRepresentation(void *param)
{
    (void)param;
    char str[12]={0};
    ReadDHT();
    OCStackResult result = OC_STACK_ERROR;
    sprintf(str, "%d.%d\0", (int)dht_dat[0], (int)dht_dat[1]);
    dht.humid = atof(str);
    sprintf(str, "%d.%d\0", (int)dht_dat[2], (int)dht_dat[3]);
    dht.temp = atof(str);

    OC_LOG_V(INFO, TAG, "Temp = %.1f, Humid = %.1f", dht.temp, dht.humid);

	char payload[256]={0};
	char value[10] = {0};

        strcat(payload, responseA);
        if(dht.state == 0)
            strcat(payload, "on");
        else
            strcat(payload, "off");
        strcat(payload, responseB);
        sprintf(value, "%.1f", dht.temp);
        strcat(payload, value);
        strcat(payload, responseC);
        sprintf(value, "%.1f", dht.humid);
        strcat(payload, value);
        strcat(payload, responseD);

    if(net_policy == POLICY_MODEL && model_check()){
	old.temp = dht.temp;
	old.humid = dht.humid;
    }
    else if(net_policy == POLICY_MODEL){
	return NULL;
    }
    delayed_time = 0;
    if (gDHTUnderObservation)
    {
        OC_LOG_V(INFO, TAG, " =====> Notifying stack of new power level");
        result = OCNotifyAllObservers (dht.handle, OC_NA_QOS);
        if (OC_STACK_NO_OBSERVERS == result)
        {
            gDHTUnderObservation = 0;
        }
    }
    return NULL;
}

char dummy[5]={0};
void setup() {
  // put your setup code here, to run once:
	char ip_str[17] = {0};
	int status = WL_IDLE_STATUS;
        uint16_t port = OC_WELL_KNOWN_PORT;

        OCLogInit();
	
	InitDHT();

	delayed_time = 0;
        
	delay(1000);
	OC_LOG(INFO, TAG, PCF("Start arduino firmware"));
	
	if(ConnectToNetwork() == -1){
		OC_LOG(ERROR, TAG, PCF("Failed to connect network"));
		return;
	}

	IPAddress ip = WiFi.localIP();
	sprintf(ip_str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);	
        if(OCInit(NULL, port, OC_SERVER) != OC_STACK_OK){
                OC_LOG(ERROR, TAG, PCF("OCStack init failed"));
        }

	createDHTResource();
}

// The loop function is called in an endless loop
void loop()
{
    OC_LOG(INFO, TAG, PCF("loop entered"));
    //in_loop_dht();
    // This artificial delay is kept here to avoid endless spinning
    // of Arduino microcontroller. Modify it as per specfic application needs.

    // Give CPU cycles to OCStack to perform send/recv and other OCStack stuff
    if (OCProcess() != OC_STACK_OK)
    {
        OC_LOG(ERROR, TAG, PCF("OCStack process error"));
        return;
    }
    OC_LOG(INFO, TAG, PCF("OCProcess exit"));
    if(net_level * 20 < delayed_time)
	ChangeDHTRepresentation(NULL);
    delay(dev_level * 10);
    delayed_time += dev_level * 10;
}

void createDHTResource()
{
    dht.state = true;
    OCStackResult res = OCCreateResource(&dht.handle,
            "core.provider",
            "oc.mi.def",
            "/a/dht",
            OCEntityHandlerCb,
            OC_DISCOVERABLE|OC_OBSERVABLE);
    OC_LOG_V(INFO, TAG, "Created DHT resource with result: %s", getResult(res));
}

const char *getResult(OCStackResult result) {
    switch (result) {
    case OC_STACK_OK:
        return "OC_STACK_OK";
    case OC_STACK_INVALID_URI:
        return "OC_STACK_INVALID_URI";
    case OC_STACK_INVALID_QUERY:
        return "OC_STACK_INVALID_QUERY";
    case OC_STACK_INVALID_IP:
        return "OC_STACK_INVALID_IP";
    case OC_STACK_INVALID_PORT:
        return "OC_STACK_INVALID_PORT";
    case OC_STACK_INVALID_CALLBACK:
        return "OC_STACK_INVALID_CALLBACK";
    case OC_STACK_INVALID_METHOD:
        return "OC_STACK_INVALID_METHOD";
    case OC_STACK_NO_MEMORY:
        return "OC_STACK_NO_MEMORY";
    case OC_STACK_COMM_ERROR:
        return "OC_STACK_COMM_ERROR";
    case OC_STACK_INVALID_PARAM:
        return "OC_STACK_INVALID_PARAM";
    case OC_STACK_NOTIMPL:
        return "OC_STACK_NOTIMPL";
    case OC_STACK_NO_RESOURCE:
        return "OC_STACK_NO_RESOURCE";
    case OC_STACK_RESOURCE_ERROR:
        return "OC_STACK_RESOURCE_ERROR";
    case OC_STACK_SLOW_RESOURCE:
        return "OC_STACK_SLOW_RESOURCE";
    case OC_STACK_NO_OBSERVERS:
        return "OC_STACK_NO_OBSERVERS";
    case OC_STACK_ERROR:
        return "OC_STACK_ERROR";
    default:
        return "UNKNOWN";
    }
}
