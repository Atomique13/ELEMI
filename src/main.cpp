#include <Arduino.h>
#include <lvgl.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "FT6236.h"
#include <Wire.h>
#include <Ticker.h> 
#include <WiFi.h>
#include <WiFiClient.h>
#include <OctoPrintAPI.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
Ticker ticker1;
#define lv_font_montserrat_48
static lv_obj_t *STATE;
static lv_obj_t *TOOL;
static lv_obj_t *BED;
static lv_obj_t *IP;
//octo
const char* ssid = "*********";          // your network SSID (name)
const char* password = "*******";  // your network password

WiFiClient client;



// You only need to set one of the of follwowing:
IPAddress ip(192, 168, 0, **************);                         // Your IP address of your OctoPrint server (inernal or external)
//char* octoprint_host = "octopi";  // Or your hostname. Comment out one or the other.

const int octoprint_httpPort = 80;  //If you are connecting through a router this will work, but you need a random port forwarded to the OctoPrint server from your router. Enter that port here if you are external
String octoprint_apikey = "********"; //See top of file or GIT Readme about getting API key

String printerOperational;
String printerPaused;
String printerPrinting;
String printerReady;
String printerText;
String printerHotend;
String printerTarget;
String payload;

// Use one of the following:
//OctoprintApi api; //Be sure to call init in setup.
OctoprintApi api(client, ip, octoprint_httpPort, octoprint_apikey);               //If using IP address
//OctoprintApi api(client, octoprint_host, octoprint_httpPort, octoprint_apikey);//If using hostname. Comment out one or the other.

unsigned long api_mtbs = 2000; //mean time between api requests (60 seconds)
unsigned long api_lasttime = 0;   //last time api request has been done
//octo

#define BUZZER_PIN 20

const int i2c_touch_addr = TOUCH_I2C_ADD;

//The Squareline Interface Code
#include "ui.h"
#include "ui_helpers.h"

//Pins for the LCD Backlight and TouchScreen
#define LCD_BL 46
#define SDA_FT6236 38
#define SCL_FT6236 39

int sldval;


class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_Parallel16 _bus_instance;
    lgfx::Touch_FT5x06  _touch_instance; 

  public:
    LGFX(void)
   {
       {                                     
           auto cfg = _bus_instance.config();
        cfg.port = 0;
        cfg.freq_write = 80000000;
        cfg.pin_wr = 18;
        cfg.pin_rd = 48;
        cfg.pin_rs = 45;

        cfg.pin_d0 = 47;
        cfg.pin_d1 = 21;
        cfg.pin_d2 = 14;
        cfg.pin_d3 = 13;
        cfg.pin_d4 = 12;
        cfg.pin_d5 = 11;
        cfg.pin_d6 = 10;
        cfg.pin_d7 = 9;
        cfg.pin_d8 = 3;
        cfg.pin_d9 = 8;
        cfg.pin_d10 = 16;
        cfg.pin_d11 = 15;
        cfg.pin_d12 = 7;
        cfg.pin_d13 = 6;
        cfg.pin_d14 = 5;
        cfg.pin_d15 = 4;

           _bus_instance.config(cfg);              
           _panel_instance.setBus(&_bus_instance); 
       }

       {                                       
           auto cfg = _panel_instance.config(); 

           cfg.pin_cs = -1;   
           cfg.pin_rst = -1;  
           cfg.pin_busy = -1; 
           cfg.memory_width = 320;   
           cfg.memory_height = 480; 
           cfg.panel_width = 320;    
           cfg.panel_height = 480;   
           cfg.offset_x = 0;        
           cfg.offset_y = 0;         
           cfg.offset_rotation = 0;  
           cfg.dummy_read_pixel = 8; 
           cfg.dummy_read_bits = 1;  
           cfg.readable = true;      
           cfg.invert = false;      
           cfg.rgb_order = false;    
           cfg.dlen_16bit = true;   
           cfg.bus_shared = true;   

           _panel_instance.config(cfg);
       }
        { 
     auto cfg = _touch_instance.config();

     cfg.x_min      = 0;    
     cfg.x_max      = 319;  
     cfg.y_min      = 0;    
     cfg.y_max      = 479;  
     cfg.pin_int    = -1;   
     cfg.bus_shared = false; 
     cfg.offset_rotation = 0;


     cfg.i2c_port = 1;      
     cfg.i2c_addr = 0x38;   
     cfg.pin_sda  = 38;    
     cfg.pin_scl  = 39;     
     cfg.freq = 400000;    

     _touch_instance.config(cfg);
     _panel_instance.setTouch(&_touch_instance);  
   } 
        
       setPanel(&_panel_instance);
   }
};

LGFX tft;

/*Change to your screen resolution*/
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 480;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 8 ];


