/*
 *  dscKeybusInterface implementation using Homie-esp8266 library.
 * 
 *  Author/Github: https://github.com/jimtng
 *  
 *  dscKeybusInterface: https://github.com/taligentx/dscKeybusInterface
 *  Homie-esp8266 (v2.0.0): https://github.com/homieiot/homie-esp8266 
 *
 *  Processes the security system status and implement the Homie convention.
 *
 *  See README.md
 * 
 *  For details on the wiring, please see:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *
 */

#include <Homie.h>
#include <dscKeybusInterface.h>
#include <time.h>

#define SOFTWARE_VERSION "1.1.0 Build 12"

// Configures the Keybus interface with the specified pins
// dscWritePin is optional, leaving it out disables the virtual keypad.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);

enum ArmType { arm_away, arm_stay };

bool homieNodeInputHandler(const HomieRange& range, const String& property, const String& value);

HomieNode homieNode("alarm", "alarm", "alarm", false, 0, 0, homieNodeInputHandler);
HomieSetting<const char*> dscAccessCode("access-code", "Alarm access code");

unsigned long dscStopTime = 0; // the time (in millis) when the dsc-stop was last requested

void setupHandler() { 
  dsc.begin();
}

void loopHandler() {
  // automatically restart dsc after 5 minutes if it was stopped
  if (dscStopTime > 0 && (millis() - dscStopTime) > 5*60*1000) {
    dsc.begin();
    dscStopTime = 0;
    homieNode.setProperty("message").setRetained(false).send("DSC Interface restarted automatically");
  } 

  dsc.loop();
  if (!dsc.statusChanged) { 
    return;
  }
  dsc.statusChanged = false;                  

  // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
  // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
  if (dsc.bufferOverflow) {
    dsc.bufferOverflow = false;
    homieNode.setProperty("message").setRetained(false).send("Keybus buffer overflow");
  }
  
  // Sends the access code when needed by the panel for arming
  if (dsc.accessCodePrompt) {
    dsc.accessCodePrompt = false;
    dsc.write(dscAccessCode.get());
  }

  if (dsc.troubleChanged) {
    dsc.troubleChanged = false;
    homieNode.setProperty("trouble").send(dsc.trouble ? "1" : "0");
  }

  if (dsc.powerChanged) {
    dsc.powerChanged = false; 
    homieNode.setProperty("power-trouble").send(dsc.powerTrouble ? "1" : "0");
  }

  if (dsc.batteryChanged) {
    dsc.batteryChanged = false;
    homieNode.setProperty("battery-trouble").send(dsc.batteryTrouble ? "1" : "0");
  }

  if (dsc.keypadFireAlarm) {
    dsc.keypadFireAlarm = false; 
    homieNode.setProperty("fire-alarm-keypad").send("1");
  }

  if (dsc.keypadAuxAlarm) {
    dsc.keypadAuxAlarm = false;
    homieNode.setProperty("aux-alarm-keypad").send("1");
  }

  if (dsc.keypadPanicAlarm) {
    dsc.keypadPanicAlarm = false;
    homieNode.setProperty("panic-alarm-keypad").send("1");
  }

  if (dsc.timestampChanged) {
    dsc.timestampChanged = false;
    if (!(dsc.year < 0 || dsc.year > 9999 || dsc.month < 1 || dsc.month > 12 || dsc.day < 1 || dsc.day > 31 || dsc.hour < 0 || dsc.hour > 23 || dsc.minute < 0 || dsc.minute > 59)) {
      char panelTime[17]; // YYYY-MM-DD HH:mm
      sprintf(panelTime, "%04d-%02d-%02d %02d:%02d", dsc.year, dsc.month, dsc.day, dsc.hour, dsc.minute);
      homieNode.setProperty("panel-time").setRetained(false).send(panelTime);
    }
  }

  String property;
  // loop through partitions
  for (byte partition = 0; partition < dscPartitions; partition++) {
    property = "partition-" + String(partition+1);

    // Publish exit delay status
    if (dsc.exitDelayChanged[partition]) {
      dsc.exitDelayChanged[partition] = false;
      homieNode.setProperty(property + "-exit-delay").send(dsc.exitDelay[partition] ? "1" : "0");
    }

    // Publish armed status
    if (dsc.armedChanged[partition]) {
      dsc.armedChanged[partition] = false;
      homieNode.setProperty(property + "-away").send(dsc.armedAway[partition] ? "1" : "0");
      homieNode.setProperty(property + "-stay").send(dsc.armedStay[partition] ? "1" : "0");
    }

    // Publish alarm status
    if (dsc.alarmChanged[partition]) {
      dsc.alarmChanged[partition] = false; 
      homieNode.setProperty(property + "-alarm").send(dsc.alarm[partition] ? "1" : "0");
    }

    // Publish fire alarm status
    if (dsc.fireChanged[partition]) {
      dsc.fireChanged[partition] = false; 
      homieNode.setProperty(property + "-fire").send(dsc.fire[partition] ? "1" : "0");
    }

    // Publish access code
    if (dsc.accessCodeChanged[partition]) {
      dsc.accessCodeChanged[partition] = false;
      homieNode.setProperty(property + "-access-code").setRetained(false).send(String(dsc.accessCode[partition], DEC).c_str());
    }
  }

  // Publish zones 1-64 status
  // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
  //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (dsc.openZonesStatusChanged) {
    dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
    for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual open zone status flag
          property = "openzone-" + String(zoneBit + 1 + (zoneGroup * 8));
          homieNode.setProperty(property).send(bitRead(dsc.openZones[zoneGroup], zoneBit) ? "1" : "0");
        }
      }
      dsc.openZonesChanged[zoneGroup] = 0; // reset the changed flags
    }
  }

  // Publish alarm zones 1-64
  // Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64 zones
  //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (dsc.alarmZonesStatusChanged) {
    dsc.alarmZonesStatusChanged = false;                           // Resets the alarm zones status flag
    for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual alarm zone status flag
          property = "alarmzone-" + String(zoneBit + 1 + (zoneGroup * 8));
          homieNode.setProperty(property).send(bitRead(dsc.alarmZones[zoneGroup], zoneBit) ? "1" : "0");
        }
      }
      dsc.alarmZonesChanged[zoneGroup] = 0;
    }
  }
}

