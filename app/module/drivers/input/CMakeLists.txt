# Copyright (c) 2020-2025 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_library_amend()

add_subdirectory_ifdef(CONFIG_ZMK_INPUT_QDEC qdec)
zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_MOCK input_mock.c)
