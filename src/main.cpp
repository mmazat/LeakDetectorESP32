/*************************************************************
  This is a DEMO. You can use it only for development and testing.
  You should open Setting.h and modify General options.

  If you would like to add these features to your product,
  please contact Blynk for Businesses:

                   http://www.blynk.io/

 *************************************************************/

//#define USE_WROVER_BOARD
#define USE_CUSTOM_BOARD          // See "Custom board configuration" in Settings.h

#define APP_DEBUG        // Comment this out to disable debug prints

#define BLYNK_PRINT Serial

#include "BlynkProvisioning.h"
#include <MyCommonBlynk.h>

//sleep time between each measurement
#define DEEP_SLEEP_TIME 300e6    //microsec  
#define INPUT_PIN 3
#define MSG_LEAK "Leak Detected."
#define MSG_NO_LEAK "No Leak Detected."
#define MSG_SLEEP "In Deep Sleep."

void setup() {
  delay(500);
  Serial.begin(115200);

  BlynkProvisioning.begin();
  pinMode(INPUT_PIN, INPUT_PULLUP);

}

void loop() {
  // This handles the network and cloud connection
  BlynkProvisioning.run();

  if (!Blynk.connected()){return;}

  char leak_detect = 0;
  Blynk.virtualWrite(V1, "\xE2\x8F\xB3", "Checking For Leak ...");
  delay(500);


  for (int i = 0; i < 2; ++i)
  {
    if (digitalRead(INPUT_PIN) == LOW) {
      Serial.println(MSG_LEAK);
      Blynk.notify(MSG_LEAK);

      Blynk.virtualWrite(V1, "\xE2\x8F\xB3", MSG_LEAK);  // Send time to Display Widget
      delay(500);

      //clear the display
      Blynk.virtualWrite(V1, "\xE2\x8F\xB3", "!!!!!!!!!!!!!!");  // Send time to Display Widget

      leak_detect = 1;
    }

    delay(1000);
  }

  if (!leak_detect)
  {
    Blynk.virtualWrite(V1, "\xE2\x8F\xB3", MSG_NO_LEAK);  // Send time to Display Widget
    Serial.println(MSG_NO_LEAK);
    delay(1000); //delay for user to see the message

    String sleep_msg = getDateAndTime() + " " + MSG_SLEEP;
    Blynk.virtualWrite(V1, sleep_msg);  // Send time to Display Widget
    Serial.println(sleep_msg);
    delay(200);// delay to make sure data is sent

    ESP.deepSleep(DEEP_SLEEP_TIME);
  }


}

