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
#include <ArduinoJson.h>

#define SOFTWARE_VERSION "2.0.0 Build 1"

// Configures the Keybus interface with the specified pins
// dscWritePin is optional, leaving it out disables the virtual keypad.
#define dscClockPin D1  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscReadPin  D2  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
#define dscWritePin D8  // esp8266: D1, D2, D8 (GPIO 5, 4, 15)
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);

enum ArmType { arm_away, arm_stay };

HomieNode alarmNode("alarm", "alarm", "alarm");
HomieNode *partitionNode[dscPartitions];
char partitionNames[dscPartitions][13]; // to store partition names for HomieNode partitions

HomieSetting<const char*> dscAccessCode("access-code", "Alarm access code");

unsigned long dscStopTime = 0; // the time (in millis) when the dsc-stop was last requested

byte previousLights[dscPartitions] /*, TODO previousStatus[dscPartitions] */;
const String bit2str[] = {"OFF", "ON"};
const char* flagMap[] = { "0", "1" };

const String lightsToJson(const byte lights) {
  String output;  
  const size_t capacity = JSON_OBJECT_SIZE(8);
  const char* lightNames[] = { "ready", "armed", "memory", "bypass", "trouble", "program", "fire", "backlight" }; 
#if ARDUINOJSON_VERSION_MAJOR >= 6
  DynamicJsonDocument doc(capacity);
  UNTESTED
#else
  DynamicJsonBuffer jb(capacity); // staticjsonbuffer doesn't seem to work, producing only 6 objects instead of 8
  JsonObject& doc = jb.createObject();
#endif
  for (byte i = 0; i < 8; i++) {
    doc[lightNames[i]] = bit2str[bitRead(lights, i)];
  }
#if ARDUINOJSON_VERSION_MAJOR >= 6
  serializeJson(doc, output);
#else
  doc.printTo(output);
#endif
  return output;
}

void resetStatus() {
  // we don't want to reset the status of zone changes, which is done by dsc.resetStatus()
  dsc.statusChanged = true;
  dsc.keybusChanged = true;
  dsc.troubleChanged = true;
  dsc.powerChanged = true;
  dsc.batteryChanged = true;
  for (byte partition = 0; partition < dscPartitions; partition++) {
    dsc.readyChanged[partition] = true;
    dsc.armedChanged[partition] = true;
    dsc.alarmChanged[partition] = true;
    dsc.fireChanged[partition] = true;
    previousLights[partition] = /* TODO previousStatus[i] = */ 0xFF;
  }
}

void setupHandler() { 
  resetStatus();
  dsc.begin();
}

