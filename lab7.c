// Matthew Gomez & Ashley Dennis
// Lab 7
// 3/11/26
// Description:reading sensors and displaying corresponding LEDs

/* Driverlib Includes */
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>

/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>

// global defines
#define PERIOD 2500
#define BUMPERPINS  GPIO_PIN0 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7
#define SENSORPINS GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3 | GPIO_PIN4 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7
#define BUMPERLEDS GPIO_PIN0|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7
#define DUTY 50
#define CLOCKDIVIDER TIMER_A_CLOCKSOURCE_DIVIDER_48
#define LEFTCHANNEL TIMER_A_CAPTURECOMPARE_REGISTER_4
#define RIGHTCHANNEL TIMER_A_CAPTURECOMPARE_REGISTER_3

// enumerations
typedef enum { motorOFF, motorForward, motorReverse, motorTest } MOTORSTATE;
// typedef enum { buttonON, buttonOFF } buttonStates;
typedef enum { POWER_UP, STANDBY, TRACKING, LOST } DESIGNMODE;
typedef enum { RED, YELLOW, GREEN, CYAN, BLUE, WHITE } LEDColor;

// func prototypes
void config432IO(void);
void configRobotIO(void);
void configPWMTimer(uint16_t clockPeriod, uint16_t clockDivider, uint16_t duty, uint16_t channel);
void bumperSwitchesHandler(void);
// void MotorMovement(void);
void processLineSensor(void);
void sensors(void);
void LEDController(void);
void LED2(void);
void powerUpMode(void);

MOTORSTATE motorState;
DESIGNMODE mode;
LEDColor color;

Timer_A_PWMConfig timerPWMConfig;
volatile uint8_t sensorData[8];

int main(void)
{
    MAP_WDT_A_holdTimer();

    config432IO();
    configRobotIO();
    powerUpMode();

    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN2); // left enable
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0); // right enable

    MAP_GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN3);

    MAP_Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);

    __delay_cycles(3000000);

    mode = STANDBY;

    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, GPIO_PIN0, GPIO_HIGH_TO_LOW_TRANSITION);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, GPIO_PIN0);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, GPIO_PIN0);

    while (1)
    {
        sensors();
        processLineSensor();
        bumperSwitchesHandler();

        LEDController();

        if (color == WHITE && mode == TRACKING)
        {
            mode = LOST;
        }
        else if (color != WHITE && mode == LOST)
        {
            mode = TRACKING;
        }

        __delay_cycles(30000);
    }
}


// MSP config
void config432IO(void)
{
    // msp leds
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN0);
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN1);
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_PIN2);

    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
}

// robot config
void configRobotIO(void)
{
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P4, BUMPERPINS);

    MAP_GPIO_enableInterrupt(GPIO_PORT_P4, BUMPERPINS);
    MAP_GPIO_interruptEdgeSelect(GPIO_PORT_P4, BUMPERPINS, GPIO_HIGH_TO_LOW_TRANSITION);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, BUMPERPINS);
    //MAP_GPIO_registerInterrupt(GPIO_PORT_P4, bumperSwitchesHandler);

    // RSLK LEDs
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P8, BUMPERLEDS);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, BUMPERLEDS);

    // motor pins
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN0 | GPIO_PIN2 | GPIO_PIN4 | GPIO_PIN5);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0 | GPIO_PIN2 | GPIO_PIN4 | GPIO_PIN5);

    // sleep Pins
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);

    // pwm Pins
    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN6 | GPIO_PIN7, GPIO_PRIMARY_MODULE_FUNCTION);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN6 | GPIO_PIN7);

    // lab 7
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN3); // CTRL_EVEN
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P9, GPIO_PIN2); // CTRL_ODD
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN3);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P9, GPIO_PIN2);
}

// configPWM
void configPWMTimer(uint16_t clockPeriod, uint16_t clockDivider, uint16_t duty, uint16_t channel)
{
    const uint32_t TIMER = TIMER_A0_BASE;
    uint16_t dutyCycle = (uint16_t)((uint32_t)duty * (uint32_t)clockPeriod / 100U);

    timerPWMConfig.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    timerPWMConfig.clockSourceDivider = clockDivider;
    timerPWMConfig.timerPeriod = clockPeriod;
    timerPWMConfig.compareOutputMode = TIMER_A_OUTPUTMODE_TOGGLE_SET;
    timerPWMConfig.compareRegister = channel;
    timerPWMConfig.dutyCycle = dutyCycle;

    MAP_Timer_A_generatePWM(TIMER, &timerPWMConfig);
    MAP_Timer_A_stopTimer(TIMER);
}

