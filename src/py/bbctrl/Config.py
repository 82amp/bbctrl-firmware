################################################################################
#                                                                              #
#                This file is part of the Buildbotics firmware.                #
#                                                                              #
#                  Copyright (c) 2015 - 2018, Buildbotics LLC                  #
#                             All rights reserved.                             #
#                                                                              #
#     This file ("the software") is free software: you can redistribute it     #
#     and/or modify it under the terms of the GNU General Public License,      #
#      version 2 as published by the Free Software Foundation. You should      #
#      have received a copy of the GNU General Public License, version 2       #
#     along with the software. If not, see <http://www.gnu.org/licenses/>.     #
#                                                                              #
#     The software is distributed in the hope that it will be useful, but      #
#          WITHOUT ANY WARRANTY; without even the implied warranty of          #
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       #
#               Lesser General Public License for more details.                #
#                                                                              #
#       You should have received a copy of the GNU Lesser General Public       #
#                License along with the software.  If not, see                 #
#                       <http://www.gnu.org/licenses/>.                        #
#                                                                              #
#                For information regarding this software email:                #
#                  "Joseph Coffland" <joseph@buildbotics.com>                  #
#                                                                              #
################################################################################

import os
import json
import logging
import pkg_resources
import subprocess
import copy

import bbctrl

log = logging.getLogger('Config')

default_config = {
    "motors": [
        {"axis": "X"},
        {"axis": "Y"},
        {"axis": "Z"},
        {"axis": "A", "power-mode" : "disabled"},
        ]
    }


class Config(object):
    def __init__(self, ctrl):
        self.ctrl = ctrl
        self.config_vars = {}

        try:
            self.version = pkg_resources.require('bbctrl')[0].version
            default_config['version'] = self.version

            # Load config template
            with open(bbctrl.get_resource('http/config-template.json'), 'r',
                      encoding = 'utf-8') as f:
                self.template = json.load(f)

            # Add all sections from template to default config
            for section in self.template:
                if not section in default_config:
                    default_config[section] = {}

        except Exception as e: log.exception(e)


    def get(self, name, default = None):
        return self.config_vars.get(name, default)


    def get_index(self, name, index, default = None):
        return self.config_vars.get(name, {}).get(str(index), None)


    def load_path(self, path):
        with open(path, 'r') as f:
            return json.load(f)


    def load(self):
        try:
            if os.path.exists('config.json'):
                config = self.load_path('config.json')
            else: config = copy.deepcopy(default_config)

            try:
                self.upgrade(config)
            except Exception as e: log.exception(e)

            # Add missing sections
            for key, value in default_config.items():
                if not key in config: config[key] = value

            return config

        except Exception as e:
            log.warning('%s', e)
            return default_config


    def upgrade(self, config):
        version = tuple(map(int, config['version'].split('.')))

        if version < (0, 2, 4):
            for motor in config['motors']:
                for key in 'max-jerk max-velocity'.split():
                    if key in motor: motor[key] /= 1000

        if version < (0, 3, 4):
            for motor in config['motors']:
                for key in 'max-accel latch-velocity search-velocity'.split():
                    if key in motor: motor[key] /= 1000

        config['version'] = self.version


    def save(self, config):
        self.upgrade(config)
        self.update(config)

        with open('config.json', 'w') as f:
            json.dump(config, f)

        subprocess.check_call(['sync'])

        log.info('Saved')


    def reset(self): os.unlink('config.json')


    def _encode_cmd(self, name, index, value, spec):
        if str(index):
            if not name in self.config_vars: self.config_vars[name] = {}
            self.config_vars[name][str(index)] = value

        else: self.config_vars[name] = value

        if not 'code' in spec: return

        if spec['type'] == 'enum':
            if value in spec['values']:
                value = spec['values'].index(value)
            else: value = spec['default']

        elif spec['type'] == 'bool': value = 1 if value else 0
        elif spec['type'] == 'percent': value /= 100.0

        self.ctrl.state.config(str(index) + spec['code'], value)


    def _encode_category(self, index, config, category, with_defaults):
        for key, spec in category.items():
            if key in config: value = config[key]
            elif with_defaults: value = spec['default']
            else: continue

            self._encode_cmd(key, index, value, spec)


    def _encode(self, index, config, tmpl, with_defaults):
        for category in tmpl.values():
            self._encode_category(index, config, category, with_defaults)


    def update(self, config, with_defaults = False):
        for name, tmpl in self.template.items():
            if name == 'motors':
                for index in range(len(config['motors'])):
                    self._encode(index, config['motors'][index], tmpl,
                                 with_defaults)

            else: self._encode_category('', config.get(name, {}), tmpl,
                                        with_defaults)


    def reload(self): self.update(self.load(), True)
