Buildbotics CNC Controller Firmware Change Log
==============================================

## v0.3.8
 -

## v0.3.7
 - Allow blocking error dialog for a period of time
 - Show actual error message on planner errors
 - Reset planner on serious error
 - Fixed console clear
 - Added helful info to Video tab
 - Changed "Console" tab to "Messages"
 - Removed spin up/down velocity options, they don't do anything
 - Allow RS485 to work when wires are swapped
 - Allow setting VFD ID
 - Only show relavant spindle config items
 - More robust video camera reset
 - Added help page
 - Allow upgrade with out Internet
 - Limit power fault reporting
 - Added load over temp, load limiting and motor overload power faults

## v0.3.6
 - Set max_usb_current=1 in /boot/config.txt from installer #103

## v0.3.5
 - Fixed dwell (G4)
 - Always show limit switch indicators regardless of motor enable
 - Fixed feed rate display
 - Added current GCode unit display
 - Fixed homed axis zeroing
 - Fixed probe pin input
 - Added reload button to video tab
 - Don't open error dialog on repeat messages
 - Handle large GCode files in browser
 - Added max lookahead limit to planner
 - Fixed GCode stopping/pausing where ramp down needs more than is in the queue
 - Added breakout box diagram to indicators
 - Initialize axes offsets to zero on startup
 - Fixed conflict between ``x`` state variable and ``x`` axis variable
 - Don't show ipv6 addresses on LCD.  They don't fit.

## v0.3.4
 - Added alternate units for motor parameters
 - Automatic config file upgrading
 - Fixed planner/jog sync
 - Fixed planner limits config
 - Accel units mm/min² -> m/min²
 - Search and latch velocity mm/min -> m/min
 - Fixed password update (broken in last version)
 - Start Web server eariler in case of Python coding errors


Change log not maintained in previous versions.  See git commit log.
