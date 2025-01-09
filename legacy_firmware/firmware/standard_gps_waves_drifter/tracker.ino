#include "Arduino.h"

#include "params.h"
#include "time_manager.h"
#include "sleep_manager.h"
#include "gnss_manager.h"
#include "watchdog_manager.h"
#include "iridium_manager.h"
#include "board_control.h"
#include "imu_manager.h"
#include "data_manager.h"
#include "wave_analyzer.h"
#include "thermistors_manager.h"
#include "sd_manager.h"

#include "RTC.h" // Include RTC library included with the Aruino_Apollo3 core
APM3_RTC myRTC; // Create instance of RTC class

// TODO: board_imu_manger -> board_imu_manager

void setup(){
  // --------------------------------------------------------------------------------
  // startup and configuration...
  delay(1000);

  // first set up the watchdog
  // WDT has 1 Hz frequency and raises interrupt after 32 ticks and resets after 32 ticks,
  // i.e. 32 seconds reset time.
  wdt.configure(WDT_1HZ, 32, 32);
  wdt.start();

  // turn_on is needed to make sure all board pins are in correct state
  // it turns off GNSS and Iridium, sets all pins to control the board, and starts debug serial
  turn_on();
  wdt.restart();
  Serial.flush();

  Serial.println("----------------");
  Serial.println("OpenMetBuoy " __DATE__ " " __TIME__ );
  Serial.println();

  // Easily set RTC using the system __DATE__ and __TIME__ macros from compiler
  //myRTC.setToCompilerTime();

  myRTC.getTime();

  Serial.printf("It is now %04d/%02d/%02d %02d:%02d:%02d\r\n", myRTC.year+2000, myRTC.month, myRTC.dayOfMonth, myRTC.hour, myRTC.minute, myRTC.seconds);

  // offer a print of the params used for the setup of the instrument
  print_params();

  // greet the user
  blink_LED_n_times(10, 5.0);

  // --------------------------------------------------------------------------------
  // some easy to put on debug help

  // ########################################
  // some debugging stuff START
  // just here for convenience; true or false set the code blocks for easy debugging
  // will test basic functionality of each main component of the instrument
  // we test with some sleep before and after to allow testing for power use

  // to test the functionalities: sleep, thermistors
  if (false){
    sleep_for_seconds(5);
    board_thermistors_manager.get_and_push_data();
    sleep_for_seconds(5);
    iridium_manager.attempt_transmit_thermistors_packets(1);
    sleep_for_seconds(5);
  }

  // to test the functionalities: sleep, gnss
  if (false){
    sleep_for_seconds(5);
    gnss_manager.get_and_push_fix(3UL * 60UL);
    sleep_for_seconds(5);
    iridium_manager.attempt_transmit_gps_fixes(1);
    sleep_for_seconds(5);
  }
  // i2c scan
  if (false){
    turn_gnss_on();
    delay(1000); // Give it time to power up
    wdt.restart();

    byte error, address;
    int nDevices;
    uint8_t waiByte[1];
    ArtemisWire.setClock(400000);

    Serial.println("Scanning... ArtemisWire");

    nDevices = 0;
    for(address = 1; address < 127; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        ArtemisWire.beginTransmission(address);
        ArtemisWire.write(0x0F);                                   // Set register address, WHOI_AM_I
        error = ArtemisWire.endTransmission(false);

        if (error == 0){
            Serial.printf("I2C device found at address 0x%02x - ", address);

            ArtemisWire.requestFrom(address, 1, true);     // Request bytes, release I2C-bus after data read
            int i = 0;                                             //
            while(ArtemisWire.available()) {
                if (i < 1)
                    waiByte[i++] = ArtemisWire.read();                               // Add data to array
            }
            Serial.printf("%d WHOI Byte 0x%02x\r\n", i, waiByte[0]);

            nDevices++;
        }
#ifdef PRINT_I2C_ERRORS
        else if (error==4) {
            Serial.print("Unknown error at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.println(address,HEX);
        }    
#endif
    }
    if (nDevices == 0)
        Serial.println("AstemisWire No I2C devices found\n");

    Serial.println("Scanning...Wire1");

    Wire1.begin();
    Serial.println(F("Wire1 started"));

    nDevices = 0;
    for(address = 1; address < 127; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire1.beginTransmission(address);
        error = Wire1.endTransmission(false);

        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02x\r\n", address);

            nDevices++;
        }
#ifdef PRINT_I2C_ERRORS
        else if (error==4) {
            Serial.print("Unknown error at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.println(address,HEX);
        }    
#endif
    }
    if (nDevices == 0)
        Serial.println("Wire1 No I2C devices found");

    turn_gnss_off();

    Serial.println("-----------------");
  }

  // SD Card check
  if (false){
    sd_manager.start();

    Serial.println("-----------------");
  }

  // to test the functionalities: sleep, waves measurements
  if (false){
    sleep_for_seconds(5);
    board_wave_analyzer.gather_and_analyze_wave_data();
    sleep_for_seconds(5);
    iridium_manager.attempt_transmit_wave_spectra();
    sleep_for_seconds(5);
  }

  // some debugging stuff END
  // ########################################

  // --------------------------------------------------------------------------------
  // startup sequence and initial fix

  // attempt to get an initial GNSS fix, to both 1) check that sky access, 2) know when to wake up
  // if get the fix, also (attempt to) transmit it at once to let know we booted
  // if cannot get an initial fix, we do not have a clear view of the sky: sleep and re-try later
  while (false){
    blink_LED_n_times(2, 1.0);

    Serial.println(F("attempt initial fix"));
    wdt.restart();

    if (gnss_manager.get_and_push_fix(timeout_first_fix_seconds)){
      Serial.println(F("success initial GNSS fix"));
      board_time_manager.print_posix_status();
      wdt.restart();

      iridium_manager.attempt_transmit_gps_fixes(1);
      wdt.restart();
      
      // we are done, ready to loop
      break;
    }
    else{
      // if no fix, sleep until next attempt to save battery
      Serial.println(F("initial GNSS fix fail"));
      sleep_for_seconds(sleep_no_initial_fix_seconds);
    }
  }

  // --------------------------------------------------------------------------------
  // perform one initial measurement of each kind to allow immediate check that good startup

  // test that all working well for the thermistors and transmit once.
  if (use_thermistor_string){
    blink_LED_n_times(8, 1.0);
    Serial.println(F("initial thermistors data acquisition..."));
    board_thermistors_manager.get_and_push_data();
    iridium_manager.attempt_transmit_thermistors_packets(1);
    delay(100);
  }

  // once we have got a GNSS fix, also try to get a wave spectrum and transmit it at once. This way,
  // we can check that "all is working well" on the board
   if (use_wave_measurements) {
    blink_LED_n_times(4, 1.0);
    Serial.println(F("initial wave spectrum acquisition..."));
    board_wave_analyzer.gather_and_analyze_wave_data();
    iridium_manager.attempt_transmit_wave_spectra();
    delay(100);
  }

  myRTC.getTime();

  Serial.printf("It is now %04d/%02d/%02d %02d:%02d:%02d\r\n", myRTC.year+2000, myRTC.month, myRTC.dayOfMonth, myRTC.hour, myRTC.minute, myRTC.seconds);

  // --------------------------------------------------------------------------------
  // done starting up

  // At this stage, we have got the initial GNSS fix and the RTC is in UTC.
  // After that, will wake up at given UTC times (see the params.h)
}

void loop(){
  //--------------------------------------------------------------------------------
  // go to sleep until next measurement

  Serial.println(F("ready to sleep"));

  // how long should we sleep? The duration between 2 measurements, minus the time we are already over the previous measurement time
  sleep_for_seconds(
    modifiable_interval_between_gnss_measurements_seconds - (board_time_manager.get_posix_timestamp() % modifiable_interval_between_gnss_measurements_seconds),
    2
  );

  // now we know that we are at the start of a GPS measurement

  //--------------------------------------------------------------------------------
  // the first thing we do is always to take a GNSS measurement (note: this could be changed if needed,
  // but in general, makes sense that each time doing any action, one wants to know the position first...)
  // also check if other actions should be taken

  Serial.println(F("-- loop --"));
  board_time_manager.print_posix_status();
  wdt.restart();

  // we should always take a GPS measurement when we wake up, see note above
  bool take_gnss_measurement {true};

  // we may take other measurements a bit less often than gnss measurements; check if this is time to take one!
  bool take_spectrum_measurement = board_wave_analyzer.time_to_measure_waves() && use_wave_measurements;
  bool take_thermistors_measurement = board_thermistors_manager.time_to_measure_thermistors() && use_thermistor_string;

  Serial.print(F("perform GPS measurement: ")); Serial.println(take_gnss_measurement);
  Serial.print(F("perform wave measurement: ")); Serial.println(take_spectrum_measurement);
  Serial.print(F("perform thermistors measurement: ")); Serial.println(take_thermistors_measurement);

  //--------------------------------------------------------------------------------
  // do the gnss measurement and transmission work

  bool gnss_fix {false};

  if (take_gnss_measurement){
    // get gnss fix
    gnss_fix = gnss_manager.get_and_push_fix(timeout_gnss_fix_seconds);
    wdt.restart();
  }

  //--------------------------------------------------------------------------------
  // NOTE: should we try to do this at a bit "off" time, for example at the end of the loop or somethink like this?
  // the motivation is that it is recommended to not use iridium at the start of an hour / day / minute (probably because
  // many people try to use it then... ? )
  // If got a fix at previous step, attempt transmissions through iridium: there are chances for iridium coverage
  // If not GPS fix, iridium that is even more sensitive to bad signal will definitely not go through
  // If the first iridium attempt goes through, transmit as much data as possible as a burst
  // the strategy is the following: if the initial transmission was successful, keep trying transmitting
  // in the limit of a max transmission time.
  bool any_successful_transmit {false};

  if (gnss_fix){
    Serial.printf("gnss time %04d/%02d/%02d %02d:%02d:%02d\r\n", gnss_manager.year, gnss_manager.month, gnss_manager.day, gnss_manager.hour, gnss_manager.minute, gnss_manager.second);

    myRTC.getTime();

    Serial.printf("rtc  time %04d/%02d/%02d %02d:%02d:%02d\r\n", myRTC.year+2000, myRTC.month, myRTC.dayOfMonth, myRTC.hour, myRTC.minute, myRTC.seconds);

    Serial.println(F("got GNSS fix, so good chances that iridium is good"));
    Serial.println(F("start transmission burst"));

    unsigned long time_start_transmission_burst = millis();
    delay(1000);
    wdt.restart();

    // try to transmit for 5 minutes at most; this is also the timeout of iridium transmission attempt, so if
    // the first attempt times out, this aborts the full burst anyways
    // if there is nothing to attempt to transmit, ie no data waiting in any buffer, abort the burst at once; no
    // point waiting idle.
    constexpr unsigned long timeout_iridium_attempt {5UL * 60UL * 1000UL};

    while (millis() - time_start_transmission_burst < timeout_iridium_attempt){  // this one should be redundant, but keep in case some user forgets testing time
      delay(500);
      wdt.restart();

      bool anything_more_to_attempt {false};

      iridium_manager.attempt_transmit_gps_fixes(modifiable_min_nbr_of_fix_per_message);
      anything_more_to_attempt |= iridium_manager.last_attempt_tried_sending();
      any_successful_transmit |= iridium_manager.last_communication_was_successful();
      delay(500);
      wdt.restart();

      // need to ensure we do not timeout already now before attempting next communication
      if (millis() - time_start_transmission_burst > timeout_iridium_attempt){
        break;
      }

      iridium_manager.attempt_transmit_wave_spectra();
      anything_more_to_attempt |= iridium_manager.last_attempt_tried_sending();
      any_successful_transmit |= iridium_manager.last_communication_was_successful();
      delay(500);
      wdt.restart();

      // need to ensure we do not timeout already now before attempting next communication
      if (millis() - time_start_transmission_burst > timeout_iridium_attempt){
        break;
      }

      iridium_manager.attempt_transmit_thermistors_packets(min_default_nbr_thermistor_packets);
      anything_more_to_attempt |= iridium_manager.last_attempt_tried_sending();
      any_successful_transmit |= iridium_manager.last_communication_was_successful();
      delay(500);
      wdt.restart();

      // need to ensure we do not timeout already now before attempting next communication
      if (millis() - time_start_transmission_burst > timeout_iridium_attempt){
        break;
      }

      // if nothing more to attempt on any kind of message, no more data to transmit, stop here!
      if (!anything_more_to_attempt){
        break;
      }

    }
    wdt.restart();

    Serial.println(F("end transmission burst"));
  }

  else{
    Serial.println(F("no GNSS fix, and iridium will likey not go through either"));
  }

  wdt.restart();
  
  // --------------------------------------------------------------------------------
  // do the thermistors measurement and attempt transmission if any success during the burst

  if (take_thermistors_measurement){
    board_thermistors_manager.get_and_push_data();
    wdt.restart();
    if (any_successful_transmit){
      iridium_manager.attempt_transmit_thermistors_packets(min_default_nbr_thermistor_packets);
    }
  }
  wdt.restart();

  // -------------------------------------------------------------------------------
  // do the wave spectrum measurement and attempt transmission if any success
  // during the burst

  if (take_spectrum_measurement){
    board_wave_analyzer.gather_and_analyze_wave_data();
    wdt.restart();
    if (any_successful_transmit){
      iridium_manager.attempt_transmit_wave_spectra();
    }
  }
  wdt.restart();

  //--------------------------------------------------------------------------------
  // the end for this loop
  wdt.restart();

  myRTC.getTime();

  Serial.printf("end of loop %04d/%02d/%02d %02d:%02d:%02d\r\n", myRTC.year+2000, myRTC.month, myRTC.dayOfMonth, myRTC.hour, myRTC.minute, myRTC.seconds);
}