void loopHandler() {
  // automatically restart dsc after 5 minutes if it was stopped
  if (dscStopTime > 0 && (millis() - dscStopTime) > 5*60*1000) {
    dsc.begin();
    dscStopTime = 0;
    alarmNode.setProperty("message").setRetained(false).send("DSC Interface restarted automatically");
  } 

  dsc.loop();
  if (!dsc.statusChanged || !dsc.keybusConnected) { 
    return;
  }
  dsc.statusChanged = false;                  

  // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
  // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
  if (dsc.bufferOverflow) {
    dsc.bufferOverflow = false;
    alarmNode.setProperty("message").setRetained(false).send("Keybus buffer overflow");
  }
  
  // Sends the access code when needed by the panel for arming
  if (dsc.accessCodePrompt) {
    dsc.accessCodePrompt = false;
    dsc.write(dscAccessCode.get());
  }

  if (dsc.troubleChanged) {
    dsc.troubleChanged = false;
    alarmNode.setProperty("trouble").send(flagMap[dsc.trouble]);
  }

  if (dsc.powerChanged) {
    dsc.powerChanged = false; 
    alarmNode.setProperty("power-trouble").send(flagMap[dsc.powerTrouble]);
  }

  if (dsc.batteryChanged) {
    dsc.batteryChanged = false;
    alarmNode.setProperty("battery-trouble").send(flagMap[dsc.batteryTrouble]);
  }

  if (dsc.keypadFireAlarm) {
    dsc.keypadFireAlarm = false; 
    alarmNode.setProperty("fire-alarm-keypad").send("1");
  }

  if (dsc.keypadAuxAlarm) {
    dsc.keypadAuxAlarm = false;
    alarmNode.setProperty("aux-alarm-keypad").send("1");
  }

  if (dsc.keypadPanicAlarm) {
    dsc.keypadPanicAlarm = false;
    alarmNode.setProperty("panic-alarm-keypad").send("1");
  }

  if (dsc.timestampChanged) {
    dsc.timestampChanged = false;
    if (!(dsc.year < 0 || dsc.year > 9999 || dsc.month < 1 || dsc.month > 12 || dsc.day < 1 || dsc.day > 31 || dsc.hour < 0 || dsc.hour > 23 || dsc.minute < 0 || dsc.minute > 59)) {
      char panelTime[17]; // YYYY-MM-DD HH:mm
      sprintf(panelTime, "%04d-%02d-%02d %02d:%02d", dsc.year, dsc.month, dsc.day, dsc.hour, dsc.minute);
      alarmNode.setProperty("panel-time").setRetained(false).send(panelTime);
    }
  }

  String prefix;
  // loop through partitions
  for (byte partition = 0; partition < dscPartitions; partition++) {
    HomieNode *node = partitionNode[partition];
    if (node == nullptr || dsc.status[partition] == 0xC7 || dsc.status[partition] == 0) { // the partition is disabled https://github.com/taligentx/dscKeybusInterface/issues/99
      continue;
    }

    // Publish exit delay status
    if (dsc.exitDelayChanged[partition]) {
      dsc.exitDelayChanged[partition] = false;
      node->setProperty("exit-delay").send(flagMap[dsc.exitDelay[partition]]);
    }

    // Publish entry delay status
    if (dsc.entryDelayChanged[partition]) {
      dsc.entryDelayChanged[partition] = false;
      node->setProperty("entry-delay").send(flagMap[dsc.entryDelay[partition]]);
    }

    // Publish armed status
    if (dsc.armedChanged[partition]) {
      dsc.armedChanged[partition] = false;
      node->setProperty("away").send(flagMap[dsc.armedAway[partition]]);
      node->setProperty("stay").send(flagMap[dsc.armedStay[partition]]);
    }

    // Publish alarm status
    if (dsc.alarmChanged[partition]) {
      dsc.alarmChanged[partition] = false; 
      node->setProperty("alarm").send(flagMap[dsc.alarm[partition]]);
    }

    // Publish fire alarm status
    if (dsc.fireChanged[partition]) {
      dsc.fireChanged[partition] = false; 
      node->setProperty("fire").send(flagMap[dsc.fire[partition]]);
    }

    // Publish access code
    if (dsc.accessCodeChanged[partition]) {
      dsc.accessCodeChanged[partition] = false;
      String msg;
      switch (dsc.accessCode[partition]) {
        case 33:
        case 34: msg = "duress"; break;
        case 40: msg = "master"; break;
        case 41:
        case 42: msg = "supervisor"; break;
        default: msg = "user"; 
      }
      node->setProperty("access-code").setRetained(false).send(msg);
    }

    // Publish light status as JSON
    if (dsc.lights[partition] != previousLights[partition]) {
      previousLights[partition] = dsc.lights[partition];
      node->setProperty("lights").send(lightsToJson(dsc.lights[partition]));
    }

    // if (dsc.status[partition] != previousStatus[partition]) {
    //   previousStatus[partition] = dsc.status[partition];
    // TODO
    // }
  }

  // Publish zones 1-64 status
  // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
  //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
  //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
  //   ...
  //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
  if (dsc.openZonesStatusChanged) {
    dsc.openZonesStatusChanged = false;    // Reset the open zones status flag
    for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
      for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
        if (bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {  // Checks an individual open zone status flag
          prefix = "openzone-" + String(zoneBit + 1 + (zoneGroup * 8));
          alarmNode.setProperty(prefix).setRetained(false).send(bitRead(dsc.openZones[zoneGroup], zoneBit) ? "1" : "0");
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
          prefix = "alarmzone-" + String(zoneBit + 1 + (zoneGroup * 8));
          alarmNode.setProperty(prefix).send(flagMap[bitRead(dsc.alarmZones[zoneGroup], zoneBit)]);
        }
      }
      dsc.alarmZonesChanged[zoneGroup] = 0;
    }
  }
}

bool onKeypad(const HomieRange& range, const String& command) {
  // we need to have a static buffer because currently dscKeybusInterface dsc.write() 
  // works directly with the passed string in inside its interrupt handler, 
  // so the string given to dsc.write needs to remain in memory
  static char commandBuffer[64]; 
  if (command.length() >= sizeof(commandBuffer)/sizeof(commandBuffer[0])) {
    alarmNode.setProperty("message").setRetained(false).send("The keypad data is too long. Max: " + String(sizeof(commandBuffer)-1));
  } else {
    unsigned long t = millis();
    while (!dsc.writeReady) {
      dsc.loop(); // wait until all pending writes are done
      if (millis() - t > 10000) { // don't wait forever - max 10s
        alarmNode.setProperty("message").setRetained(false).send("ERROR: Timed out");
        return true;
      }
    } 
    strcpy(commandBuffer, command.c_str());
    dsc.write(commandBuffer); 
  }
  return true;
}

// Arm - the partition argument is 1 based
void arm(byte partition, ArmType armType = arm_away) {
  if (!dsc.armed[partition - 1] && !dsc.exitDelay[partition - 1]) {  // Read the argument sent from the homey flow
    dsc.writePartition = partition;
    dsc.write(armType == arm_away ? 'w' : 's'); 
  }
}

