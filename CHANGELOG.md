# Changelog

## [1.2.0] - 2023-04-08
### Added
- Add button repeat support to forms to allow holding directional keys when editing fields.
- Add menu items during game play for adjusting audio settings and ending the current game.

### Changed
- Implement an RNG formula rather than relying on standard library.
- Options screen will maintain the Music and SFX settings from the previous game.
- Change "Sound" label to "SFX".

### Fixed
- Fix some memory leaks.
- Fix performance issues that were preventing a constant 50fps during game play.

## [1.1.0] - 2022-12-11
### Added
- Add options screen to set game seed, starting level, music, and sound.
- Add current seed to board scene.
- Add buttons on game over screen to either replay the same game (incl. seed) or start a new game.

## [1.0.1] - 2022-10-18
### Fixed
- Fix music not playing when restarting after a game over.