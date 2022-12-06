# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/ubuntu/esp/esp-idf/components/bootloader/subproject"
  "/home/ubuntu/esp/applications/weather/build/bootloader"
  "/home/ubuntu/esp/applications/weather/build/bootloader-prefix"
  "/home/ubuntu/esp/applications/weather/build/bootloader-prefix/tmp"
  "/home/ubuntu/esp/applications/weather/build/bootloader-prefix/src/bootloader-stamp"
  "/home/ubuntu/esp/applications/weather/build/bootloader-prefix/src"
  "/home/ubuntu/esp/applications/weather/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ubuntu/esp/applications/weather/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
