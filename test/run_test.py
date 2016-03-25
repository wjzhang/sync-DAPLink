#
# DAPLink Interface Firmware
# Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
 
"""
DAPLink validation and testing tool

optional arguments:
  -h, --help            show this help message and exit
  --targetdir TARGETDIR
                        Directory with pre-built target test images.
  --user USER           MBED username (required for compile-api)
  --password PASSWORD   MBED password (required for compile-api)
  --firmwaredir FIRMWAREDIR
                        Directory with firmware images to test
  --firmware {k20dx_k64f_if,lpc11u35_efm32gg_stk_if,...} (run script with --help to see full list)
                        Firmware to test
  --logdir LOGDIR       Directory to log test results to
  --noloadif            Skip load step for interface.
  --notestendpt         Dont test the interface USB endpoints.
  --loadbl              Load bootloader before test.
  --testdl              Run DAPLink specific tests. The DAPLink test tests
                        bootloader updates so use with caution
  --testfirst           If multiple boards of the same type are found only
                        test the first one.
  --verbose {Minimal,Normal,Verbose,All}
                        Verbose output
  --dryrun              Print info on configurations but dont actually run
                        tests.
  --force               Try to run tests even if there are problems. Delete logs from previous run.
Example usages
------------------------

Test all built projects in the repository:
test_all.py --user <username> --password <password>

Test everything on a single project in the repository:
test_all.py --project <project> --testfirst --user <username>
    --password <password>

Verify that the USB endpoints are working correctly on
an existing board with firmware already loaded:
test_all.py --noloadif --user <username> --password <password>
"""
from __future__ import absolute_import
from __future__ import print_function

import os
import shutil
import argparse
import subprocess
from enum import Enum
from hid_test import test_hid
from serial_test import test_serial
from msd_test import test_mass_storage
from daplink_board import get_all_attached_daplink_boards
from project_generator.generate import Generator
from test_info import TestInfo
from daplink_firmware import load_bundle_from_project, load_bundle_from_release
from firmware import Firmware
from target import load_target_bundle, build_target_bundle
from test_daplink import daplink_test

DEFAULT_TEST_DIR = './test_results'

VERB_MINIMAL = 'Minimal'    # Just top level errors
VERB_NORMAL = 'Normal'      # Top level errors and warnings
VERB_VERBOSE = 'Verbose'    # All errors and warnings
VERB_ALL = 'All'            # All errors
VERB_LEVELS = [VERB_MINIMAL, VERB_NORMAL, VERB_VERBOSE, VERB_ALL]


def test_endpoints(workspace, parent_test):
    """Run tests to validate DAPLINK fimrware"""
    test_info = parent_test.create_subtest('test_endpoints')
    test_hid(workspace, test_info)
    test_serial(workspace, test_info)
    test_mass_storage(workspace, test_info)


class TestConfiguration(object):
    """Wrap all the resources needed to run a test"""
    def __init__(self, name):
        self.name = name
        self.board = None
        self.target = None
        self.if_firmware = None
        self.bl_firmware = None

    def __str__(self):
        name_board = '<None>'
        name_target = '<None>'
        name_if_firmware = '<None>'
        name_bl_firmware = '<None>'
        if self.board is not None:
            name_board = self.board.name
        if self.target is not None:
            name_target = self.target.name
        if self.if_firmware is not None:
            name_if_firmware = self.if_firmware.name
        if self.bl_firmware is not None:
            name_bl_firmware = self.bl_firmware.name
        return "APP=%s BL=%s Board=%s Target=%s" % (name_if_firmware,
                                                    name_bl_firmware,
                                                    name_board, name_target)


