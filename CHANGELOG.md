# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]

## [2.0.0] 2019-06-30
Changed to v2.x due to the breaking change in the partition topics

### Added
- The alarm panel's time is published via MQTT Topic `homie/device-id/alarm/panel-time` and set through `homie/device-id/alarm/panel-time/set`
- MQTT Topic `homie/device-id/partition-N/entry-delay` to indicate that the alarm is in the entry-delay state
- MQTT Topic `homie/device-id/partition-N/access-code` the access code is published whenever the alarm was armed/disarmed
- MQTT Topic `homie/device-id/partition-N/lights` the light status for the partition

### Changed
- Major change: each partition is now implemented as a individual node. 
  MQTT topics `partition-N-xxx` moved from the `alarm` node to individual `partition-N` nodes. Hence the topics will change from `homie/device-id/alarm/partition-N-away` to `homie/device-id/partition-N/away` etc. Note the removal of `alarm` in the topic.
- Only active partitions are announced. If partitions were added/removed from the alarm after boot up, perform a reboot via `maintenance/set`: `reboot`
- Updated library dependency to use Homie-esp8266 v3.0.1 (Homie convention v3)
- Updated library dependency to use dscKeybusInterface v1.3 (develop branch)
- `openzone-N` topics changed from retained to not retained

### Fixed
- Fixed a bug when writing to `keypad` topic

## [1.0.1] 2019-06-05
### Added
- MQTT Topic `maintenance` - See (README.md). Added the ability to stop and start the dsc interface, needed to avoid crashing during config update

### Removed
- MQTT Topic `reboot` has been removed. Its functionality has been incorporated into the new `maintenance` topic

## [1.0.0 Build 110] 2019-06-01
- First Release