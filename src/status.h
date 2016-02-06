/******************************************************************************\

                   This file is part of the TinyG firmware.

                     Copyright (c) 2016, Buildbotics LLC
                             All rights reserved.

        The C! library is free software: you can redistribute it and/or
        modify it under the terms of the GNU Lesser General Public License
        as published by the Free Software Foundation, either version 2.1 of
        the License, or (at your option) any later version.

        The C! library is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
        Lesser General Public License for more details.

        You should have received a copy of the GNU Lesser General Public
        License along with the C! library.  If not, see
        <http://www.gnu.org/licenses/>.

        In addition, BSD licensing may be granted on a case by case basis
        by written permission from at least one of the copyright holders.
        You may request written permission by emailing the authors.

                For information regarding this software email:
                               Joseph Coffland
                            joseph@buildbotics.com

\******************************************************************************/

#ifndef STATUS_H
#define STATUS_H

/************************************************************************************
 * STATUS CODES
 *
 * Status codes are divided into ranges for clarity and extensibility. At some point
 * this may break down and the whole thing will get messy(er), but it's advised not
 * to change the values of existing status codes once they are in distribution.
 *
 * Ranges are:
 *
 *   0 - 19      OS, communications and low-level status
 *
 *  20 - 99      Generic internal and application errors. Internal errors start at 20 and work up,
 *               Assertion failures start at 99 and work down.
 *
 * 100 - 129    Generic data and input errors - not specific to Gcode or TinyG
 *
 * 130 -        Gcode and TinyG application errors and warnings
 *
 * See main.c for associated message strings. Any changes to the codes may also require
 * changing the message strings and string array in main.c
 *
 * Most of the status codes (except STAT_OK) below are errors which would fail the command,
 * and are returned by the failed command and reported back via JSON or text.
 * Some status codes are warnings do not fail the command. These can be used to generate
 * an exception report. These are labeled as WARNING
 */

#include <stdint.h>

typedef uint8_t stat_t;
extern stat_t status_code;

void print_status_message(const char *msg, stat_t status);

// ritorno is a handy way to provide exception returns
// It returns only if an error occurred. (ritorno is Italian for return)
#define ritorno(a) if ((status_code = a) != STAT_OK) {return status_code;}

// OS, communications and low-level status
#define STAT_OK 0                        // function completed OK
#define STAT_ERROR 1                     // generic error return EPERM
#define STAT_EAGAIN 2                    // function would block here (call again)
#define STAT_NOOP 3                      // function had no-operation
#define STAT_COMPLETE 4                  // operation is complete
#define STAT_TERMINATE 5                 // operation terminated (gracefully)
#define STAT_RESET 6                     // operation was hard reset (sig kill)
#define STAT_EOL 7                       // function returned end-of-line
#define STAT_EOF 8                       // function returned end-of-file
#define STAT_FILE_NOT_OPEN 9
#define STAT_FILE_SIZE_EXCEEDED 10
#define STAT_NO_SUCH_DEVICE 11
#define STAT_BUFFER_EMPTY 12
#define STAT_BUFFER_FULL 13
#define STAT_BUFFER_FULL_FATAL 14
#define STAT_INITIALIZING 15              // initializing - not ready for use
#define STAT_ENTERING_BOOT_LOADER 16      // this code actually emitted from boot loader, not TinyG
#define STAT_FUNCTION_IS_STUBBED 17

// Internal errors and startup messages
#define STAT_INTERNAL_ERROR 20            // unrecoverable internal error
#define STAT_INTERNAL_RANGE_ERROR 21      // number range other than by user input
#define STAT_FLOATING_POINT_ERROR 22      // number conversion error
#define STAT_DIVIDE_BY_ZERO 23
#define STAT_INVALID_ADDRESS 24
#define STAT_READ_ONLY_ADDRESS 25
#define STAT_INIT_FAIL 26
#define STAT_ALARMED 27
#define STAT_FAILED_TO_GET_PLANNER_BUFFER 28
#define STAT_GENERIC_EXCEPTION_REPORT 29    // used for test

#define STAT_PREP_LINE_MOVE_TIME_IS_INFINITE 30
#define STAT_PREP_LINE_MOVE_TIME_IS_NAN 31
#define STAT_FLOAT_IS_INFINITE 32
#define STAT_FLOAT_IS_NAN 33
#define STAT_PERSISTENCE_ERROR 34
#define STAT_BAD_STATUS_REPORT_SETTING 35

