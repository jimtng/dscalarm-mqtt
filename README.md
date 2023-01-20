# dscalarm-mqtt

A DSC Alarm bridge to MQTT, intended to run on an ESP8266 or ESP32 device connected to your 
DSC Alarm system's key bus. It allows you to get notifications of alarm events, arm, disarm
and send keypad commands through MQTT over wifi. This enables the integration of your 
DSC alarm system into a home automation platform, primarily written for OpenHAB.

It uses the dscKeybusInterface library to communicate with the DSC alarm, and ESPHome for
the main infrastruture such as OTA, configuration, etc.

For a detailed wiring diagram, please see:
https://github.com/taligentx/dscKeybusInterface


## Features
- MQTT Based
- OTA Updateable (Thanks to ESPHome)
- Updates the DSC Panel's internal clock from NTP
- Tested with OpenHAB, but it should work with other home automation systems.

## ESPHome

Please see https://esphome.io for information on how to build / upload this project.


## MQTT Topics:
All topics are prefixed with `device_name/` which by default is `dsc-alarm` but this can be 
changed in the yaml file.

### MQTT messages from the device

These topics are published by the device.

| Topic                          | Payload                                                                                                                                  |
| ------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `status`                       | `online` or `offline`. This indicates whether esphome is connected to mqtt.                                                              |
| `keybus`                       | `on` when the keybus is connected. `off` otherwise.                                                                                      |
| `trouble`                      | `on` when the panel encountered a problem. `off` otherwise.                                                                              |
| `power-trouble`                | `on` when the panel has lost AC power. `off` otherwise.                                                                                  |
| `battery-trouble`              | `on` when it has problems with the battery. `off` otherwise.                                                                             |
| `keypad/alarm`                 | The device will publish a message to this topic when the corresponding alarm was triggered. Valid values are: `panic`, `aux`, and `fire` |
| `panel-time`                   | The device will publish the current time maintained by the alarm panel in the format of `YYYY-MM-DD HH:MM`                               |
| `partition/N/alarm`            | `on` when the alarm was triggered. `off` otherwise.                                                                                      |
| `partition/N/armed`            | `on` when the alarm is armed. `off` otherwise.                                                                                           |
| `partition/N/armed-stay`       | `on` when the alarm is armed in away mode. `off` otherwise.                                                                              |
| `partition/N/armed-away`       | `on` when the alarm is armed in stay mode. `off` otherwise.                                                                              |
| `partition/N/state`            | The current state of partition `N`: `disarmed`, `exit_delay`, `armed_away`, `armed_stay`, or `triggered`                                 |
| `partition/N/lights/ready`     | `on` or `off` the current status of the ready LED.                                                                                       |
| `partition/N/lights/armed`     | `on` or `off`                                                                                                                            |
| `partition/N/lights/memory`    | `on` or `off`                                                                                                                            |
| `partition/N/lights/bypass`    | `on` or `off`                                                                                                                            |
| `partition/N/lights/trouble`   | `on` or `off`                                                                                                                            |
| `partition/N/lights/program`   | `on` or `off`                                                                                                                            |
| `partition/N/lights/fire`      | `on` or `off`                                                                                                                            |
| `partition/N/lights/backlight` | `on` or `off`                                                                                                                            |

### MQTT commands
Publish an MQTT message to the following topics in order to control the device.

| Topic                   | Payload                                                                                                                                              |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `command`               | `disarm`: disarm all partitions using the configured access code.                                                                                    |
| `command`               | `arm_away`: arm all partitions to away mode                                                                                                          |
| `command`               | `arm_stay`: arm all partitions to stay mode                                                                                                          |
| `command`               | `trigger_fire`: trigger the fire alarm                                                                                                               |
| `command`               | `reboot`: reboot the esp device                                                                                                                      |
| `command`               | `reset-status`: resets the internal status so all values are re-published.                                                                           |
| `write`                 | Sends the given payload directly to the DSC keybus. This can be used to program the alarm, for example.                                              |
| `panel-time/update`     | `now`: instructs the device to update the DSC alarm's internal clock based on the ESP's NTP clock. Normally this is done automatically once a month. |
| `partition/N/armed/set` | `on` to arm partition `N`. `off` to disarm it.                                                                                                       |
| `zone/N/motion`         | `on` when motion is detected. `off` otherwise.                                                                                                       |
| `zone/N/alarm`          | `on` when an alarm is triggered by this zone. `off` otherwise.                                                                                       |

### To arm/disarm a partition, publish to:
- `dsc-alarm/partition/N/armed/set`: `on` to arm in away mode, `off` to disarm

