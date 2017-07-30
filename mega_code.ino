#include <stdio.h> 
#include <stdint.h> 
#include <stdbool.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>     //Library for LCD on main I2C bus
//#include "SHLtypes.h"
#include <Sensirion.h>

//Defines *******************************************************
#define TMP04_OVEN_MEGA         43
#define TMP04_UV_MEGA           45
#define UV_RELAY                7         
#define UV_BALLAST               4      
#define INTERRUPT_UV_DOOR        3
#define INTERRUPT_PAUSE         18
#define INTERRUPT_TEMP_DOOR      2
#define INTERRUPT_READY         26 
#define W_HEATER                36
#define PUMP                     8
#define HUMIDITY_DRIVER_DATA    35
#define HUMIDITY_DRIVER_CLK     33
#define HUMIDITY_DRIVER_LATCH   31
#define W_LEVEL1                30
#define W_LEVEL2                32
#define W_TEMP_MEGA             34
#define HEATER_BED               5
#define HUM_SDA_2               22
#define HUM_SCL_2               24
#define HUM_SDA_1               51      //Humidity Sensors not real I2C
#define HUM_SCL_1               53

// I2C defines
#define FILE_NAME_LENGTH 30
#define MASTERADDRESS 1
#define SLAVEADDRESS 8
#define YUN_READY 49
#define YUN_ERROR 39

//Other Defines 
#define MAX_SET_POINTS 100
#define MILIHOUR 3.6e6
#define MILIMIN 60000
#define MAX_ERRORS 9
#define CONTROLLER_STEP 500 
#define FILE_NAME_LENGTH 30

//#define NUM_HUMIDITY_STATES 5
#define OFF 0
#define UV_HUMIDIFY 0b00000111
#define UV_DEHUMIDIFY 0b10000110
#define OVEN_HUMIDIFY 0b01100001
#define OVEN_DEHUMIDIFY 0b1110000

//**********************************************************************


//Types ****************************************************************
//Structs (Capatalised)
typedef struct Sensor_Data{
    float uvBox_temp, uvBox_humidity , uvBox_dew, uvBox_UV;
    float oven_temp, oven_humidity , oven_dew;
    /*float humidifier_temp;
    //Pulled High
    bool float_switch1;         //Upper 
    bool float_switch2;         //Lower
    bool uvBox_door;
    //Switch - 0 paused*/ 
    bool pause_switch;
    
} Sensor_Data;
    
typedef struct SetPoint{
    int        setPoint_time;
    uint8_t    setPoint_oven_temp;              //Only integer temperatures and humidities can be set. Temp:0<T<120
    uint8_t    setPoint_oven_humidity;          //Relative Humidity: 0<RH<100
    //uint8_t    setPoint_uvBox_temp;           //Future Expansion
    uint8_t    setPoint_uvBox_humidity;         //Relative Humidity: 0<RH<100
    uint8_t    setPoint_UVPercent;              //integer percent 0<UV<100
    }setPoint;
    
typedef struct Program_Info{ 
    uint8_t chamber_select;                        //Which chambers are ON
    uint8_t num_setPoints;                          //Number of oven program set points
    uint8_t log_stepSize;                           //Logging frequency 
    char data_file_name[FILE_NAME_LENGTH]; 
    setPoint setPoints[MAX_SET_POINTS];                      
    } Program_Info;
    
     

//*********************************************************************************

//GLOBALS
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
Program_Info program = {0}; 
uint8_t errors[MAX_ERRORS] = {0}; 
Sensor_Data sensor_data = {0}; 
Sensirion SHT_heat = Sensirion(HUM_SDA_1, HUM_SCL_1);
Sensirion SHT_UV = Sensirion(HUM_SDA_2, HUM_SCL_2);
uint8_t old_uv_val = 0;
uint8_t i2csendcode = 0;  //Some small variable to tell Yun what to send
//Function Prototypes

//********************************************************************************


