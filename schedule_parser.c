// ADD CHMAMBER SELECT DATA READ TESTS. IF ONLY ONE CHMABER SELECTED SHOULD NOT HAVE DATA FOR THE OTHER


#include <stdio.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define BUFSIZE 2000 
#define MIN_SET_POINTS 2
#define MAX_SET_POINTS 100

#define UV_BOX_NAME UV_Box
#define HOT_BOX_NAME Oven
#define NUM_COMMANDS 8

#define FILE_NAME_LENGTH 30 

#define SET_POINT_TIMES 0
#define SET_POINT_TEMPERATURES 1 
#define SET_POINT_OVEN_HUMIDITY 2
#define SET_POINT_UV_HUMIDITY 3
#define SET_POINT_UVPERCENT 4 
#define LOG_STEPSIZE 5
#define CHAMBER_SELECT 6
#define DATA_FILE_NAME 7 
#define MAX_TEMP 130

#define debug(x) printf("%s\n",x);

//************************************************************************************************
//Structs
typedef struct SetPoint{
    int         setPoint_time;
    uint8_t     setPoint_oven_temp;              //Only integer temperatures and humidities can be set. Temp:0<T<130
    uint8_t     setPoint_oven_humidity;          //Relative Humidity: 0<RH<100
    uint8_t     setPoint_uvBox_humidity;         //Relative Humidity: 0<RH<100
    uint8_t     setPoint_UVPercent;              //integer percent 0<UV<100
    }setPoint;
    
typedef struct Program_Info{ 
    uint8_t chamber_select;                         //Which chambers are ON
    int num_setPoints;                          //Number of oven program set points
    uint8_t log_stepSize;                           //Logging frequency 
    char data_file_name[FILE_NAME_LENGTH]; 
    setPoint setPoints[];                       //IN embedded dont use flexible
    } Program_Info;

//**************************************************************************************
//Function Prototypes
static Program_Info* Program_Reader_alloc( char *filename );
static void str_maker( char *line );
static int get_args( Program_Info *program_p, int command_list, char *line );
static int Program_Info_Printer( Program_Info *program );
int Program_hex_writer( Program_Info *program, char *filename );
static int Program_Bin_Writer( Program_Info *program, char *filename );

//**************************************************************************************
int main(int argc, char** argv){
    if(argc != 2){
        fprintf(stderr,"Incorrect number of arguments.\nPlease provide filename only.\n");
        exit(EXIT_FAILURE);
    }
    Program_Info *program = Program_Reader_alloc( argv[1] );
    Program_Info_Printer( program );
    //Program_hex_writer( program, "arduino_out.hex" );
    Program_Bin_Writer( program, "arduino_out.bin" );
    free( program );
    return 0;
    
}

