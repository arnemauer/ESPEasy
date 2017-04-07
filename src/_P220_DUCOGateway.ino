//#################################### Plugin 220: DUCOGateway ########################################
//
//  DUCO GATEWAY to read DUCO BOX information
//  http://arnemauer.nl/ducobox-gateway/
//#######################################################################################################



#define PLUGIN_220
#define PLUGIN_ID_220           220
#define PLUGIN_NAME_220         "DUCO Gateway"
#define PLUGIN_VALUENAME1_220   "FLOW_PERCENTAGE" // networkcommand
#define PLUGIN_VALUENAME2_220   "DUCO_STATUS" // networkcommand
#define PLUGIN_VALUENAME3_220   "CO2_PPM" // nodeparaget 2 74
#define PLUGIN_VALUENAME4_220   "CO2_TEMP" // // networkcommand
#define PLUGIN_READ_TIMEOUT_220   1500 // DUCOBOX askes "live" CO2 sensor info, so answer takes sometimes a second.

#define DUCOBOX_NODE_NUMBER     1
#define CO2_NODE_NUMBER         2

boolean Plugin_220_init = false;

//18 bytes
#define CMD_READ_CO2_SIZE 18
byte cmdReadCO2[CMD_READ_CO2_SIZE] = { 0x4e , 0x6f , 0x64 , 0x65 , 0x50 , 0x61 , 0x72 , 0x61 , 0x47 , 0x65 , 0x74, 0x20, 0x32, 0x20, 0x37, 0x34, 0x0d, 0x0a};
#define ANSWER_READ_CO2_SIZE 16
byte answerReadCO2[ANSWER_READ_CO2_SIZE] = {0x6e, 0x6f , 0x64 , 0x65 , 0x70 , 0x61 , 0x72 , 0x61 , 0x67 , 0x65 , 0x74 , 0x20 , 0x32 , 0x20 , 0x37 , 0x34};

// 9 bytes
#define CMD_READ_NETWORK_SIZE 9
byte cmdReadNetwork[CMD_READ_NETWORK_SIZE] = {0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x0d, 0x0a};
#define ANSWER_READ_NETWORK_SIZE 8
byte answerReadNetwork[ANSWER_READ_NETWORK_SIZE] = {0x6e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x0d};

// define buffers, large, indeed. The entire datagram checksum will be checked at once
#define SERIAL_BUFFER_SIZE 750
//#define NETBUF_SIZE 600
byte serial_buf[SERIAL_BUFFER_SIZE];
unsigned int P220_bytes_read = 0;
byte task_index = 0;

float duco_data[4];

int stopword = 0; // 0x0d 0x20 0x20 0x0d 0x3e 0x20

unsigned int rows[15]; // byte number where a new row starts
unsigned int row_count = 0;