bool onKeypad(const HomieRange& range, const String& command) {
  static char commandBuffer[256];
  if (command.length() >= sizeof(commandBuffer)/sizeof(commandBuffer[0])) {
    homieNode.setProperty("message").setRetained(false).send("The keypad data is too long. Max: " + sizeof(commandBuffer)-1);
  } else {
    strcpy(commandBuffer, command.c_str());
    dsc.write(commandBuffer);  
  }
  return true;
}

// Arm - the partition argument is 1 based
void arm(byte partition, ArmType armType = arm_away) {
  if (!dsc.armed[partition - 1] && !dsc.exitDelay[partition - 1]) {  // Read the argument sent from the homey flow
    dsc.writePartition = partition;
    dsc.write(armType == arm_away ? "w" : "s");  // use the write(char *) version, not the write(char)
  }
}

// Disarm - the partition argument is 1 based
void disarm(byte partition) {
   if ((dsc.armed[partition - 1] || dsc.exitDelay[partition - 1])) {
    dsc.writePartition = partition;         // Sets writes to the partition number
    dsc.write(dscAccessCode.get());
  }
}

// Extracts the digits at the beginning of the given string
// returns -1 if no prefix digits are found, otherwise 
// returns the digits and remove them from the passed argument
// e.g. 1xxx -> return 1, str = "xxx"
byte extractPrefixDigits(String &str) {
  byte prefix = -1;
  unsigned i = 0;
  while (i < str.length() && isdigit(str[i])) i++;
  
  if (i > 0) {
    String digits = str.substring(0, i);
    prefix = digits.toInt();
    str.remove(0, i);
  }
  return prefix;
}

bool homieNodeInputHandler(const HomieRange& range, const String& property, const String& value) {
// bool homieNodeInputHandler(const String& property, const HomieRange& range, const String& value) {
  if (!property.startsWith("partition-")) {
    return false;
  }

  // property should be in this format: "partition-X-yyy"
  String prop = property.substring(10);
  byte partition = extractPrefixDigits(prop); // prop should now contain -away or -stay
  prop.remove(0, 1); // now remove that leading dash

  // validate partition number
  if (partition < 1 || partition > dscPartitions) { // invalid partition number
    homieNode.setProperty("message").setRetained(false).send("Partition number is out of range (1-" + String(dscPartitions) + ")");
    return false;
  }

  if (prop == "away") {
    value == "0" || value.equalsIgnoreCase("off") ? disarm(partition) : arm(partition, arm_away);
    return true;
  } else if (prop == "stay") {
    value == "0" || value.equalsIgnoreCase("off") ? disarm(partition) : arm(partition, arm_stay);
    return true;
  } 
  return false;
}