void setup(){

    //PIN MODES 
    pinMode(TMP04_OVEN_MEGA,INPUT);
    pinMode(TMP04_UV_MEGA,INPUT);
    pinMode(UV_RELAY,OUTPUT);
    pinMode(UV_BALLAST,OUTPUT);
    pinMode(INTERRUPT_UV_DOOR,INPUT);
    pinMode(INTERRUPT_PAUSE,INPUT);
    pinMode(INTERRUPT_TEMP_DOOR,INPUT);
    pinMode(INTERRUPT_READY,INPUT);
    pinMode(W_HEATER,OUTPUT);
    pinMode(PUMP,OUTPUT);
    pinMode(HUMIDITY_DRIVER_DATA,OUTPUT);
    pinMode(HUMIDITY_DRIVER_CLK,OUTPUT);
    pinMode(HUMIDITY_DRIVER_LATCH,OUTPUT);
    pinMode(W_LEVEL1,INPUT);
    pinMode(W_LEVEL2,INPUT);
    pinMode(W_TEMP_MEGA,INPUT);
    pinMode(HEATER_BED,OUTPUT);
    pinMode(YUN_READY,INPUT);
    pinMode(YUN_ERROR,INPUT);

    //Set internal pull up resistor
    digitalWrite(W_LEVEL1, HIGH);
    digitalWrite(W_LEVEL2, HIGH);

    Serial.begin(9600);
    //Serial.println("starting");
    //Start with everything off
    Shutdown_System();
    //Setup lcd
    lcd.begin(20,4);
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("SHL Accelerated");
    lcd.setCursor(0,1);
    lcd.print("Degradation Testing");
    lcd.setCursor(5,2);
    lcd.print("Facility");
    lcd.setCursor(2,3);
    lcd.print("Initiallising...");
    delay(2000);
    lcd.clear();
   
    // populate_sensor_info();
    //populate_program_info(); 

   //Wait for Yun to be ready (wireless setup usually takes couple of minutes)
     while( digitalRead(YUN_READY) == LOW ){ 
         Serial.println("Ready Pin Low");
        lcd.setCursor(3,1);
        lcd.print("Reading SD.");
        delay(500); 
        lcd.clear();
        lcd.setCursor(3,1);
        lcd.print("Reading SD..");
        delay(500);
        lcd.clear();
        lcd.setCursor(3,1);
        lcd.print("Reading SD...");
        delay(500);
        lcd.clear();
          }

    //Yun will send HIGH to pin if error reading SD card
          while(digitalRead(YUN_ERROR) == HIGH){
            Serial.println("SD Error");
            lcd.setCursor(3,1);
            lcd.print("SD ERROR!");
            delay(10000);
          }

//********************************************************************************
    
  Wire.begin();    // join i2c bus (address optional for master)

  //Send code for program structure from Yun
  i2csendcode=0;    //data
  Wire.beginTransmission(SLAVEADDRESS);
  Wire.write(i2csendcode);
  Wire.endTransmission();
  delay(1000);
  
  //Request program structure data
  
  byte * p = (byte *) &program;
  for(int i = 0; i < sizeof(program); i++){
    Wire.requestFrom(SLAVEADDRESS,1);
    *p++ = Wire.read();
  }

  //Print setpoints on LCD

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(program.data_file_name);
   lcd.setCursor(0,1);
     if(program.chamber_select==11){
      lcd.print("Both Chambers ON");
     }
     else if(program.chamber_select==10){
      lcd.print("Only Heat ON");
     }
     else if(program.chamber_select == 01){
      lcd.print("Only UV ON)");
     }   
  lcd.setCursor(0,2);
 // lcd.print("Setpoints:");
  //lcd.print(program.num_setPoints);
  //lcd.setCursor(0,3);
  lcd.print("Time: ");
    for (int i = 0; i < program.num_setPoints; i++){
      lcd.print(program.setPoints[i].setPoint_time);
      lcd.print(" ");
    }
  delay(5000);

/*Serial.print("num_setPoints = ");
  Serial.println(program.num_setPoints);
  Serial.print("log_stepSize = "); 
  Serial.println(program.log_stepSize); 
  Serial.print("chamber_select = "); 
  Serial.println(program.chamber_select); 
  Serial.println(program.data_file_name);
  
  for(int i=0;i<program.num_setPoints;i++){
    Serial.println("****************************");
    Serial.print("Setpoint ");
    Serial.println(i);
    Serial.print("setPoint_time = "); 
    Serial.println(program.setPoints[i].setPoint_time); 
    Serial.print("setPoint_temperature = "); 
    Serial.println(program.setPoints[i].setPoint_oven_temp); 
    Serial.print("setPoint_oven_humidity = " ); 
    Serial.println(program.setPoints[i].setPoint_oven_humidity); 
    Serial.print("setPoint_uvBox_humidity = " );
    Serial.println(program.setPoints[i].setPoint_uvBox_humidity); 
    Serial.print("setPoint_uv_level = " );
    Serial.println(program.setPoints[i].setPoint_UVPercent); 
  }*/

  //Send code for checksum
  i2csendcode=1;    //checksum
  Serial.print("sendcode: ");
  Serial.println(i2csendcode); 
  Wire.beginTransmission(SLAVEADDRESS);
  Wire.write(i2csendcode);
  Wire.endTransmission();
  delay(1000);
  //Request data for checksum
  Wire.requestFrom(SLAVEADDRESS,1);
  byte checksum_yun=Wire.read();
  Serial.print("Checksum from Yun: ");
  Serial.println(checksum_yun);
  byte checksum_mega=i2ccheckprogram();
  Serial.print("Checksum on local: ");
  Serial.println(checksum_mega);
}


