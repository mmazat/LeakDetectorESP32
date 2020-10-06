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

#include "BlynkProvisioning.h"
#include <MyCommonBlynk.h>
#include <stdint.h>
#include <esp_deep_sleep.h>
#include <driver/rtc_io.h>
#include <esp_bt_main.h>
#include <esp_bt.h>
#include <esp_wifi.h>

//sleep time between each measurement
#define DEEP_SLEEP_TIME 3600 * 12 * 10e6 //microsec
#define WATCH_DOG_TIMEOUT 12e6           //microsec
#define CONFIG_PIN 32                    //pin to go to config mode
const gpio_num_t wakeup_port = gpio_num_t::GPIO_NUM_33;

#define MSG_LEAK "Leak Detected."
#define MSG_SLEEP "Slept again."

//https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/

/**
 * The purpose of this program is to simply demonstrate the use
 * of the Watchdog timer on the ESP32
 * Source: Jack Rickard https://www.youtube.com/watch?v=7kLy2iwIvy8
 */
hw_timer_t *watchdogTimer = NULL;
void hibernate()
{
  esp_wifi_stop();
  esp_deep_sleep_start();
}
bool configMode = false;
void interruptReboot()
{
  if (configMode)
  {
    return;
  }

  Serial.println("Watchdog timed out!");
  delay(200);
 hibernate();
}


void setup()
{
  esp_bluedroid_disable();
  esp_bt_controller_disable();
  Serial.begin(115200);
  BlynkProvisioning.begin();

  pinMode(CONFIG_PIN, INPUT_PULLUP);

  //ext1 wake up doesn't need prepherials to be on, only RTC
  //esp_sleep_enable_ext1_wakeup(WAKE_UP_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);

  //gpio_pullup_en(GPIO_NUM_34);
  rtc_gpio_init(wakeup_port);
  rtc_gpio_set_direction(wakeup_port, rtc_gpio_mode_t::RTC_GPIO_MODE_INPUT_ONLY);
  //rtc_gpio_pullup_en(wakeup_port);
  esp_sleep_enable_ext0_wakeup(wakeup_port, 1); //0 low 1 high

  //enable watch dog timer, to sleep mcu no matter what
  watchdogTimer = timerBegin(0, 80, true);                  //timer 0 divisor 80
  timerAlarmWrite(watchdogTimer, WATCH_DOG_TIMEOUT, false); // set time in uS must be fed within this time or reboot
  timerAttachInterrupt(watchdogTimer, &interruptReboot, true);
  timerAlarmEnable(watchdogTimer); // enable interrupt

  //By default, esp_deep_sleep_start function will power down all RTC power domains which are not needed by the enabled wakeup sources
  // esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_MAX, ESP_PD_OPTION_OFF);  
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME);
  delay(100);
}

void loop()
{
  if (!configMode)
  {
    if (digitalRead(CONFIG_PIN) == LOW)
    {
      configMode = true;
      BlynkState::set(MODE_WAIT_CONFIG); //blynk.run will take it from there
      Serial.println("Config mode started");
    }
  }

  // This handles the network and cloud connection
  BlynkProvisioning.run();

  if (!Blynk.connected())
  {
    return;
  }

  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  bool wakeup_by_leak = false;
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    wakeup_by_leak = true;
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    wakeup_by_leak = true;
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }

  if (wakeup_by_leak)
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

hibernate();
}
