/*
 * test.c - tinyg test sets
 * Part of TinyG project
 *
 * Copyright (c) 2010 - 2015 Alden S. Hart Jr.
 *
 * This file ("the software") is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 as published by the
 * Free Software Foundation. You should have received a copy of the GNU General Public
 * License, version 2 along with the software.  If not, see <http://www.gnu.org/licenses/>.
 *
 * THE SOFTWARE IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL, BUT WITHOUT ANY
 * WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "tinyg.h"             // #1
#include "config.h"            // #2
#include "controller.h"
#include "planner.h"
#include "test.h"
#include "util.h"

// regression test files
#ifdef __CANNED_TESTS

#include "tests/test_001_smoke.h"             // basic functionality
#include "tests/test_002_homing.h"            // G28.1 homing cycles
#include "tests/test_003_squares.h"            // square moves
#include "tests/test_004_arcs.h"            // arc moves
#include "tests/test_005_dwell.h"            // dwells embedded in move sequences
#include "tests/test_006_feedhold.h"        // feedhold - requires manual ! and ~ entry
#include "tests/test_007_Mcodes.h"            // M codes synchronized w/moves (planner queue)
#include "tests/test_008_json.h"            // JSON parser and IO
#include "tests/test_009_inverse_time.h"    // inverse time mode
#include "tests/test_010_rotary.h"            // ABC axes
#include "tests/test_011_small_moves.h"        // small move test
#include "tests/test_012_slow_moves.h"        // slow move test
#include "tests/test_013_coordinate_offsets.h"    // what it says
#include "tests/test_014_microsteps.h"        // test all microstep settings
#include "tests/test_050_mudflap.h"            // mudflap test - entire drawing
#include "tests/test_051_braid.h"            // braid test - partial drawing

#endif

#ifdef __TEST_99
#include "tests/test_099.h"                    // diagnostic test file. used to diagnose specific issues
#endif

/*
 * run_test() - system tests from FLASH invoked by $test=n command
 *
 *     By convention the character array containing the test must have the same
 *    name as the file name.
 */
uint8_t run_test(nvObj_t *nv) {
    switch ((uint8_t)nv->value) {
        case 0: return STAT_OK;
#ifdef __CANNED_TESTS
        case 1: test_open(PGMFILE(&test_smoke),PGM_FLAGS); break;
        case 2: test_open(PGMFILE(&test_homing),PGM_FLAGS); break;
        case 3: test_open(PGMFILE(&test_squares),PGM_FLAGS); break;
        case 4: test_open(PGMFILE(&test_arcs),PGM_FLAGS); break;
        case 5: test_open(PGMFILE(&test_dwell),PGM_FLAGS); break;
        case 6: test_open(PGMFILE(&test_feedhold),PGM_FLAGS); break;
        case 7: test_open(PGMFILE(&test_Mcodes),PGM_FLAGS); break;
        case 8: test_open(PGMFILE(&test_json),PGM_FLAGS); break;
        case 9: test_open(PGMFILE(&test_inverse_time),PGM_FLAGS); break;
        case 10: test_open(PGMFILE(&test_rotary),PGM_FLAGS); break;
        case 11: test_open(PGMFILE(&test_small_moves),PGM_FLAGS); break;
        case 12: test_open(PGMFILE(&test_slow_moves),PGM_FLAGS); break;
        case 13: test_open(PGMFILE(&test_coordinate_offsets),PGM_FLAGS); break;
        case 14: test_open(PGMFILE(&test_microsteps),PGM_FLAGS); break;
        case 50: test_open(PGMFILE(&test_mudflap),PGM_FLAGS); break;
        case 51: test_open(PGMFILE(&test_braid),PGM_FLAGS); break;
#endif
#ifdef __TEST_99
        case 99: test_open(PGMFILE(&test_99),PGM_FLAGS); break;
#endif
        default:
          fprintf_P(stderr,PSTR("Test #%d not found\n"),(uint8_t)nv->value);
          return STAT_ERROR;
    }

    return STAT_OK;
}