// Disarm - the partition argument is 1 based
void disarm(byte partition) {
   if ((dsc.armed[partition - 1] || dsc.exitDelay[partition - 1])) {
    dsc.writePartition = partition;         // Sets writes to the partition number
    dsc.write(dscAccessCode.get());
  }
}

bool onRefreshStatus(const HomieRange& range, const String& command) {
  resetStatus();
  alarmNode.setProperty("refresh-status").setRetained(false).send("OK");
  return true;
}

bool onMaintenance(const HomieRange& range, const String& command) {
  if (command == "reboot") {
    alarmNode.setProperty("message").setRetained(false).send("Rebooting...");
    Homie.reboot();
  } else if (command == "dsc-stop") {
    dsc.stop();
    dscStopTime = millis();
    alarmNode.setProperty("message").setRetained(false).send("DSC Interface stopped");
  } else if (command == "dsc-start") {
    if (dscStopTime > 0) {
      dscStopTime = 0;
      dsc.begin();
    }
    alarmNode.setProperty("message").setRetained(false).send("DSC Interface started");
  } else {
    alarmNode.setProperty("message").setRetained(false).send("Unknown maintenance command");
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
    alarmNode.setProperty("message").setRetained(false).send("OK");
  } else {
    alarmNode.setProperty("message").setRetained(false).send("ERROR: DSC Keybus is not connected");
  }
  return true;
}

void onHomieEvent(const HomieEvent& event) {
  switch (event.type) {
    case HomieEventType::MQTT_READY: resetStatus(); break;
    case HomieEventType::OTA_STARTED: dsc.stop(); break;
    // case HomieEventType::OTA_SUCCESSFUL:
    // case HomieEventType::OTA_FAILED: dsc.begin(); break;
    default: break; // to silence compiler warning
  }
}

void setup() {
  Serial.begin(115200);

  Homie_setFirmware("dsc-alarm", SOFTWARE_VERSION);
  Homie.disableResetTrigger();
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  Homie.onEvent(onHomieEvent);

  alarmNode.advertise("refresh-status").settable(onRefreshStatus);
  alarmNode.advertise("maintenance").settable(onMaintenance);
  alarmNode.advertise("keypad").settable(onKeypad); // write to dsc alarm
  alarmNode.advertise("trouble");
  alarmNode.advertise("power-trouble");
  alarmNode.advertise("battery-trouble");
  alarmNode.advertise("fire-alarm-keypad");
  alarmNode.advertise("aux-alarm-keypad");
  alarmNode.advertise("panic-alarm-keypad");
  alarmNode.advertise("panel-time").settable(onPanelTime);
  alarmNode.advertise("debug").settable([](const HomieRange &range, const String &value) {
    for (byte i = 0; i < dscPartitions; i++) {
      alarmNode.setProperty("debug").setRetained(false).send(String(i) + ": " + String(dsc.status[i], HEX));
    }
    return true;
  });

  dsc.begin();
  for (int i = 0; i < 3*10; i++) { dsc.loop(); delay(100); }; 

  for (byte i = 0; i < dscPartitions; i++) { 
    if (dsc.status[i] == 0 || dsc.status[i] == 0xC7) {
      partitionNode[i] = nullptr;
      Serial << "Partition " + String(i+1) + " inactive" << endl;
      continue;
    }

    char istr[3]; 
    strcpy(partitionNames[i], "partition-");
    if (i < 99) { // avoid the highly unlikely buffer overflow
      strcat(partitionNames[i], itoa(i+1, istr, 10));
    }
    HomieNode *node = new HomieNode(partitionNames[i], partitionNames[i], "partition"); // node id/names must remain allocated as HomieNode makes no copy of it
    partitionNode[i] = node;
    node->advertise("away").settable([i](const HomieRange &range, const String &value) {
      if (value == "1" || value.equalsIgnoreCase("on") || value.equalsIgnoreCase("arm")) {
        arm(i+1, arm_away);
      } else {
        disarm(i+1);
      }
      return true;
    });
    node->advertise("stay").settable([i](const HomieRange &range, const String &value) {
      if (value == "1" || value.equalsIgnoreCase("on") || value.equalsIgnoreCase("arm")) {
        arm(i+1, arm_stay);
      } else {
        disarm(i+1);
      }
      return true;
    });
    node->advertise("alarm");
    node->advertise("exit-delay");
    node->advertise("entry-delay");
    node->advertise("fire");
    node->advertise("access-code");
  }
  dsc.stop(); // we need to stop it before Homie.setup(), and starts/begins again inside Homie's setupHandler().

  Homie.setup();
}

void loop() {
  Homie.loop();
}


