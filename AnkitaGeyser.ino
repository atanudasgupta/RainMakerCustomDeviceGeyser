//  ESP RainMaker - for Children room Geyer , device name Ankita Geyser
// Nov 2022 - Work pending , match  up code in loop, write_callback, onTimer, stopTimer with masterbedroom Geyser

#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "esp_timer.h"


const char *OTAName = "AnkitaGeyser01" ; // name of OTA service, change it

#define DEFAULT_POWER_MODE false
#define DEFAULT_DURATION_LEVEL 5
const char *service_name = "PROV_1234";
const char *pop = "abcd1234";

 
//GPIO for virtual device
static int gpio_0 = 0;
static int gpio_relay = 4 ; // use the correct GPIO where the relay is attached
 

int remaining_time=DEFAULT_DURATION_LEVEL; // used to track remaining time using timer

bool relay_state = false;  // intial state of relay

Param time_paramR("Remaining", "custom.param.text", value(DEFAULT_DURATION_LEVEL), PROP_FLAG_READ ); 

 
static Device my_device("Ankita Geyser", "custom.device.geyser", &gpio_relay);  // change name of the device here

// timer related variables
int onceonly=0; // just to ensure the timer interrupt does not get called after relay is off.

esp_timer_handle_t timer; // esp soft timer 

void IRAM_ATTR  onTimer();
const esp_timer_create_args_t timerParameters = { .callback = reinterpret_cast<esp_timer_cb_t>( onTimer) };

// using these constants to be able to update the client mobile app as needed outside of the writecall back

const char *remaining ="Remaining"; // variable for time remaining - made global
const char *powerS = "Power";  // // variable for time remaining - made global





void sysProvEvent(arduino_event_t *sys_event)
{
    switch (sys_event->event_id) {      
        case ARDUINO_EVENT_PROV_START:

        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
        printQR(service_name, pop, "ble");
        break;
        default:;
    }
}
void stop_timer() {
        int error = esp_timer_is_active(timer);
        Serial.printf("return code esp_time_is_active= %d, onceonly : %d \n", error,onceonly);
         
        if ( onceonly >1) return; 
        if (esp_timer_is_active(timer)) 
        {
            onceonly++;
            int err1 =esp_timer_stop(timer);
            int err2=esp_timer_delete (timer);
            if (err1==ESP_OK && err2 == ESP_OK)
               Serial.printf("TImer has been stopped: err1: %d : err2: %d\n", err1, err2);
            else  
               Serial.printf("Timer cant be  stopped: err1: %d : err2: %d\n", err1, err2);
        }
}

// timer interrupt for checking every 1 minute
void IRAM_ATTR  onTimer() {

 
  remaining_time--;
  my_device.updateAndReportParam(remaining, remaining_time);
  if ( remaining_time == 0 ) {
    digitalWrite(gpio_relay, LOW);
    my_device.updateAndReportParam(powerS, DEFAULT_POWER_MODE); // required to turn off power on app
     
  }
 
 
  
}

// call back function gets called whenever we modify state in app

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();
   
     if(strcmp(param_name, "Power") == 0) {
        Serial.printf("Received value = %s for %s - %s\n", val.val.b? "true" : "false", device_name, param_name);
        relay_state = val.val.b;
        (relay_state == false) ? digitalWrite(gpio_relay, LOW) : digitalWrite(gpio_relay, HIGH);

        param->updateAndReport(val);
        
          
          if ((relay_state==false) && esp_timer_is_active(timer)) // turn off the timer once the geyser switch is off
         {
             stop_timer();
             Serial.printf("stopped timer from callback\n");
         }
         
 
      
               
    } else if (strcmp(param_name, "Duration") == 0) {
        Serial.printf("\nReceived value = %d for %s - %s\n", val.val.i, device_name, param_name);
        param->updateAndReport(val);
        remaining_time = val.val.i; 
        my_device.updateAndReportParam(remaining, remaining_time); // update remaining time to the value given
    }
}



void setup()
{
    Serial.begin(115200);
    pinMode(gpio_0, INPUT);
    pinMode(gpio_relay, OUTPUT);
    digitalWrite(gpio_relay, DEFAULT_POWER_MODE);

    Node my_node;    
    my_node = RMaker.initNode("ESP RainMaker Geyser1");

    //Create custom dimmer device
    my_device.addNameParam();
    my_device.addPowerParam(DEFAULT_POWER_MODE);
    my_device.assignPrimaryParam(my_device.getParamByName(ESP_RMAKER_DEF_POWER_NAME));

    //Create and add a custom level parameter
    Param level_paramD("Duration", "custom.param.level", value(DEFAULT_DURATION_LEVEL), PROP_FLAG_READ | PROP_FLAG_WRITE);
    level_paramD.addBounds(value(0), value(30), value(1));
    level_paramD.addUIType(ESP_RMAKER_UI_SLIDER);
    my_device.addParam(level_paramD);

    // add another parameter for time remaining

     
     time_paramR.addUIType(ESP_RMAKER_UI_TEXT);
     my_device.addParam(time_paramR);
 
    my_device.addCb(write_callback);  // attach the callback to the device
    
    //Add custom geyser device to the node   
    my_node.addDevice(my_device);

     //If you want to enable scheduling, set time zone for your region using setTimeZone(). 
    //The list of available values are provided here https://rainmaker.espressif.com/docs/time-service.html

    
   // RMaker.setTimeZone("Asia/Kolkata");

     
    // Alternatively, enable the Timezone service and let the phone apps set the appropriate timezone
    RMaker.enableTZService();

    RMaker.enableSchedule();

    RMaker.start();

    WiFi.onEvent(sysProvEvent);

    // provision over BLE

    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);

 
    
    my_device.updateAndReportParam(powerS, DEFAULT_POWER_MODE);   


// Classic OTA

ArduinoOTA.setHostname(OTAName);  

// OTA code added
ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  

  
       
}


void loop()
{
    ArduinoOTA.handle();
    if (relay_state==true && onceonly==0 ) {
      
      // Timer initialize and set for 1 minute wakeups
        esp_timer_create(&timerParameters, &timer);
        esp_timer_start_periodic(timer, 60000000); // 1 minute
        onceonly=1;

      
      }
      if (onceonly == 1 && remaining_time == 0 ) {
        stop_timer();
        onceonly=0;
        remaining_time=DEFAULT_DURATION_LEVEL;

      }
     
        

        // mainly for resetting using gpio_o , normally not used
        
    if(digitalRead(gpio_0) == LOW) { //Push button pressed

        // Key debounce handling
        delay(100);
        int startTime = millis();
        while(digitalRead(gpio_0) == LOW) delay(50);
        int endTime = millis();

        if ((endTime - startTime) > 10000) {  
          // If key pressed for more than 10secs, reset all
          Serial.printf("Reset to factory.\n");
          RMakerFactoryReset(2);
        } else if ((endTime - startTime) > 3000) {
          Serial.printf("Reset Wi-Fi.\n");
          // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
          RMakerWiFiReset(2);
        } else {
          // Toggle device state
          relay_state = !relay_state;
          Serial.printf("Toggle State to %s.\n", relay_state ? "true" : "false");
          my_device.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, relay_state);
          (relay_state == false) ? digitalWrite(gpio_relay, LOW) : digitalWrite(gpio_relay, HIGH);
      }
    }
  
    
    delay(100);
}
