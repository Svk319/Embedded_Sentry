#include <mbed.h>
#include <math.h>
#define PI 3.14159265358979323846
#define GESTURE_DURATION 5

/*
IMPORTANT! 
1.If you want to run the code for specific method, please comment the code for other methods in the main while loop. 
  Otherwise you will run into bugs instead of geting the correct result.
2. The Sensor should be tied exactly to the right side of the knee.
*/
// Example 2
SPI spi(PF_9, PF_8, PF_7,PC_1,use_gpio_ssel); // mosi, miso, sclk, cs

//address of first register with gyro data
#define OUT_X_L 0x28

//register fields(bits): data_rate(2),Bandwidth(2),Power_down(1),Zen(1),Yen(1),Xen(1)
#define CTRL_REG1 0x20

//configuration: 200Hz ODR,50Hz cutoff, Power on, Z on, Y on, X on
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1

//register fields(bits): reserved(1), endian-ness(1),Full scale sel(2), reserved(1),self-test(2), SPI mode(1)
#define CTRL_REG4 0x23

//configuration: reserved,little endian,500 dps,reserved,disabled,4-wire mode
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0

#define SPI_FLAG 1
#define TICKER_FLAG 0
#define USRDY_FLAG 0

uint8_t write_buf[32]; 
uint8_t read_buf[32];

EventFlags flags;
//volatile int tickerFlag = 0;
//The spi.transfer() function requires that the callback
//provided to it takes an int parameter
void spi_cb(int event){
  flags.set(SPI_FLAG);
  

}
void flip()
{
    flags.set(TICKER_FLAG);
}

float inputDataX[1000];
float inputDataY[1000];
float inputDataZ[1000];
float pwdDataX[1000];
float pwdDataY[1000];
float pwdDataZ[1000];
uint16_t iter = 0;
uint16_t pwdSize = 0;//this denotes the index till which the pwdData array is filled up so that we can validate only till that index
uint16_t inputKeySize = 0;//this denotes the index till which the inputData array is filled up so that we can validate only till that index
Ticker t;
bool pwdSet=false;

//float* 
void readData(bool pwd){
int16_t raw_gx,raw_gy,raw_gz;
      float gx, gy, gz;
      //prepare the write buffer to trigger a sequential read
      write_buf[0]=OUT_X_L|0x80|0x40;
      //start sequential sample reading
      spi.transfer(write_buf,7,read_buf,7,spi_cb,SPI_EVENT_COMPLETE );
      flags.wait_all(SPI_FLAG);
      //read_buf after transfer: garbage byte, gx_low,gx_high,gy_low,gy_high,gz_low,gz_high
      //Put the high and low bytes in the correct order lowB,HighB -> HighB,LowB
      raw_gx=( ( (uint16_t)read_buf[2] ) <<8 ) | ( (uint16_t)read_buf[1] );
      raw_gy=( ( (uint16_t)read_buf[4] ) <<8 ) | ( (uint16_t)read_buf[3] );
      raw_gz=( ( (uint16_t)read_buf[6] ) <<8 ) | ( (uint16_t)read_buf[5] );

      //printf("RAW|\tgx: %d \t gy: %d \t gz: %d\t",raw_gx,raw_gy,raw_gz);

      gx=((float)raw_gx)*(17.5f*0.017453292519943295769236907684886f / 1000.0f);
      gy=((float)raw_gy)*(17.5f*0.017453292519943295769236907684886f / 1000.0f);
      gz=((float)raw_gz)*(17.5f*0.017453292519943295769236907684886f / 1000.0f);
      
      //printf("Actual|\tgx: %4.5f \t gy: %4.5f \t gz: %4.5f\n",gx,gy,gz);
      // if pwd is false, it means we are attempting to unlock and so we store it accordingly
      if(pwd){
      inputDataX[iter]=gx;
      inputDataY[iter]=gy;
      inputDataZ[iter]=gz;
      iter++;
      }
      // if pwd is true, it means we are setting the password and so we store it accordingly
      else{
      pwdDataX[iter]=gx;
      pwdDataY[iter]=gy;
      pwdDataZ[iter]=gz;
      iter++;
      }
      //float readings[3]={gx,gy,gz};
      //return readings;
}

