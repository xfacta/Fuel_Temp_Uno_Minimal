
// FuelLevel Temperature and Voltage bars on LCD
// including fan control
// This is a temporary setup
// on a Uno
// Testing mode shows raw analog input values
// Button press to enable test mode
// Averaging of fuel level readings
// Throw away a dummy analog read to allow the ADC to settle



#define Version "Fuel Temp Uno V5"



//========================== Set These Manually ==========================

int Fan_On_Hyst = 20000;      // msec Hysteresis
const int Fan_On_Temp = 89;   // degrees C fan on
const int Warning_Fuel = 10;  // remaining fuel warning level

const float vcc_ref = 4.92;  // measure the 5 volts DC and set it here
const float R1 = 1000.0;     // measure and set the voltage divider values
const float R2 = 2200.0;

//========================================================================



//========================== Demo mode ===================================
// choose one or the other, or both false for real operation

bool Demo_Mode = false;

bool Calibration_Mode = false;

//========================================================================



// Define the LCD display type
#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


/*
  analogReference(DEFAULT);
  DEFAULT: the default analog reference of 5 volts (on 5V Arduino boards) or 3.3 volts (on 3.3V Arduino boards)
  INTERNAL: a built-in reference, equal to 1.1 volts on the ATmega168 or ATmega328P and 2.56 volts on the ATmega32U4 and ATmega8 (not available on the Arduino Mega)
  INTERNAL1V1: a built-in 1.1V reference (Arduino Mega only)
  INTERNAL2V56: a built-in 2.56V reference (Arduino Mega only)
  EXTERNAL: the voltage applied to the AREF pin (0 to 5V only) is used as the reference
  #define aref_voltage 4.096
*/


// Pin definitions for inputs
const int Button_Pin = A0;  // this board has several buttons on analog pin A0
const int Temp_Pin = A1;    // Temperature analog input pin
const int Fuel_Pin = A2;    // Fuel level analog input pin


// Pin definitions for outputs
const int Relay_Pin = 2;  // Relay for fan control


// Input variables
int Temp_Celsuis, Fuel_Level, Dummy;
uint32_t Fuel_Raw;
float Fuel_Average, Temp_Float;
String the_string;


// Times of last important events
uint32_t Fan_On_Time;
uint32_t Button_Press_Time;


// Meter display event timing and status
uint32_t Loop_Interval = 4000;  // 4 seconds between updates
uint32_t Loop_Time, Loop_Count;


// ##################################################################################################################################################


void setup() {


  analogReference(DEFAULT);

  //Improved randomness for testing
  unsigned long seed = 0, count = 32;
  while (--count)
    seed = (seed << 1) | (analogRead(A6) & 1);
  randomSeed(seed);

  // Initialise the LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);

  // Set any pins that might be used

  // Outputs
  pinMode(Relay_Pin, OUTPUT);
  digitalWrite(Relay_Pin, LOW);  // ensure fan relay is OFF at least until temperature is measured

  // Digital inputs
  // none

  // Analog inputs
  pinMode(Button_Pin, INPUT);
  pinMode(Temp_Pin, INPUT);
  pinMode(Fuel_Pin, INPUT);

  // Holding down a button during startup changes to Calibration mode
  // this is blocking
  if (analogRead(Button_Pin) < 1000) {
    delayMicroseconds(50);  //debounce
    while (analogRead(Button_Pin) < 1000) {
      Button_Press_Time++;  // count the milliseconds the button is pressed
      if ((Button_Press_Time / 100) % 2) {
        lcd.print("X");
      } else lcd.print("+");
      lcd.setCursor(0, 0);
      delay(1);
      if (Button_Press_Time > 2000) {
        Calibration_Mode = true;
        Loop_Interval = 1000;
        lcd.print("=");
        delay(1000);
        break;
      }
    }
    Button_Press_Time = 0;  // zero this in case we ever use it again
  }

  //cant have both modes at once
  if (Calibration_Mode == true) Demo_Mode = false;

  // Display startup text
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(Version);
  if (Calibration_Mode == true) {
    lcd.setCursor(0, 1);
    lcd.print("Calibration Mode");
  }
  if (Demo_Mode == true) {
    lcd.setCursor(3, 1);
    lcd.print("Demo Mode");
  }
  delay(2000);

  // Display static text
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Fuel");
  lcd.setCursor(1, 1);
  lcd.print("Temp");

  Loop_Time = millis();

}  // end void setup


// ##################################################################################################################################################
// ##################################################################################################################################################


void loop() {


  // =======================================================
  // Always do these every loop
  // =======================================================

  Read_Fuel();

  // =======================================================
  // Do these items at an interval
  // =======================================================

  if (millis() >= Loop_Time + Loop_Interval) {
    Update_Fuel();
    Update_Temp();
    if (Calibration_Mode == false) {
      Control_Fan();
    }
    Loop_Time = millis();
  }


}  // end void loop


// ##################################################################################################################################################
// ##################################################################################################################################################

void Read_Fuel() {


  // The fuel sender is read every loop and the loops are counted
  // in order to average the value of a large number of loops
  // to counter the sloshing effect of the fuel
  Dummy = analogRead(Fuel_Pin);
  Fuel_Raw += analogRead(Fuel_Pin);
  Loop_Count++;


}  // End void Read_Fuel


// ##################################################################################################################################################



