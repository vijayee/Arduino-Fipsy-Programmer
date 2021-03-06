#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#pragma hdrstop
#include "MachXO2.h"

#include <SPI.h>


/* Convenient types */
typedef unsigned char uint8_t;
typedef unsigned short WORD;
typedef unsigned long uint32_t;

/* Windows function conversions */
#define Sleep(t)  usleep(t*1000)

/* Subroutine declarations */
uint32_t Fipsy_Open(void);
uint32_t Fipsy_Close(void);
uint32_t Fipsy_ReadDeviceID(uint8_t *DeviceID);
uint32_t Fipsy_ReadUniqueID(uint8_t *UniqueID);
uint32_t Fipsy_EraseAll(void);
uint32_t Fipsy_LoadConfiguration(void);
uint32_t Fipsy_WriteFeatures(uint8_t *FeatureRow, uint8_t *Feabits);
uint32_t Fipsy_WriteConfiguration(char *JEDECFileName);
char JEDEC_SeekNextNonWhitespace(void);
char JEDEC_SeekNextKeyChar(void);
uint8_t JEDEC_ReadFuseuint8_t(uint8_t *Fuseuint8_t);
 
/* General purpose subroutine declarations */
uint32_t SPI_Transaction(uint8_t Count, void *Data); 
int ErrorMessage(char *ErrorDescription, char *ErrorType);
// Predefined error messages
#define ErrorFileNotFound()         ErrorMessage("Unable to open the specified file", "File Error")
#define ErrorFileFormat()           ErrorMessage("File format is not valid for JEDEC", "File Error")
#define ErrorBadSetting()           ErrorMessage("JEDEC file has SPI slave port disabled - programming aborted", "File Error")
#define ErrorBadValue()             ErrorMessage("Specified value is out of range", "Parameter Error")
#define ErrorBadLength()            ErrorMessage("Requested length is not valid", "Parameter Error")
#define ErrorBadAddress()           ErrorMessage("Bad offset or length given", "Parameter Error")
#define ErrorNullPointer()          ErrorMessage("Data pointer provided is NULL", "Parameter Error")
#define ErrorNotErased()            ErrorMessage("The FPGA must be erased to program", "Operation Order Error")
#define ErrorNotOpen()              ErrorMessage("The SPI connection has not been initialized", "Operation Order Error")
#define ErrorTimeout()              ErrorMessage("Timed out waiting for FPGA busy", "Timeout Error")
#define ErrorBadUsage()             ErrorMessage("fipsyloader <jedec file name>", "Usage")
#define ErrorHardware()             ErrorMessage("Could not open SPI port", "Hardware Error")

/* Local data definitions */

// General purpose message buffer
char UserMsg[1000];

// SPI port stream identifier or handle
int SPIPort = 0;

// General purpose buffer used to transact data on SPI
// This is bigger than most routines need, but reduces repeated declarations
// and is bigger than the actual SPI transaction can be, meaning there is 
// always enough room
uint8_t SPIBuf[100];

// Count of uint8_ts in the SPI buffer
// This is filled based on the count to send in a transaction
// On return from a transaction, this will specify the number of uint8_ts returned
// uint8_ts returned is the entire SPI transaction, not just the data, so the value
// should not change unless something went wrong.
uint8_t MachXO2_Count = -1;
 
// Macro to set the most frequently used dummy values for the SPI buffer
uint8_t SPIBUF_DEFAULT[20] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
#define SPIBUFINIT                  { MachXO2_Count = 0; memcpy(SPIBuf, SPIBUF_DEFAULT, 20); }

// Defines for key elements of SPI transactions
#define MachXO2_Command             SPIBuf[0]
#define pMachXO2_Operand            (&SPIBuf[1])
#define pMachXO2_Data               (&SPIBuf[4])

// Macro to complete an SPI transaction of the specified count with the global buffer
// The global buffer count is set with the parameter, saving coding steps 
#define MachXO2_SPITrans(c)         { MachXO2_Count = c; SPI_Transaction(MachXO2_Count, SPIBuf); }   

// Flag indicating hardware has been opened and the port is ready
#define HWIsOpen                    (SPIPort > 0)

// Flag indicating that a caller has erased the FPGA
uint8_t FPGAIsErased = 0;

// File stream pointer for JEDEC file 
// See comments with the parsing support functions
FILE *JFile = NULL;

// Predefined advanced error message macro including closing the file on a format error
#define EXIT_FILEFORMATERROR    { fclose(JFile); return(ErrorFileFormat()); }
#define EXIT_FILEBADSETTINGERROR  { fclose(JFile); return(ErrorBadSetting()); }



