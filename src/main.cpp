/*************************************************************
  This is a DEMO. You can use it only for development and testing.
  You should open Setting.h and modify General options.

  If you would like to add these features to your product,
  please contact Blynk for Businesses:

                   http://www.blynk.io/

 *************************************************************/

//#define USE_WROVER_BOARD
#define USE_CUSTOM_BOARD // See "Custom board configuration" in Settings.h
#define APP_DEBUG        // Comment this out to disable debug prints
#define BLYNK_PRINT Serial

//must be on top of blynkprovisoning
#define DONT_RESTART_AFTER_ERROR

#include "BlynkProvisioning.h"
#include <MyCommonBlynk.h>
#include <stdint.h>
#include <driver/rtc_io.h>
#include <esp_deep_sleep.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <rom/rtc.h>

//sleep time between each measurement
#define uS_TO_S_FACTOR 1000000UL //must be UL
#define DEEP_SLEEP_TIME_SEC 3600UL*12UL               //12 hours   
#define WATCH_DOG_TIMEOUT 12UL*uS_TO_S_FACTOR     
#define CONFIG_PIN 32                                //pin to go to config mode
const gpio_num_t LEAK_PIN = gpio_num_t::GPIO_NUM_33; //35 is not touch

#define MSG_LEAK "Leak Detected."
#define MSG_SLEEP "No Leak, sleeping."

//https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/

/**
 * The purpose of this program is to simply demonstrate the use
 * of the Watchdog timer on the ESP32
 * Source: Jack Rickard https://www.youtube.com/watch?v=7kLy2iwIvy8
 */
hw_timer_t *watchdogTimer = NULL;
void gotoSleep()
{
  //avoid grue meditation error panic
  //https://esp32.com/viewtopic.php?t=8675
  Serial.flush();
  Serial.end();  

  //setup wakeup port as rtc gpio
  rtc_gpio_init(LEAK_PIN);
  rtc_gpio_set_direction(LEAK_PIN, rtc_gpio_mode_t::RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(LEAK_PIN);
  esp_sleep_enable_ext0_wakeup(LEAK_PIN, 0); //0 low 1 high

  esp_deep_sleep_start();
}

bool configMode = false;
bool leak_detected = false;
void interruptReboot()
{
  if (configMode)
  {
    return;
  }

  if (leak_detected)
  {
    Serial.println("watchdog timedout, restart to try report leak");
    ESP.restart();
  }
  else
  {
    Serial.println("watchdog timedout, safe to sleep as no leak is detected");
    gotoSleep();
  }
}

void detectLeak()
{
  for (int i = 0; i < 3; ++i)
  {
    if (digitalRead(LEAK_PIN) == LOW)
    {
      leak_detected = true;
    }
    delay(100);
  }
}

void setup()
{
  delay(100);
  esp_bluedroid_disable();
  esp_bt_controller_disable();
  delay(100);
  Serial.begin(115200);

  pinMode(CONFIG_PIN, INPUT_PULLUP);
  configMode = digitalRead(CONFIG_PIN) == LOW;

  rtc_gpio_deinit(LEAK_PIN);
  pinMode(LEAK_PIN, INPUT_PULLUP);

  if (!configMode)
  {
    //enable watch dog timer, to sleep mcu no matter what
    watchdogTimer = timerBegin(0, 80, true);                  //timer 0 divisor 80
    timerAlarmWrite(watchdogTimer, WATCH_DOG_TIMEOUT, false); // set time in uS must be fed within this time or reboot
    timerAttachInterrupt(watchdogTimer, &interruptReboot, true);
    timerAlarmEnable(watchdogTimer); // enable interrupt
  }

  /**
   * check for a leak once here, just to change behaviour of watch dog to restart 
   * it means if there is a leak and board can't connect to blynk, restart the board instead of sleep
   * working great!
  */
  detectLeak();

  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_SEC*uS_TO_S_FACTOR);

  delay(100);
  BlynkProvisioning.begin();
  if (configMode)
  {
    configMode = true;
    BlynkState::set(MODE_WAIT_CONFIG); //blynk.run will take it from there
    Serial.println("Config mode started");
  }
}

void loop()
{

  // This handles the network and cloud connection
  BlynkProvisioning.run();

  if (!Blynk.connected())
  {
    return;
  }

  detectLeak();

  if (leak_detected)
  {
    String leak_msg = getDateAndTime() + " " + MSG_LEAK;
    for (int i = 0; i < 5; ++i)
    {
      Serial.println(leak_msg);
      Blynk.notify(leak_msg);
      Blynk.virtualWrite(V1, "\xE2\x8F\xB3", leak_msg); // Send time to Display Widget
      delay(1000);
    }
  }
  else
  {
    String sleep_msg = getDateAndTime() + " " + MSG_SLEEP;
    Blynk.virtualWrite(V1, sleep_msg); // Send time to Display Widget
    Serial.println(sleep_msg);
    delay(200); // delay to make sure data is sent
  }

  gotoSleep();
}