//********************************************************************************

void loop(){ 
    Measure_Sensors(); 
    log_data();
    Serial.println("measure sensor complete");
    LCD_print_running(0);
    Serial.println("LCD running complete");
    program_run(); 
    Serial.println("program run complete");
    program_complete(); 
}

/*uint8_t populate_program_info(void){
    program.num_setPoints = 4; 
    program.log_stepSize = 1; 
    program.chamber_select = 11; 
    
    program.setPoints[0].setPoint_time = 0; 
    program.setPoints[1].setPoint_time = 1;
    program.setPoints[2].setPoint_time = 2;
    program.setPoints[3].setPoint_time = 4;
    
    program.setPoints[0].setPoint_oven_temp = 40; 
    program.setPoints[1].setPoint_oven_temp = 90; 
    program.setPoints[2].setPoint_oven_temp = 120;
    //program.setPoints[3].setPoint_oven_temp = 90; 
    
    program.setPoints[0].setPoint_oven_humidity = 55; 
    program.setPoints[1].setPoint_oven_humidity = 60; 
    program.setPoints[2].setPoint_oven_humidity = 70;
    //program.setPoints[3].setPoint_oven_humidity = 90; 
    
    program.setPoints[0].setPoint_UVPercent = 100; 
    program.setPoints[1].setPoint_UVPercent = 0; 
    program.setPoints[2].setPoint_UVPercent = 100;
    //program.setPoints[3].setPoint_UVPercent = 100; 
    
    program.setPoints[0].setPoint_uvBox_humidity = 60; 
    program.setPoints[1].setPoint_uvBox_humidity = 60; 
    program.setPoints[2].setPoint_uvBox_humidity = 80;
    //program.setPoints[3].setPoint_uvBox_humidity = 90; 
    
    return 0; 
}

uint8_t populate_sensor_info(void){

   sensor_data.uvBox_temp = 12.3;
   sensor_data.uvBox_humidity = 23.4;
   sensor_data.uvBox_dew = 34.5;
   sensor_data.oven_temp = 45.6;
   sensor_data.oven_humidity = 56.7;
   sensor_data.oven_dew = 67.8;

    return 0; 
}*/

//********************************************************************************
void Measure_Sensors( void ){
    sensor_data.pause_switch = digitalRead( INTERRUPT_PAUSE ); 
    if( sensor_data.pause_switch == LOW ){
        Pause(); 
    }
    sensor_data.oven_temp = get_temp( TMP04_OVEN_MEGA );
    sensor_data.uvBox_temp = get_temp( TMP04_UV_MEGA );
    Serial.println(sensor_data.oven_temp);
    Serial.println(sensor_data.uvBox_temp);
    float temp; 
    SHT_heat.measure( &temp, &(sensor_data.oven_humidity),  &(sensor_data.oven_dew) );
    SHT_UV.measure( &(sensor_data.uvBox_temp), &(sensor_data.uvBox_humidity), &(sensor_data.uvBox_dew) );
}

//********************************************************************************
void Pause( void ){
    lcd.clear();
    lcd.setCursor(3,1);
    lcd.print("System Paused");
    Shutdown_System();    
    while( digitalRead( INTERRUPT_PAUSE ) == LOW ){
        delay(100);
    }
}

//********************************************************************************
void Shutdown_System( void ){
    digitalWrite( HEATER_BED, LOW ); 
    digitalWrite(PUMP, LOW); 
    set_valves( OFF ); 
    digitalWrite( UV_RELAY, LOW );
    old_uv_val = 0;
    //Send Yun File Close Signal 
}

//********************************************************************************
float get_temp( uint8_t pin ){ 
      float high_t, low_t;
      float temp[3] = {0,0,0}; 
      float av_temp = 0; 
      
      for(int i = 0; i<3; i++){ 
         high_t = pulseIn(pin,HIGH);
         low_t  = pulseIn(pin,LOW);     
         temp[i] = 235 - ((400*high_t)/(low_t)); 
         delay(10);
      }
      av_temp = ((temp[0]+temp[1]+temp[2])/(float)3.0);
      
      return av_temp; 
} 