String incoming;


void setup() {
  Serial.begin(115200); // use the same baud-rate as the python side

  //Set up SPI with Fipsy
  //digitalWrite(SS, HIGH);   
  pinMode(SS, OUTPUT);
  SPI.begin ();
  SPI.setBitOrder(MSBFIRST);
  
}

void loop() {
 // send data only when you receive data:
        if (Serial.available() > 0) {
                // read the incoming uint8_t:
                incoming = Serial.readStringUntil('\n');

                // say what you got:
                Serial.print("I received: ");
                Serial.println(incoming);

                if (incoming=="do_check_id"){
                  delay(100);
                   uint8_t id[10];

                    // Configure SPI connection to module
                    if(!Fipsy_Open()){
                        Serial.println("Error: Could not open SPI port");
                        return;
                      }
                  
                    // Read and print the ids
                    if(!Fipsy_ReadDeviceID(id)) { Fipsy_Close(); return(-1); };
                    printf("Device ID = %02X %02X %02X %02X\n\r", id[0], id[1], id[2], id[3]);
                  
                  //Serial.println("101012");
                }
        }

 
   
   /*
   
    // FPGA programming - these print error messages if they fail
    // Try to erase the device 
    if(!Fipsy_EraseAll()) { Fipsy_Close(); return(-1); };
    // Program FPGA 
    if(!Fipsy_WriteConfiguration(argv[1])){ 
      Fipsy_Close(); 
      return(-1); };
   */

}

uint32_t Fipsy_Open(void)
 { 
  int ret;
  uint8_t mode = 0;
  uint8_t bits = 8;
  uint32_t speed = 400000;

  // Open the port
  //This is done is the setup() function in Arduino

  // Set configurations for this port
  //This is done on a transaction basis

  // Send a NOP to wakeup the device
  SPIBUFINIT;                  
  MachXO2_Command = MACHXO2_CMD_NOP;
  pMachXO2_Operand[0] = 0xFF;         
  pMachXO2_Operand[1] = 0xFF;         
  pMachXO2_Operand[2] = 0xFF;    
  MachXO2_SPITrans(4);
       
  // Return success
  return(1);
 }

 /* SPI_TRANSACTION completes the data transfer to and/or from the device 
   per the methods required by this system.  This uses the global defined
   SPI port handle, which is assumed to be open if this call is reached 
   from a routine in this code.  It is also assumed that the arguments are
   valid based on the controlled nature of calls to this routine.
*/

uint32_t SPI_Transaction(uint8_t Count, void *Data)   
 {
   SPISettings mySPISettings(400000, MSBFIRST, SPI_MODE0); 
   SPI.beginTransaction(mySPISettings);
   SPI.transfer(&Data, Count);
   SPI.endTransaction();

   /*
  int ret;
  // Rx and Tx as the same uint8_t buffer
  uint8_t *pbRx = Data;
  uint8_t *pbTx = Data;
  // Structure of transfer 
  struct spi_ioc_transfer msg = {
   .tx_buf = (unsigned long)pbTx,
   .rx_buf = (unsigned long)pbRx,
   .len = Count,
   .delay_usecs = 1,
   .speed_hz = 400000,
   .bits_per_word = 8,
  };

  // Complete the transfer
  ret = ioctl(SPIPort, SPI_IOC_MESSAGE(1), &msg);

  

  // Return result 
  return((ret < 1) ? 0 : 1);   

   */

   return 1;
 }

 /* FIPSY_READDEVICEID retrieves the device identification number from the
   FPGA connected to the SPI port.  This number can be used to verify that
   the SPI is working and we are talking to the right device.  To improve
   future flexibility, this routine does not decide if this is actually
   the right device, but just returns the four uint8_ts it got.
*/

uint32_t Fipsy_ReadDeviceID(uint8_t *DeviceID)
 {
  // All exported library functions get this check of hardware and arguments    
  //if(!HWIsOpen) return(ErrorNotOpen());
  if(DeviceID == NULL){
      Serial.println("Error: Data pointer provided is NULL");
      return(-1);
    }
    
  // Construct the command  
  SPIBUFINIT;
  MachXO2_Command = MACHXO2_CMD_READ_DEVICEID;
  MachXO2_SPITrans(8);
  
  // Get the data 
  memcpy(DeviceID, pMachXO2_Data, 4);
      
  // Return success
  return(1);
 }


 /* FIPSY_CLOSE closes the SPI port connection.

*/

uint32_t Fipsy_Close(void)
 {  
  // Close the port
  SPI.end();  
  SPIPort = -1;
 
  // Return success
  return(1);
 }