class TestManager(object):
    """Handle tests configuration running and results"""

    class _STATE(Enum):
        INIT = 0
        CONFIGURED = 1
        COMPLETE = 2

    def __init__(self):
        # By default test all configurations and boards
        self._target_list = []
        self._board_list = []
        self._firmware_list = []
        self._only_test_first = False
        self._load_if = True
        self._load_bl = True
        self._test_daplink = True
        self._test_ep = True

        # Internal state
        self._state = self._STATE.INIT
        self._test_configuration_list = None
        self._all_tests_pass = None
        self._firmware_filter = None
        self._untested_firmware = None

    @property
    def all_tests_pass(self):
        assert self._all_tests_pass is not None, 'Must call run_tests first'
        return self._all_tests_pass

    def set_test_first_board_only(self, first):
        """Only test one board of each type"""
        assert isinstance(first, bool)
        assert self._state is self._STATE.INIT
        self._only_test_first = first

    def set_load_if(self, load):
        """Load new interface firmware before testing"""
        assert isinstance(load, bool)
        assert self._state is self._STATE.INIT
        self._load_if = load

    def set_load_bl(self, load):
        """Load new bootloader firmware before testing"""
        assert isinstance(load, bool)
        assert self._state is self._STATE.INIT
        self._load_bl = load

    def set_test_daplink(self, run_test):
        """Run DAPLink specific tests"""
        assert isinstance(run_test, bool)
        assert self._state is self._STATE.INIT
        self._test_daplink = run_test

    def set_test_ep(self, run_test):
        """Test each endpoint - MSD, CDC, HID"""
        assert isinstance(run_test, bool)
        assert self._state is self._STATE.INIT
        self._test_ep = run_test

    def add_firmware(self, firmware_list):
        """Add firmware to be tested"""
        assert self._state is self._STATE.INIT
        self._firmware_list.extend(firmware_list)

    def add_boards(self, board_list):
        """Add boards to be used for testing"""
        assert self._state is self._STATE.INIT
        self._board_list.extend(board_list)

    def add_targets(self, target_list):
        """Add targets to be used for testing"""
        assert self._state is self._STATE.INIT
        self._target_list.extend(target_list)

    def set_firmware_filter(self, name_list):
        """Test only the project names passed given"""
        assert self._state is self._STATE.INIT
        assert self._firmware_filter is None
        self._firmware_filter = set(name_list)

    def run_tests(self):
        """Run all configurations"""
        # Tests can only be run once per TestManager instance
        assert self._state is self._STATE.CONFIGURED
        self._state = self._STATE.COMPLETE

        all_tests_pass = True
        for test_configuration in self._test_configuration_list:
            board = test_configuration.board
            test_info = TestInfo(test_configuration.name)
            test_configuration.test_info = test_info

            test_info.info("Board: %s" % test_configuration.board)
            test_info.info("Application: %s" %
                           test_configuration.if_firmware)
            test_info.info("Bootloader: %s" %
                           test_configuration.bl_firmware)
            test_info.info("Target: %s" % test_configuration.target)

            if self._load_if:
                if_path = test_configuration.if_firmware.hex_path
                board.load_interface(if_path, test_info)

            valid_bl = test_configuration.bl_firmware is not None
            if self._load_bl and valid_bl:
                bl_path = test_configuration.bl_firmware.hex_path
                board.load_bootloader(bl_path, test_info)

            board.set_check_fs_on_remount(True)

            if self._test_daplink:
                daplink_test(test_configuration, test_info)

            if self._test_ep:
                test_endpoints(test_configuration, test_info)

            if test_info.get_failed():
                all_tests_pass = False

        self._all_tests_pass = all_tests_pass

    def print_results(self, info_level):
        assert self._state is self._STATE.COMPLETE
        # Print info for boards tested
        for test_configuration in self._test_configuration_list:
            print('')
            test_info = test_configuration.test_info
            if info_level == VERB_MINIMAL:
                test_info.print_msg(TestInfo.FAILURE, 0)
            elif info_level == VERB_NORMAL:
                test_info.print_msg(TestInfo.WARNING, None)
            elif info_level == VERB_VERBOSE:
                test_info.print_msg(TestInfo.WARNING, None)
            elif info_level == VERB_ALL:
                test_info.print_msg(TestInfo.INFO, None)
            else:
                # This should never happen
                assert False

    def write_test_results(self, directory, git_sha=None, local_changes=None,
                           info_level=TestInfo.INFO):
        assert self._state is self._STATE.COMPLETE

        assert not os.path.exists(directory)
        os.mkdir(directory)

        # Write out version of tools used for test
        tools_file = directory + os.sep + 'requirements.txt'
        with open(tools_file, "w") as file_handle:
            command = ['pip', 'freeze']
            subprocess.check_call(command, stdin=subprocess.PIPE,
                                  stdout=file_handle,
                                  stderr=subprocess.STDOUT)

        # Write out each test result
        for test_configuration in self._test_configuration_list:
            test_info = test_configuration.test_info
            file_path = directory + os.sep + test_info.name + '.txt'
            with open(file_path, 'w') as file_handle:
                file_handle.write("Test configuration: %s\n" %
                                  test_configuration)
                file_handle.write("Board: %s\n" % test_configuration.board)
                file_handle.write("Application: %s\n" %
                                  test_configuration.if_firmware)
                file_handle.write("Bootloader: %s\n" %
                                  test_configuration.bl_firmware)
                file_handle.write("Target: %s\n" % test_configuration.target)
                file_handle.write("\n")
                test_info.print_msg(info_level, None, log_file=file_handle)

        # Write out summary
        summary_file = directory + os.sep + 'summary.txt'
        with open(summary_file, "w") as file_handle:
            # Overall result
            if self.all_tests_pass:
                file_handle.write("All tests pass\n\n")
            else:
                file_handle.write("One or more tests have failed\n\n")

            if git_sha is not None and local_changes is not None:
                file_handle.write("Git info for test:\n")
                file_handle.write("  Git SHA: %s\n" % git_sha)
                file_handle.write("  Local changes: %s\n" % local_changes)
            file_handle.write("\n")

            # Results for each test
            file_handle.write("Test settings:\n")
            file_handle.write("  Load application before test: %s\n" %
                              self._load_if)
            file_handle.write("  Load bootloader before test: %s\n" %
                              self._load_bl)
            file_handle.write("  Run DAPLink specific tests: %s\n" %
                              self._test_daplink)
            file_handle.write("  Run endpoint tests: %s\n" %
                              self._test_ep)
            file_handle.write("\n")

            # Results for each test
            file_handle.write("Tested configurations:\n")
            for test_configuration in self._test_configuration_list:
                test_info = test_configuration.test_info
                test_passed = test_info.get_failed() == 0
                result_str = 'Pass' if test_passed else 'Fail'
                file_handle.write("  %s: %s\n" %
                                  (test_configuration, result_str))
            file_handle.write("\n")

            # Untested firmware
            untested_list = self.get_untested_firmware()
            if len(untested_list) == 0:
                file_handle.write("All firmware in package tested\n")
            else:
                file_handle.write("Untested firmware:\n")
                for untested_fw in self.get_untested_firmware():
                    file_handle.write("  %s\n" % untested_fw.name)
            file_handle.write("\n")

        # Target test images
        target_dir = directory + os.sep + 'target'
        os.mkdir(target_dir)
        for target in self._target_list:
            new_hex = target_dir + os.sep + os.path.basename(target.hex_path)
            shutil.copy(target.hex_path, new_hex)
            new_bin = target_dir + os.sep + os.path.basename(target.bin_path)
            shutil.copy(target.bin_path, new_bin)

    def get_test_configurations(self):
        assert self._state in (self._STATE.CONFIGURED,
                               self._STATE.COMPLETE)
        return self._test_configuration_list

    def get_untested_firmware(self):
        assert self._state in (self._STATE.CONFIGURED,
                               self._STATE.COMPLETE)
        return self._untested_firmware

    def build_test_configurations(self, parent_test):
        assert self._state is self._STATE.INIT
        self._state = self._STATE.CONFIGURED
        test_info = parent_test.create_subtest('Build test configuration')

        # Create table mapping each board id to a list of boards with that ID
        board_id_to_board_list = {}
        for board in self._board_list:
            board_id = board.get_board_id()
            if board_id not in board_id_to_board_list:
                board_id_to_board_list[board_id] = []
            board_list = board_id_to_board_list[board_id]
            if self._only_test_first and len(board_list) > 1:
                # Ignore this board since we already have one
                test_info.info('Ignoring extra boards of type 0x%x' %
                               board_id)
                continue
            board_list.append(board)

        # Create a table mapping each board id to a target
        board_id_to_target = {}
        for target in self._target_list:
            assert target.board_id not in board_id_to_target, 'Multiple ' \
                'targets found for board id "%s"' % target.board_id
            board_id_to_target[target.board_id] = target

        # Create a list for bootloader firmware and interface firmware
        bootloader_firmware_list = []
        filtered_interface_firmware_list = []
        for firmware in self._firmware_list:
            if firmware.type == Firmware.TYPE.BOOTLOADER:
                bootloader_firmware_list.append(firmware)
            elif firmware.type == Firmware.TYPE.INTERFACE:
                name = firmware.name
                if ((self._firmware_filter is None) or
                        (name in self._firmware_filter)):
                    filtered_interface_firmware_list.append(firmware)
            else:
                assert False, 'Unsupported firmware type "%s"' % firmware.type

        # Explicitly specified boards must be present
        fw_name_set = set(fw.name for fw in filtered_interface_firmware_list)
        if self._firmware_filter is not None:
            assert self._firmware_filter == fw_name_set

        # Create a table mapping each hic to a bootloader
        hic_id_to_bootloader = {}
        for firmware in bootloader_firmware_list:
            hic_id = firmware.hic_id
            assert hic_id not in hic_id_to_bootloader, 'Duplicate ' \
                'bootloaders for HIC "%s" not allowed'
            hic_id_to_bootloader[hic_id] = firmware

        # Create a test configuration for each interface and supported board
        # combination
        test_conf_list = []
        self._untested_firmware = []
        for firmware in filtered_interface_firmware_list:
            board_id = firmware.board_id
            hic_id = firmware.hic_id
            bl_firmware = None
            target = None

            # Check if there is a board to test this firmware
            # and if not skip it
            if board_id not in board_id_to_board_list:
                self._untested_firmware.append(firmware)
                test_info.info('No board to test firmware %s' % firmware.name)
                continue

            # Get target
            target = None
            target_required = not self._test_ep and not self._test_daplink
            if board_id in board_id_to_target:
                target = board_id_to_target[board_id]
            elif target_required:
                self._untested_firmware.append(firmware)
                test_info.info('No target to test firmware %s' %
                               firmware.name)
                continue

            # Check for a bootloader
            bl_required = self._load_bl or self._test_daplink
            if hic_id in hic_id_to_bootloader:
                bl_firmware = hic_id_to_bootloader[hic_id]
            elif bl_required:
                self._untested_firmware.append(firmware)
                test_info.info('No bootloader to test firmware %s' %
                               firmware.name)
                continue

            # Create a test configuration for each board
            board_list = board_id_to_board_list[board_id]
            for board in board_list:
                if firmware.hic_id != board.hic_id:
                    test_info.warning('FW HIC ID %s != Board HIC ID %s' %
                                      (firmware.hic_id, board.hic_id))
                if bl_firmware is not None:
                    if firmware.hic_id != bl_firmware.hic_id:
                        test_info.warning('FW HIC ID %s != BL HIC ID %s' %
                                          (firmware.hic_id,
                                           bl_firmware.hic_id))
                if target is not None:
                    assert firmware.board_id == target.board_id

                test_conf = TestConfiguration(firmware.name + ' ' + board.name)
                test_conf.if_firmware = firmware
                test_conf.bl_firmware = bl_firmware
                test_conf.board = board
                test_conf.target = target
                test_conf_list.append(test_conf)
        self._test_configuration_list = test_conf_list