// bumper handling func
void bumperSwitchesHandler(void)
{
    uint16_t status;
    __delay_cycles(30000);
    
    status = MAP_GPIO_getEnabledInterruptStatus(GPIO_PORT_P4);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P4, status);

    if (status & GPIO_PIN0)
    {
        if (mode == STANDBY)
        {
            mode = TRACKING;
        }
        else if (mode == TRACKING || mode == LOST)
        {
            mode = STANDBY;
        }
    }
}

// led1 controller function
void LEDController(void)
{

    switch (mode)
    {
        case POWER_UP:
            motorState = motorOFF;
            break;

        case STANDBY:
            MAP_GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
            __delay_cycles(1500000);
            break;

        case TRACKING:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
            processLineSensor();
            break;

        case LOST:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
            processLineSensor();
            motorState = motorOFF;
            break;

        default:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
            break;
    }
}


// led2
void LED2(void)
{
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);

    switch (color)
    {
        case RED:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN0);
            break;

        case YELLOW:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1);
            break;

        case GREEN:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN1);
            break;

        case CYAN:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN1 | GPIO_PIN2);
            break;

        case BLUE:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN2);
            break;

        case WHITE:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
            break;

        default:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
            break;
    }
}

// sensor reading func
void sensors(void)
{
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN3);
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P9, GPIO_PIN2);

    MAP_GPIO_setAsOutputPin(GPIO_PORT_P7, SENSORPINS);
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P7, SENSORPINS);

    __delay_cycles(30);

    MAP_GPIO_setAsInputPin(GPIO_PORT_P7, SENSORPINS);

    __delay_cycles(3000);

    // reading sensors; 0-3 even 4-7 odd
    sensorData[0] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN0) != 0);
    sensorData[1] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN1) != 0);
    sensorData[2] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN2) != 0);
    sensorData[3] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN3) != 0);
    sensorData[4] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN4) != 0);
    sensorData[5] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN5) != 0);
    sensorData[6] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN6) != 0);
    sensorData[7] = (MAP_GPIO_getInputPinValue(GPIO_PORT_P7, GPIO_PIN7) != 0);

    // turn off IR 
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN3);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P9, GPIO_PIN2);
}

// processLineSensor function handling the cases 
void processLineSensor(void)
{
    uint8_t RightSide = (uint8_t)(sensorData[0] + sensorData[1] + sensorData[2] + sensorData[3]);
    uint8_t LeftSide  = (uint8_t)(sensorData[4] + sensorData[5] + sensorData[6] + sensorData[7]);

    if (LeftSide == 0 && RightSide == 0)
        color = WHITE;
    else if (RightSide > 0 && LeftSide == 0)
        color = BLUE;
    else if (LeftSide > 0 && RightSide == 0)
        color = RED;
    else if (LeftSide == RightSide)
        color = GREEN;
    else if (RightSide > LeftSide)
        color = CYAN;
    else
        color = YELLOW;

    LED2();
}


// powerUpMode func handling the first state
void powerUpMode(void)
{
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P1, GPIO_PIN0);
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
    MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P8, GPIO_PIN0 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);

    __delay_cycles(3000000); // 1 sec toggle

    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2);
    MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P8, GPIO_PIN0 | GPIO_PIN5 | GPIO_PIN6 | GPIO_PIN7);
}

/* 
// motorMovement func
void MotorMovement(void)
{
    switch (motorState)
    {
        case motorOFF:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN7); // left sleep 
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN6); // right sleep 
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN2); // left enable 
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN0); // right enable
            break;

        case motorForward:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN2);
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN0);

            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4);
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN5);

            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN7);
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN6);

            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN7);
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN6);
            break;

        case motorReverse:
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN2);
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN0);

            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN4);
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN5);

            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN7);
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN6);

            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN7);
            MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN6);
            break;

        case motorTest:
            MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN7);
            break;
    }
}
*/