void Update_Fuel() {


  // Display Fuel_Level amount here


  if (Calibration_Mode == true) {
    Dummy = analogRead(Fuel_Pin);
    Fuel_Level = analogRead(Fuel_Pin);
  }

  if (Demo_Mode == true) {
    // ----------------- FOR TESTING -----------------
    Fuel_Level = 22 + random(0, 25) - random(0, 25);
    //Fuel_Level = 20;
    // -----------------------------------------------
  }

  if (Calibration_Mode == false && Demo_Mode == false) {
    // ----------------- FOR REAL -----------------

    // Use the total raw fuel reading and loop count to gain a average value
    // then reset the raw fuel value and loop count
    Fuel_Average = float(Fuel_Raw / Loop_Count);

    // Fuel level formula for standard Datsun 1600 tank sender
    // using this formula
    // Litres = -0.00077 * X^2 + 0.12 * X + 41.4
    // the formual uses the raw analog read pin value, NOT a calculated voltage
    Fuel_Level = int(-0.00077 * pow(Fuel_Average, 2) + 0.12 * Fuel_Average + 41.4);
    Fuel_Level = constrain(Fuel_Level, 0, 45);
    Fuel_Raw = 0;
    Loop_Count = 0;

    // -----------------------------------------------
  }

  // Print the value
  // extra spaces appended to allow for calibration mode
  the_string = String(Fuel_Level);
  if (Fuel_Level < 1000) the_string = the_string + " ";
  if (Fuel_Level < 100) the_string = the_string + " ";
  if (Fuel_Level < 10) the_string = the_string + " ";
  lcd.setCursor(6, 0);
  lcd.print(the_string);


}  // End void Update_Fuel


// ##################################################################################################################################################


void Update_Temp() {


  // Get the temperature


  if (Calibration_Mode == true) {
    Dummy = analogRead(Temp_Pin);
    Temp_Celsuis = analogRead(Temp_Pin);
  }

  if (Demo_Mode == true) {
    // ----------------- FOR TESTING -----------------
    Temp_Celsuis = 80 + random(0, 25) - random(0, 25);
    Fan_On_Hyst = 8000;
    //Temp_Celsuis = 15;
    // -----------------------------------------------
  }

  if (Calibration_Mode == false && Demo_Mode == false) {
    // ----------------- FOR REAL -----------------
    // Comment or uncomment different sections here
    // for different sensors

    Dummy = analogRead(Temp_Pin);
    Temp_Float = float(analogRead(Temp_Pin));

    // Formula for modified sender using 10k NTC, 1k reference resistor to 5.0v
    // the formula uses the raw analog read pin value, NOT a calculated voltage
    // -0.00001246X^3+0.00675X^2-1.577X+217.136
    //Temp_Celsuis = int(-0.00001246 * pow(Temp_Float, 3) + 0.00675 * pow(Temp_Float, 2) - 1.577 * Temp_Float + 217.136);

    // Formula for standard SR20 temperature gauge sender, 1k reference resistor to 5.0v
    Temp_Celsuis = int(-32.36 * log(Temp_Float) + 203.82);

    // Using DS8B20 for temperature
    // wait until sensor is ready
    /*
      if (temp_sensor_valid == true)
      {
      sensor.requestTemperatures();
      while (!sensor.isConversionComplete());
      Temp_Celsuis = sensor.getTempC();
      if (Status_Priority == 5) Status_Priority = 0;
      }
      else
      // if the sensor is not present set the temp and turn on the fan
      {
      Temp_Celsuis = 90;
      Status_Priority = 5;
      Status_Change_Time = millis();
      }
      // ----------------- End real measurement ------
      }
    */

    // These not needed for DS18B20
    // uncomment for NTC sensors

    // If using SR20 sensor
    Temp_Celsuis = constrain(Temp_Celsuis, 20, 120);

    // If using 10k NTC sensor
    // Temp_Celsuis = constrain(Temp_Celsuis, 0, 120);
  }

  // Print the temperature value
  // extra spaces appended to allow for calibration mode
  the_string = String(Temp_Celsuis);
  if (Temp_Celsuis < 1000) the_string = the_string + " ";
  if (Temp_Celsuis < 100) the_string = the_string + " ";
  if (Temp_Celsuis < 10) the_string = the_string + " ";
  lcd.setCursor(6, 1);
  lcd.print(the_string);


}  // End void Update_Temp


// ##################################################################################################################################################


void Control_Fan() {


  // Use the temperature to control fan

  // Turn on the fan and remember the time whenever temp goes too high
  if (Temp_Celsuis >= Fan_On_Temp) {
    // set the Fan_On_Time every loop until the temp drops
    // ensure the relay is on
    if (digitalRead(Relay_Pin) == LOW) digitalWrite(Relay_Pin, HIGH);
    Fan_On_Time = millis();
    // print the fan symbol
    lcd.setCursor(13, 1);
    lcd.print("Fan");
  }

  // Turn off the fan when lower than Fan_On_Temp degrees and past the Hysteresis time
  if ((Temp_Celsuis < Fan_On_Temp) && (millis() >= (Fan_On_Time + Fan_On_Hyst))) {
    // ensure the relay is off
    if (digitalRead(Relay_Pin) == HIGH) digitalWrite(Relay_Pin, LOW);
    // print a blank over the fan symbol
    lcd.setCursor(13, 1);
    lcd.print("   ");
  }


}  // end void Control_Fan


// ##################################################################################################################################################
// ##################################################################################################################################################
