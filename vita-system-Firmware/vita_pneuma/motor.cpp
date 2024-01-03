#include "Arduino.h"
#include "alarm.h"
#include "global.h"
#include "motor.h"
#include "pwm.h"
#if DEBUG_CODE != 0
#include "SendOnlySoftwareSerial.h"
#endif

#ifndef MOTOR_PWM
#include <AccelStepper.h>

  static AccelStepper motor(1,STEP_PIN,DIR_PIN);
#else
  #ifndef ACCELERATE_EVERY_PULSE
    static unsigned int gTargetFrequencySpeed;
    static unsigned int gCurrentFrequencySpeed;
    static unsigned int gStepsMaxVal = 0;
    static unsigned long gInitialMovMicroTime;
    static unsigned long gMotorTotalTime;
    static unsigned long gMotorAccelerationTime;
  #else

  #endif
#endif

volatile long gCurrentMotorPosition = 0;
volatile long gTargetMotorPosition = 0;
volatile char gMotorDirection = 0;


void motorInit()
{
  #ifndef MOTOR_PWM
    #if MOTOR_ACCELERATION == 0
      // the MaxSpeed is used to determine the max configurable speed whenever there is no acceleration
      // otherwise, it is used to determine the final desired speed
      motor.setMaxSpeed(MAX_MOTOR_SPEED);
    #endif
    motor.setMinPulseWidth(MIN_PULSE_WIDTH);
  #else
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pwm_init(&gCurrentMotorPosition, &gTargetMotorPosition, &gMotorDirection);
    pwm_stop();
  #endif
}

long motorDistanceToGo()
{
  long distance;
  #ifndef MOTOR_PWM
    distance = motor.distanceToGo();
  #else
    distance = gTargetMotorPosition - gCurrentMotorPosition;
  #endif

  if((gMotorDirection == -1 && distance > 0) ||
     (gMotorDirection == 1 && distance < 0))
    return 0;
  else
    return distance;
}
#ifndef MOTOR_PWM
  void motorMoveTo(long targetPosition, InsExpSign inspExp, float velocity, float acceleration)
  {
    gTargetMotorPosition = targetPosition;
    gMotorDirection = ROTATIONAL_DIRECTION*inspExp;

    #if MOTOR_ACCELERATION == 0
      // Code used when there is no acceleration
      motor.moveTo(gTargetMotorPosition);
      motor.setSpeed(gMotorDirection*velocity); //positive or negative
    #else
      // Code with acceleration
      motor.setAcceleration(acceleration); //always positive

      // steps the target position to the inspiration position
      motor.setMaxSpeed(velocity); //max speed must be positive
      motor.moveTo(gTargetMotorPosition);
    #endif
  }
  void motorUpdatePosition()
  {
    gCurrentMotorPosition = motor.currentPosition();
  }
  void motorRun()
  {
    #if MOTOR_ACCELERATION == 0
      //Runs at the currently selected speed until the target position is reached. Does not implement accelerations.
      motor.runSpeedToPosition();
    #else
      //With acceleration
      motor.run();
    #endif
  }