def get_firmware_names(project_dir):

    # Save current directory
    cur_dir = os.getcwd()
    os.chdir(project_dir)
    try:
        all_names = set()
        projects = list(Generator('projects.yaml').generate())
        for project in projects:
            assert project.name not in all_names
            all_names.add(project.name)
    finally:
        # Restore the current directory
        os.chdir(cur_dir)
    return list(all_names)


def get_git_info(project_dir):
    cur_dir = os.getcwd()
    os.chdir(project_dir)

    # Get the git SHA.
    try:
        git_sha = subprocess.check_output(["git", "rev-parse",
                                           "--verify", "HEAD"])
        git_sha = git_sha.strip()
    except (subprocess.CalledProcessError, WindowsError) :
        print("#> ERROR: Failed to get git SHA, do you "
              "have git in your PATH environment variable?")
        exit(-1)

    # Check are there any local, uncommitted modifications.
    try:
        subprocess.check_output(["git", "diff", "--no-ext-diff",
                                 "--quiet", "--exit-code"])
    except subprocess.CalledProcessError:
        git_has_changes = True
    else:
        git_has_changes = False

    os.chdir(cur_dir)

    return git_sha, git_has_changes


def main():
    self_path = os.path.abspath(__file__)
    test_dir = os.path.dirname(self_path)
    daplink_dir = os.path.dirname(test_dir)

    # We make assumptions that break if user copies script file outside the test dir
    if os.path.basename(test_dir) != "test":
        print("Error - this script must reside in the test directory")
        exit(-1)

    git_sha, local_changes = get_git_info(daplink_dir)
    firmware_list = get_firmware_names(daplink_dir)
    firmware_choices = [firmware for firmware in firmware_list if
                        firmware.endswith('_if')]

    description = 'DAPLink validation and testing tool'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--targetdir',
                        help='Directory with pre-built target test images.',
                        default=None)
    parser.add_argument('--user', type=str, default=None,
                        help='MBED username (required for compile-api)')
    parser.add_argument('--password', type=str, default=None,
                        help='MBED password (required for compile-api)')
    parser.add_argument('--firmwaredir',
                        help='Directory with firmware images to test',
                        default=None)
    parser.add_argument('--firmware', help='Firmware to test', action='append',
                        choices=firmware_choices, default=[], required=False)
    parser.add_argument('--logdir', help='Directory to log test results to',
                        default=DEFAULT_TEST_DIR)
    parser.add_argument('--noloadif', help='Skip load step for interface.',
                        default=False, action='store_true')
    parser.add_argument('--notestendpt', help='Dont test the interface '
                        'USB endpoints.', default=False, action='store_true')
    parser.add_argument('--loadbl', help='Load bootloader before test.',
                        default=False, action='store_true')
    parser.add_argument('--testdl', help='Run DAPLink specific tests. '
                        'The DAPLink test tests bootloader updates so use'
                        'with caution',
                        default=False, action='store_true')
    parser.add_argument('--testfirst', help='If multiple boards of the same '
                        'type are found only test the first one.',
                        default=False, action='store_true')
    parser.add_argument('--verbose', help='Verbose output',
                        choices=VERB_LEVELS, default=VERB_NORMAL)
    parser.add_argument('--dryrun', default=False, action='store_true',
                        help='Print info on configurations but dont '
                        'actually run tests.')
    parser.add_argument('--force', action='store_true', default=False,
                        help='Try to run tests even if there are problems. Delete logs from previous run.')
    args = parser.parse_args()

    use_prebuilt = args.targetdir is not None
    use_compile_api = args.user is not None and args.password is not None

    # Validate args
    if not args.notestendpt:
        if not use_prebuilt and not use_compile_api:
            print("Endpoint test requires target test images.")
            print("  Directory with pre-built target test images")
            print("  must be specified with '--targetdir'")
            print("OR")
            print("  Mbed login credentials '--user' and '--password' must")
            print("  be specified so test images can be built with")
            print("  the compile API.")
            exit(-1)

    firmware_explicitly_specified = len(args.firmware) != 0
    test_info = TestInfo('DAPLink')
    if args.targetdir is not None:
        target_dir = args.targetdir
    else:
        target_dir = daplink_dir + os.sep + 'tmp'
        build_target_bundle(target_dir, args.user, args.password, test_info)

    if os.path.exists(args.logdir):
        if args.force:
            shutil.rmtree(args.logdir)
        else:
            print('Error - test results directory "%s" already exists' %
                  args.logdir)
            exit(-1)

    # Get all relevant info
    if args.firmwaredir is None:
        firmware_bundle = load_bundle_from_project()
    else:
        firmware_bundle = load_bundle_from_release(args.firmwaredir)
    target_bundle = load_target_bundle(target_dir)
    all_firmware = firmware_bundle.get_firmware_list()
    all_boards = get_all_attached_daplink_boards()
    all_targets = target_bundle.get_target_list()

    for board in all_boards:
        if board.get_mode() == board.MODE_BL:
            print('Switching to APP mode on board: %s' % board.unique_id)
            try:
                board.set_mode(board.MODE_IF)
            except Exception:
                print('Unable to switch mode on board: %s' % board.unique_id)

    # Make sure firmware is present
    if firmware_explicitly_specified:
        all_firmware_names = set(fw.name for fw in all_firmware)
        firmware_missing = False
        for firmware_name in args.firmware:
            if firmware_name not in all_firmware_names:
                firmware_missing = True
                test_info.failure('Cannot find firmware %s' % firmware_name)
        if firmware_missing:
            test_info.failure('Firmware missing - aborting test')
            exit(-1)

    # Create manager and add resources
    tester = TestManager()
    tester.add_firmware(all_firmware)
    tester.add_boards(all_boards)
    tester.add_targets(all_targets)
    if firmware_explicitly_specified:
        tester.set_firmware_filter(args.firmware)

    # Configure test manager
    tester.set_test_first_board_only(args.testfirst)
    tester.set_load_if(not args.noloadif)
    tester.set_test_ep(not args.notestendpt)
    tester.set_load_bl(args.loadbl)
    tester.set_test_daplink(args.testdl)

    # Build test configurations
    tester.build_test_configurations(test_info)

    test_config_list = tester.get_test_configurations()
    if len(test_config_list) == 0:
        test_info.failure("Nothing that can be tested")
        exit(-1)
    else:
        test_info.info('Test configurations to be run:')
        index = 0
        for test_config in test_config_list:
            test_info.info('    %i: %s' % (index, test_config))
            index += 1
    test_info.info('')

    untested_list = tester.get_untested_firmware()
    if len(untested_list) == 0:
        test_info.info("All firmware can be tested")
    else:
        test_info.info('Fimrware that will not be tested:')
        for untested_firmware in untested_list:
            test_info.info('    %s' % untested_firmware.name)
    test_info.info('')

    if firmware_explicitly_specified and len(untested_list) != 0:
        test_info.failure("Exiting because not all firmware could be tested")
        exit(-1)

    # If this is a dryrun don't run tests, just print info
    if args.dryrun:
        exit(0)

    # Run tests
    tester.run_tests()

    # Print test results
    tester.print_results(args.verbose)
    tester.write_test_results(args.logdir,
                              git_sha=git_sha,
                              local_changes=local_changes)

    # Warn about untested boards
    print('')
    for firmware in tester.get_untested_firmware():
        print('Warning - configuration %s is untested' % firmware.name)

    if tester.all_tests_pass:
        print("All boards passed")
        exit(0)
    else:
        print("Test Failed")
        exit(-1)


if __name__ == "__main__":
    main()
