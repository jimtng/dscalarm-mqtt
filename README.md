# dscalarm-mqtt

A DSC Alarm bridge to MQTT, intended to run on an ESP8266 device connected to your DSC Alarm system's key bus. It allows you to get notifications of alarm events, arm, disarm and send keypad commands through MQTT over wifi. This enables the integration of your DSC alarm system into a home automation platform such as OpenHAB, Home-assistant, etc.

It uses the dscKeybusInterface library to communicate with the DSC alarm, and the Homie convention for MQTT communication.

For a detailed wiring diagram, please see:
https://github.com/taligentx/dscKeybusInterface


## Features:
- MQTT Based
- OTA Updateable (Thanks to Homie)
- Configurable Wifi, MQTT, device id, and Alarm's access code. See the Setup Instructions below
- Tested with OpenHAB, but it should work with other home automation systems


## MQTT Topics:

Base MQTT Topic (prepend this to all the topics below):
```
homie/device-id/alarm/
```

### General MQTT topics:
- `trouble`
- `power-trouble`
- `battery-trouble`
- `fire-alarm-keypad`
- `aux-alarm-keypad`
- `panic-alarm-keypad`

### Partitions:
- `partition-1-away`:  0 (disarmed) | 1 (armed away)
- `partition-1-stay`:  0 (disarmed) | 1 (armed stay)
- `partition-1-alarm`: 0 (no alarm) | 1 (triggered)
- `partition-1-fire`:  0 (no alarm) | 1 (fire alarm)
- ...
- `partition-N-xxxx` as above

### Zones:
- `openzone-1`: 0 (no movement) | 1 (movement detected)
- ...
- `openzone-N`: as above

- `alarmzone-1`: 0|1
- ...
- `alarmzone-N`: as above

## Commands
### To request status refresh, publish to
- `refresh-status/set`: 1

### To reboot the device, publish to
- `reboot/set`: 1

### To arm/disarm a partition, publish to:
- `partition-N-away/set`: 0 (disarm) | 1 (arm partition - away mode)
- `partition-N-stay/set`: 0 (disarm) | 1 (arm partition - stay mode)

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
To change/update your access code that's stored on the device once it's operational (i.e. connected to your MQTT server), publish to
`homie/device-id/$implementation/config/set {"settings":{"access-code":"1234"}}`
e.g.
```
mosquitto_pub -t 'homie/device-id/$implementation/config/set' -m '{"settings":{"access-code":"1234"}}'
```


