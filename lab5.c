// Matthew Gomez Morales & Ashley Dennis
// EEL 4742L
// 2/17/26
// Lab 5

/* DriverLib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>

#define PERIOD 1500
#define DUTY 50// Range 0 to 100
#define BUMPERLEDS GPIO_PIN0|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7
#define CLOCKDIVIDER TIMER_A_CLOCKSOURCE_DIVIDER_48
#define LEFTCHANNEL TIMER_A_CAPTURECOMPARE_REGISTER_4
#define RIGHTCHANNEL TIMER_A_CAPTURECOMPARE_REGISTER_3

// function prototypes
void config432IO(void);
void configRobotIO(void);
void readBumperSwitches(void);
void configPWMTimer(uint16_t clockPeriod, uint16_t clockDivider, uint16_t duty, uint16_t channel);
void updateRSLKLEDs(void);
void powerUpMode(void);
void wheelRotation(void);
void bumperSwitchesHandler(void);

//enums
typedef enum {motorOFF,motorForward,motorReverse,motorTest} motorStates;
typedef enum {buttonOFF, buttonON} buttonStates;

volatile buttonStates b0ButtonState;
volatile buttonStates bnButtonState;

// global variables
Timer_A_PWMConfig timerPWMConfig;
motorStates leftMotorState;
motorStates rightMotorState;
motorStates motorState;

int main(void)
{
    /* Halting the Watchdog */
    MAP_WDT_A_holdTimer();
    config432IO();
    configRobotIO();

    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN2);
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN0);

    configPWMTimer(PERIOD, CLOCKDIVIDER, DUTY, LEFTCHANNEL);
    configPWMTimer(PERIOD, CLOCKDIVIDER, DUTY, RIGHTCHANNEL);

    // initialize motor to off
    motorState = motorOFF;
    wheelRotation();
    powerUpMode();

    b0ButtonState = buttonOFF;
    bnButtonState = buttonOFF;
    MAP_Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
     __enable_interrupts();

    while(1)
    {
        if(b0ButtonState == buttonOFF)
        {
            MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
            __delay_cycles(6000000);  // 2 seconds
        }

        if (b0ButtonState == buttonON)
            {
                MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);

                // turn on RSLK LEDs
                leftMotorState  = motorForward;
                rightMotorState = motorForward;
                updateRSLKLEDs();

                // forward mode
                motorState = motorForward;
                wheelRotation();


                while(b0ButtonState == buttonON &&
                    bnButtonState == buttonOFF);

                motorState = motorOFF;
                wheelRotation();

                // turn off leds
                leftMotorState  = motorOFF;
                rightMotorState = motorOFF;
                updateRSLKLEDs();

                __delay_cycles(6000000);

                if(b0ButtonState == buttonON)
                {
                    motorState = motorReverse;
                    wheelRotation();

                    leftMotorState  = motorReverse;
                    rightMotorState = motorReverse;
                    updateRSLKLEDs();
                    __delay_cycles(10000000);

                    motorState = motorOFF;
                    wheelRotation();

                    leftMotorState  = motorOFF;
                    rightMotorState = motorOFF;
                    updateRSLKLEDs();

                    b0ButtonState = buttonOFF;
                    bnButtonState = buttonOFF;
                }


            }
    }
}

// MSP IO configuration
void config432IO(void) {

// set MSP leds as outputs and off
MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);

MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);

}

