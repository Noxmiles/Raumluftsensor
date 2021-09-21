/*
  Bachelorarbeit Alexander Ochs
  Innenraumluft  
*/

#include <MKRWAN.h>
#include <Wire.h>
#include "bsec.h"
#include <CayenneLPP.h>
#include "iAQCoreI2C.h"

// Constants
#define SEALEVELPRESSURE_HPA (1013.25)
Bsec iaqSensor;   // BME680
iAQCoreI2C iaq;   // iAQ Core C
String output;
double max = 80;  // lpp max 80 byte (160 hex digits)
CayenneLPP lpp(max);
uint16_t getDataEveryMin = 5;   //Wartezeit zw Senden von Messwerten in Minuten. Min 1 Minute
const unsigned long delayTime = getDataEveryMin * 60 * 1000;
unsigned long messZeit = 0;

LoRaModem modem;

// LoRaModem modem(Serial1);

#include "arduino_secrets.h"
// Please enter your sensitive data in the Secret tab or arduino_secrets.h
String appEui = SECRET_APP_EUI;
String appKey = SECRET_APP_KEY;







void setup() {
  delay(2000);
  Serial.println(delayTime);

  if( String(SECRET_APP_KEY).substring(0,4) == "25FE" )
    Serial.println("Board Alpha");
  else if ( String(SECRET_APP_KEY).substring(0,4) == "99B3" )
    Serial.println("Board Beta");
  else if ( String(SECRET_APP_KEY).substring(0,4) == "DD8D" )
    Serial.println("Board Gamma");
  else
    Serial.println("SECRET_APP_KEY not (correctly) set! lookup in software.");


  
  // + + + + LoRaWAN Setup begins here + + + +
  
  Serial.begin(9600);
  delay(1000);
  //while (!Serial);  // use ONLY if connected to PC
  if (!modem.begin(EU868)) {
    Serial.println("Failed to start module");
    while (1) {}
  };
  Serial.print("Your module version is: ");
  Serial.println(modem.version());
  Serial.print("Your device EUI is: ");
  Serial.println(modem.deviceEUI());

  int connected = modem.joinOTAA(appEui, appKey);
  if (!connected) {
    Serial.println("Something went wrong; are you indoor? Move near a window and retry");
    while (1) {}
  }

  // Set poll interval to 60 secs.
  modem.minPollInterval(60);
  // NOTE: independently by this setting the modem will
  // not allow to send more than one message every 2 minutes,
  // this is enforced by firmware and can not be changed.

  // DataRate see table below.
  //Serial.println(modem.dataRate(5));





  // + + + + BME680 Setup begins here + + + +
  Wire.begin();

  iaqSensor.begin(0x77, Wire);
  //output = "\nBSEC library version " + String(iaqSensor.version.major) + "." + String(iaqSensor.version.minor) + "." + String(iaqSensor.version.major_bugfix) + "." + String(iaqSensor.version.minor_bugfix);
  Serial.println(output);
  checkIaqSensorStatus();

  bsec_virtual_sensor_t sensorList[10] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  iaqSensor.updateSubscription(sensorList, 10, float(0.333));
  checkIaqSensorStatus();

  // Print the header
  //output = "Timestamp [ms], raw temperature [°C], pressure [hPa], raw relative humidity [%], gas [Ohm], IAQ, IAQ accuracy, temperature [°C], relative humidity [%], Static IAQ, CO2 equivalent, breath VOC equivalent";
  //Serial.println(output);





  // + + + + iAQ Core C Setup beging here + + + +

  if (!iaq.begin())
  {
    Serial.println("begin() failed. Check the Connection to your iAQ-Core!");
  }
}


/*
  DataRate  Modulation  SF  BW  bit/s   range
  0         LoRa        12  125   250   far   // Default for loop
  1         LoRa        11  125   440
  2         LoRa        10  125   980
  3         LoRa        9   125   1'760
  4         LoRa        8   125   3'125
  5         LoRa        7   125   5'470 near  // Default for setup
  6         LoRa        7   250   11'000      // Does not work
*/








