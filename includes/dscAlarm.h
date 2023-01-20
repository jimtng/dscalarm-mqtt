/**
 *
 * ESPHome Component to integrate DSC Keybus Interface
 * Home: https://github.com/jimtng/esphome-dscalarm
 *
 * Based on https://github.com/Dilbert66/esphome-dsckeybus, but heavily modified, so it is not
 * compatible with it.
 *
 */

#include "esphome.h"
#include "dscKeybusInterface.h"

class DSCKeybus : public Component {
 public:
  enum KeypadStatus { TROUBLE, POWER_TROUBLE, BATTERY_TROUBLE };
  enum KeypadAlarm { FIRE_ALARM, AUX_ALARM, PANIC_ALARM };
  enum PartitionStatus { DISARMED, ARMED_STAY, ARMED_AWAY, EXIT_DELAY, TRIGGERED };

  enum AlarmCommand {
    ARM_STAY = 's',
    ARM_AWAY = 'w',
    TRIGGER_PANIC_ALARM = 'p',
    TRIGGER_FIRE_ALARM = 'f',
    DISARM = 'd'
  };

  DSCKeybus(byte clockPin, byte readPin, byte writePin, const char *accessCode)
      : dsc(clockPin, readPin, writePin), accessCode(accessCode) {}

  void onKeybusConnectionChange(std::function<void(bool isConnected)> callback) {
    keybusConnectionChangeCallback = callback;
  }

  void onKeypadStatusChange(std::function<void(KeypadStatus keypadStatus, bool isActive)> callback) {
    keypadStatusChangeCallback = callback;
  }

  void onKeypadAlarm(std::function<void(KeypadAlarm alarmType)> callback) { keypadAlarmCallback = callback; }

  void onPanelTimeChange(std::function<void(std::string time)> callback) { panelTimeChangeCallback = callback; }

  void onPartitionAlarmChange(std::function<void(uint8_t partition, bool triggered)> callback) {
    partitionAlarmChangeCallback = callback;
  }

  void onPartitionArmedChange(
      std::function<void(uint8_t partition, bool armed, bool armedStay, bool ArmedAway)> callback) {
    partitionArmedChangeCallback = callback;
  }

  void onPartitionStatusMessage(std::function<void(uint8_t partition, std::string msg)> callback) {
    partitionStatusMessageCallback = callback;
  }

  void onPartitionStatusChange(std::function<void(uint8_t partition, PartitionStatus status)> callback) {
    partitionStatusChangeCallback = callback;
  }

  // lights is a bitmap for:
  // const char* lightNames[] = { "ready", "armed", "memory", "bypass", "trouble", "program", "fire", "backlight" };
  void onLightStatusChange(std::function<void(uint8_t partition, byte lights)> callback) {
    lightStatusChangeCallback = callback;
  }

  void onZoneStatusChange(std::function<void(uint8_t zone, bool isOpen)> callback) {
    zoneStatusChangeCallback = callback;
  }

  void onZoneAlarmChange(std::function<void(uint8_t zone, bool isOpen)> callback) {
    zoneAlarmChangeCallback = callback;
  }

  void onFireStatusChange(std::function<void(uint8_t partition, bool isActive)> callback) {
    fireStatusChangeCallback = callback;
  }

  bool isConnected() { return dsc.keybusConnected; }

  void resetStatus() {
    ESP_LOGI(LOG_PREFIX, "Resetting status");
    dsc.resetStatus();
    dsc.timestampChanged = true;
    for (auto i = 0; i < dscPartitions; i++) {
      previousLights[i] = 0xFF;
      previousStatus[i] = 0xFF;
    }
  }

  void disconnect() {
    ESP_LOGI(LOG_PREFIX, "Disconnecting from keybus");
    dsc.stop();
    dsc.keybusChanged = true;
    dsc.keybusConnected = false;
    dsc.statusChanged = false;
  }

  void connect() {
    ESP_LOGI(LOG_PREFIX, "Connecting to keybus");
    dsc.processModuleData = false;  // Controls if keypad and module data is processed and displayed (default: false)
    dsc.resetStatus();
    dsc.begin();
  }

