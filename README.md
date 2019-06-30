# dscalarm-mqtt

A DSC Alarm bridge to MQTT, intended to run on an ESP8266 device connected to your DSC Alarm system's key bus. It allows you to get notifications of alarm events, arm, disarm and send keypad commands through MQTT over wifi. This enables the integration of your DSC alarm system into a home automation platform such as OpenHAB, Home-assistant, etc.

It uses the dscKeybusInterface library to communicate with the DSC alarm, and the Homie convention for MQTT communication.

For a detailed wiring diagram, please see:
https://github.com/taligentx/dscKeybusInterface


## Features
- MQTT Based
- OTA Updateable (Thanks to Homie)
- Configurable Wifi, MQTT, device id, and Alarm's access code. See the Setup Instructions below
- Tested with OpenHAB, but it should work with other home automation systems. Let me know if you're using it with other home automation systems so I can update this document.

## Usage Examples
- To arm partition 1 to away mode, publish to `homie/device-id/partition-1/away/set`: `1`
- To disarm it, publish to `homie/device-id/partition-1/away/set`: `0`
- To monitor motions on zone 1 (regardless of armed status), subscribe to `homie/device-id/alarm/openzone-1`
- To know partition 1 alarm has been triggered, subscribe to `homie/device-id/partition-1/alarm`
- When zone 3 triggered an alarm `homie/device-id/alarm/alarmzone-3` will be published with a value of `1`
- To know whether partition 1 is armed, subscribe to `homie/device-id/partition-1/away` for away mode, or `homie/device-id/partition-1/stay` for stay mode

## Homie
The Homie convention specifies the following syntax for MQTT topics:
`homie/device-id/nodename/xxx`

- The `device-id` can be set in Homie configuration - see **Initial Setup** below.

## Homie Nodes
- `alarm` for the main alarm functionalities
- `partition-N` for partition related status/commands

So for `alarm` node, the full MQTT topics will start with `homie/device-id/alarm/`, and for the `partition-N` the full MQTT topics will be `homie/device-id/partition-N/`

## MQTT Topics:

### General MQTT topics:
- `homie/device-id/alarm/trouble` this corresponds to the "Trouble" light / status of the alarm
- `homie/device-id/alarm/power-trouble`
- `homie/device-id/alarm/battery-trouble`
- `homie/device-id/alarm/fire-alarm-keypad` 
- `homie/device-id/alarm/aux-alarm-keypad`
- `homie/device-id/alarm/panic-alarm-keypad`
- `homie/device-id/alarm/panel-time` provides the date/time stored in the alarm system formatted as YYYY-MM-DD HH:mm

### Partitions:
Each partition is implemented as a homie node, so the base MQTT topic for partition 1 is `homie/device-id/partition-N/`
- `homie/device-id/partition-1/away`:  0 (disarmed) | 1 (armed away)
- `homie/device-id/partition-1/stay`:  0 (disarmed) | 1 (armed stay)
- `homie/device-id/partition-1/alarm`: 0 (no alarm) | 1 (the alarm has been triggered)
- `homie/device-id/partition-1/entry-delay`: 0 | 1 (the system is in entry-delay state)
- `homie/device-id/partition-1/exit-delay`: 0 | 1 (the system is in exit-delay state)
- `homie/device-id/partition-1/fire`:  0 (no alarm) | 1 (fire alarm)
- `homie/device-id/partition-1/access-code`: The access code used to arm/disarm
- `homie/device-id/partition-1/lights`: Status lights in JSON { "ready": "ON|OFF", "armed": "ON|OFF", "memory": "ON|OFF", "bypass": "ON|OFF", "trouble": "ON|OFF", "program": "ON|OFF", "fire": "ON|OFF", "backlight": "ON|OFF"}
- ...
- `homie/device-id/partition-N/xxxx` as above

### To arm/disarm a partition, publish to:
- `homie/device-id/partition-N/away/set`: 1 | on | arm = arm partition N - away mode. 0 | off | disarm = disarm
- `homie/device-id/partition-N/stay/set`: 1 | on | arm = arm partition N - stay mode. 0 | off | disarm = disarm

