#include <Bridge.h>
#include <Console.h>
#include <FileIO.h>
#include <HttpClient.h>
#include <Mailbox.h>
#include <Process.h>
#include <YunClient.h>
#include <YunServer.h>
#include <Wire.h>
Process date; 


//Defines *******************************************************
#define MAX_SET_POINTS 100
#define FILENAME "/mnt/sda1/arduino_out.bin"
#define FILE_NAME_LENGTH 30
#define MASTERADDRESS 1
#define SLAVEADDRESS 8
#define YUN_READY 7
#define YUN_ERROR 8

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
    uint8_t    setPoint_oven_temp;              //Only integer temperatures and humidities can be set. Temp:0<T<130
    uint8_t    setPoint_oven_humidity;          //Relative Humidity: 0<RH<100
    //uint8_t    setPoint_uvBox_temp;             //Only integer temperatures and humidities can be set. Temp:0<T<130
    uint8_t    setPoint_uvBox_humidity;         //Relative Humidity: 0<RH<100
    uint8_t    setPoint_UVPercent;              //integer percent 0<UV<100
} setPoint;
    
typedef struct Program_Info{ 
    uint8_t chamber_select;                        //Which chambers are ON
    uint8_t num_setPoints;                          //Number of oven program set points
    uint8_t log_stepSize;                           //Logging frequency 
    char data_file_name[FILE_NAME_LENGTH];
    setPoint setPoints[MAX_SET_POINTS];                       //Flexible Array Member of 
} Program_Info;

//*********************************************************************************
//Prototypes
int Yun_Bin_Reader( Program_Info *program );
int Yun_Program_Writer( Program_Info *program ); 

//*********************************************************************************

//GLOBALS
Program_Info program = {0};
Sensor_Data sensor_data = {0}; 
uint8_t i2csendcode=1;
byte i2ccheck(byte *);
uint8_t title = 0;

//*********************************************************************************

void setup(){
  pinMode(YUN_READY, OUTPUT); 
  pinMode(YUN_ERROR, OUTPUT);
  digitalWrite(YUN_ERROR, LOW);
  digitalWrite(YUN_READY, LOW); 
  Bridge.begin();
  Console.begin(); //Try to limit console use as it slows down program (uses bridge communication)
  FileSystem.begin();  
  delay(6000); //required for linux processor to start
  //Console.println("Begin reader...");

  Yun_Bin_Reader( &program );
  digitalWrite(YUN_READY, HIGH);

  //Console.println("Ready pin High!");
  //Yun_Program_Writer( &program );
  
  Wire.begin(SLAVEADDRESS);     // join i2c bus
  Wire.onRequest(requestEvent); // register event
  Wire.onReceive(receiveEvent);

}

//*********************************************************************************
void loop(){
  //check if it's time to data log
  delay(500);
  if(i2csendcode == 3){
    logData();
    i2csendcode = 5;
    interrupts();
  }
} 

//*********************************************************************************
/*int Yun_Program_Writer( Program_Info *program ){
  Console.print("num_setPoints = "); 
  Console.println( program->num_setPoints ); 
  
  Console.print("log_stepSize = "); 
  Console.println(  program->log_stepSize ); 

  Console.print("chamber_select = "); 
  Console.println( program->chamber_select ); 
 
  for(int i = 0; i < program->num_setPoints; i++){
    Console.print("Set Point: "); 
    Console.println( i ); 

    Console.print( "setPoint_time = "); 
    Console.println( program->setPoints[i].setPoint_time); 

    Console.print( "setPoint_temperature = "); 
    Console.println( program->setPoints[i].setPoint_oven_temp );  

    Console.print( "setPoint_oven_humidity = " ); 
    Console.println( program->setPoints[i].setPoint_oven_humidity ); 

    Console.print( "setPoint_uvBox_humidity = " );
    Console.println( program->setPoints[i].setPoint_uvBox_humidity ); 

    Console.print( "setPoint_uv_level = " );
    Console.println( program->setPoints[i].setPoint_UVPercent ); 
    }
    return 0; 
}*/