## Library Dependencies
- [dscKeybusReader v3.0](https://github.com/taligentx/dscKeybusInterface.git)

## Tips When Working with the Alarm

- My alarm would go off when I opened my panel enclosure. It uses zone 5 for this detection. 
  In order to stop it from going off, bypass zone 5 using `*1` to enter bypass mode. 
  The zone LEDs will light up for bypassed zones. To toggle the zone bypass, enter the 
  2 digit zone number, e.g. `05`. When zone 5 is lit up, it will be bypassed. 
  Press `#` to return to ready state.

- While working/testing the alarm, disconnect the internal / external speakers, and replace it 
  with a 10K resistor. This will avoid disturbing the neighbours.

## OpenHAB Integration

### MQTT Broker thing
I have a separate mqtt.things to define the broker bridge. It can be used/referenced by mqtt things in other files.

```java
// Adjust the connection settings accordingly
Bridge mqtt:broker:mosquitto [ host="x.x.x.x", secure="false" ]
```

### dscalarm.things

This assumes that you've defined an MQTT bridge thing called `mqtt:broker:mosquito` (as above).

```java
Thing mqtt:topic:dsc "Alarm System" (mqtt:broker:mosquitto) {
    Channels:
        Type contact : trouble "Trouble" [ stateTopic="dsc-alarm/trouble", on="on", off="off" ]
        Type contact : power_trouble "Power Trouble" [ stateTopic="dsc-alarm/power-trouble", on="on", off="off" ]
        Type contact : battery_trouble "Battery Trouble" [ stateTopic="dsc-alarm/battery-trouble", on="on", off="off" ]
        Type string  : keypad_alarm "Keypad Alarm" [ stateTopic="dsc-alarm/keypad/alarm" ]

        Type switch  : partition_1_armed "Alarm Armed" [ stateTopic="dsc-alarm/partition/1/armed", commandTopic="dsc-alarm/partition/1/armed/set" ]

        Type contact : partition_1_alarm "Alarm" [ stateTopic="dsc-alarm/partition/1/alarm", on="on", off="off" ]
        Type string  : partition_1_state "State" [ stateTopic="dsc-alarm/partition/1/state" ]

        Type contact : motionzone_1 "Living Room" [ stateTopic="dsc-alarm/zone/1/motion", on="on", off="off" ]
        Type contact : motionzone_2 "Lounge Room" [ stateTopic="dsc-alarm/zone/2/motion", on="on", off="off" ]
        Type contact : motionzone_3 "Master Bedroom" [ stateTopic="dsc-alarm/zone/3/motion", on="on", off="off" ]
        Type contact : motionzone_4 "Study Room" [ stateTopic="dsc-alarm/zone/4/motion", on="on", off="off" ]
        Type contact : motionzone_5 "Panel Open" [ stateTopic="dsc-alarm/zone/5/motion", on="on", off="off" ]
        Type contact : motionzone_6 "Siren Tampered" [ stateTopic="dsc-alarm/zone/6/motion", on="on", off="off" ]

        Type contact : alarmzone_1 "Living Room Triggered" [ stateTopic="dsc-alarm/zone/1/alarm", on="on", off="off" ]
        Type contact : alarmzone_2 "Lounge Room Triggered" [ stateTopic="dsc-alarm/zone/2/alarm", on="on", off="off" ]
        Type contact : alarmzone_3 "Master Bedroom Triggered" [ stateTopic="dsc-alarm/zone/3/alarm", on="on", off="off" ]
        Type contact : alarmzone_4 "Study Triggered" [ stateTopic="dsc-alarm/zone/4/alarm", on="on", off="off" ]
        Type contact : alarmzone_5 "Panel Open Triggered" [ stateTopic="dsc-alarm/zone/5/alarm", on="on", off="off" ]
        Type contact : alarmzone_6 "Siren Tampered Triggered" [ stateTopic="dsc-alarm/zone/6/alarm", on="on", off="off" ]
}        }        
```

### dscalarm.items
```java
Switch  Alarm_Armed          "Alarm Armed"            {channel="mqtt:topic:dsc:partition_1_armed", autoupdate="false"}
Contact Alarm_Trouble        "Trouble"                {channel="mqtt:topic:dsc:trouble"}
String  Alarm_State          "Alarm State"            {channel="mqtt:topic:dsc:partition_1_state"}
Contact Alarm_Triggered      "Alarm Triggered"        {channel="mqtt:topic:dsc:partition_1_alarm"}

Contact LivingRoom_Motion    "Living Room Motion"     {channel="mqtt:topic:dsc:motionzone_1"}
Contact LoungeRoom_Motion    "Lounge Room Motion"     {channel="mqtt:topic:dsc:motionzone_2"}
Contact MasterBedRoom_Motion "Master Bed Room Motion" {channel="mqtt:topic:dsc:motionzone_3"}
Contact StudyRoom_Motion     "Study Room Motion"      {channel="mqtt:topic:dsc:motionzone_4"}
```

### dcalarm.rules
```
val destinationEmail = "me@example.com"

rule "Alarm Triggered"
when
    Item Alarm_Triggered changed to OPEN
then
    val message = now.toString("yyyy-MM-dd HH:mm:ss") + " The Security Alarm has been triggered!!!";
    sendBroadcastNotification(message)
    sendMail(destinationEmail, message, message)
end
```

## Credits

* Thanks to [@taligentx](https://github.com/taligentx) for the wonderful [dscKeybusinterface library](https://github.com/taligentx/dscKeybusInterface).
* The implementation of the custom component was inspired by https://github.com/Dilbert66/esphome-dsckeybus