void recordKey(){
        thread_sleep_for(100);
        flags.clear(TICKER_FLAG);
        iter=0;
        t.attach(&flip, GESTURE_DURATION*1000ms);
        while(0==TICKER_FLAG){
            readData(pwdSet);
        }
        t.detach();
        pwdSize=iter;
        iter=0;
    }

void enterKey(){
        thread_sleep_for(100);
        flags.clear(TICKER_FLAG);
        iter=0;
        t.attach(&flip, GESTURE_DURATION*1000ms);
        while(0==TICKER_FLAG){
            readData(pwdSet);
        }
        t.detach();
        inputKeySize=iter;
        iter=0;
    }
    // need to implement this method
    // we need to identify subarray based on clockwise(negative) or anticlockwise(positive) rotation, 
    //then average out the speed for every 10 values which implements moving average filter approximately.
    //  For each of those averaged values we need to multiply by corresponding time taken to get displacement.
    // we need calibration initially to know how much time does one array element reading denotes.
    // This can be done by keeping the gyroscope idle for 4 seconds and we need to check how many outputs it has produced. 
    //Lets say 4k in 4 seconds(for X dimension alone), then one array element corresponds to 1/1000 seconds or 1 ms.
    // either we can calibrate or go by datasheet frequency of ODR.
float angularDisplacement(){

    return 0;
}
// need to implement this method as per the meeting.
//success needs LED INDICATION
bool validate(){
    return true;
}

int main() {
    // Setup the spi for 8 bit data, high steady state clock,
    // second edge capture, with a 1MHz clock rate
    spi.format(8,3);
    spi.frequency(1'000'000);

    write_buf[0]=CTRL_REG1;
    write_buf[1]=CTRL_REG1_CONFIG;
    spi.transfer(write_buf,2,read_buf,2,spi_cb,SPI_EVENT_COMPLETE );
    flags.wait_all(SPI_FLAG);

    write_buf[0]=CTRL_REG4;
    write_buf[1]=CTRL_REG4_CONFIG;
    spi.transfer(write_buf,2,read_buf,2,spi_cb,SPI_EVENT_COMPLETE );
    flags.wait_all(SPI_FLAG); 
    DigitalIn usr(USER_BUTTON);
    flags.clear(USRDY_FLAG);
    printf("Press the User Button to store the password gesture. The gesture has to be input within four seconds");
    
    thread_sleep_for(1000);
    while (1) {
    if(usr.read()==1){
        if(!pwdSet){
            recordKey();
            pwdSet=true;
            thread_sleep_for(1000);
            printf("Press the User Button to input the gesture and unlock the resource. The gesture has to be input within four seconds");
        }
        else{
        enterKey();
        thread_sleep_for(1000);
        if(validate()){
            printf("Success! The resource is unlocked");
        }
        else{
            printf("Unlock failed");
            thread_sleep_for(1000);
            printf("Press the User Button to input the gesture and unlock the resource. The gesture has to be input within four seconds");

        }
        }
    }
    }
}
/*
double calculateDistance1(){
  double realData;
  double totalDist1 = 0;
  for (int i = 0; i < 600; i++){
    dataSet[i] = abs(dataSet[i]);
    if (dataSet[i] > 5000){
      // Transform to angle velocity. 
      realData = (0.00875 * (dataSet[i] + 63));
      totalDist1 = totalDist1 + ((double) 0.05) * (realData / 360) * 2 * PI * 1.015;
    }
  }
  return totalDist1;
}
*/

/*
Function: Calibration1
Idea: Elimintate the flucturation of the raw data.
      calculate the average value of raw data without moving the clip
*/
/*
double calibration1(){
  double total = 0;
  for (int i = 0; i < 4000; i++){
    total+=dataSet[i];
    //printf("%d ",dataSet[i]);
  }
  return total/4000;
}
*/