// Robot IO configuration
void configRobotIO(void)
{

    // bumper switches
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);

    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, GPIO_PIN0);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, GPIO_PIN2);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, GPIO_PIN3);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, GPIO_PIN5);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, GPIO_PIN6);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, GPIO_PIN7);

    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, GPIO_PIN0, GPIO_HIGH_TO_LOW_TRANSITION);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, GPIO_PIN2, GPIO_HIGH_TO_LOW_TRANSITION);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, GPIO_PIN3, GPIO_HIGH_TO_LOW_TRANSITION);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, GPIO_PIN5, GPIO_HIGH_TO_LOW_TRANSITION);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, GPIO_PIN6, GPIO_HIGH_TO_LOW_TRANSITION);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, GPIO_PIN7, GPIO_HIGH_TO_LOW_TRANSITION);

    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, GPIO_PIN0);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, GPIO_PIN2);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, GPIO_PIN3);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, GPIO_PIN5);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, GPIO_PIN6);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, GPIO_PIN7);

    MAP_GPIO_setAsOutputPin(GPIO_PORT_P8, BUMPERLEDS);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, BUMPERLEDS);

    MAP_GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN0 | GPIO_PIN2 | GPIO_PIN4 | GPIO_PIN5);

    //sleep
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);

    // pwm
    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2,GPIO_PIN6, GPIO_PRIMARY_MODULE_FUNCTION);
    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2,GPIO_PIN7, GPIO_PRIMARY_MODULE_FUNCTION);

    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0 | GPIO_PIN2 | GPIO_PIN4 | GPIO_PIN5);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN6 | GPIO_PIN7);
    MAP_GPIO_registerInterrupt(GPIO_PORT_P4, bumperSwitchesHandler);

}

// Configure PWM Timer
void configPWMTimer(uint16_t clockPeriod, uint16_t clockDivider, uint16_t duty, uint16_t channel)
{
    const uint32_t TIMER=TIMER_A0_BASE;
    uint16_t dutyCycle = duty*clockPeriod/100;
    timerPWMConfig.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    timerPWMConfig.clockSourceDivider = clockDivider;
    timerPWMConfig.timerPeriod = clockPeriod;
    timerPWMConfig.compareOutputMode = TIMER_A_OUTPUTMODE_TOGGLE_SET;
    timerPWMConfig.compareRegister = channel;
    timerPWMConfig.dutyCycle = dutyCycle;
    MAP_Timer_A_generatePWM(TIMER, &timerPWMConfig);
    MAP_Timer_A_stopTimer(TIMER);
}

// update RSLK LEDs
void updateRSLKLEDs(void)
{
    switch (leftMotorState)
    {
        case motorOFF:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8,
                GPIO_PIN0 | GPIO_PIN6);
            break;

        case motorForward:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN0); // Front ON
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN6);  // Rear OFF
            break;

        case motorReverse:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN0);  // Front OFF
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN6); // Rear ON
            break;

        case motorTest:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8,
                GPIO_PIN0 | GPIO_PIN6);
            break;
    }

    switch (rightMotorState)
    {
        case motorOFF:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8,
                GPIO_PIN5 | GPIO_PIN7);
            break;

        case motorForward:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN5); // Front ON
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN7);  // Rear OFF
            break;

        case motorReverse:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN5);  // Front OFF
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN7); // Rear ON
            break;

        case motorTest:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8,
                GPIO_PIN5 | GPIO_PIN7);
            break;
    }
}

// init power up mode
void powerUpMode(void)
{
    // turn on all LEDS
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);

    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN0 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);

    __delay_cycles(6000000);

    // Turn off all LEDs
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN0 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);

}

// controls wheel rotation based on motor state
void wheelRotation(void)
{
switch(motorState)
     {
         case motorOFF:
             MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);
             MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4 | GPIO_PIN5);
             break;

         case motorForward:
             MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN7| GPIO_PIN6);
             MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4 | GPIO_PIN5);
             break;

         case motorReverse:
             MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN7| GPIO_PIN6);
             MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN4 | GPIO_PIN5);
             break;

         case motorTest:
         default:
             MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);
             MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4 | GPIO_PIN5);
             break;
}
}

// bumper switch handler

void bumperSwitchesHandler(void)
{
    uint16_t status;

    __delay_cycles(30000);
    status = MAP_GPIO_getEnabledInterruptStatus(GPIO_PORT_P4);
    switch (status)
    {
        case GPIO_PIN0:
            if (b0ButtonState == buttonON)
                b0ButtonState = buttonOFF;
            else
                b0ButtonState = buttonON;
            break;

        case GPIO_PIN2:   // BS1
        case GPIO_PIN3:   // BS2
        case GPIO_PIN5:   // BS3
        case GPIO_PIN6:   // BS4
        case GPIO_PIN7:   // BS5
        if( bnButtonState == buttonON)
               bnButtonState = buttonOFF;
           else
               bnButtonState = buttonON;
            break;

    }

}