//*********************************************************************************
int Yun_Bin_Reader( Program_Info *program ){
    uint8_t high_byte = 0; 
    uint8_t low_byte = 0; 
    
    File program_file = FileSystem.open( FILENAME, FILE_READ );
    
    if( program_file ){
        while( program_file.available() != 0 ){
            for(int i = 0; i < FILE_NAME_LENGTH; i++ ){
                program->data_file_name[i] = (char)program_file.read();
            }
            program->num_setPoints  =   (uint8_t)program_file.read();
            program->log_stepSize   =   (uint8_t)program_file.read();
            program->chamber_select =   (uint8_t)program_file.read();

            for(int i = 0; i<program->num_setPoints; i++){
                
                low_byte = (uint8_t)program_file.read();        //Double Check Endian Issues, Time given in 2 bytes 
                high_byte = (uint8_t)program_file.read();
                
                program->setPoints[i].setPoint_time             = (high_byte<<8)|low_byte;
                program->setPoints[i].setPoint_oven_temp        = (uint8_t)program_file.read();  
                program->setPoints[i].setPoint_oven_humidity    = (uint8_t)program_file.read(); 
                program->setPoints[i].setPoint_uvBox_humidity   = (uint8_t)program_file.read(); 
                program->setPoints[i].setPoint_UVPercent        = (uint8_t)program_file.read();
            }
            if( program_file.available() != 0 ){
              //  Console.println("Error, Should have read all bytes"); 
                digitalWrite(YUN_ERROR, HIGH);
            }
        }
        program_file.close();
    }
    else{
        //Console.println("Error openning file");  
        digitalWrite(YUN_ERROR, HIGH);
    }
    return 0; 
}

//*********************************************************************************
void requestEvent(){
  static int data_count=0;
  if(i2csendcode==0){ //Send Program Data
    //Console.println("Sending programdata...");
    Wire.write((byte *)&program+data_count,1);
    data_count++;
  }
  else if(i2csendcode==1){ //send checksum
    byte checksum_yun=i2ccheckprogram();
    Wire.write(&checksum_yun,1);    
  }
}

//*********************************************************************************
void receiveEvent(int howMany){
  i2csendcode = Wire.read();    // receive byte

//Disable interupts if its time to log data
  if(i2csendcode == 3);{
    noInterrupts();
  }
}

//*********************************************************************************
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
void logData() {
  
String titleString;  
String dataString;

String path = "/mnt/sda1/";
path +=  program.data_file_name;
char pathway[41];
path.toCharArray(pathway, 41);

//read values into sensor_data
     byte * pp = (byte *) &sensor_data;
  for(int i = 0; i < 28; i++){ //Max 32 bytes in I2C buffer
        *pp++ = Wire.read();
  }
  
    titleString = "  Date: \t Time: \t UV Temp: \t UV Humid: \t UV Dew: \t OVEN Temp \t OVEN Humid \t OVEN Dew";
  
    dataString += getTimeStamp();
    dataString += "\t";
    dataString += sensor_data.uvBox_temp;
    dataString += "\t";  
    dataString += sensor_data.uvBox_humidity;
    dataString += "\t";  
    dataString += sensor_data.uvBox_dew;
    dataString += "\t";  
    dataString += sensor_data.oven_temp;
    dataString += "\t";  
    dataString += sensor_data.oven_humidity;
    dataString += "\t";  
    dataString += sensor_data.oven_dew;
        
  // open the file. The FileSystem card is mounted at the following "/mnt/sda1/specfied output file name"
  File dataFile = FileSystem.open(pathway, FILE_APPEND);
 
  if (dataFile) {
       
    if(title == 0){
      dataFile.println(titleString);
      title = 1;
      Console.println(titleString);
    }
        
    dataFile.println(dataString);
    dataFile.close();
   // Console.println(dataString);
  }  
  else {
    digitalWrite(YUN_ERROR, HIGH);
    //Console.println("error opening datalog.txt");
  } 
}

//*********************************************************************************
String getTimeStamp() {
  String result;
  date.begin("/bin/date");
  date.addParameter("+%d/%m/%Y \t %T");
  date.run();

    while(date.available()>0) {
    char c = date.read();
      if(c != '\n'){
        result += c;
      }
    }
 return result;
}

//*********************************************************************************
