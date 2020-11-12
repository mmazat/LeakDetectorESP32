/*************************************************************
  This is a DEMO. You can use it only for development and testing.
  You should open Setting.h and modify General options.

  If you would like to add these features to your product,
  please contact Blynk for Businesses:

                   http://www.blynk.io/

 *************************************************************/

//#define USE_WROVER_BOARD
#define USE_ESP32S_BARE // See "Custom board configuration" in Settings.h
#define APP_DEBUG       // Comment this out to disable debug prints
#define BLYNK_PRINT Serial

/** this flag must be on top of blynkprovisoning
* if blynk fails to connect, it restarts the board
* which can be endless and drain the battery
*/
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
#include "driver/adc.h"

//sleep time between each measurement
#define uS_TO_S_FACTOR 1000000UL         //must be UL
#define DEEP_SLEEP_TIME_SEC 3600UL * 1UL //1
#define WATCH_DOG_TIMEOUT 12UL * uS_TO_S_FACTOR
#define CONFIG_PIN 32 //pin to go to config mode when is low
const gpio_num_t LEAK_PIN = gpio_num_t::GPIO_NUM_33;

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

  //uint64_t mask = 0;
  //mask |= 1 << LEAK_PIN_ID;
  //rtc_gpio_isolate(LEAK_PIN);
  //esp_deep_sleep_enable_ext1_wakeup(mask,ESP_EXT1_WAKEUP_ANY_HIGH);

  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  //if peripherial disabled, the pullup resistors won't work
  //esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

  /**significantly reduced the deep sleep power consumption
 * went from 1mA to 5uA after applying this command
 */
  adc_power_off();

  //sweet dreams
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
  Serial.println("Checking for Leak");
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

  Serial.begin(115200);
  Serial.println("Booted");

  esp_bluedroid_disable();
  esp_bt_controller_disable();

  delay(100);
  Serial.println("Disabled unused devices");

  pinMode(CONFIG_PIN, INPUT_PULLUP);
  configMode = digitalRead(CONFIG_PIN) == LOW;
  Serial.println("Checked for config mode");

  rtc_gpio_deinit(LEAK_PIN);
  pinMode(LEAK_PIN, INPUT_PULLUP);

  if (!configMode)
  {
    Serial.println("Not in config mode, start watch dog");
    //enable watch dog timer, to sleep mcu no matter what
    watchdogTimer = timerBegin(0, 80, true);                  //timer 0 divisor 80
    timerAlarmWrite(watchdogTimer, WATCH_DOG_TIMEOUT, false); // set time in uS must be fed within this time or reboot
    timerAttachInterrupt(watchdogTimer, &interruptReboot, true);
    timerAlarmEnable(watchdogTimer); // enable interrupt
  }
  else
  {
    Serial.println("Config mode started");
  }

  /**
   * check for a leak once here, just to change behaviour of watch dog to restart 
   * it means if there is a leak and board can't connect to blynk, restart the board instead of sleep
   * working great!
  */
  detectLeak();

  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_SEC * uS_TO_S_FACTOR);

  delay(300);
  Serial.println("Starting blink");
  BlynkProvisioning.begin();
  if (configMode)
  {
    configMode = true;
    BlynkState::set(MODE_WAIT_CONFIG); //blynk.run will take it from there
    Serial.println("Config mode started");
  }

  Serial.println("finish setup");
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
