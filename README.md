<!--
  Copyright (c) 2022 Laczen

  SPDX-License-Identifier: Apache-2.0
-->
# zephyr_goodies - module that combines multiple out of tree solutions for zephyr 

This modules provides multiple out of tree solutions for zephyr:

1. shared_info: a replacement for the zephyr retained_mem driver,
2. eeprom_disk: a driver for using eeprom as a disk,
3. storage: a subsystem for working with storage,

To enable the module the west manifest or
the submanifest file (`zephyr/submanifests/example.yaml`) can be altered:

```
# Example manifest file you can include into the main west.yml file.
#
# To make this work, copy this file's contents to a new file,
# 'example.yaml', in the same directory.
#
# Then change the 'name' and 'url' below and run 'west update'.
#
# Your module will be added to the local workspace and kept in sync
# every time you run 'west update'.
#
# If you want to fetch a particular commit rather than the main
# branch, change the 'revision' line accordingly.

manifest:
  projects:
    - name: zephyr_goodies
      url: https://github.com/Laczen/zephyr_goodies
      revision: main
```

After the project has been added calling `west update` ads the module to
the zephyr workspace. If your workspace is called `zephyr_project` the module
can be found under `zephyr_project/zephyr_goodies`. The module uses the same
folder layout as zephyr.