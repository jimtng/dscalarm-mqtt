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
 *  Wiring:
 *      DSC Aux(+) ---+--- esp8266 NodeMCU Vin pin
 *                    |
 *                    +--- 5v voltage regulator --- esp8266 Wemos D1 Mini 5v pin
 *
 *      DSC Aux(-) --- esp8266 Ground
 *
 *                                         +--- dscClockPin (esp8266: D1, D2, D8)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (esp8266: D1, D2, D8)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (esp8266: D1, D2, D8)
 *            Ground --- NPN emitter --/
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904 (EBC)
 *   -- BC547, BC548, BC549
 *
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *
 */

#include <Homie.h>
#include <dscKeybusInterface.h>

#define SOFTWARE_VERSION "1.0.1"

// Configures the Keybus interface with the specified pins
// dscWritePin is optional, leaving it out disables the virtual keypad.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin D2   // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);

enum ArmType { arm_away, arm_stay };

bool homieNodeInputHandler(const String& property, const HomieRange& range, const String& value);

HomieNode homieNode("alarm", "alarm", homieNodeInputHandler);
HomieSetting<const char*> dscAccessCode("access-code", "Alarm access code");

unsigned long dscStopTime = 0; // the time (in millis) when the dsc-stop was last requested

void resetDscStatus() {
  dsc.statusChanged = true;
  dsc.keybusChanged = true;
  dsc.troubleChanged = true;
  dsc.powerChanged = true;
  dsc.batteryChanged = true;
  for (byte partition = 0; partition < dscPartitions; partition++) {
    dsc.armedChanged[partition] = true;
    dsc.alarmChanged[partition] = true;
    dsc.exitDelayChanged[partition] = true;
    dsc.entryDelayChanged[partition] = true;
    dsc.fireChanged[partition] = true;
  }
  dsc.openZonesStatusChanged = true;
  dsc.alarmZonesStatusChanged = true;
  for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
    dsc.openZonesChanged[zoneGroup] = dsc.alarmZonesChanged[zoneGroup] = 0xFF;
  }
}

void dscEnd() {
  timer1_detachInterrupt();
  detachInterrupt(digitalPinToInterrupt(dscClockPin));  
}

void setupHandler() { 
  dsc.begin();
}

void loopHandler() {
  // autmatically restart dsc after 5 minutes if it was stopped
  if (dscStopTime > 0 && (millis() - dscStopTime) > 5*60*1000) {
    dsc.begin();
    dscStopTime = 0;
    homieNode.setProperty("message").setRetained(false).send("DSC Interface restarted automatically");
  } 

  if (!(dsc.handlePanel() && dsc.statusChanged)) { 
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
  if (dsc.accessCodePrompt && dsc.writeReady) {
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
          bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual open zone status flag
          property = "openzone-" + String(zoneBit + 1 + (zoneGroup * 8));
          homieNode.setProperty(property).send(bitRead(dsc.openZones[zoneGroup], zoneBit) ? "1" : "0");
        }
      }
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
          bitWrite(dsc.alarmZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual alarm zone status flag
          property = "alarmzone-" + String(zoneBit + 1 + (zoneGroup * 8));
          homieNode.setProperty(property).send(bitRead(dsc.alarmZones[zoneGroup], zoneBit) ? "1" : "0");
        }
      }
    }
  }
}

bool onKeypad(const HomieRange& range, const String& command) {
     dsc.write(command.c_str());  
     return true;
}

// Arm - the partition argument is 0 based
void arm(byte partition, ArmType armType = arm_away) {
  if (!dsc.armed[partition] && !dsc.exitDelay[partition]) {  // Read the argument sent from the homey flow
    dsc.writePartition = partition + 1;
    dsc.write(armType == arm_away ? "w" : "s");  // use the write(char *) version, not the write(char)
  }
}

// Disarm - the partition argument is 0 based
void disarm(byte partition) {
   if ((dsc.armed[partition] || dsc.exitDelay[partition])) {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write(dscAccessCode.get());
  }
}

// returns -1 if no prefix digit is found in str, otherwise 
// returns the partition prefix (1-8), and remove it from the command string
// e.g. 1xxx -> return 1, str = "xxx"
byte extractPrefixDigits(String &str) {
  byte prefix = -1;
  unsigned i = 0;
  while (i < str.length() && isdigit(str[i])) i++;
  
  if (i > 0) {
    String pName = str.substring(0, i);
    prefix = pName.toInt();
    str.remove(0, i);
  }
  return prefix;
}

bool homieNodeInputHandler(const String& property, const HomieRange& range, const String& value) {
  if (!property.startsWith("partition-")) {
    return false;
  }

  // property should be in this format: "partition-X-yyy"
  String prop = property;
  prop.remove(0, 10); // remove the "partition-" prefix so prop starts with the partition number
  byte partition = extractPrefixDigits(prop) - 1;
  // validate partition number
  if (partition < 0 || partition >= dscPartitions) { // invalid partition number
    homieNode.setProperty("message").setRetained(false).send("Partition number is out of range (1-" + String(dscPartitions) + ")");
    return false;
  }

  prop.remove(0,1); // remove the dash after the digit

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
  resetDscStatus();
  homieNode.setProperty("refresh-status").setRetained(false).send("OK");
  return true;
}

bool onDscActive(const HomieRange& range, const String& command) {
  if (command == "1") {
    dsc.begin();
  } else {
    dscEnd();
  }
  return true;
}

bool onMaintenance(const HomieRange& range, const String& command) {
  if (command == "reboot") {
    homieNode.setProperty("maintenance").setRetained(false).send("Rebooting...");
    Homie.reboot();
  } else if (command == "dsc-stop") {
    dscEnd();
    homieNode.setProperty("maintenance").setRetained(false).send("DSC Interface stopped");
    dscStopTime = millis();
    if (dscStopTime == 0) dscStopTime = 1; // just in case millis() returned 0
  } else if (command == "dsc-start") {
    dscStopTime = 0;
    dsc.begin();
    homieNode.setProperty("maintenance").setRetained(false).send("DSC Interface started");
  } else {
    homieNode.setProperty("maintenance").setRetained(false).send("Unknown maintenance command");
    return false;
  }
  return true;
}

void onHomieEvent(const HomieEvent& event) {
  switch (event.type) {
    case HomieEventType::MQTT_READY: resetDscStatus(); break;
    case HomieEventType::OTA_STARTED: dscEnd(); break;
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

  for (byte i = 1; i <= dscPartitions; i++) {
    String pName = "partition-" + String(i) + "-";
    homieNode.advertise(String(pName + "away").c_str()).settable();
    homieNode.advertise(String(pName + "stay").c_str()).settable();
    homieNode.advertise(String(pName + "exit-delay").c_str());
    homieNode.advertise(String(pName + "alarm").c_str());
    homieNode.advertise(String(pName + "fire").c_str());
  }

  Homie.setup();
}

void loop() {
  Homie.loop();
}