  void disarm(uint8_t partition = 1, const char *code = nullptr) {
    sendAlarmCommand(partition, AlarmCommand::DISARM, code);
  }
  void armStay(uint8_t partition = 1) { sendAlarmCommand(partition, AlarmCommand::ARM_STAY); }
  void armAway(uint8_t partition = 1) { sendAlarmCommand(partition, AlarmCommand::ARM_AWAY); }
  void triggerFireAlarm(uint8_t partition = 1) { sendAlarmCommand(partition, AlarmCommand::TRIGGER_FIRE_ALARM); }
  void triggerPanicAlarm(uint8_t partition = 1) { sendAlarmCommand(partition, AlarmCommand::TRIGGER_PANIC_ALARM); }
  void write(const char key, uint8_t partition = 1) {
    dsc.writePartition = partition;
    dsc.write(key);
  }
  bool write(std::string keystring, uint8_t partition = 1) {
    static char buffer[50];
    if (!dsc.writeReady) {
      return false;
    }

    auto end = keystring.copy(buffer, sizeof(buffer) - 1);
    buffer[end] = '\0';
    ESP_LOGD(LOG_PREFIX, "Writing keys: %s", buffer);
    dsc.writePartition = partition;
    dsc.write(buffer);
    return true;
  }
  bool writeReady() { return dsc.writeReady; }
  bool setTime(unsigned int year, byte month, byte day, byte hour, byte minute, byte timePartition = 1) {
    return dsc.setTime(year, month, day, hour, minute, accessCode, timePartition);
  }

 private:
  std::function<void(bool)> keybusConnectionChangeCallback;
  std::function<void(KeypadStatus, bool)> keypadStatusChangeCallback;
  std::function<void(KeypadAlarm)> keypadAlarmCallback;

  std::function<void(uint8_t, PartitionStatus)> partitionStatusChangeCallback;
  std::function<void(uint8_t, std::string)> partitionStatusMessageCallback;
  std::function<void(uint8_t, bool)> partitionAlarmChangeCallback;
  std::function<void(uint8_t, bool, bool, bool)> partitionArmedChangeCallback;
  std::function<void(uint8_t, bool)> fireStatusChangeCallback;

  std::function<void(uint8_t, bool)> zoneStatusChangeCallback;
  std::function<void(uint8_t, bool)> zoneAlarmChangeCallback;

  std::function<void(std::string)> panelTimeChangeCallback;
  std::function<void(uint8_t, byte)> lightStatusChangeCallback;

  const char *LOG_PREFIX = "dsc";
  dscKeybusInterface dsc;
  const char *accessCode;
  byte previousStatus[dscPartitions];
  byte previousLights[dscPartitions];

  void notifyKeybusConnectionChange(bool isConnected) {
    if (keybusConnectionChangeCallback) {
      keybusConnectionChangeCallback(isConnected);
    }
  }

  void notifyKeypadStatusChange(KeypadStatus status, bool isActive) {
    if (keypadStatusChangeCallback) {
      keypadStatusChangeCallback(status, isActive);
    }
  }

  void notifyKeypadAlarm(KeypadAlarm alarmType) {
    if (keypadAlarmCallback) {
      keypadAlarmCallback(alarmType);
    }
  }

  void notifyPartitionStatusMessage(uint8_t partition, byte status) {
    if (!partitionStatusMessageCallback) {
      return;
    }
    std::string msg = str_sprintf("%02X: %s", status, String(statusText(status)).c_str());
    partitionStatusMessageCallback(partition + 1, msg);
  }

  void notifyPartitionStatusChange(uint8_t partition, PartitionStatus status) {
    if (partitionStatusChangeCallback) {
      partitionStatusChangeCallback(partition + 1, status);
    }
  }

  void setup() override {
    dsc.resetStatus();
    dsc.begin();
  }

  void sendAlarmCommand(uint8_t partition, AlarmCommand command, const char *code = nullptr) {
    if (partition > dscPartitions) {
      ESP_LOGE(LOG_PREFIX, "Partition number out of range: %d", partition);
      return;
    }
    auto partition_index = partition - 1;
    switch (command) {
      case AlarmCommand::DISARM:
        if (dsc.armed[partition_index] || dsc.exitDelay[partition_index]) {
          dsc.writePartition = partition;
          if (!code) {
            code = this->accessCode;
          }
          dsc.write(code);
        }
        break;

      case AlarmCommand::ARM_STAY:
      case AlarmCommand::ARM_AWAY:
        if (dsc.armed[partition_index] || dsc.exitDelay[partition_index]) {
          break;
        }
        // fall through below
      case AlarmCommand::TRIGGER_FIRE_ALARM:
      case AlarmCommand::TRIGGER_PANIC_ALARM:
        dsc.writePartition = partition;
        dsc.write(command);
        break;
    }
  }