### Zones:
#### General Motion Detection
These will be published whenever motion is detected within the zone, regardless of the armed state:
- `homie/device-id/alarm/openzone-1`: 0 (no movement) | 1 (movement detected)
- ...
- `homie/device-id/alarm/openzone-N`: as above

#### Triggered Alarm Zones
These will be published when the alarm was triggered by zone motion detection during the armed state:
- `homie/device-id/alarm/alarmzone-1`: 0|1
- ...
- `homie/device-id/alarm/alarmzone-N`: as above

## Commands
### To request status refresh, publish to
- `homie/device-id/alarm/refresh-status/set`: 1

### To set the panel date/time, publish to
- `homie/device-id/alarm/panel-time/set`: "YYYY-MM-DD HH:mm"

### To reboot the device, publish to
- `homie/device-id/alarm/maintenance/set`: "reboot" to reboot
- `homie/device-id/alarm/maintenance/set`: "dsc-stop" to stop dsc keybus interrupts
- `homie/device-id/alarm/maintenance/set`: "dsc-start" to start dsc keybus interface

The DSC Keybus interrupts seem to interfere with the OTA and Config update operation (when publishing to `homie/device-id/$implementation/config/set`), causing the ESP8266 to reset, and in the case of configuration update, to cause a corruption in the Homie configuration file and put Homie into the initial configuration mode.

So before sending a config update, stop the DSC interrupts by publishing to `homie/device-id/alarm/maintenance/set`: "dsc-stop". Once the config update has been made, restart the DSC interface using `homie/device-id/alarm/maintenance/set`: "dsc-start" or sending a reboot request.

## Initial Setup
Homie needs to be configured before it can connect to your Wifi / MQTT server. 
Standard Homie configuration methods are of course supported. However, the easiest is to follow these steps:

- Create a file called config.json, replacing the values accordingly
```
{
    "wifi": {
        "ssid":"Your-Wifi-SSID",
        "password":"WifiKey"
    },
    "mqtt":{
        "host":"YOUR-MQTT-SERVER",
        "port":1883,
        "auth":false,
        "username":"",
        "password":""
    },
    "name":"DSC Alarm",
    "ota":{"enabled":true},
    "device_id":"dsc-alarm",
    "settings":{"access-code":"1234"}
}
```
- Connect to **"Homie-xxxxx"** access point
- Type:
```
curl -X PUT http://192.168.123.1/config --header "Content-Type: application/json" -d @config.json
```

## Updating The Stored Alarm Access Code

Before updating Homie config, the DSC interface needs to be deactivated / stopped, because it interferes with writing the configuration file. To do this, publish an MQTT message to
`homie/device-id/alarm/maintenance/set` `dsc-stop`

After setting the configuration, reactivate the DSC interface by publishing to `homie/device-id/alarm/maintenance/set` `dsc-start`

To change/update your access code that's stored on the device once it's operational (i.e. connected to your MQTT server), publish to
`homie/device-id/$implementation/config/set {"settings":{"access-code":"1234"}}`
e.g.
```
mosquitto_pub -t 'homie/device-id/alarm/maintenance/set' -m dsc-stop
mosquitto_pub -t 'homie/device-id/$implementation/config/set' -m '{"settings":{"access-code":"1234"}}'
mosquitto_pub -t 'homie/device-id/alarm/maintenance/set' -m dsc-start
```

Note 
- The last step may be unnecessary because the ESP8266 seems to crash and restart, but the config gets updated.
- You can update the wifi / mqtt connection details in the same manner

## Library Dependencies
- [Homie-esp8266 v3.x](https://github.com/homieiot/homie-esp8266.git#develop-v3)
- [dscKeybusReader v1.3](https://github.com/taligentx/dscKeybusInterface.git#develop)

## Tips When Working with the Alarm

- My alarm would go off when I opened my panel enclosure. It uses zone 5 for this detection. In order to stop it from going off, bypass zone 5 using `*1` to enter bypass mode. The zone LEDs will light up for bypassed zones. To toggle the zone bypass, enter the 2 digit zone number, e.g. `05`. When zone 5 is lit up, it will be bypassed. Press `#` to return to ready state.

- While working/testing the alarm, disconnect the internal / external speakers, and replace it with a 10K resistor. This will avoid disturbing the neighbours.
