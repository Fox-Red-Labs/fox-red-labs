# Three ways, Zephyr: the shared app plus one glue file.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(three_ways)

target_include_directories(app PRIVATE ../common)
if(CONFIG_SOC_SERIES_J722S_R5)
  # BeagleY-AI MCU R5F: RAM console + remoteproc trace, no ADC, no GPIO (boards/beagley_ai_j722s_mcu_r5f0_0.*)
  target_sources(app PRIVATE ../common/app.c src/glue_zephyr_r5f.c)
else()
  target_sources(app PRIVATE ../common/app.c src/glue_zephyr.c)
  # Zephyr 4.4.2 has no MSPM0 ADC driver and no Kconfig switch for these two DriverLib files, so the app compiles them.
  target_sources(app PRIVATE
    ${ZEPHYR_TI_MODULE_DIR}/mspm0/source/ti/driverlib/dl_adc12.c
    ${ZEPHYR_TI_MODULE_DIR}/mspm0/source/ti/driverlib/dl_vref.c)
endif()