//********************************************************************************
void program_run(void){
    long unsigned start_time = millis(); 
    delay(1);
    long unsigned current_time = millis();
    long unsigned last_log_time = current_time;
    long unsigned last_controller_update = current_time; 
    uint8_t current_setPoint = 0; 
    
    while( current_setPoint < program.num_setPoints ){
        while( (current_time - start_time) < program.setPoints[current_setPoint+1].setPoint_time * MILIHOUR ){
         
            current_time = millis();

  //Check Data Logging Time
            if( current_time > (last_log_time + program.log_stepSize*MILIMIN) ){
                log_data(); //Also Log Error State
                last_log_time = millis();
            }
  //Controller update rate: (500ms)         
           if( current_time > (last_controller_update + CONTROLLER_STEP ) ){
                Measure_Sensors();
                LCD_print_running( current_setPoint );
                temperature_controller( current_setPoint );
                humidity_controller( current_setPoint ); 
                UV_controller( current_setPoint );
                last_controller_update = millis();
            }
        }
    current_setPoint++; 
    Serial.println("Set Point incremented");
    }
}

//********************************************************************************
void temperature_controller( uint8_t current_setPoint ){
    //UV chamber only
    if( program.chamber_select == 01){
        digitalWrite( HEATER_BED, LOW );
        return;
    }
    else if( sensor_data.oven_temp < program.setPoints[current_setPoint].setPoint_oven_temp){ 
        digitalWrite( HEATER_BED, HIGH );
          
    }
    else{ 
        digitalWrite( HEATER_BED, LOW ); 
    }
}

//********************************************************************************
void humidity_controller(uint8_t current_setPoint){
   // float oven_humidity_deviation, uv_humidity_deviation;  
   // oven_humidity_deviation = (sensor_data.oven_humidity - program.setPoints[current_setPoint].setPoint_oven_humidity )/ program.setPoints[current_setPoint].setPoint_oven_humidity;
   // uv_humidity_deviation = (sensor_data.uvBox_humidity - program.setPoints[current_setPoint].setPoint_uvBox_humidity )/ (sensor_data.uvBox_humidity - program.setPoints[current_setPoint].setPoint_uvBox_humidity); 
    
    //UV chmaber only
    if( program.chamber_select == 01){
        if( sensor_data.uvBox_humidity < program.setPoints[current_setPoint].setPoint_uvBox_humidity ){
            set_valves( UV_HUMIDIFY ); 
            digitalWrite( PUMP, HIGH );
            return;
        }
        else if( sensor_data.uvBox_humidity > 1.2 * program.setPoints[current_setPoint].setPoint_uvBox_humidity ){
            set_valves( UV_DEHUMIDIFY ); 
            digitalWrite( PUMP, HIGH ); 
            return;
        }
        else{
            digitalWrite( PUMP, LOW );
            set_valves( OFF ); 
            return;
        }
    }
    //Heat Chamber only
    else if( program.chamber_select == 10){
        if( sensor_data.oven_humidity < program.setPoints[current_setPoint].setPoint_oven_humidity ){
            set_valves( OVEN_HUMIDIFY ); 
            digitalWrite( PUMP, HIGH );
            return;
        }
        else if( sensor_data.oven_humidity > 1.2 * program.setPoints[current_setPoint].setPoint_oven_humidity ){
            set_valves( OVEN_DEHUMIDIFY ); 
            digitalWrite( PUMP, HIGH );
            return; 
        }
        else{
            set_valves( OFF );
            digitalWrite( PUMP, LOW );
            return;
        }
    }
    //Both Chambers Selected
    else if( program.chamber_select == 11) {
        //Precedence for UV chamber - because easier to control to high tollerance
        if( sensor_data.uvBox_humidity < program.setPoints[current_setPoint].setPoint_uvBox_humidity ){
            set_valves( UV_HUMIDIFY ); 
            digitalWrite( PUMP, HIGH );
            return; 
        }
        else if( sensor_data.oven_humidity < program.setPoints[current_setPoint].setPoint_oven_humidity ){
            set_valves( OVEN_HUMIDIFY ); 
            digitalWrite( PUMP, HIGH );
            return; 
        }
        else if( sensor_data.uvBox_humidity > 1.2 * program.setPoints[current_setPoint].setPoint_uvBox_humidity ){
            set_valves( UV_DEHUMIDIFY ); 
            digitalWrite( PUMP, HIGH ); 
            return; 
        }
        else if( sensor_data.oven_humidity > 1.2 * program.setPoints[current_setPoint].setPoint_oven_humidity ){
            set_valves( OVEN_DEHUMIDIFY ); 
            digitalWrite( PUMP, HIGH ); 
            return; 
        }
        else{ 
            digitalWrite( PUMP, LOW );
            set_valves( OFF );
            return; 
        }
    }
}    