static Program_Info* Program_Reader_alloc(char *filename){
    const char *command_list[ NUM_COMMANDS ] = {"setPoint_Times",  "setPoint_Temperature", 
                                                "setPoint_Oven_Humidity","setPoint_UVBox_Humidity", 
                                                "setPoint_UVPercent","log_stepSize","chamber_select", "data_file_name"};    
                                                //PROGMEM later
    char line[BUFSIZE];
    int num_setPoints = 0;

    FILE *program_filep = fopen(filename, "r");                 //Open file for reading
    if(program_filep == NULL){
        fprintf( stderr,"File:%s could not be oppened.\n", filename );  //Change to LCD_err_writer
        exit(EXIT_FAILURE);         //Exit to ready_check
    }
    
    while( fgets( line, BUFSIZE, program_filep ) != NULL ){			    	//Go through the file and count the number of valid body lines
        str_maker( line );                                          //Format line
        if( *line == '#' || *line == '\0' ) continue;				//Skip comment and blank lines
        
        if( sscanf( line, "num_setPoints\t%d", &num_setPoints) == 1 ){  //get number of set points
            if( num_setPoints < MIN_SET_POINTS || num_setPoints > MAX_SET_POINTS ){
                fprintf(stderr, "Error: num_setPoints must be between %d and %d.\n", MIN_SET_POINTS, MAX_SET_POINTS);   
                exit(EXIT_FAILURE);
            }
            break;
        }
        else{ 
            fprintf(stderr,"scanf fail. Expected 'num_setPoints' - must be first command with 1 argument.\n");
            exit(EXIT_FAILURE);
        }
    }
    Program_Info *program_p = malloc( sizeof(Program_Info) + num_setPoints*sizeof(setPoint) );
     
        if( program_p == NULL ){
            fprintf(stderr, "Malloc Fail in creating Program_Info instance.\n");
            exit(EXIT_FAILURE);
        }
    program_p->num_setPoints = num_setPoints;         //Set num_setPoints in program struct
    char *token; 
    int command_index = 0;
    char linecpy[BUFSIZE];
    int command_found = 0; 
   
    while( fgets( line, BUFSIZE, program_filep ) != NULL ){
        str_maker( line );                                          //Format line
        if( *line == '#' || *line == '\0' ) continue;				//Skip comment and blank lines
        strcpy( linecpy, line);
        //Tokenise lines for following commands 
        token = strtok( line, " ");
        for( int i = 0; i < NUM_COMMANDS; i++){
            if ( strcmp(token, command_list[i]) == 0 ){
                command_index = i; 
                command_found = 1;
                break;
            }
        }
        if(command_found == 0){
            fprintf(stderr, "Command %s not recognised. \n", token); 
            exit(EXIT_FAILURE);
        }
        switch( command_index ){
            case SET_POINT_TIMES:                                 //setPoint_times
                if( get_args( program_p, command_index, linecpy ) != num_setPoints ){
                    fprintf(stderr, "Error reading 'setPoint_times'. Expected $num_setPoints arguments.\n");
                    exit(EXIT_FAILURE);
                }
            break;
            case SET_POINT_TEMPERATURES:                                 //setPoint_Temperatures
                  if( get_args( program_p, command_index, linecpy ) != (num_setPoints - 1) ){
                    fprintf(stderr, "Error reading 'setPoint_Temperatures'. Expected $(num_setPoints-1) arguments.\n");
                    exit(EXIT_FAILURE);
                }
            break;
            case SET_POINT_OVEN_HUMIDITY:                                 //setPoint_oven_humidity
                  if( get_args( program_p, command_index, linecpy ) != (num_setPoints - 1) ){
                    fprintf(stderr, "Error reading 'setPoint_oven_humidity'. Expected $(num_setPoints-1) arguments.\n");
                    exit(EXIT_FAILURE);
                }
            break;
            case SET_POINT_UV_HUMIDITY:                                 //setPoint_uv_humidity
                  if( get_args( program_p, command_index, linecpy ) != (num_setPoints - 1) ){
                    fprintf(stderr, "Error reading 'setPoint_uvBox_humidity'. Expected $(num_setPoints-1) arguments.\n");
                    exit(EXIT_FAILURE);
                }
            break;
             case SET_POINT_UVPERCENT:                                 //setPoint_uv_level
                  if( get_args( program_p, command_index, linecpy ) != (num_setPoints - 1) ){
                    fprintf(stderr, "Error reading 'setPoint_uv_level'. Expected $(num_setPoints-1) arguments.\n");
                    exit(EXIT_FAILURE);
                }
            break;
            case LOG_STEPSIZE:                                 //log_stepSize
                  if( get_args( program_p, command_index, linecpy ) != 1 ){
                    fprintf(stderr, "Error reading 'log_stepSize'. Expected 1 argument.\n");
                    exit(EXIT_FAILURE);
                }
            break;
            case CHAMBER_SELECT:                                 //chamber select
                  if( get_args( program_p, command_index, linecpy ) != 1 ){
                    fprintf(stderr, "Error reading 'chamber_select'. Expected 1 argument.\n");
                    exit(EXIT_FAILURE);
                }
            break;
            case DATA_FILE_NAME: 
                if( get_args( program_p, command_index, linecpy ) != 1 ){
                    fprintf(stderr, "Error reading 'chamber_select'. Expected 1 argument.\n");
                    exit(EXIT_FAILURE);
                }
            break; 
        }
    }
    fclose( program_filep );
    
    return program_p;
}