bool onRefreshStatus(const HomieRange& range, const String& command) {
  dsc.resetStatus();
  homieNode.setProperty("refresh-status").setRetained(false).send("OK");
  return true;
}

// bool onDscActive(const HomieRange& range, const String& command) {
//   if (command == "1") {
//     dsc.begin();
//   } else {
//     dsc.stop();
//   }
//   return true;
// }

bool onMaintenance(const HomieRange& range, const String& command) {
  if (command == "reboot") {
    homieNode.setProperty("message").setRetained(false).send("Rebooting...");
    Homie.reboot();
  } else if (command == "dsc-stop") {
    dsc.stop();
    dscStopTime = millis();
    homieNode.setProperty("message").setRetained(false).send("DSC Interface stopped");
  } else if (command == "dsc-start") {
    if (dscStopTime > 0) {
      dscStopTime = 0;
      dsc.begin();
    }
    homieNode.setProperty("message").setRetained(false).send("DSC Interface started");
  } else {
    homieNode.setProperty("message").setRetained(false).send("Unknown maintenance command");
    return false;
  }
  return true;
}

// parse the provided time and set the dsc panel time
bool onPanelTime(const HomieRange& range, const String& command) {
  if (dsc.keybusConnected) {
    struct tm tm; // note tm_year is year since 1900, and tm_mon is month since January. Add offsets accordingly
    strptime(command.c_str(), "%Y-%m-%d %H:%M", &tm);
    dsc.setTime(1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, dscAccessCode.get());
    homieNode.setProperty("message").setRetained(false).send("OK");
    // homieNode.setProperty("message").setRetained(false).send("OK: " + 
    //   String(tm.tm_year + 1900, DEC) + "-" + String(1+tm.tm_mon, DEC) + "-" + String(tm.tm_mday) + " " +
    //   String(tm.tm_hour, DEC) + ":" + String(tm.tm_min, DEC));
  } else {
    homieNode.setProperty("message").setRetained(false).send("ERROR: DSC Keybus is not connected");
  }
  return true;
}

void onHomieEvent(const HomieEvent& event) {
  switch (event.type) {
    case HomieEventType::MQTT_READY: dsc.resetStatus(); break;
    case HomieEventType::OTA_STARTED: dsc.stop(); break;
    case HomieEventType::OTA_SUCCESSFUL:
    case HomieEventType::OTA_FAILED: dsc.begin(); break;
  }
}

void setup() {
  Serial.begin(115200);

  Homie_setFirmware("dsc-alarm", SOFTWARE_VERSION);
  Homie.disableResetTrigger();
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  Homie.onEvent(onHomieEvent);

  homieNode.advertise("refresh-status").settable(onRefreshStatus);
  homieNode.advertise("maintenance").settable(onMaintenance);
  homieNode.advertise("keypad").settable(onKeypad); // write to dsc alarm
  homieNode.advertise("trouble");
  homieNode.advertise("power-trouble");
  homieNode.advertise("battery-trouble");
  homieNode.advertise("fire-alarm-keypad");
  homieNode.advertise("aux-alarm-keypad");
  homieNode.advertise("panic-alarm-keypad");
  homieNode.advertise("panel-time").settable(onPanelTime);

  for (byte i = 1; i <= dscPartitions; i++) { 
    String digits = "partition-" + String(i) + "-";
    homieNode.advertise(String(digits + "away").c_str()).settable();
    homieNode.advertise(String(digits + "stay").c_str()).settable();
    homieNode.advertise(String(digits + "exit-delay").c_str());
    homieNode.advertise(String(digits + "alarm").c_str());
    homieNode.advertise(String(digits + "fire").c_str());
    // homieNode.advertise(String(digits + "access-code").c_str()); //## BUG: It seems that the mqtt library cannot handle large messages
  }

  Homie.setup();
}

void loop() {
  Homie.loop();
}