  void loop() override {
    static uint8_t zone;

    dsc.loop();
    if (!dsc.statusChanged) {
      return;
    }
    dsc.statusChanged = false;

    if (dsc.bufferOverflow) {
      ESP_LOGE(LOG_PREFIX, "Keybus buffer overflow");
      dsc.bufferOverflow = false;
    }

    if (dsc.keybusChanged) {
      dsc.keybusChanged = false;
      notifyKeybusConnectionChange(dsc.keybusConnected);
    }

    if (dsc.accessCodePrompt && dsc.writeReady) {
      dsc.accessCodePrompt = false;
      ESP_LOGD(LOG_PREFIX, "got access code prompt");
      dsc.write(accessCode);
    }

    if (dsc.troubleChanged) {
      dsc.troubleChanged = false;
      notifyKeypadStatusChange(KeypadStatus::TROUBLE, dsc.trouble);
    }

    if (dsc.powerChanged) {
      dsc.powerChanged = false;
      notifyKeypadStatusChange(KeypadStatus::POWER_TROUBLE, dsc.powerTrouble);
    }

    if (dsc.batteryChanged) {
      dsc.batteryChanged = false;
      notifyKeypadStatusChange(KeypadStatus::BATTERY_TROUBLE, dsc.batteryTrouble);
    }

    if (dsc.keypadFireAlarm) {
      dsc.keypadFireAlarm = false;
      notifyKeypadAlarm(KeypadAlarm::FIRE_ALARM);
    }

    if (dsc.keypadAuxAlarm) {
      dsc.keypadAuxAlarm = false;
      notifyKeypadAlarm(KeypadAlarm::AUX_ALARM);
    }

    if (dsc.keypadPanicAlarm) {
      dsc.keypadPanicAlarm = false;
      notifyKeypadAlarm(KeypadAlarm::PANIC_ALARM);
    }

    for (byte partition = 0; partition < dscPartitions; partition++) {
      if (dsc.disabled[partition]) {
        continue;
      }

      if (previousStatus[partition] != dsc.status[partition]) {
        previousStatus[partition] = dsc.status[partition];
        notifyPartitionStatusMessage(partition, dsc.status[partition]);
      }

      if (dsc.alarmChanged[partition] && partitionAlarmChangeCallback) {
        dsc.alarmChanged[partition] = false;
        partitionAlarmChangeCallback(partition + 1, dsc.alarm[partition]);
      }

      bool updateDisarmed = false;

      if (dsc.armedChanged[partition]) {
        dsc.armedChanged[partition] = false;
        if (partitionArmedChangeCallback) {
          partitionArmedChangeCallback(partition + 1, dsc.armed[partition], dsc.armedStay[partition],
                                       dsc.armedAway[partition]);
        }

        if (dsc.armed[partition]) {
          auto status = dsc.armedStay[partition] ? PartitionStatus::ARMED_STAY : PartitionStatus::ARMED_AWAY;
          notifyPartitionStatusChange(partition, status);
          dsc.exitDelayChanged[partition] = false;
        }
      }

      if (dsc.exitDelayChanged[partition]) {
        dsc.exitDelayChanged[partition] = false;
        if (dsc.exitDelay[partition]) {
          notifyPartitionStatusChange(partition, PartitionStatus::EXIT_DELAY);
        } else if (!dsc.armed[partition]) {
          updateDisarmed = true;
        }
      }

      if (updateDisarmed && !dsc.armed[partition] && !dsc.exitDelay[partition]) {
        notifyPartitionStatusChange(partition, PartitionStatus::DISARMED);
      }

      if (dsc.fireChanged[partition] && fireStatusChangeCallback) {
        dsc.fireChanged[partition] = false;
        fireStatusChangeCallback(partition + 1, dsc.fire[partition]);
      }

      if (dsc.lights[partition] != previousLights[partition] && lightStatusChangeCallback) {
        previousLights[partition] = dsc.lights[partition];
        lightStatusChangeCallback(partition + 1, dsc.lights[partition]);
      }
    }

    // Publishes zones 1-64 status in a separate topic per zone
    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
    //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.openZonesStatusChanged && zoneStatusChangeCallback) {
      dsc.openZonesStatusChanged = false;
      zone = 0;
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          zone++;
          if (!bitRead(dsc.openZonesChanged[zoneGroup], zoneBit)) {
            continue;
          }
          bool status = bitRead(dsc.openZones[zoneGroup], zoneBit);
          zoneStatusChangeCallback(zone, status);
        }
        dsc.openZonesChanged[zoneGroup] = 0;
      }
    }

    // Zone alarm status is stored in the alarmZones[] and alarmZonesChanged[] arrays using 1 bit per zone, up to 64
    // zones
    //   alarmZones[0] and alarmZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   alarmZones[1] and alarmZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   alarmZones[7] and alarmZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if (dsc.alarmZonesStatusChanged && zoneAlarmChangeCallback) {
      dsc.alarmZonesStatusChanged = false;
      zone = 0;
      for (byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) {
        for (byte zoneBit = 0; zoneBit < 8; zoneBit++) {
          zone++;
          if (bitRead(dsc.alarmZonesChanged[zoneGroup], zoneBit)) {
            bool status = bitRead(dsc.alarmZones[zoneGroup], zoneBit);
            zoneAlarmChangeCallback(zone, status);
          }
        }
        dsc.alarmZonesChanged[zoneGroup] = 0;
      }
    }

    if (dsc.timestampChanged && panelTimeChangeCallback) {
      dsc.timestampChanged = false;
      if (dsc.year >= 0 && dsc.year <= 9999 && dsc.month > 0 && dsc.month < 13 && dsc.day > 0 && dsc.day < 32 &&
          dsc.hour >= 0 && dsc.hour < 24 && dsc.minute >= 0 && dsc.minute < 60) {
        std::string time = str_sprintf("%04d-%02d-%02d %02d:%02d", dsc.year, dsc.month, dsc.day, dsc.hour, dsc.minute);
        panelTimeChangeCallback(time);
      }
    }
  }

  const __FlashStringHelper *statusText(uint8_t statusCode) {
    switch (statusCode) {
      case 0x01:
        return F("Ready");
      case 0x02:
        return F("Stay zones open");
      case 0x03:
        return F("Zones open");
      case 0x04:
        return F("Armed stay");
      case 0x05:
        return F("Armed away");
      case 0x06:
        return F("No entry delay");
      case 0x07:
        return F("Failed to arm");
      case 0x08:
        return F("Exit delay");
      case 0x09:
        return F("No entry delay");
      case 0x0B:
        return F("Quick exit");
      case 0x0C:
        return F("Entry delay");
      case 0x0D:
        return F("Alarm memory");
      case 0x10:
        return F("Keypad lockout");
      case 0x11:
        return F("Alarm");
      case 0x14:
        return F("Auto-arm");
      case 0x15:
        return F("Arm with bypass");
      case 0x16:
        return F("No entry delay");
      case 0x17:
        return F("Power failure");  //??? not sure
      case 0x22:
        return F("Alarm memory");
      case 0x33:
        return F("Busy");
      case 0x3D:
        return F("Disarmed");
      case 0x3E:
        return F("Disarmed");
      case 0x40:
        return F("Keypad blanked");
      case 0x8A:
        return F("Activate zones");
      case 0x8B:
        return F("Quick exit");
      case 0x8E:
        return F("Invalid option");
      case 0x8F:
        return F("Invalid code");
      case 0x9E:
        return F("Enter * code");
      case 0x9F:
        return F("Access code");
      case 0xA0:
        return F("Zone bypass");
      case 0xA1:
        return F("Trouble menu");
      case 0xA2:
        return F("Alarm memory");
      case 0xA3:
        return F("Door chime on");
      case 0xA4:
        return F("Door chime off");
      case 0xA5:
        return F("Master code");
      case 0xA6:
        return F("Access codes");
      case 0xA7:
        return F("Enter new code");
      case 0xA9:
        return F("User function");
      case 0xAA:
        return F("Time and Date");
      case 0xAB:
        return F("Auto-arm time");
      case 0xAC:
        return F("Auto-arm on");
      case 0xAD:
        return F("Auto-arm off");
      case 0xAF:
        return F("System test");
      case 0xB0:
        return F("Enable DLS");
      case 0xB2:
        return F("Command output");
      case 0xB7:
        return F("Installer code");
      case 0xB8:
        return F("Enter * code");
      case 0xB9:
        return F("Zone tamper");
      case 0xBA:
        return F("Zones low batt.");
      case 0xC6:
        return F("Zone fault menu");
      case 0xC8:
        return F("Service required");
      case 0xD0:
        return F("Keypads low batt");
      case 0xD1:
        return F("Wireless low bat");
      case 0xE4:
        return F("Installer menu");
      case 0xE5:
        return F("Keypad slot");
      case 0xE6:
        return F("Input: 2 digits");
      case 0xE7:
        return F("Input: 3 digits");
      case 0xE8:
        return F("Input: 4 digits");
      case 0xEA:
        return F("Code: 2 digits");
      case 0xEB:
        return F("Code: 4 digits");
      case 0xEC:
        return F("Input: 6 digits");
      case 0xED:
        return F("Input: 32 digits");
      case 0xEE:
        return F("Input: option");
      case 0xF0:
        return F("Function key 1");
      case 0xF1:
        return F("Function key 2");
      case 0xF2:
        return F("Function key 3");
      case 0xF3:
        return F("Function key 4");
      case 0xF4:
        return F("Function key 5");
      case 0xF8:
        return F("Keypad program");
      case 0xFF:
        return F("Disabled");
      default:
        return F("Unknown");
    }
  }
};