static int get_args( Program_Info *program_p, int command_index, char *line ){
    int i = 0; 
    char *token;
    token = strtok(line, " ");
    while( token != NULL ){
        token = strtok( NULL, " ");
        if( token == NULL ) break;
        if ( command_index == SET_POINT_TIMES ){
            if( (i==0 && atoi( token ) == 0) || ( i>0 && atoi( token ) > program_p->setPoints[i-1].setPoint_time) ){
                program_p->setPoints[i].setPoint_time = atoi( token );
            }
            else{
                fprintf(stderr, "setPoint_times arguments must be in ascending order, positive and start at zero.\n");
                exit(EXIT_FAILURE);
            }
        }
        if ( command_index == SET_POINT_TEMPERATURES ){
            if( atoi( token ) >= 0 && atoi( token ) <= MAX_TEMP ){
                program_p->setPoints[i].setPoint_oven_temp = atoi( token );
                if( atoi(token) < 25 ){
                    fprintf(stderr,"Warning: Tempertures below ambient are not achievable.\n");
                }
            }
            else{
                fprintf(stderr, "setPoint_oven_temp arguments must be 0<= temp <= 120C.\n");
                exit(EXIT_FAILURE);
            }
        }
        if ( command_index == SET_POINT_OVEN_HUMIDITY ){
            if( atoi( token ) >= 0 && atoi( token ) <= 100 ){
                program_p->setPoints[i].setPoint_oven_humidity = atoi( token );
            }
            else{
                fprintf(stderr, "setPoint_oven_humidity arguments must be 0<= %%RH <= 100.\n");
                exit(EXIT_FAILURE);
            }
        }
        if ( command_index == SET_POINT_UV_HUMIDITY ){
            if( atoi( token ) >= 0 && atoi( token ) <= 100 ){
                program_p->setPoints[i].setPoint_uvBox_humidity = atoi( token );
        
            }
            else{
                fprintf(stderr, "setPoint_uvBox_humidity arguments must be 0<= %%RH <= 100.\n");
                exit(EXIT_FAILURE);
            }
        }
        if ( command_index == SET_POINT_UVPERCENT ){
            if( atoi( token ) >= 0 && atoi( token ) <= 100 ){
                program_p->setPoints[i].setPoint_UVPercent = atoi( token );
                if( atoi(token) < 20 ) fprintf( stderr, "UV levels below 20%% not permitted\n");
            }
            else{
                fprintf(stderr, "setPoint_uv_level arguments must be 0<= %%UV <= 100.\n");
                exit(EXIT_FAILURE);
            }
        }
        if ( command_index == LOG_STEPSIZE ){
            if( atoi (token) > 0 && atoi(token) < 256  ){
                program_p->log_stepSize = atoi( token );
            }
            else{
                fprintf(stderr, "log_stepSize must be positive integer.\n");
                exit(EXIT_FAILURE);
            }
        }
        if ( command_index == CHAMBER_SELECT ){
            if( atoi( token ) == 01 || atoi( token ) == 10 || atoi( token ) == 11 ){
                program_p->chamber_select = atoi( token );   
            }
            else{ 
                fprintf(stderr, "chamber_select format incorrect. At least 1 Chamber must be ON. \n");
                exit(EXIT_FAILURE);
            }
        }
        if( command_index == DATA_FILE_NAME ){
            if( strlen( token ) > 2 && strlen( token ) < FILE_NAME_LENGTH ){
                strcpy(program_p->data_file_name, token); 
            }
            else{ 
                fprintf(stderr, "data_file_name incorrect. Must be between 2 and FILE_NAME_LENGTH ASCII characters\n");
                exit(EXIT_FAILURE);
            }
        }
        i++;
    }
    return i;             //Number of data points read 
}