// Assertion failures - build down from 99 until they meet the system internal errors
#define STAT_CONFIG_ASSERTION_FAILURE 90
#define STAT_ENCODER_ASSERTION_FAILURE 92
#define STAT_STEPPER_ASSERTION_FAILURE 93
#define STAT_PLANNER_ASSERTION_FAILURE 94
#define STAT_CANONICAL_MACHINE_ASSERTION_FAILURE 95
#define STAT_CONTROLLER_ASSERTION_FAILURE 96
#define STAT_STACK_OVERFLOW 97
#define STAT_MEMORY_FAULT 98                     // generic memory corruption
#define STAT_GENERIC_ASSERTION_FAILURE 99        // generic assertion failure - unclassified

// Application and data input errors

// Generic data input errors
#define STAT_UNRECOGNIZED_NAME 100              // parser didn't recognize the name
#define STAT_INVALID_OR_MALFORMED_COMMAND 101   // malformed line to parser
#define STAT_BAD_NUMBER_FORMAT 102              // number format error
#define STAT_UNSUPPORTED_TYPE 103               // An otherwise valid number or JSON type is not supported
#define STAT_PARAMETER_IS_READ_ONLY 104         // input error: parameter cannot be set
#define STAT_PARAMETER_CANNOT_BE_READ 105       // input error: parameter cannot be set
#define STAT_COMMAND_NOT_ACCEPTED 106           // command cannot be accepted at this time
#define STAT_INPUT_EXCEEDS_MAX_LENGTH 107       // input string is too long
#define STAT_INPUT_LESS_THAN_MIN_VALUE 108      // input error: value is under minimum
#define STAT_INPUT_EXCEEDS_MAX_VALUE 109        // input error: value is over maximum

#define STAT_INPUT_VALUE_RANGE_ERROR 110        // input error: value is out-of-range
#define STAT_JSON_SYNTAX_ERROR 111              // JSON input string is not well formed
#define STAT_JSON_TOO_MANY_PAIRS 112            // JSON input string has too many JSON pairs
#define STAT_JSON_TOO_LONG 113                  // JSON input or output exceeds buffer size

// Gcode errors and warnings (Most originate from NIST - by concept, not number)
// Fascinating: http://www.cncalarms.com/
#define STAT_GCODE_GENERIC_INPUT_ERROR 130              // generic error for gcode input
#define STAT_GCODE_COMMAND_UNSUPPORTED 131              // G command is not supported
#define STAT_MCODE_COMMAND_UNSUPPORTED 132              // M command is not supported
#define STAT_GCODE_MODAL_GROUP_VIOLATION 133            // gcode modal group error
#define STAT_GCODE_AXIS_IS_MISSING 134                  // command requires at least one axis present
#define STAT_GCODE_AXIS_CANNOT_BE_PRESENT 135           // error if G80 has axis words
#define STAT_GCODE_AXIS_IS_INVALID 136                  // an axis is specified that is illegal for the command
#define STAT_GCODE_AXIS_IS_NOT_CONFIGURED 137           // WARNING: attempt to program an axis that is disabled
#define STAT_GCODE_AXIS_NUMBER_IS_MISSING 138           // axis word is missing its value
#define STAT_GCODE_AXIS_NUMBER_IS_INVALID 139           // axis word value is illegal

#define STAT_GCODE_ACTIVE_PLANE_IS_MISSING 140          // active plane is not programmed
#define STAT_GCODE_ACTIVE_PLANE_IS_INVALID 141          // active plane selected is not valid for this command
#define STAT_GCODE_FEEDRATE_NOT_SPECIFIED 142           // move has no feedrate
#define STAT_GCODE_INVERSE_TIME_MODE_CANNOT_BE_USED 143 // G38.2 and some canned cycles cannot accept inverse time mode
#define STAT_GCODE_ROTARY_AXIS_CANNOT_BE_USED 144       // G38.2 and some other commands cannot have rotary axes
#define STAT_GCODE_G53_WITHOUT_G0_OR_G1 145             // G0 or G1 must be active for G53
#define STAT_REQUESTED_VELOCITY_EXCEEDS_LIMITS 146
#define STAT_CUTTER_COMPENSATION_CANNOT_BE_ENABLED 147
#define STAT_PROGRAMMED_POINT_SAME_AS_CURRENT_POINT 148
#define STAT_SPINDLE_SPEED_BELOW_MINIMUM 149

#define STAT_SPINDLE_SPEED_MAX_EXCEEDED 150
#define STAT_S_WORD_IS_MISSING 151
#define STAT_S_WORD_IS_INVALID 152
#define STAT_SPINDLE_MUST_BE_OFF 153
#define STAT_SPINDLE_MUST_BE_TURNING 154                // some canned cycles require spindle to be turning when called
#define STAT_ARC_SPECIFICATION_ERROR 155                // generic arc specification error
#define STAT_ARC_AXIS_MISSING_FOR_SELECTED_PLANE 156    // arc is missing axis (axes) required by selected plane
#define STAT_ARC_OFFSETS_MISSING_FOR_SELECTED_PLANE 157 // one or both offsets are not specified
#define STAT_ARC_RADIUS_OUT_OF_TOLERANCE 158            // WARNING - radius arc is too small or too large - accuracy in question
#define STAT_ARC_ENDPOINT_IS_STARTING_POINT 159