#else
  void motorUpdatePosition()
  {
  }
  double calcFreqSpeed(int frequency, unsigned char pwmPrescaler)
  {
    unsigned int prescaler = 1;
    if(frequency > MAX_PWM_VALUE)
      frequency = MAX_PWM_VALUE;
    switch(pwmPrescaler)
    {
      case PWM_PRESCALE_1:
        prescaler = 1;
      break;
      case PWM_PRESCALE_8:
        prescaler = 8;
      break;
	  case PWM_PRESCALE_32:
        prescaler = 32;
      break;
	  case PWM_PRESCALE_64:
        prescaler = 64;
      break;
      case PWM_PRESCALE_128:
        prescaler = 128;
      break;
      case PWM_PRESCALE_256:
        prescaler = 256;
      break;
      case PWM_PRESCALE_1024:
        prescaler = 1024;
      break;
    }
    return (((double)16000000)/((double)2*(prescaler)*(1+frequency)));
  }

  int findFreqSpeed(double targetSpeed, unsigned char *pwmPrescaler)
  {
    double oldDifference = 1e6, newDifference;
    int returnVal = MAX_PWM_VALUE;
    /*
    #if DEBUG_CODE != 0
      txSerial.print(F("targetSpeed: "));
      txSerial.println((float)targetSpeed);
    #endif// */
    //we go through prescalers between 64 and 1024. This is necessary for slower speeds.
    for(*pwmPrescaler = PWM_PRESCALE_64; *pwmPrescaler <= PWM_PRESCALE_1024; (*pwmPrescaler)++)
    {
      //if the slowest speed of this prescaler is faster than the desired speed
      if(calcFreqSpeed(255, *pwmPrescaler) > targetSpeed)
        continue;
      for(int i = MAX_PWM_VALUE; i >= 2; i--)
      {
        //obtain the new difference to the target speed
        newDifference = targetSpeed - calcFreqSpeed(i, *pwmPrescaler);

        //if the new speed if faster than the target one
        if((newDifference < 0 && oldDifference > 0))
        {
		  returnVal = i;// *
		  ////if the old value is closer than the new value
		  //if(newDifference*(-1) > oldDifference)
		  //	returnVal++; //use the old value
		  //cannot use a value bigger than the MAX_PWM_VALUE
		  if(returnVal > MAX_PWM_VALUE)
		  	returnVal = MAX_PWM_VALUE;// */
          return returnVal;
	    }
        oldDifference = newDifference;
      }
    }
    *pwmPrescaler = PWM_PRESCALE_1024;
    //no speed have been found
    return returnVal;
  }

  #ifndef ACCELERATE_EVERY_PULSE
    float calcMeanSpeed(unsigned int startFreq, unsigned int endFreq, unsigned char pwmPrescaler)
    {
      double mean = 0, i;
      //the end freq is lower than the start freq
      for(i = startFreq; i >= endFreq; i--)
      {
        mean += calcFreqSpeed(i, pwmPrescaler);
      }
      mean = mean/(float)(startFreq-endFreq+1);
      /*
      #if DEBUG_CODE != 0
        txSerial.print(F("s spd: "));
        txSerial.print(calcFreqSpeed(startFreq, pwmPrescaler));
        txSerial.print(F(",e spd: "));
        txSerial.print(calcFreqSpeed(endFreq, pwmPrescaler));
        txSerial.print(F(",mean: "));
        txSerial.println(mean);
      #endif// */
      return mean;
    }
    void motorPWMMoveTo(long targetPosition, InsExpSign inspExp, unsigned int targetFreqSpeed, unsigned int startFreqSpeed, unsigned long motorTotalTime, unsigned long motorAccelerationTime, unsigned char pwmPrescaler)
    {
      gTargetMotorPosition = targetPosition;
      gMotorDirection = ROTATIONAL_DIRECTION*inspExp;

      gTargetFrequencySpeed = targetFreqSpeed;
      gStepsMaxVal = startFreqSpeed;//gTargetFrequencySpeed + MAX_RANGE_PWM_VALUE;
      if(gStepsMaxVal > MAX_PWM_VALUE)
        gStepsMaxVal = MAX_PWM_VALUE;
      gCurrentFrequencySpeed = gStepsMaxVal;
      gMotorTotalTime = motorTotalTime;
      //enforce the zero acceleration if MOTOR_ACCELERATION equals zero
      #if MOTOR_ACCELERATION == 0
        gMotorAccelerationTime = 0;
      #else
        gMotorAccelerationTime = motorAccelerationTime;
      #endif
      //Adjust the direction
      digitalWrite(DIR_PIN, gMotorDirection == -1? 0: 1);
      //adjust the prescaler
      pwm_configure_prescaler(pwmPrescaler);
      //set the frequency and start the PWM.
      pwm_changeFreq(gCurrentFrequencySpeed);
      // The gCurrentMotorPosition, gTargetMotorPosition and gMotorDirection will make it work properly
      pwm_startRun();
      //obtain the initial time
      gInitialMovMicroTime = micros();
    }
    void motorRun()
    {
        unsigned long timeFromStart = micros() - gInitialMovMicroTime;
        //are we accelerating?
        if(timeFromStart < gMotorAccelerationTime)
        {
          //recalculate the acceleration speed frequency
          //Example for 50 target freq: = 255 - (0.25)*(255-50) = 203
          int newFreq = gStepsMaxVal - (timeFromStart/(double)gMotorAccelerationTime)*(gStepsMaxVal-gTargetFrequencySpeed);
          if(gCurrentFrequencySpeed != newFreq)
          {
            // update the motor speed
            gCurrentFrequencySpeed = newFreq;
            pwm_changeFreq(gCurrentFrequencySpeed);
          }
        } else if(timeFromStart < gMotorTotalTime - gMotorAccelerationTime) //  Did we reach the full speed phase?
        {
          if(gCurrentFrequencySpeed != gTargetFrequencySpeed)
          {
              gCurrentFrequencySpeed = gTargetFrequencySpeed;
              pwm_changeFreq(gCurrentFrequencySpeed);
          }
        }
        else if(timeFromStart < gMotorTotalTime) //deceleration phase
        {
          //recalculate the deceleration speed frequency
          //Example for 50 target freq: = 50 + (0.25)*(255-50) = 101
          int newFreq = gTargetFrequencySpeed + ((1-((gMotorTotalTime-timeFromStart)/(double)gMotorAccelerationTime))*(gStepsMaxVal-gTargetFrequencySpeed));
          if(gCurrentFrequencySpeed != newFreq)
          {
            // update the motor speed
            gCurrentFrequencySpeed = newFreq;
            pwm_changeFreq(gCurrentFrequencySpeed);
          }
        } else {
          //for whatever the reason we did not reach the target position in time
          //keep going with minimal speed
          if(gCurrentFrequencySpeed != gStepsMaxVal)
          {
              gCurrentFrequencySpeed = gStepsMaxVal;
              pwm_changeFreq(gCurrentFrequencySpeed);
              //pwm_stop();
          }
        }
    }
  #else
    void calcMeanSpeed(float *meanAccelSpeed, float *stentTime,  unsigned int *startSpeedFreq,  unsigned int *targetSpeedFreq, float totalTime, unsigned char pwmPrescaler)
    {
        //we must first determine the maximum time we can spend in the accel/decel. stage
        float maximumTime = totalTime / MOTOR_ACCELERATION;
        float calcTime;
        //reset the time used to calculate the stentTime so far
        *stentTime = 0;
        //note that *targetSpeedFreq is always smaller than *startSpeedFreq
        for(unsigned int i = *targetSpeedFreq; i < *startSpeedFreq; i++)
        {
          calcTime = 1/calcFreqSpeed(i, pwmPrescaler);
          //will we reach the maximum time availabe for accel/decel?
          if((*stentTime)+calcTime >= maximumTime)
          {
            //if so, we mus change the expStartSpeedFreq so that it is with in available time
            *startSpeedFreq = i-1;
            break;
          }
          *stentTime += calcTime;
        }
        if(*stentTime != 0)
          *meanAccelSpeed = (*startSpeedFreq - *targetSpeedFreq)/(*stentTime);
        else
          *meanAccelSpeed = 0;
          /*
        #if DEBUG_CODE == 1
          txSerial.print(F("pwmPr: "));
          txSerial.print(pwmPrescaler);
          txSerial.print(F(", *expTFr: "));
          txSerial.print(*targetSpeedFreq);
          txSerial.print(F(" *expStSpF: "));
          txSerial.print(*startSpeedFreq);
          txSerial.print(F(" *meanASp: "));
          txSerial.print(*meanAccelSpeed);
          txSerial.print(F(" *stentT: "));
          txSerial.println(*stentTime);
        #endif*/
    }
    void motorPWMMoveTo(long targetPosition, InsExpSign inspExp, unsigned int targetFreqSpeed, unsigned int startFreqSpeed, unsigned char pwmPrescaler)
    {
      gTargetMotorPosition = targetPosition;
      gMotorDirection = ROTATIONAL_DIRECTION*inspExp;
      if(startFreqSpeed > MAX_PWM_VALUE)
        startFreqSpeed = MAX_PWM_VALUE;
      digitalWrite(DIR_PIN, gMotorDirection == -1? 0: 1);
      //adjust the prescaler
      pwm_configure_prescaler(pwmPrescaler);
      // The gCurrentMotorPosition, gTargetMotorPosition and gMotorDirection will make it work properly
      pwm_startRun(startFreqSpeed, targetFreqSpeed);
    }
    void motorRun()
    {
    }
  #endif
#endif


void motorStop()
{
  #ifndef MOTOR_PWM
    motor.stop();
  #else
    pwm_stop();
  #endif
}
long motorGetPosition()
{
  #ifndef MOTOR_PWM
    gCurrentMotorPosition = motor.currentPosition();
  #endif
  return gCurrentMotorPosition;
}
void motorSetPosition(long newPosition)
{
  #ifndef MOTOR_PWM
    motor.setCurrentPosition(newPosition);
  #endif
  gCurrentMotorPosition = newPosition;
}
