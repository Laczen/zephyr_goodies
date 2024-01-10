<!--
  Copyright (c) 2022 Laczen

  SPDX-License-Identifier: Apache-2.0
-->
# zephyr_sharedram - module to enable sharing data between applications 

This modules enables sharing information on an area of ram between consecutive
applications (e.g. a bootloader and an application). It is a replacement for
the retained_mem driver in zephyr. To enable the module the west manifest or
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
    - name: zephyr_sharedram
      url: https://github.com/Laczen/zephyr_shared_data
      revision: main
```

After the project has been added calling `west update` ads the module to
the zephyr workspace. If your workspace is called `zephyr_project` tests can 
be found under `zephyr_project/zephyr_shared_data/tests`.