//********************************************************************************
void set_valves( uint8_t state ){ 
    //Shift Register Code
    digitalWrite( PUMP, LOW ); 
    digitalWrite(HUMIDITY_DRIVER_LATCH, LOW);
    shiftOut(HUMIDITY_DRIVER_DATA, HUMIDITY_DRIVER_CLK, MSBFIRST, state );
    digitalWrite( HUMIDITY_DRIVER_LATCH, HIGH);  
}

//********************************************************************************
void LCD_print_running(uint8_t current_setPoint){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("H.Temp: ");
    lcd.print( sensor_data.oven_temp, 1 );
    lcd.print("C/");
    lcd.print( program.setPoints[current_setPoint].setPoint_oven_temp, 1 ); 
    lcd.print("C");
    lcd.setCursor(0,1);
    lcd.print("H.%RH: ");
    lcd.print(sensor_data.oven_humidity, 1);
    lcd.print("%/");
    lcd.print( program.setPoints[current_setPoint].setPoint_oven_humidity ); 
    lcd.print("%");
    lcd.setCursor(0,2);
    lcd.print("UV%RH: ");
    lcd.print(sensor_data.uvBox_humidity, 1);
    lcd.print("%/");
    lcd.print( program.setPoints[current_setPoint].setPoint_uvBox_humidity, 1 ); 
    lcd.print("%");
    lcd.setCursor(0,3);
    lcd.print("UV: ");
      
    if(program.setPoints[current_setPoint].setPoint_UVPercent){
       lcd.print("ON");
      // lcd.print(program.setPoints[current_setPoint].setPoint_UVPercent,1);
    }
    else {
       lcd.print("OFF");
      // lcd.print(program.setPoints[current_setPoint].setPoint_UVPercent,1);
    }  
}

//********************************************************************************
void program_complete(void){ 
    lcd.clear(); 
    lcd.setCursor(4, 1);
    lcd.print("Complete"); 
    lcd.setCursor(0, 2);
    lcd.print("Please Turn OFF");
    
    while(1){
        delay(100);
    }
}

//********************************************************************************
void log_data(void){ 
 i2csendcode=3;    //sendcode for datalog (1byte)
 Wire.beginTransmission(SLAVEADDRESS);
 Wire.write(i2csendcode); //place datalog "code" in buffer first
   //send sensor data
  int data_count=0;
    for (int i = 0 ; i< 28; i++){ //28 bytes -> 7 floats plus datalog code 29 bytes
      Wire.write((byte *)&sensor_data+data_count,1);
      data_count++;
    }
 Wire.endTransmission();
 delay(1000);

    //Yun will send HIGH to pin if error reading SD card
          while(digitalRead(YUN_ERROR) == HIGH){
            Serial.println("SD Error");
            lcd.clear();
            lcd.setCursor(3,1);
            lcd.print("SD ERROR!");
            delay(10000);
          }
 
 lcd.clear(); 
 lcd.setCursor(3,1); 
 lcd.print("Data Logged"); 
 Serial.println("data logged!");

 delay(1000);
}

//********************************************************************************
void UV_controller( uint8_t current_setPoint ){ 
       int uv_val = map( program.setPoints[current_setPoint].setPoint_UVPercent, 0, 100, 0, 255);
    
    if( program.chamber_select == 10 || uv_val == 0 ){ 
        digitalWrite(UV_RELAY, LOW); 
        return; 
    }
    else if(uv_val != old_uv_val){ 
        digitalWrite(UV_RELAY, LOW);
        //analogWrite(UV_BALLAST, uv_val); 
        delay(10);
        digitalWrite(UV_RELAY, HIGH); 
        old_uv_val = uv_val; 
    }
}

//********************************************************************************
byte i2ccheckprogram(){
  byte checksum=0;

  checksum+=program.num_setPoints;
  checksum+=program.chamber_select;
  checksum+=program.log_stepSize;
  
  for(int i=0; i<100;i++){
    checksum+=program.setPoints[i].setPoint_time;
    checksum+=program.setPoints[i].setPoint_time>>8;
    checksum+=program.setPoints[i].setPoint_oven_temp;
    checksum+=program.setPoints[i].setPoint_oven_humidity;
    //checksum+=program.setPoints[i].setPoint_uvBox_temp;
    checksum+=program.setPoints[i].setPoint_uvBox_humidity;
    checksum+=program.setPoints[i].setPoint_UVPercent;
  }

  return checksum;
}

 //*********************************************************************************   
