# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]

## [1.0.1] 2019-06-05
### Added
- MQTT Topic `maintenance` - See README.md. Added the ability to stop and start the dsc interface, needed to avoid crashing during config update

### Removed
- MQTT Topic `reboot` has been removed. Its functionality has been incorporated into the new `maintenance` topic

## [1.0.0 Build 110] 2019-06-01
- First Release