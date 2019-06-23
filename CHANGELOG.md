# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- MQTT Topic `panel-time` to get/set the alarm panel time
- MQTT Topic `partition-N-access-code` the access code is published whenever the alarm was armed/disarmed

### Changed
- Updated library dependency to use Homie-esp8266 v3.0.1
- Updated library dependency to use dscKeybusInterface v1.3 (develop branch)

### Fixed
- Writing to `keypad` topic no longer causes an unexpected behaviour due to a bug in the dscKeybusInterface library.

## [1.0.1] 2019-06-05
### Added
- MQTT Topic `maintenance` - See (README.md). Added the ability to stop and start the dsc interface, needed to avoid crashing during config update

### Removed
- MQTT Topic `reboot` has been removed. Its functionality has been incorporated into the new `maintenance` topic

## [1.0.0 Build 110] 2019-06-01
- First Release