boolean Plugin_220(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;
  static byte connectionState=0;

  switch (function)
  {

    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_220;
        Device[deviceCount].VType = SENSOR_TYPE_QUAD; // 3 outputs of 2 bytes each = 6 bytes
        Device[deviceCount].ValueCount = 4;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = false;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }


    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_220);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_220));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_220));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_220));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_220));
      break;
      }

      case PLUGIN_WEBFORM_LOAD:
         {
           char tmpString[256];
           sprintf_P(tmpString, PSTR("<TR><TD>--------------------------<TD> Dont use 'IDX' above!"));
           string += tmpString;
           sprintf_P(tmpString, PSTR("<TR><TD>IDX Flow percentage:<TD><input type='text' name='plugin_220_IDX1' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[0]);
           string += tmpString;
           sprintf_P(tmpString, PSTR("<TR><TD>IDX DUCOBOX status:<TD><input type='text' name='plugin_220_IDX2' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[1]);
           string += tmpString;
           sprintf_P(tmpString, PSTR("<TR><TD>IDX CO2-sensor PPM:<TD><input type='text' name='plugin_220_IDX3' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[2]);
           string += tmpString;
           sprintf_P(tmpString, PSTR("<TR><TD>IDX CO2-sensor temperture:<TD><input type='text' name='plugin_220_IDX4' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[3]);
           string += tmpString;
        //   sprintf_P(tmpString, PSTR("<TR><TD>RX Receive Timeout (mSec):<TD><input type='text' name='plugin_220_rxwait' value='%u'>"), ExtraTaskSettings.TaskDevicePluginConfigLong[4]);
        //   string += tmpString;
           sprintf_P(tmpString, PSTR("<TR><TD>--------------------------<TD>Dont use 'Send Data to controller' below! "));
           string += tmpString;
           success = true;
           break;
         }

       case PLUGIN_WEBFORM_SAVE:
         {
           String plugin1 = WebServer.arg("plugin_220_IDX1");
           ExtraTaskSettings.TaskDevicePluginConfigLong[0] = plugin1.toInt();
           String plugin2 = WebServer.arg("plugin_220_IDX2");
           ExtraTaskSettings.TaskDevicePluginConfigLong[1] = plugin2.toInt();
           String plugin3 = WebServer.arg("plugin_220_IDX3");
           ExtraTaskSettings.TaskDevicePluginConfigLong[2] = plugin3.toInt();
           String plugin4 = WebServer.arg("plugin_220_IDX4");
           ExtraTaskSettings.TaskDevicePluginConfigLong[3] = plugin4.toInt();
        //   String plugin5 = WebServer.arg("plugin_220_rxwait");
        //   ExtraTaskSettings.TaskDevicePluginConfigLong[4] = plugin5.toInt();
           success = true;
           break;
         }

    case PLUGIN_INIT:
      {


        addLog(LOG_LEVEL_ERROR,"!P220 - start init!");

        LoadTaskSettings(event->TaskIndex);

        Serial.begin(115200, SERIAL_8N1);
        Plugin_220_init = true;

      duco_data[0] = 0; // flow %
      duco_data[1] = 0; // DUCO_STATUS
      duco_data[2] = 0; // CO2 ppm
      duco_data[3] = 0.0; // co2 temp

        // temp woraround, ESP Easy framework does not currently prepare this...
        for (byte y = 0; y < TASKS_MAX; y++){
                       if (Settings.TaskDeviceNumber[y] == PLUGIN_ID_220){
                         task_index = y;
                         break;
                       }
        }

        addLog(LOG_LEVEL_DEBUG,"!P220 - init done");
        success = true;
        break;
      }


      case PLUGIN_READ:
      {
            if (Plugin_220_init)
            {
              String log = F("READDUCO");
              addLog(LOG_LEVEL_DEBUG, log);

          readNetworkList();
          readCO2PPM();

          UserVar[event->BaseVarIndex]   = duco_data[0]; // flow percentage
         Plugin_220_Controller_Update(task_index, event->BaseVarIndex,  ExtraTaskSettings.TaskDevicePluginConfigLong[0], SENSOR_TYPE_SINGLE,0);

UserVar[event->BaseVarIndex+1]   = duco_data[1]; // flow percentage
Plugin_220_Controller_Update(task_index, event->BaseVarIndex+1,  ExtraTaskSettings.TaskDevicePluginConfigLong[1], SENSOR_TYPE_SINGLE,0);

UserVar[event->BaseVarIndex+2]   = duco_data[2]; // flow percentage
Plugin_220_Controller_Update(task_index, event->BaseVarIndex+2,  ExtraTaskSettings.TaskDevicePluginConfigLong[2], SENSOR_TYPE_SINGLE,0);

UserVar[event->BaseVarIndex+3]   = duco_data[3]; // flow percentage
Plugin_220_Controller_Update(task_index,event->BaseVarIndex+3,  ExtraTaskSettings.TaskDevicePluginConfigLong[3], SENSOR_TYPE_SINGLE,1);

            success = true;
            break;
          } // end off   if (Plugin_049_init)

      } // end of case PLUGIN_READ:



      return success;
      }

}


unsigned int getValueByNodeNumberAndColumn(unsigned int nodeNumber,unsigned int columnNumber ){
//0,1,2,3 = headers etc.
//rows[0] = 0; // row zero = command in answer from duco
//row_count = 1;
unsigned int start_byte_number = 0;
unsigned int end_byte_number = 0;
unsigned int row_number;
for (int k = 4; k <= row_count; k++) {
    unsigned int number_size = 0;
    unsigned int row_node_number = 0;

      for (int j = 0; j <= 6; j++) {
        if( serial_buf[ rows[k]+j ] != 0x20){
          if(serial_buf[ rows[k]+j ] >= '0' && serial_buf[ rows[k]+j ] <= '9'){
            if(number_size > 0){
              row_node_number = row_node_number + (( serial_buf[ rows[k]+j ] - '0' ) * (10 * number_size)) ;
            }else{
              row_node_number = row_node_number + ( serial_buf[ rows[k]+j ] - '0' );
              number_size++;
            }
          }
        }
    }
/*
    String log7 = F("nodenr: ");
    char lossebyte7[10];
    sprintf_P(lossebyte7, PSTR("%u - %u"), row_node_number,nodeNumber);
    log7 += lossebyte7;
    addLog(LOG_LEVEL_DEBUG, log7);
*/
    if(row_node_number == nodeNumber){
      //rows[k] == startbyte of row with device id
      start_byte_number = rows[k];
      end_byte_number   = rows[k+1];
      //row_number        = k;
      break;
    }
}
/*
String log = F("bytes btween: ");
char lossebytee[10];
sprintf_P(lossebytee, PSTR("%u - %u"), start_byte_number,end_byte_number);
log += lossebytee;
addLog(LOG_LEVEL_DEBUG, log);

*/

    if(start_byte_number > 0){

          unsigned int column_counter = 0;
          for (int i = start_byte_number; i <= end_byte_number; i++) { // smaller then P220_bytes_read OR... the next row start?
              if(serial_buf[i] == 0x7c){
/*
                String log11 = F("column colnr-bytenr: ");
                char lossebyte11[10];
                sprintf_P(lossebyte11, PSTR("%u - %u"), column_counter,i);
                log11 += lossebyte11;
                addLog(LOG_LEVEL_DEBUG, log11);
*/

                column_counter++;
                if(column_counter >= columnNumber){
                  return i+1;
                  break;
                }
              }
          }
    }
  return 0;
}

int sendSerialCommand(byte command[], int size){
char log[200];
char lossebyte[10];

    // RECEIVE RESPONSE
    int error=0;
    //byte response = 0;
    for (int m = 0; m <= size-1; m++) {
       delay(10); // 20ms = buffer overrun while reading serial data. // ducosoftware uses +-15ms.
       int bytesSend = Serial.write(command[m]);
       if(bytesSend != 1){
         error=1;
         break;
       }
    } // end of forloop

    // if error: log, clear command and flush buffer!
    if(error == 1){
      String log = F("DUCOGW: Error, failed sending command. Clear command and buffer");
      addLog(LOG_LEVEL_ERROR, log);

      // press "enter" to clear send bytes.
      Serial.write(0x0d);
      delay(20);
      Serial.write(0x0a);

      // empty buffer
      long start = millis();
      while ((millis() - start) < 200) {
        while(Serial.available() > 0){
          Serial.read();
        }
      }
      return 0;

    }else{
      String log = F("DUCOGW: send command successfull!");
      addLog(LOG_LEVEL_DEBUG, log);
      return 1;
    }

  } // end of sendSerialCommand



// read CO2 temp, ventilation %, DUCO status
void readNetworkList(){
P220_bytes_read = 0;
stopword = 0;
  // SEND COMMAND
  int commandSendResult = sendSerialCommand(cmdReadNetwork, CMD_READ_NETWORK_SIZE);

// RECEIVE RESPONSE
if(commandSendResult == 1){
  long start = millis();
  int counter = 0;
  while (((millis() - start) < PLUGIN_READ_TIMEOUT_220) && (stopword < 6)) {
    if (Serial.available() > 0) {
      byte ch = Serial.read();
      serial_buf[P220_bytes_read] = ch;
      P220_bytes_read++;

      if ((ch == 0x0d) || (ch == 0x20) || (ch == 0x3e)) {
        // karakter van stopwoord gevonden, stopwoordteller ophogen
        stopword++;
      }else{
        stopword = 0;
      }
    }
  }// end while

/*
        String logstring99 = F("serialbytes: ");
        char lossebyte99[6];
        sprintf_P(lossebyte99, PSTR("%u "), P220_bytes_read);
        logstring99 += lossebyte99;
        addLog(LOG_LEVEL_INFO, logstring99);
*/
//delay(20);
//  node|addr|type    |ptcl|cerr|prnt|asso|stts|stat|cntdwn|%dbt|trgt|cval|snsr|ovrl|capin 1
//logArray(serial_buf, 120+39, 39);
//delay(20);

  if(stopword == 6){

      // check returncommand
      int commandReceived = false;
          for (int j = 0; j <= ANSWER_READ_NETWORK_SIZE-1; j++) {
            if(serial_buf[j] == answerReadNetwork[j]){
              commandReceived = true;
            }else{
              commandReceived = false;
              break;
            }
        }

        if(commandReceived == true){
          String log = F("DUCOGW: EXPECTED command network received.");
          addLog(LOG_LEVEL_DEBUG, log);
        }else{
          String log = F("DUCOGW: unexpected command network received.");
          addLog(LOG_LEVEL_DEBUG, log);
        }


      if(commandReceived == true){
      //  if(1==2){

      // gegevens verwerken

        // DUCOBOX prints a device list; lets find the startbyte of each row.
        // row 0 = ".network" (command of response
        // row 1 =  "Network:"
        // row 2 = "  --- start list ---"
        // row 3 = "  node|addr|type|ptcl|cerr|prnt|asso|stts|stat|cntdwn|%dbt|trgt|cval|snsr|ovrl|capin |capout|tree|temp|info" (header)

          rows[0] = 0; // row zero = command in answer from duco
          row_count = 1;

          // END divide by rows
          String log = F("regel startbytes: ");
          char lossebytee[20];


          for (int i = 0; i <= P220_bytes_read-1; i++) {
            if(serial_buf[i] == 0x0d){
              rows[row_count] = i;
              row_count++;
              //sprintf_P(lossebytee, PSTR("%u>%d - "), row_count, i);
              //log += lossebytee;
            }
          }

          rows[row_count+1] = P220_bytes_read;

          //addLog(LOG_LEVEL_DEBUG, log);

            /////////// READ VENTILATION % ///////////
            // row 4 = duco box
            // 7c = scheidingslijn
            // for loop, na 10 x 7c = . %dbt = ventilation%.
            unsigned int start_ventilation_byte = getValueByNodeNumberAndColumn(DUCOBOX_NODE_NUMBER, 10);

/*
            String log6 = F("start_ventilation_byte: ");
            char lossebyte6[10];
            sprintf_P(lossebyte6, PSTR("%u "), start_ventilation_byte);
            log6 += lossebyte6;
            addLog(LOG_LEVEL_DEBUG, log6);
          delay(10);
*/

            if(start_ventilation_byte > 0){
              unsigned int number_size = 0;
              unsigned int ventilation_value_bytes[3];
              for (int j = 0; j <= 3; j++) {
                if( (serial_buf[start_ventilation_byte+j] == 0x20) || (serial_buf[start_ventilation_byte+j] == 0x7C) ){ // 0x20 is spatie achter het getal, 0x7c is einde value, negeren!
                  number_size = j-1;
                  break;
                }else if (serial_buf[start_ventilation_byte+j] >= '0' && serial_buf[start_ventilation_byte+j] <= '9'){
                  ventilation_value_bytes[j] = serial_buf[start_ventilation_byte+j] - '0';
                }else{
                  ventilation_value_bytes[j] = 0;
                }
              }

              // logging

            String logstring4 = F("Ventilation valuebytes: ");
            char lossebyte4[25];
            sprintf_P(lossebyte4, PSTR("%02X %02X %02X sizenr %u"), serial_buf[start_ventilation_byte],serial_buf[start_ventilation_byte+1],serial_buf[start_ventilation_byte+2],number_size);
            logstring4 += lossebyte4;
            addLog(LOG_LEVEL_DEBUG, logstring4);

              delay(20);


          unsigned int temp_ventilation_value = 0;
          if(number_size == 0){ // one byte number
                  temp_ventilation_value  = (ventilation_value_bytes[0]);
          }else if(number_size == 1){ // two byte number
                  temp_ventilation_value  = ((ventilation_value_bytes[0] * 10) + (ventilation_value_bytes[1]));
          }else if(number_size == 2){ // three byte number
                  temp_ventilation_value  = ((ventilation_value_bytes[0] * 100) + (ventilation_value_bytes[1] * 10) + (ventilation_value_bytes[2]));
          }

          if( (temp_ventilation_value >= 0) && (temp_ventilation_value <= 100)){
            duco_data[0] = (float)temp_ventilation_value;
          }else{
            // logging
          String logstring = F("DUCOBOX: ventilation value not between 0 and 100%. Ignore value.");
          addLog(LOG_LEVEL_DEBUG, logstring);
          }

          // logging
        String logstring = F("Ventilation value: ");
        char lossebyte[15];
        sprintf_P(lossebyte, PSTR("%u"), temp_ventilation_value);
        logstring += lossebyte;
        addLog(LOG_LEVEL_DEBUG, logstring);
        }
            /////////// READ VENTILATION % ///////////



    /////////// READ DUCOBOX status ///////////
    // row 4 = duco box
    // for loop, na 8 x 7c. = STAT
    // domoticz value -> "duco value" = duco modename;
    // 0 -> "auto" = AutomaticMode;

    // 1 -> "man1" = ManualMode1;
    // 2 -> "man2" = ManualMode2;
    // 3 -> "man3" = ManualMode3;

    // 4 -> "empt" = EmptyHouse;
    // 5 -> "alrm" = AlarmMode;

    // 11 -> "cnt1" = PermanentManualMode1;
    // 12 -> "cnt2" = PermanentManualMode2;
    // 13 -> "cnt3"= PermanentManualMode3;

    // 20 -> "aut0" = AutomaticMode;
    // 21 -> "aut1" = Boost10min;
    // 22 -> "aut2" = Boost20min;
    // 23 -> "aut3" = Boost30min;
    unsigned int start_ducoboxstatus_byte = getValueByNodeNumberAndColumn(DUCOBOX_NODE_NUMBER, 8);

    // logging
  String logstring = F("ducobox statusbytes: ");
  char lossebyte[10];
  sprintf_P(lossebyte, PSTR("%c %c %c %c"), serial_buf[start_ducoboxstatus_byte],serial_buf[start_ducoboxstatus_byte+1],serial_buf[start_ducoboxstatus_byte+2],serial_buf[start_ducoboxstatus_byte+3]);
  logstring += lossebyte;
  addLog(LOG_LEVEL_DEBUG, logstring);

    if(start_ducoboxstatus_byte > 0){
        char ducostatus_value_bytes[3];
              unsigned int number_size = 0;

              for (int j = 0; j <= 5; j++) {
                if( (serial_buf[start_ducoboxstatus_byte+j] == 0x20) || (serial_buf[start_ducoboxstatus_byte+j] == 0x7C) ){ // 0x20 is spatie achter het getal, 0x7c is einde value, negeren!
                  number_size = j-1;
                  break;
                }else if (serial_buf[start_ducoboxstatus_byte+j] >= '0' && serial_buf[start_ducoboxstatus_byte+j] <= '9'){
                  ducostatus_value_bytes[j] = serial_buf[start_ducoboxstatus_byte+j] - '0';
                }else{
                  ducostatus_value_bytes[j] = 0;
                }
              }

//check aut = 41 55 54 4f = AUTO
if( (serial_buf[start_ducoboxstatus_byte+0] == 0x41) && (serial_buf[start_ducoboxstatus_byte+1] == 0x55) && (serial_buf[start_ducoboxstatus_byte+2] == 0x54)){
    // possible options are: AUTO, AUT0, AUT1, AUT2, AUT3
      if(serial_buf[start_ducoboxstatus_byte+3] == 0x4F){ // check if AUTO
        duco_data[1] = 0;
      }else{
          if (serial_buf[start_ducoboxstatus_byte+3] >= '0' && serial_buf[start_ducoboxstatus_byte+3] <= '9'){
              unsigned int modeNumber = serial_buf[start_ducoboxstatus_byte+3] - '0'; // convert ascii to int

              if(modeNumber == 0){ // check if AUT0
                duco_data[1] = 20;
              }else if(modeNumber == 1){ // check if AUT1
                duco_data[1] = 21;
              }else if(modeNumber == 2){ // check if AUT2
                duco_data[1] = 22;
              }else if(modeNumber == 3){ // check if AUT3
                duco_data[1] = 23;
              }
          }
      }
} else // end of AUT check

// 4d 41 4e
//check man
if( (serial_buf[start_ducoboxstatus_byte+0] == 0x4d) && (serial_buf[start_ducoboxstatus_byte+1] == 0x41) && (serial_buf[start_ducoboxstatus_byte+2] == 0x4e)){
    // possible options are: MAN1, MAN2, MAN3
          if (serial_buf[start_ducoboxstatus_byte+3] >= '0' && serial_buf[start_ducoboxstatus_byte+3] <= '9'){
              unsigned int modeNumber = serial_buf[start_ducoboxstatus_byte+3] - '0'; // convert ascii to int

              if(modeNumber == 1){ // check if MAN1
                duco_data[1] = 1;
              }else if(modeNumber == 2){ // check if MAN2
                duco_data[1] = 2;
              }else if(modeNumber == 3){ // check if MAN3
                duco_data[1] = 3;
              }
          }
} else // end of MAN check

// 43 4e 54
//check CNT
if( (serial_buf[start_ducoboxstatus_byte+0] == 0x43) && (serial_buf[start_ducoboxstatus_byte+1] == 0x4e) && (serial_buf[start_ducoboxstatus_byte+2] == 0x54)){
    // possible options are: MAN1, MAN2, MAN3
          if (serial_buf[start_ducoboxstatus_byte+3] >= '0' && serial_buf[start_ducoboxstatus_byte+3] <= '9'){
              unsigned int modeNumber = serial_buf[start_ducoboxstatus_byte+3] - '0'; // convert ascii to int

              if(modeNumber == 1){ // check if MAN1
                duco_data[1] = 11;
              }else if(modeNumber == 2){ // check if MAN2
                duco_data[1] = 12;
              }else if(modeNumber == 3){ // check if MAN3
                duco_data[1] = 13;
              }
          }
} else // end of MAN check

//45 4d 50 54
//check empt
if( (serial_buf[start_ducoboxstatus_byte+0] == 0x45) && (serial_buf[start_ducoboxstatus_byte+1] == 0x4d) && (serial_buf[start_ducoboxstatus_byte+2] == 0x50) && (serial_buf[start_ducoboxstatus_byte+3] == 0x54)){
  duco_data[1] = 4;
}else

//41 4c 52 4d
//check alrm
if( (serial_buf[start_ducoboxstatus_byte+0] == 0x41) && (serial_buf[start_ducoboxstatus_byte+1] == 0x4c) && (serial_buf[start_ducoboxstatus_byte+2] == 0x52) && (serial_buf[start_ducoboxstatus_byte+3] == 0x4d)){
  duco_data[1] = 5;
}else{
  duco_data[1] = 999;
  // 999 = NOT AVAILABLE
  // logging
String logstring = F("DUCOBOX: status not available.");
addLog(LOG_LEVEL_DEBUG, logstring);
}

String logstring = F("Ducobox status mode: ");
char lossebyte[6];
sprintf_P(lossebyte, PSTR("%d "), (int)duco_data[1]);
logstring += lossebyte;
addLog(LOG_LEVEL_DEBUG, logstring);




            } //             if(start_ducoboxstatus_byte > 0){
            /////////// READ DUCOBOX status ///////////




            /////////// READ CO2 temperture % ///////////
            // row 5 = co2
            // for loop, na 18 x 7c. = temp
            unsigned int start_CO2temp_byte = getValueByNodeNumberAndColumn(CO2_NODE_NUMBER, 18);
            int CO2temp_value_bytes[3];

            if(start_CO2temp_byte > 0){
              for (int j = 0; j <= 3-1; j++) {
                if(serial_buf[start_CO2temp_byte+j] == 0x20){ // 0d is punt achter het getal, negeren!
                  CO2temp_value_bytes[j] = 0;
                }else if (serial_buf[start_CO2temp_byte+j] >= '0' && serial_buf[start_CO2temp_byte+j] <= '9'){
                  CO2temp_value_bytes[j] = serial_buf[start_CO2temp_byte+j] - '0';
                }else{
                  CO2temp_value_bytes[j] = 0;
                }
              }
              /*
              String logstring33 = F("CO2TEMP: ");
              char lossebyte33[20];
               sprintf_P(lossebyte33, PSTR("%02X %02X %02X "),CO2temp_value_bytes[0],CO2temp_value_bytes[1],CO2temp_value_bytes[2] );
               logstring33 += lossebyte33;
               addLog(LOG_LEVEL_ERROR,logstring33);
*/

            float temp_CO2_temp_value = (float)(CO2temp_value_bytes[0] *10) + (float)CO2temp_value_bytes[1] + (float)(CO2temp_value_bytes[2]/10.0);
            if (0 <= temp_CO2_temp_value && temp_CO2_temp_value <= 50){ // between
              duco_data[3] = temp_CO2_temp_value;
            }else{
              // logging
            String logstring = F("DUCOBOX: CO2 temp value not between 0 and 50 degrees celsius. Ignoring value.");
            char lossebyte[6];

            addLog(LOG_LEVEL_DEBUG, logstring);
            }


               String logstring3 = F("Ducobox CO temp: ");
               char lossebyte3[10];
               sprintf_P(lossebyte3, PSTR("%d%d.%d"), CO2temp_value_bytes[0], CO2temp_value_bytes[1], CO2temp_value_bytes[2]);
               logstring3 += lossebyte3;
               addLog(LOG_LEVEL_DEBUG, logstring3);

            }
            /////////// READ CO2 temperture % ///////////






      } // if(commandReceived == true){


  }else{ // if(stopword == 6){}
      // timout occurred, stopword not found!
      String log = F("DUCOGW: package receive timeout.");
      addLog(LOG_LEVEL_DEBUG, log);
  }

}// if(commandSendResult == 1){

//  return 0;
} // end of int readNetworkList(){



int readCO2PPM(){
P220_bytes_read = 0;
stopword = 0;

// SEND COMMAND: nodeparaget 2 74
int commandSendResult = sendSerialCommand(cmdReadCO2, CMD_READ_CO2_SIZE);

// RECEIVE RESPONSE
if(commandSendResult == 1){
  long start = millis();
  int counter = 0;
  while (((millis() - start) < PLUGIN_READ_TIMEOUT_220) && (stopword < 6)) {
    if (Serial.available() > 0) {
      byte ch = Serial.read();
      serial_buf[P220_bytes_read] = ch;
      P220_bytes_read++;

      if ((ch == 0x0d) || (ch == 0x20) || (ch == 0x3e)) {
        stopword++;         // karakter van stopwoord gevonden, stopwoordteller ophogen
      }else{
        stopword = 0;
      }
    }
  }// end while


  if(stopword == 6){

      // check returncommand
      int commandReceived = false;
          for (int j = 0; j <= ANSWER_READ_CO2_SIZE-1; j++) {
            if(serial_buf[j] == answerReadCO2[j]){
              commandReceived = true;
            }else{
              commandReceived = false;
              break;
            }
        }

        if(commandReceived == true){
          String log = F("DUCOGW: EXPECTED command read co2 received.");
          addLog(LOG_LEVEL_DEBUG, log);
        }else{
          String log = F("DUCOGW: unexpected command read co2 received.");
          addLog(LOG_LEVEL_DEBUG, log);
        }



      if(commandReceived == true){
//j
        for (int i = 0; i <= P220_bytes_read-1; i++) {
        // 2d 2d 3e 20 = "- - >  "
        if(serial_buf[i] == 0x2d && serial_buf[i+1] == 0x2d && serial_buf[i+2] == 0x3e && serial_buf[i+3] == 0x20){
/*
              char log[30];
                sprintf_P(log, PSTR("value found: %02X %02X %02X"), serial_buf[i+4],serial_buf[i+5],serial_buf[i+6]);
                addLog(LOG_LEVEL_ERROR,log);
*/
        // if ducobox failes to receive current co2 ppm than it says failed (66 61 69 6c 65 64).
        //if(serial_buf[i+4] == 0x66){
        if (serial_buf[i+4] >= '0' && serial_buf[i+4] <= '9'){
          // the first byte is a valid number...

                int value[4];
                unsigned int number_size = 0;
                for (int j = 0; j <= 4; j++) {
                  if(serial_buf[i+4+j] == 0x0d){ // 0d is punt achter het getal, negeren!
                  //  value[j] = 0;
                    number_size = j-1;
                    break;
                  }else if (serial_buf[i+4+j] >= '0' && serial_buf[i+4+j] <= '9'){
                    value[j] = serial_buf[i+4+j] - '0';
                  }else{
                    value[j] = 0;
                  }
                }

                String logstring3 = F("CO2 PPM: ");
                char lossebyte3[10];
                sprintf_P(lossebyte3, PSTR("%d%d%d"), value[0], value[1], value[2]);
                logstring3 += lossebyte3;
                addLog(LOG_LEVEL_DEBUG, logstring3);


              if(number_size == 0){
                      duco_data[2] = (float)(value[0]);
              }else if(number_size == 1){
                      duco_data[2] = (float)((value[0] * 10) + (value[1]));
              }else if(number_size == 2){
                      duco_data[2] = (float)((value[0] * 100) + (value[1] * 10) + (value[2]));
              }else if(number_size == 3){
                      duco_data[2] = (float)((value[0] * 1000) + (value[1] * 100) + (value[2] * 10) + (value[3]));
              }

      }else{
        String log = F("DUCOGW: No CO2 PPM value found in package.");
        addLog(LOG_LEVEL_DEBUG, log);
      }


        break; // ignore filtered value?
        }

        } // end for loop


      }// if(commandReceived == true){

      }else{ // if(stopword == 6){}
          // timout occurred, stopword not found!
          String log = F("DUCOGW: package receive timeout.");
          addLog(LOG_LEVEL_DEBUG, log);
      }


} //if(commandSendResult == 1){


  return 0;
}




int logArray(uint8_t array[], int len, int fromByte) {
unsigned int counter= 1;

  String logstring = F("Pakket ontvangen: ");
  char lossebyte[6];

  for (int i = fromByte; i <= len-1; i++) {

      sprintf_P(lossebyte, PSTR("%02X"), array[i]);
      logstring += lossebyte;
      if( (counter * 60) == i){
        counter++;
        logstring += F("END");

        addLog(LOG_LEVEL_ERROR,logstring);
        delay(50);
         logstring = F("");
      }

  }

  logstring += F("END");
 addLog(LOG_LEVEL_ERROR,logstring);
 return -1;
}


// https://www.letscontrolit.com/forum/viewtopic.php?f=4&t=2104&p=9908&hilit=no+svalue#p9908

void Plugin_220_Controller_Update(byte TaskIndex, byte BaseVarIndex, int IDX, byte SensorType, int valueDecimals)
{

//if (Settings.Protocol && IDX > 0)
//{
LoadTaskSettings(TaskIndex);
// settings.procotol = 3 bytes, 3 controllers. loop en check if enabled?
//for (byte x=0; x < CONTROLLER_MAX; x++) { }
// see /src/Controller.ino line 33-47

ExtraTaskSettings.TaskDeviceValueDecimals[0] = valueDecimals;

// backward compatibility for version 148 and lower...
#if defined(BUILD) && BUILD <= 150
#define CONTROLLER_MAX 0
#endif

for (byte x=0; x < CONTROLLER_MAX; x++){
  byte ControllerIndex = x;

if (Settings.ControllerEnabled[ControllerIndex] && Settings.Protocol[ControllerIndex])
  {


      byte ProtocolIndex = getProtocolIndex(Settings.Protocol[ControllerIndex]);
      struct EventStruct TempEvent;
      TempEvent.TaskIndex = TaskIndex;
      TempEvent.ControllerIndex = ControllerIndex;
      TempEvent.BaseVarIndex = BaseVarIndex; // ophogen per verzending.
      TempEvent.idx = IDX;
      TempEvent.sensorType = SensorType;
      CPlugin_ptr[ProtocolIndex](CPLUGIN_PROTOCOL_SEND, &TempEvent, dummyString);
      char log[30];
            sprintf_P(log, PSTR("DATA SEND! jeej") );
            addLog(LOG_LEVEL_DEBUG,log);
    }
  }

}
//}
