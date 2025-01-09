# Work Progress

## add SD card to save data

save RAW vertical acceleration data, welch output, gnss data and packets queued for sending to iridium
    - could we save RAW accelometer, gyrp and mangotometer to get directions

## measure currents

    - sleep without SD card ~ 0.1 mA (no LEDs on)
    - current with accel/gyro (LSM6DS3)+ MAG(LIS3MDL) 7.9 mA
    - measure with/without SD card, with SD card 8.2 mA (SD card sleeping, 300 uA)
    - measure with only accel/gyro
    - measure complete cycle

## build

comile with SparkFun:apollo3:amap3micromod for SD card to work

## add battery calculation