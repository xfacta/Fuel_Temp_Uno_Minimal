
// FuelLevel Temperature and Voltage bars on LCD
// including fan control
// This is a temporary setup
// on an Arduino Uno
// Testing mode shows raw analog input values
// Button press to enable test mode
// Uses multimap instead of formulas



#define Version "Fuel Temp Uno V5"



//========================== Set These Manually ==========================

int         Fan_On_Hyst = 20000;    // msec Hysteresis
const int   Fan_On_Temp = 89;       // degrees C fan on

const float vcc_ref     = 4.92;      // measure the 5 volts DC and set it here
const float R1          = 1000.0;    // measure and set the voltage divider values
const float R2          = 2200.0;

//========================================================================



//========================== Demo mode ===================================
// choose one or the other, or both false for real operation

bool Demo_Mode        = false;

bool Calibration_Mode = false;

//========================================================================



#include <MultiMap.h>

///================== Multimap calibration table ==========================
// Use calbration mode and measured real values to populate the arrays
// measure a real value and record matching the arduino calibration value
// output = Multimap<datatype>(input, inputArray, outputArray, size)
// Input array must have increasing values of the analog input
// Output array is the converted human readable value
// Output is constrained to the range of values in the arrays

// Datsun fuel tank sender Litres
const int fuel_sample_size = 10;
int       fuel_cal_in[]    = { 42, 110, 151, 185, 199, 216, 226, 240, 247, 255 };
int       fuel_cal_out[]   = { 45, 40, 35, 30, 25, 20, 15, 20, 5, 0 };

// SR20 temp sender Celcius
const int temp_sample_size = 11;
int       temp_cal_in[]    = { 15, 20, 25, 30, 33, 44, 61, 85, 109, 198, 332 };
int       temp_cal_out[]   = { 120, 108, 100, 94, 90, 80, 70, 60, 53, 36, 20 };

// NTC temp sender Celcius
//const int temp_sample_size = 11;
//int temp_cal_in[] = { 89, 107, 128, 152, 177, 203, 228, 250, 269, 284, 296 };
//int temp_cal_out[] = { 120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20 };


//========================================================================


// remaining fuel warning level
const int Warning_Fuel = fuel_cal_out[0] / 4;


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
#define Button_Pin A0    // this board has several buttons on analog pin A0
#define Temp_Pin   A1    // Temperature analog input pin
#define Fuel_Pin   A2    // Fuel level analog input pin


// Pin definitions for outputs
#define Relay_Pin 2    // Relay for fan control


// Input variables
int    Fuel_Level, Last_Fuel_Level, Temp_Celsuis;
int    Dummy;
int    Raw_Value;    //use float when using formulas
String the_string;


// Times of last important events
uint32_t Fan_On_Time;
uint32_t Button_Press_Time;


// Meter display event timing and status
// 4 seconds between updates
uint32_t Loop_Interval = 4000;
uint32_t Loop_Time;



// ##################################################################################################################################################