/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p )
{
  uint32_t w = ( area->x2 - area->x1 + 1 );
  uint32_t h = ( area->y2 - area->y1 + 1 );

  tft.startWrite();
  tft.setAddrWindow( area->x1, area->y1, w, h );
  tft.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
  tft.endWrite();

  lv_disp_flush_ready( disp );
}


/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    uint16_t touchX, touchY;

    bool touched = tft.getTouch(&touchX, &touchY);

    if(touchX>screenWidth || touchY > screenHeight) {
      Serial.println("Y or y outside of expected parameters..");
    }
    else {
      data->state = touched ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; 
      data->point.x = touchX;
      data->point.y = touchY;
    }

}


void touch_init()
{
  // I2C init
  Wire.begin(SDA_FT6236, SCL_FT6236);
  byte error, address;

  Wire.beginTransmission(i2c_touch_addr);
  error = Wire.endTransmission();

  if (error == 0)
  {
    Serial.print("I2C device found at address 0x");
    Serial.print(i2c_touch_addr, HEX);
    Serial.println("  !");
  }
  else if (error == 4)
  {
    Serial.print("Unknown error at address 0x");
    Serial.println(i2c_touch_addr, HEX);
  }
}

void lvgl_loop(void *parameter)
{

  while (true)
  {
    lv_timer_handler();
    delay(5);
  }
  vTaskDelete(NULL);
}

void guiHandler()
{

  xTaskCreatePinnedToCore(
      // xTaskCreate(
      lvgl_loop,   // Function that should be called
      "LVGL Loop", // Name of the task (for debugging)
      20480,       // Stack size (bytes)
      NULL,        // Parameter to pass
      1,           // Task priority
      // NULL
      NULL, // Task handle
      1);
}

void setup()
{
  Serial.begin(115200); /* prepare for possible serial debug */
  //octo
 delay(10);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  /* Explicitly set the ESP32 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.print(".");
    //ESP.restart();
  }
  ArduinoOTA.setHostname("myesp32");
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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
  //octo
  //Buzzer
  //pinMode(BUZZER_PIN, OUTPUT);
  //ledcSetup(4, 5000, 8);
  //ledcAttachPin(BUZZER_PIN, 4);
  

  tft.begin();          /* TFT init */
  tft.setRotation( 2 ); /* Landscape orientation, flipped */
  tft.setBrightness(128);
  
  delay(500);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  touch_init();
  
  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf, NULL, screenWidth * screenHeight / 8 );

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register( &indev_drv );


  ui_init();
  guiHandler();
  IP = lv_label_create(lv_scr_act());
  lv_label_set_text(IP, WiFi.localIP().toString().c_str());
  lv_obj_align(IP,LV_ALIGN_CENTER, 0,-200 );
  STATE = lv_label_create(lv_scr_act());
  lv_label_set_text(STATE, "STATE");
  lv_obj_align(STATE,LV_ALIGN_CENTER, 70,-150 );
  TOOL = lv_label_create(lv_scr_act());
  lv_label_set_text(TOOL, "TOOL");
  lv_obj_align(TOOL,LV_ALIGN_CENTER, 70, 0);
  BED = lv_label_create(lv_scr_act());
  lv_label_set_text(BED, "BED");
  lv_obj_align(BED,LV_ALIGN_CENTER, 70, 150);
}

void loop()
{
  ArduinoOTA.handle();
  //***************************************************lvgl刷新********************
  lv_timer_handler(); /* let the GUI do its work */
  //ledcWrite(4, 0);//关闭蜂鸣器
  delay(1000);
  //octo
  if (millis() - api_lasttime > api_mtbs || api_lasttime==0) {  //Check if time has expired to go check OctoPrint
    if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
      if(api.getOctoprintVersion()){
        Serial.print("Octoprint API: ");
        Serial.println(api.octoprintVer.octoprintApi);
        Serial.print("Octoprint Server13: ");
        Serial.println(api.octoprintVer.octoprintServer);
      }
      Serial.println();
      if(api.getPrinterStatistics()){
        String ST=api.printerStats.printerState;
        String TO=String(api.printerStats.printerTool0TempActual);
        String BE=String(api.printerStats.printerBedTempActual);
        Serial.print("Printer Current State: ");
        Serial.println(ST);
        lv_label_set_text(STATE, ST.c_str());
        Serial.print("Printer Temp - Tool0 (°C):  ");
        Serial.println(TO);
        lv_label_set_text(TOOL, TO.c_str());
        Serial.print("Printer State - Bed (°C):  ");
        Serial.println(BE);
        lv_label_set_text(BED, BE.c_str());
      }
    }
    api_lasttime = millis();  //Set api_lasttime to current milliseconds run
  }
  //octo
}

