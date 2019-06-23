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
- `maintenance/set`: "reboot" to reboot
- `maintenance/set`: "dsc-stop" to stop dsc keybus interrupts
- `maintenance/set`: "dsc-start" to start dsc keybus interface

The DSC Keybus interrupts seem to interfere with the OTA and Config update operation (when publishing to `homie/device-id/$implementation/config/set`), causing the ESP8266 to reset, and in the case of configuration update, to cause a corruption in the Homie configuration file and put Homie into the initial configuration mode.

So before sending a config update, stop the DSC interrupts by publishing to `maintenance/set`: "dsc-stop". Once the config update has been made, restart the DSC interface using `maintenance/set`: "dsc-start" or sending a reboot request.


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

Before updating Homie config, the DSC interface needs to be deactivated / stopped, because it interferes with writing the configuration file. To do this, publish an MQTT message to
`maintenance/set` `dsc-stop`

After setting the configuration, reactivate the DSC interface by publishing to `maintenance/set` `dsc-start`

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