void setup()
    {


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
    // ensure fan relay is OFF at least until temperature is measured
    digitalWrite(Relay_Pin, LOW);

    // Digital inputs
    // none

    // Analog inputs
    pinMode(Button_Pin, INPUT);
    pinMode(Temp_Pin, INPUT);
    pinMode(Fuel_Pin, INPUT);

    // Holding down a button during startup changes to Calibration mode
    // this is blocking
    if (analogRead(Button_Pin) < 1000)
        {
        delayMicroseconds(60);    //debounce
        while (analogRead(Button_Pin) < 1000)
            {
            // count the milliseconds the button is pressed
            Button_Press_Time++;
            if ((Button_Press_Time / 100) % 2)
                {
                lcd.print("X");
                }
            else
                lcd.print("+");
            lcd.setCursor(0, 0);
            delay(1);
            if (Button_Press_Time > 2000)
                {
                Calibration_Mode = true;
                Loop_Interval    = 1000;
                lcd.print("=");
                delay(1000);
                break;
                }
            }
        // zero this in case we ever use it again
        Button_Press_Time = 0;
        }

    //cant have both modes at once
    if (Calibration_Mode == true)
        Demo_Mode = false;
    if (Demo_Mode == true)
        Fan_On_Hyst = Fan_On_Hyst / 3;

    // Display startup text
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(Version);
    if (Calibration_Mode)
        {
        lcd.setCursor(0, 1);
        lcd.print("Calibration Mode");
        }
    if (Demo_Mode)
        {
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

    }    // end void setup



// ##################################################################################################################################################
// ##################################################################################################################################################



void loop()
    {


    // =======================================================
    // Always do these every loop
    // =======================================================

    if (!Calibration_Mode)
        Control_Fan();


    // =======================================================
    // Do these items at an interval
    // =======================================================

    if (millis() - Loop_Time >= Loop_Interval)
        {
        Update_Fuel();
        Update_Temp();
        Loop_Time = millis();
        }


    }    // end void loop



// ##################################################################################################################################################
// ##################################################################################################################################################



void Update_Fuel()
    {


    // Get Fuel_Level amount here
    // do a read and ignore it, allowing the true value to settle
    Dummy     = analogRead(Fuel_Pin);
    Raw_Value = analogRead(Fuel_Pin);

    if (Calibration_Mode)
        Fuel_Level = Raw_Value;

    if (Demo_Mode)
        {
        // ----------------- FOR TESTING -----------------
        Fuel_Level = 22 + random(0, 25) - random(0, 25);
        //Fuel_Level = 20;
        // -----------------------------------------------
        }

    if (!Calibration_Mode && !Demo_Mode)
        {
        // ----------------- FOR REAL -----------------
        // Fuel level formula for standard Datsun 1600 tank sender
        // using this formula
        // Litres = -0.00077 * X^2 + 0.12 * X + 41.4
        // the formual uses the raw analog read pin value, NOT a calculated voltage

        //Raw_Value = float(analogRead(Fuel_Pin));
        //Fuel_Level = int(-0.00077 * pow(Raw_Value, 2) + 0.12 * Raw_Value + 41.4);

        Fuel_Level = multiMap<int>(Raw_Value, fuel_cal_in, fuel_cal_out, fuel_sample_size);

        // -----------------------------------------------

        // Limit the change between readings
        // to combat fuel sloshing
        Fuel_Level      = (Fuel_Level + Last_Fuel_Level) / 2;
        Last_Fuel_Level = Fuel_Level;
        }

    // Print the value
    // extra spaces appended to allow for calibration mode
    the_string = String(Fuel_Level);
    if (Fuel_Level < 1000)
        the_string = the_string + " ";
    if (Fuel_Level < 100)
        the_string = the_string + " ";
    if (Fuel_Level < 10)
        the_string = the_string + " ";
    lcd.setCursor(6, 0);
    lcd.print(the_string);


    }    // End void Update_Fuel



// ##################################################################################################################################################



void Update_Temp()
    {


    // Get the temperature
    Dummy     = analogRead(Temp_Pin);    // do a read and ignore it, allowing the true value to settle
    Raw_Value = analogRead(Temp_Pin);

    if (Calibration_Mode)
        Temp_Celsuis = Raw_Value;

    if (Demo_Mode)
        {
        // ----------------- FOR TESTING -----------------
        Temp_Celsuis = 80 + random(0, 25) - random(0, 25);
        Fan_On_Hyst  = 8000;
        //Temp_Celsuis = 15;
        // -----------------------------------------------
        }

    if (!Calibration_Mode && !Demo_Mode)
        {
        // ----------------- FOR REAL -----------------
        // Comment or uncomment different sections here
        // for different sensors

        // Formula for modified sender using 10k NTC, 1k reference resistor to 5.0v
        // the formula uses the raw analog read pin value, NOT a calculated voltage
        // -0.00001246X^3+0.00675X^2-1.577X+217.136
        //Raw_Value = float(analogRead(Temp_Pin)) + 0.5;
        //Temp_Celsuis = int(-0.00001246 * pow(Raw_Value, 3) + 0.00675 * pow(Raw_Value, 2) - 1.577 * Raw_Value + 217.136);

        // Formula for standard SR20 temperature gauge sender, 1k reference resistor to 5.0v
        //Raw_Value = float(analogRead(Temp_Pin));
        //Temp_Celsuis = int(-32.36 * log(Raw_Value) + 203.82);

        Temp_Celsuis = multiMap<int>(Raw_Value, temp_cal_in, temp_cal_out, temp_sample_size);

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
        }

    // Print the temperature value
    // extra spaces appended to allow for calibration mode
    the_string = String(Temp_Celsuis);
    if (Temp_Celsuis < 1000)
        the_string = the_string + " ";
    if (Temp_Celsuis < 100)
        the_string = the_string + " ";
    if (Temp_Celsuis < 10)
        the_string = the_string + " ";
    lcd.setCursor(6, 1);
    lcd.print(the_string);


    }    // End void Update_Temp



// ##################################################################################################################################################



void Control_Fan()
    {

    // Use the temperature to control fan


    // Turn on the fan and remember the time whenever temp goes too high
    if (Temp_Celsuis >= Fan_On_Temp)
        {
        // set the Fan_On_Time every loop until the temp drops
        // ensure the relay is on
        digitalWrite(Relay_Pin, HIGH);
        Fan_On_Time = millis();
        // print the fan symbol
        lcd.setCursor(13, 1);
        lcd.print("Fan");
        }

    // Turn off the fan when lower than Fan_On_Temp degrees and past the Hysteresis time
    if ((Temp_Celsuis < Fan_On_Temp) && (millis() >= (Fan_On_Time + Fan_On_Hyst)))
        {
        // ensure the relay is off
        digitalWrite(Relay_Pin, LOW);
        // print the blank fan symbol
        lcd.setCursor(13, 1);
        lcd.print("   ");
        }


    }    // end void Control_Fan



// ##################################################################################################################################################
// ##################################################################################################################################################