#define STAT_P_WORD_IS_MISSING 160                      // P must be present for dwells and other functions
#define STAT_P_WORD_IS_INVALID 161                      // generic P value error
#define STAT_P_WORD_IS_ZERO 162
#define STAT_P_WORD_IS_NEGATIVE 163                     // dwells require positive P values
#define STAT_P_WORD_IS_NOT_AN_INTEGER 164               // G10s and other commands require integer P numbers
#define STAT_P_WORD_IS_NOT_VALID_TOOL_NUMBER 165
#define STAT_D_WORD_IS_MISSING 166
#define STAT_D_WORD_IS_INVALID 167
#define STAT_E_WORD_IS_MISSING 168
#define STAT_E_WORD_IS_INVALID 169

#define STAT_H_WORD_IS_MISSING 170
#define STAT_H_WORD_IS_INVALID 171
#define STAT_L_WORD_IS_MISSING 172
#define STAT_L_WORD_IS_INVALID 173
#define STAT_Q_WORD_IS_MISSING 174
#define STAT_Q_WORD_IS_INVALID 175
#define STAT_R_WORD_IS_MISSING 176
#define STAT_R_WORD_IS_INVALID 177
#define STAT_T_WORD_IS_MISSING 178
#define STAT_T_WORD_IS_INVALID 179

// TinyG errors and warnings
#define STAT_GENERIC_ERROR 200
#define STAT_MINIMUM_LENGTH_MOVE 201                    // move is less than minimum length
#define STAT_MINIMUM_TIME_MOVE 202                      // move is less than minimum time
#define STAT_MACHINE_ALARMED 203                        // machine is alarmed. Command not processed
#define STAT_LIMIT_SWITCH_HIT 204                       // a limit switch was hit causing shutdown
#define STAT_PLANNER_FAILED_TO_CONVERGE 205             // trapezoid generator can through this exception

#define STAT_SOFT_LIMIT_EXCEEDED 220                    // soft limit error - axis unspecified
#define STAT_SOFT_LIMIT_EXCEEDED_XMIN 221               // soft limit error - X minimum
#define STAT_SOFT_LIMIT_EXCEEDED_XMAX 222               // soft limit error - X maximum
#define STAT_SOFT_LIMIT_EXCEEDED_YMIN 223               // soft limit error - Y minimum
#define STAT_SOFT_LIMIT_EXCEEDED_YMAX 224               // soft limit error - Y maximum
#define STAT_SOFT_LIMIT_EXCEEDED_ZMIN 225               // soft limit error - Z minimum
#define STAT_SOFT_LIMIT_EXCEEDED_ZMAX 226               // soft limit error - Z maximum
#define STAT_SOFT_LIMIT_EXCEEDED_AMIN 227               // soft limit error - A minimum
#define STAT_SOFT_LIMIT_EXCEEDED_AMAX 228               // soft limit error - A maximum
#define STAT_SOFT_LIMIT_EXCEEDED_BMIN 229               // soft limit error - B minimum

#define STAT_SOFT_LIMIT_EXCEEDED_BMAX 220               // soft limit error - B maximum
#define STAT_SOFT_LIMIT_EXCEEDED_CMIN 231               // soft limit error - C minimum
#define STAT_SOFT_LIMIT_EXCEEDED_CMAX 232               // soft limit error - C maximum

#define STAT_HOMING_CYCLE_FAILED 240                    // homing cycle did not complete
#define STAT_HOMING_ERROR_BAD_OR_NO_AXIS 241
#define STAT_HOMING_ERROR_ZERO_SEARCH_VELOCITY 242
#define STAT_HOMING_ERROR_ZERO_LATCH_VELOCITY 243
#define STAT_HOMING_ERROR_TRAVEL_MIN_MAX_IDENTICAL 244
#define STAT_HOMING_ERROR_NEGATIVE_LATCH_BACKOFF 245
#define STAT_HOMING_ERROR_SWITCH_MISCONFIGURATION 246

#define STAT_PROBE_CYCLE_FAILED 250                     // probing cycle did not complete
#define STAT_PROBE_ENDPOINT_IS_STARTING_POINT 251
#define STAT_JOGGING_CYCLE_FAILED 252                   // jogging cycle did not complete

// !!! Do not exceed 255 without also changing stat_t typedef

#endif // STATUS_H