static void str_maker(char *line){ 
	int i = 0; 
	bool line_ends = false;
	while( line[i] != '\0' ){ 					//Append a NULL byte at the first newline or carriage return 
		if(line[i] == '\n' || line[i] == '\r'){ 			
			line[i] = '\0'; 
			line_ends = true;
		}
		++i; 
	}
	if(line_ends == false){ 				//Ensures a single line of data isn't so long that it doesn't fit in BUFSIZEE 
		fprintf(stderr,"Unacceptable file format. Single file line too long. Make sure final line has a newline. \n"); 
		exit(EXIT_FAILURE);
	}
}

static int Program_Info_Printer( Program_Info *program ){
    printf("num_setPoints = %d\n", program->num_setPoints);
    printf("log_stepSize = %d\n", program->log_stepSize);
    printf("chamber_select = %d\n", program->chamber_select);
    for(int i = 0; i < program->num_setPoints; i++){
        printf("\nSet Point:%d    *************\n",i);
        printf("setPoint_time = %i\n",program->setPoints[i].setPoint_time);
        printf("setPoint_temperature = %d\n",program->setPoints[i].setPoint_oven_temp);
        printf("setPoint_oven_humidity = %d\n",program->setPoints[i].setPoint_oven_humidity);
        printf("setPoint_uvBox_humidity = %d\n",program->setPoints[i].setPoint_uvBox_humidity);
        printf("setPoint_uv_level = %d%%\n",program->setPoints[i].setPoint_UVPercent);
        printf("\n data_file_name = %s\n",program->data_file_name);
    }
    printf("**************************\n Success!\n");
    return 0; 
}
//Big Endian! 
int Program_hex_writer( Program_Info *program, char *filename ){
    FILE *outfile = fopen( filename, "w"); 
    
    fprintf( outfile,"%02X%02X%02X", program->num_setPoints, program->log_stepSize, program->chamber_select );
    
    for(int i = 0; i<program->num_setPoints; i++ ){
         fprintf( outfile,"%04X%02X%02X%02X%02X", program->setPoints[i].setPoint_time, program->setPoints[i].setPoint_oven_temp,
                                                  program->setPoints[i].setPoint_oven_humidity, program->setPoints[i].setPoint_uvBox_humidity, 
                                                  program->setPoints[i].setPoint_UVPercent );
    }
    fclose( outfile );
    return 0; 
}
//Little Endian!    
static int Program_Bin_Writer( Program_Info *program, char *filename ){
    uint8_t buffer1[3] = { (uint8_t)program->num_setPoints, program->log_stepSize, program->chamber_select }; 
    uint8_t buffer2[4] = {0};       
    uint16_t buffer3 = 0; 
    char buffer4[FILE_NAME_LENGTH];
    strcpy(buffer4, program->data_file_name);
    
    FILE *outfile = fopen( filename, "w"); 
    
    fwrite( buffer4, sizeof(*buffer4), sizeof(buffer4), outfile ); 
    fwrite( buffer1, sizeof(*buffer1), sizeof(buffer1), outfile ); 
    
    for( int i = 0; i < program->num_setPoints; i++){
        buffer2[0] = program->setPoints[i].setPoint_oven_temp;
        buffer2[1] = program->setPoints[i].setPoint_oven_humidity;
        buffer2[2] = program->setPoints[i].setPoint_uvBox_humidity;
        buffer2[3] = program->setPoints[i].setPoint_UVPercent; 
        
        buffer3 = (uint16_t) program->setPoints[i].setPoint_time; 
        
        fwrite( &buffer3, sizeof(buffer3), 1, outfile );
        fwrite( buffer2, sizeof(*buffer2), sizeof(buffer2), outfile );
    }
    fclose(outfile);
    return 0; 
}
    
    