void loop()
{
  
  
  // Zeitschleife
  if( (messZeit + delayTime) >= millis())
  {
    //Serial.println("Messzeit: " + String(messZeit) + ", Millis: " + millis() + ", Next: " + (messZeit + delayTime) + ", DelayTime: " + delayTime  );
    iaqSensor.run();
    //Serial.println(iaqSensor.iaqAccuracy);
    delay(3000);
  }
  else
  {
    messZeit = millis();
    Serial.print("Start bei ");
    Serial.println(millis() / 1000);
    
    Serial.println("Starting loop");
    //Serial.println("Messzeit: " + String(messZeit) + ", Millis: " + millis() + ", Next: " + (messZeit + delayTime) + ", DelayTime: " + delayTime  );
    messZeit -= messZeit % delayTime;
    digitalWrite(LED_BUILTIN, HIGH);  // LED an bei Messung
    
    lpp.reset();

    
    
    // + + + + BME 680 loop + + + +
    
    unsigned long time_trigger = millis();
    if (iaqSensor.run()) { // If new data is available
      output = "";
      //output = String(time_trigger);
      //output += ", " + String(iaqSensor.rawTemperature);
      
      output += /*", " +*/ String(iaqSensor.pressure) + "hPa";      // lpp: hPa
      lpp.addBarometricPressure(1, (iaqSensor.pressure / 100));
      
      //output += ", " + String(iaqSensor.rawHumidity);
      
      output += ", " + String(iaqSensor.gasResistance) + "Ω"; // lpp: Ohm
      lpp.addLuminosity(2,  (iaqSensor.gasResistance / 1000));
      
      //output += ", " + String(iaqSensor.iaq);           // best for mobile usse
      
      output += ", " + String(iaqSensor.iaqAccuracy) + " IAQacc";   // lpp (0 = power an, 3 = max accuracy)
      lpp.addDigitalInput(3, (uint8_t)iaqSensor.iaqAccuracy);
      
      output += ", " + String(iaqSensor.temperature) + "C";   // lpp: C
      lpp.addTemperature(4, iaqSensor.temperature);
      
      output += ", " + String(iaqSensor.humidity) + "%";      // lpp: rel %
      lpp.addRelativeHumidity(5, iaqSensor.humidity);
      
      output += ", " + String(iaqSensor.staticIaq) + " sIAQ";     // lpp best for indoor use
      lpp.addLuminosity(6, (uint16_t)iaqSensor.staticIaq);
      
      output += ", " + String(iaqSensor.co2Equivalent) + " eCO2";         // lpp: ppm, indoor only
      lpp.addLuminosity(7, (uint16_t)iaqSensor.co2Equivalent);
      
      output += ", " + String(iaqSensor.breathVocEquivalent) + " eBreath";   // lpp: ppm, indoor only
      lpp.addAnalogInput(8, iaqSensor.breathVocEquivalent);
    
    
    
      lpp.addLuminosity(13, iaqSensor.bme680Status);
      lpp.addLuminosity(14, iaqSensor.status);
    
      
      
      Serial.println(output);
    } else {
      checkIaqSensorStatus();
    }
    
    
    
    
    
    // + + + + iAQ Core C loop + + + +
    
    delay(1000);
    

    
    Serial.print("sensor has valid value: "); Serial.println(iaq.hasValue() ? "true" : "false");
    
    Serial.println(iaq.getStatus());
    lpp.addDigitalInput(12, iaq.getStatus());
    
    if (iaq.isRunin())
      Serial.println("sensor is warm up phase");
    else if (iaq.isBusy())
      Serial.println("sensor is busy");
    else if (iaq.isValid())
    {
      Serial.print("CO2 equivalent: "); Serial.print(iaq.getCO2()); Serial.println("ppm");
      lpp.addLuminosity(9, iaq.getCO2());
      Serial.print("TVOC equivalent: "); Serial.print(iaq.getTVOC()); Serial.println("ppb");
      lpp.addLuminosity(10, iaq.getTVOC());
      Serial.print("Sensor Resistance: "); Serial.print(iaq.getResistance()); Serial.println("Ohm");
      lpp.addLuminosity(11, (iaq.getResistance() / 1000));
    }
    else if (iaq.isError())
      Serial.println("sensor is in error state");
    
    
    
    
    
    // + + + + LoRaWAN loop + + + +
    
    delay(1000);
    
    int err;
    modem.beginPacket();
    //modem.print(msg);
    //modem.print(sensorData);
    modem.write(lpp.getBuffer(), lpp.getSize());
    err = modem.endPacket(true);
    if (err > 0) {
      Serial.println("Message sent correctly!");
    } else {
      Serial.println("Error sending message :(");
      Serial.println("(you may send a limited amount of messages per minute, depending on the signal strength");
      Serial.println("it may vary from 1 message every couple of seconds to 1 message every minute)");
    }
    
    delay(1000);
    
    if (!modem.available()) {
      Serial.println("No downlink message received at this time.");
    Serial.print("Ende bei ");
    Serial.println(millis() / 1000);
      digitalWrite(LED_BUILTIN, LOW);  // LED aus in Pause
      return;
    }
    char rcv[64];
    int i = 0;
    while (modem.available()) {
      rcv[i++] = (char)modem.read();
    }
    Serial.print("Received: ");
    for (unsigned int j = 0; j < i; j++) {
      Serial.print(rcv[j] >> 4, HEX);
      Serial.print(rcv[j] & 0xF, HEX);
      Serial.print(" ");
    }
    Serial.print("\nTo ASCII: ");
    for (unsigned int j = 0; j < i; j++) {
      Serial.print(rcv[j]);
    }
    Serial.println();
    
        Serial.print("Ende bei ");
    Serial.println(millis() / 1000);

    digitalWrite(LED_BUILTIN, LOW);  // LED aus in Pause
    
  }
}




// + + + + Hilfsfunktionen + + + +
void checkIaqSensorStatus(void)
{
  //Serial.println("He");
  if (iaqSensor.status != BSEC_OK) {
    if (iaqSensor.status < BSEC_OK) {
      output = "BSEC error code : " + String(iaqSensor.status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BSEC warning code : " + String(iaqSensor.status);
      Serial.println(output);
    }
  }

  if (iaqSensor.bme680Status != BME680_OK) {
    if (iaqSensor.bme680Status < BME680_OK) {
      output = "BME680 error code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
      for (;;)
        errLeds(); /* Halt in case of failure */
    } else {
      output = "BME680 warning code : " + String(iaqSensor.bme680Status);
      Serial.println(output);
    }
  }
}

void errLeds(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
}
