//-----------------------------------------------------
// #### MICROINVERTER PROJECT  F030 - Custom Board ####
// ##
// ## @Author: Med
// ## @Editor: Emacs - ggtags
// ## @TAGS:   Global
// ## @CPU:    STM32F030
// ##
// #### MAIN.C ########################################
//-----------------------------------------------------

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gpio.h"
#include "tim.h"
#include "uart.h"
#include "hard.h"

#include "core_cm0.h"
#include "adc.h"
#include "dma.h"
#include "flash_program.h"
#include "dsp.h"
#include "it.h"
#include "sync.h"
#include "test_functions.h"

// Externals -----------------------------------------------

// -- Externals from or for Serial Port --------------------
volatile unsigned char tx1buff [SIZEOF_DATA];
volatile unsigned char rx1buff [SIZEOF_DATA];
volatile unsigned char usart1_have_data = 0;

// -- Externals from or for the ADC ------------------------
volatile unsigned short adc_ch [ADC_CHANNEL_QUANTITY];
volatile unsigned char seq_ready = 0;

// -- Externals for the timers -----------------------------
volatile unsigned short timer_led = 0;

// -- Externals used for analog or digital filters ---------
volatile unsigned short take_temp_sample = 0;


// ------- Definiciones para los filtros -------
#ifdef USE_FREQ_75KHZ
#define UNDERSAMPLING_TICKS    45
#define UNDERSAMPLING_TICKS_SOFT_START    90
#endif
#ifdef USE_FREQ_48KHZ
#define UNDERSAMPLING_TICKS    10
#define UNDERSAMPLING_TICKS_SOFT_START    20
#endif


// Globals -------------------------------------------------
volatile unsigned char overcurrent_shutdown = 0;


// ------- de los timers -------
volatile unsigned short wait_ms_var = 0;
volatile unsigned short timer_standby;
volatile unsigned short timer_meas;
volatile unsigned char timer_filters = 0;


#ifdef USE_FREQ_12KHZ
// Select Current Signal
#define USE_SIGNAL_CURRENT_1A

// Select Voltage Signal
#define USE_SIGNAL_VOLTAGE_185V
// #define USE_SIGNAL_VOLTAGE_220V

// Select Soft-Start Signal
#define USE_SIGNAL_SOFT_START_64V

#define SIZEOF_SIGNAL    120
#endif

#ifdef  USE_FREQ_24KHZ
// Select Current Signal
#define USE_SIGNAL_CURRENT_01_A
// #define USE_SIGNAL_CURRENT_05_A
// #define USE_SIGNAL_CURRENT_075_A
// #define USE_SIGNAL_CURRENT_1_A

// Select Voltage Signal
// #define USE_SIGNAL_VOLTAGE_185V
#define USE_SIGNAL_VOLTAGE_220V

// Select Soft-Start Signal
#define USE_SIGNAL_SOFT_START_64V

#define SIZEOF_SIGNAL    240
#endif

#if (SIZEOF_SIGNAL == 120)
unsigned short sin_half_cycle [SIZEOF_SIGNAL] = {26,53,80,106,133,160,186,212,238,264,
                                                 290,316,341,366,391,416,440,464,488,511,
                                                 534,557,579,601,622,643,664,684,704,723,
                                                 742,760,777,795,811,827,843,857,872,885,
                                                 899,911,923,934,945,955,964,972,980,988,
                                                 994,1000,1005,1010,1014,1017,1019,1021,1022,1023,
                                                 1022,1021,1019,1017,1014,1010,1005,1000,994,988,
                                                 980,972,964,955,945,934,923,911,899,885,
                                                 872,857,843,827,811,795,777,760,742,723,
                                                 704,684,664,643,622,601,579,557,534,511,
                                                 488,464,440,416,391,366,341,316,290,264,
                                                 238,212,186,160,133,106,80,53,26,0};


#elif (SIZEOF_SIGNAL == 240)
unsigned short sin_half_cycle [SIZEOF_SIGNAL] = {13,26,40,53,66,80,93,106,120,133,
                                                 146,160,173,186,199,212,225,238,251,264,
                                                 277,290,303,316,328,341,354,366,379,391,
                                                 403,416,428,440,452,464,476,488,499,511,
                                                 523,534,545,557,568,579,590,601,612,622,
                                                 633,643,654,664,674,684,694,704,713,723,
                                                 732,742,751,760,769,777,786,795,803,811,
                                                 819,827,835,843,850,857,865,872,879,885,
                                                 892,899,905,911,917,923,929,934,939,945,
                                                 950,955,959,964,968,972,976,980,984,988,
                                                 991,994,997,1000,1003,1005,1008,1010,1012,1014,
                                                 1015,1017,1018,1019,1020,1021,1022,1022,1022,1023,
                                                 1022,1022,1022,1021,1020,1019,1018,1017,1015,1014,
                                                 1012,1010,1008,1005,1003,1000,997,994,991,988,
                                                 984,980,976,972,968,964,959,955,950,945,
                                                 939,934,929,923,917,911,905,899,892,885,
                                                 879,872,865,857,850,843,835,827,819,811,
                                                 803,795,786,777,769,760,751,742,732,723,
                                                 713,704,694,684,674,664,654,643,633,622,
                                                 612,601,590,579,568,557,545,534,523,511,
                                                 499,488,476,464,452,440,428,416,403,391,
                                                 379,366,354,341,328,316,303,290,277,264,
                                                 251,238,225,212,199,186,173,160,146,133,
                                                 120,106,93,80,66,53,40,26,13,0};


#else
#error "Select SIZEOF_SIGNAL in main.c"
#endif


#ifdef USE_SIGNAL_CURRENT_01_A
#define KI_SIGNAL_PEAK_MULTIPLIER    279   // 0.1 Apk
#define KI_SIGNAL_50_PERCENT         140   // 0.05 Apk
                                           // respecto de la ipk de salida VI_Sense = 3 . Ipeak
                                           // puntos ADC = 3 . Ipeak . 1023 / 3.3
#endif

#ifdef USE_SIGNAL_CURRENT_05_A
#define KI_SIGNAL_PEAK_MULTIPLIER    465   // depende de cual es la medicion del opamp de corriente
#define KI_SIGNAL_50_PERCENT         232   // 0.25 Apk
                                           // respecto de la ipk de salida VI_Sense = 3 . Ipeak
                                           // puntos ADC = 3 . Ipeak . 1023 / 3.3
#endif

#ifdef USE_SIGNAL_CURRENT_075_A
#define KI_SIGNAL_PEAK_MULTIPLIER    697   // 0.75 Apk
#define KI_SIGNAL_50_PERCENT         349   // 0.37 Apk
                                           // respecto de la ipk de salida VI_Sense = 3 . Ipeak
                                           // puntos ADC = 3 . Ipeak . 1023 / 3.3
#endif

#ifdef USE_SIGNAL_CURRENT_1_A
#define KI_SIGNAL_PEAK_MULTIPLIER    930   // 1 Apk
#define KI_SIGNAL_50_PERCENT         465   // 0.5 Apk
                                           // respecto de la ipk de salida VI_Sense = 3 . Ipeak
                                           // puntos ADC = 3 . Ipeak . 1023 / 3.3
#endif

#ifdef USE_SIGNAL_VOLTAGE_185V
#define KV_SIGNAL_PEAK_MULTIPLIER    435    //depende de cual es la medicion del sensor de tension
#endif

#ifdef USE_SIGNAL_VOLTAGE_220V
#define KV_SIGNAL_PEAK_MULTIPLIER    517    //depende de cual es la medicion del sensor de tension
#endif

#ifdef USE_SIGNAL_SOFT_START_64V
#define K_SOFT_START_PEAK_MULTIPLIER    150
#endif


#define INDEX_TO_MIDDLE    47
#define INDEX_TO_FALLING    156
#define INDEX_TO_REVERT    204
    
unsigned short * p_current_ref;
unsigned short * p_voltage_ref;
pid_data_obj_t current_pid;
pid_data_obj_t voltage_pid;

// Module Functions ----------------------------------------
void PWM_Off (void);
unsigned short CurrentLoop (unsigned short, unsigned short);
void CurrentLoop_Change_to_LowGain (void);
void CurrentLoop_Change_to_HighGain (void);
unsigned short VoltageLoop (unsigned short, unsigned short);
void TimingDelay_Decrement (void);
extern void EXTI4_15_IRQHandler(void);



//-------------------------------------------//
// @brief  Main program.
// @param  None
// @retval None
//------------------------------------------//
int main(void)
{
    char s_lcd [120];

    ac_sync_state_t ac_sync_state = START_SYNCING;


    //GPIO Configuration.
    GPIO_Config();

    //ACTIVAR SYSTICK TIMER
    if (SysTick_Config(48000))
    {
        while (1)	/* Capture error */
        {
            if (LED)
                LED_OFF;
            else
                LED_ON;

            for (unsigned char i = 0; i < 255; i++)
            {
                asm (	"nop \n\t"
                        "nop \n\t"
                        "nop \n\t" );
            }
        }
    }

//---------- Pruebas de Hardware --------//
    EXTIOff ();
    USART1Config();
    
    //---- Welcome Code ------------//
    //---- Defines from hard.h -----//
#ifdef HARD
    Usart1Send((char *) HARD);
    Wait_ms(100);
#else
#error	"No Hardware defined in hard.h file"
#endif

#ifdef SOFT
    Usart1Send((char *) SOFT);
    Wait_ms(100);
#else
#error	"No Soft Version defined in hard.h file"
#endif

#ifdef FEATURES
    WelcomeCodeFeatures(s_lcd);
#endif

    
    TIM_3_Init();    //Used for mosfet channels control and ADC synchro

    TIM_16_Init();    //free running with tick: 1us
    TIM16Enable();
    TIM_17_Init();    //with int, tick: 1us

    // Initial Setup for the synchro module
    SYNC_InitSetup();
    
    PWM_Off();
    EnablePreload_Mosfet_HighLeft;
    EnablePreload_Mosfet_HighRight;

    //ADC and DMA configuration
    AdcConfig();
    DMAConfig();
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    ADC1->CR |= ADC_CR_ADSTART;
    //end of ADC & DMA

    EXTIOn();

    // Test Functions
    // TF_RelayConnect();
    // TF_RelayACPOS();
    // TF_RelayACNEG();
    // TF_RelayFiftyHz();
    TF_OnlySyncAndPolarity();
    // TF_Check_Sequence_Ready();
        
    
    //--- Grid Tied Mode ----------
#ifdef GRID_TIED_FULL_CONECTED

    // Initial Setup for PID Controller
    PID_Flush_Errors(&current_pid);
    CurrentLoop_Change_to_LowGain();

    typedef enum {
        SIGNAL_RISING = 0,
        SIGNAL_MIDDLE,
        SIGNAL_FALLING,
        SIGNAL_REVERT
        
    } signal_state_e;

    signal_state_e signal_state = SIGNAL_RISING;
    unsigned short d = 0;
    unsigned short cycles_before_start = CYCLES_BEFORE_START;
    
    while (1)
    {
        switch (ac_sync_state)
        {
        case START_SYNCING:
            PWM_Off();
            LED_OFF;

            EnablePreload_Mosfet_HighLeft;
            EnablePreload_Mosfet_HighRight;
            PID_Flush_Errors(&current_pid);

            SYNC_Restart();
            cycles_before_start = CYCLES_BEFORE_START;
            ac_sync_state = SWITCH_RELAY_TO_ON;
            break;

        case SWITCH_RELAY_TO_ON:
            //Check voltage and polarity
            if (SYNC_All_Good())
            {
                RELAY_ON;
                timer_standby = 200;
                ac_sync_state = WAIT_RELAY_TO_ON;
            }
            break;
            
        case WAIT_RELAY_TO_ON:
            if (!timer_standby)
            {
                SYNC_Sync_Now_Reset();
                ac_sync_state = WAIT_SYNC_FEW_CYCLES_BEFORE_START;
            }
            break;

            // few cycles before actual begin
        case WAIT_SYNC_FEW_CYCLES_BEFORE_START:
            if (SYNC_Sync_Now())
            {
                if (SYNC_Last_Polarity_Check() == POLARITY_NEG)
                {
                    //ahora es positiva la polaridad, prendo el led
#ifdef USE_LED_FOR_MAIN_POLARITY_BEFORE_GEN
                    LED_ON;
#endif
                }
                else if (SYNC_Last_Polarity_Check() == POLARITY_POS)
                {
                    //ahora es negativa la polaridad, apago el led
#ifdef USE_LED_FOR_MAIN_POLARITY_BEFORE_GEN
                    LED_OFF;
#endif

                    //reviso si debo empezar a generar
                    if (cycles_before_start)
                        cycles_before_start--;
                    else
                        ac_sync_state = WAIT_FOR_FIRST_SYNC;
                }
                else    //debe haber un error en synchro
                    ac_sync_state = START_SYNCING;
                
                SYNC_Sync_Now_Reset();
            }            
            break;
            
        case WAIT_FOR_FIRST_SYNC:
            //por cuestiones de seguridad empiezo siempre por positivo
            if (SYNC_Sync_Now())
            {
                if (SYNC_Last_Polarity_Check() == POLARITY_NEG)
                {
                    ChangeLed(LED_GENERATING);
                    ac_sync_state = WAIT_CROSS_NEG_TO_POS;

                    HIGH_RIGHT(DUTY_NONE);
                    LOW_LEFT(DUTY_NONE);
                    LOW_RIGHT(DUTY_ALWAYS);
                    sequence_ready_reset;
                }
                else if (SYNC_Last_Polarity_Check() == POLARITY_POS)
                {
                    //no hago nada
                    //quiero empezar siempre por positivo
                }
                else    //debe haber un error en synchro
                    ac_sync_state = START_SYNCING;
                
                SYNC_Sync_Now_Reset();
            }
            break;
        
        case GEN_POS:
            if (sequence_ready)
            {
                sequence_ready_reset;
#ifdef USE_LED_FOR_PID_CALCS
                LED_ON;
#endif
                //Adelanto la seniales de corriente,
                if (p_current_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    unsigned char signal_index = (unsigned char) (p_current_ref - sin_half_cycle);
                    
                    //loop de corriente
                    unsigned int calc = *p_current_ref * KI_SIGNAL_PEAK_MULTIPLIER;
                    calc = calc >> 10;

                    switch (signal_state)
                    {
                    case SIGNAL_RISING:
                        d = CurrentLoop ((unsigned short) calc, I_Sense_Pos);
                        HIGH_LEFT(d);

                        if (signal_index > INDEX_TO_MIDDLE)
                        {
                            CurrentLoop_Change_to_HighGain();
                            signal_state = SIGNAL_MIDDLE;
                        }
                        break;

                    case SIGNAL_MIDDLE:
                        d = CurrentLoop ((unsigned short) calc, I_Sense_Pos);
                        HIGH_LEFT(d);

                        if (signal_index > INDEX_TO_FALLING)
                        {
                            CurrentLoop_Change_to_LowGain();
                            signal_state = SIGNAL_FALLING;
                        }
                        break;

                    case SIGNAL_FALLING:
                        d = CurrentLoop ((unsigned short) calc, I_Sense_Pos);
                        HIGH_LEFT(d);

                        if (signal_index > INDEX_TO_REVERT)
                        {
                            // CurrentLoop_Change_to_LowGain();
                            signal_state = SIGNAL_REVERT;
                            HIGH_LEFT(DUTY_NONE);
                        }
                        break;

                    case SIGNAL_REVERT:
                        // d = CurrentLoop ((unsigned short) calc, I_Sense_Pos);
                        // HIGH_LEFT(d);

                        // if (signal_index > 204)
                        // {
                        //     CurrentLoop_Change_to_LowGain();
                        //     signal_state = SIGNAL_REVERT;
                        // }
                        break;
                        
                    }
                    
                    p_current_ref++;
                }
                else
                    //termino de generar la senoidal, corto el mosfet
                    LOW_RIGHT(DUTY_NONE);
                
#ifdef USE_LED_FOR_PID_CALCS
                LED_OFF;
#endif
            }    //end of sequence_ready
            
            if (SYNC_Sync_Now())
            {
                ac_sync_state = WAIT_CROSS_POS_TO_NEG;
                SYNC_Sync_Now_Reset();

                HIGH_LEFT(DUTY_NONE);
                LOW_RIGHT(DUTY_NONE);
                LOW_LEFT(DUTY_ALWAYS);
                sequence_ready_reset;
            }
            else if (!SYNC_All_Good())
            {
                PWM_Off();
#ifdef USE_LED_FOR_PROTECTIONS
                LED_ON;
#endif
                RELAY_OFF;
                ac_sync_state = START_SYNCING;

                //aviso el tipo de error
                if (!SYNC_Frequency_Check())
                    sprintf(s_lcd, "bad freq pos: %d\n", SYNC_delta_t2());

                else if (!SYNC_Pulses_Check())
                    sprintf(s_lcd, "bad sync pulses pos: %d\n", SYNC_delta_t1());

                else if (!SYNC_Vline_Check())
                    sprintf(s_lcd, "bad vline voltage pos: %d\n", SYNC_vline_max());
                else
                    sprintf(s_lcd, "unknow error in pos\n");
                
                Usart1Send(s_lcd);
#ifdef USE_LED_FOR_PROTECTIONS
                LED_OFF;
#endif                
                SYNC_Sync_Now_Reset();
            }
            break;

        case WAIT_CROSS_POS_TO_NEG:
            //espero un sequence_ready para asegurar valores conocidos en el pwm
            if (sequence_ready)
            {
                sequence_ready_reset;
                if (SYNC_Last_Polarity_Check() == POLARITY_POS)
                {
                    PID_Flush_Errors(&current_pid);
                    p_current_ref = sin_half_cycle;
                    CurrentLoop_Change_to_LowGain();
                    signal_state = SIGNAL_RISING;
                    ac_sync_state = GEN_NEG;
                    
#ifdef USE_LED_FOR_MAIN_POLARITY                
                    LED_OFF;
#endif                
                }
                else    //debe haber un error de synchro
                    ac_sync_state = START_SYNCING;
            }
            break;
            
        case GEN_NEG:
            if (sequence_ready)
            {
                sequence_ready_reset;
#ifdef USE_LED_FOR_PID_CALCS
                LED_ON;
#endif
                //Adelanto la senial de corriente,
                if (p_current_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    unsigned char signal_index = (unsigned char) (p_current_ref - sin_half_cycle);
                    
                    //loop de corriente
                    unsigned int calc = *p_current_ref * KI_SIGNAL_PEAK_MULTIPLIER;
                    calc = calc >> 10;

                    switch (signal_state)
                    {
                    case SIGNAL_RISING:
                        d = CurrentLoop ((unsigned short) calc, I_Sense_Neg);
                        HIGH_RIGHT(d);

                        if (signal_index > INDEX_TO_MIDDLE)
                        {
                            CurrentLoop_Change_to_HighGain();
                            signal_state = SIGNAL_MIDDLE;
                        }
                        break;

                    case SIGNAL_MIDDLE:
                        d = CurrentLoop ((unsigned short) calc, I_Sense_Neg);
                        HIGH_RIGHT(d);

                        if (signal_index > INDEX_TO_FALLING)
                        {
                            CurrentLoop_Change_to_LowGain();
                            signal_state = SIGNAL_FALLING;
                        }
                        break;

                    case SIGNAL_FALLING:
                        d = CurrentLoop ((unsigned short) calc, I_Sense_Neg);
                        HIGH_RIGHT(d);

                        if (signal_index > INDEX_TO_REVERT)
                        {
                            // CurrentLoop_Change_to_LowGain();
                            signal_state = SIGNAL_REVERT;
                            HIGH_RIGHT(DUTY_NONE);
                        }
                        break;

                    case SIGNAL_REVERT:
                        // d = CurrentLoop ((unsigned short) calc, I_Sense_Neg);
                        // HIGH_RIGHT(d);

                        // if (signal_index > 204)
                        // {
                        //     CurrentLoop_Change_to_LowGain();
                        //     signal_state = SIGNAL_REVERT;
                        // }
                        break;
                        
                    }
                    
                    p_current_ref++;
                }
                else
                    //termino de generar la senoidal, corto el mosfet
                    LOW_LEFT(DUTY_NONE);
                
#ifdef USE_LED_FOR_PID_CALCS
                LED_OFF;
#endif                
            }    //end of sequence_ready

            //reviso todo el tiempo si debo cambiar de ciclo o si todo sigue bien
            if (SYNC_Sync_Now())
            {
                ac_sync_state = WAIT_CROSS_NEG_TO_POS;
                SYNC_Sync_Now_Reset();

                HIGH_RIGHT(DUTY_NONE);
                LOW_LEFT(DUTY_NONE);
                LOW_RIGHT(DUTY_ALWAYS);
                sequence_ready_reset;
            }
            else if (!SYNC_All_Good())
            {
                PWM_Off();
#ifdef USE_LED_FOR_PROTECTIONS
                LED_ON;
#endif                
                RELAY_OFF;
                ac_sync_state = START_SYNCING;

                //aviso el tipo de error
                if (!SYNC_Frequency_Check())
                    sprintf(s_lcd, "bad freq neg: %d\n", SYNC_delta_t2());

                else if (!SYNC_Pulses_Check())
                    sprintf(s_lcd, "bad sync pulses neg: %d\n", SYNC_delta_t1());

                else if (!SYNC_Vline_Check())
                    sprintf(s_lcd, "bad vline voltage neg: %d\n", SYNC_vline_max());
                else
                    sprintf(s_lcd, "unknow error in neg\n");
                
                Usart1Send(s_lcd);
#ifdef USE_LED_FOR_PROTECTIONS
                LED_OFF;
#endif                
                SYNC_Sync_Now_Reset();
            }
            
            break;

        case WAIT_CROSS_NEG_TO_POS:
            //espero un sequence_ready para asegurar valores conocidos en el pwm
            if (sequence_ready)
            {
                sequence_ready_reset;
                if (SYNC_Last_Polarity_Check() == POLARITY_NEG)
                {
                    PID_Flush_Errors(&current_pid);
                    p_current_ref = sin_half_cycle;
                    CurrentLoop_Change_to_LowGain();
                    signal_state = SIGNAL_RISING;
                    ac_sync_state = GEN_POS;
                
#ifdef USE_LED_FOR_MAIN_POLARITY                
                    LED_ON;
#endif
                }
                else    //debe haber un error de synchro
                    ac_sync_state = START_SYNCING;
            }
            break;
            
        case JUMPER_PROTECTED:
            if (!timer_standby)
            {
                if (!STOP_JUMPER)
                {
                    ac_sync_state = JUMPER_PROTECT_OFF;
                    timer_standby = 400;
                }
            }                
            break;

        case JUMPER_PROTECT_OFF:
            if (!timer_standby)
            {
                //vuelvo a INIT
                ac_sync_state = START_SYNCING;
                Usart1Send((char *) "Protect OFF\n");                    
            }                
            break;            

        case OVERCURRENT_ERROR:
            if (!timer_standby)
            {
                ChangeLed(LED_STANDBY);
                ac_sync_state = START_SYNCING;
            }
            break;

        }

        SYNC_Update_Sync();
        SYNC_Update_Polarity();

        if (SYNC_Cycles_Cnt() > 100)
        {
            SYNC_Cycles_Cnt_Reset();
            sprintf(s_lcd, "d_t1_bar: %d d_t2: %d pol: %d st: %d vline: %d\n",
                    SYNC_delta_t1_half_bar(),
                    SYNC_delta_t2_bar(),
                    SYNC_Last_Polarity_Check(),
                    ac_sync_state,
                    SYNC_Vline_Max());
            Usart1Send(s_lcd);            
        }

        //Cosas que no tienen tanto que ver con las muestras o el estado del programa
        if ((STOP_JUMPER) &&
            (ac_sync_state != JUMPER_PROTECTED) &&
            (ac_sync_state != JUMPER_PROTECT_OFF) &&            
            (ac_sync_state != OVERCURRENT_ERROR))
        {
            RELAY_OFF;
            HIGH_LEFT(DUTY_NONE);
            HIGH_RIGHT(DUTY_NONE);

            LOW_RIGHT(DUTY_NONE);
            LOW_LEFT(DUTY_NONE);
            
            ChangeLed(LED_JUMPER_PROTECTED);
            Usart1Send((char *) "Protect ON\n");
            timer_standby = 1000;
            ac_sync_state = JUMPER_PROTECTED;
        }
        
        if (overcurrent_shutdown)
        {
            RELAY_OFF;
            if (overcurrent_shutdown == 1)
            {
                Usart1Send("Overcurrent POS\n");
                ChangeLed(LED_OVERCURRENT_POS);
            }
            else
            {
                Usart1Send("Overcurrent NEG\n");
                ChangeLed(LED_OVERCURRENT_NEG);
            }

            timer_standby = 10000;
            overcurrent_shutdown = 0;
            ac_sync_state = OVERCURRENT_ERROR;
        }

#ifdef USE_LED_FOR_MAIN_STATES
        UpdateLed();
#endif
    }
    
#endif    // GRID_TIE_FULL_CONNECTED
    
    return 0;
}

//--- End of Main ---//


void PWM_Off (void)
{
    DisablePreload_Mosfet_HighLeft;
    DisablePreload_Mosfet_HighRight;

    LOW_LEFT(DUTY_NONE);
    HIGH_LEFT(DUTY_NONE);
    LOW_RIGHT(DUTY_NONE);
    HIGH_RIGHT(DUTY_NONE);

    EnablePreload_Mosfet_HighLeft;
    EnablePreload_Mosfet_HighRight;
}


unsigned short CurrentLoop (unsigned short setpoint, unsigned short new_sample)
{
    short d = 0;
    
    current_pid.setpoint = setpoint;
    current_pid.sample = new_sample;
    d = PID(&current_pid);
                    
    if (d > 0)
    {
        if (d > DUTY_100_PERCENT)
        {
            d = DUTY_100_PERCENT;
            current_pid.last_d = DUTY_100_PERCENT;
        }
    }
    else
    {
        d = DUTY_NONE;
        current_pid.last_d = DUTY_NONE;
    }

    return (unsigned short) d;
}


void CurrentLoop_Change_to_HighGain (void)
{
    current_pid.kp = 10;
    current_pid.ki = 3;
    current_pid.kd = 0;
}


void CurrentLoop_Change_to_LowGain (void)
{
    current_pid.kp = 10;
    current_pid.ki = 3;
    current_pid.kd = 0;    
}


unsigned short VoltageLoop (unsigned short setpoint, unsigned short new_sample)
{
    short d = 0;
    
    voltage_pid.setpoint = setpoint;
    voltage_pid.sample = new_sample;
    d = PID_Small_Ki(&voltage_pid);
                    
    if (d > 0)
    {
        if (d > DUTY_100_PERCENT)
        {
            d = DUTY_100_PERCENT;
            voltage_pid.last_d = DUTY_100_PERCENT;
        }
    }
    else
    {
        d = DUTY_NONE;
        voltage_pid.last_d = DUTY_NONE;
    }

    return (unsigned short) d;
}


void TimingDelay_Decrement(void)
{
    if (wait_ms_var)
        wait_ms_var--;

    if (timer_standby)
        timer_standby--;

    if (take_temp_sample)
        take_temp_sample--;

    if (timer_meas)
        timer_meas--;

    if (timer_led)
        timer_led--;

    if (timer_filters)
        timer_filters--;

#ifdef INVERTER_ONLY_SYNC_AND_POLARITY
    if (timer_no_sync)
        timer_no_sync--;
#endif
}


#define AC_SYNC_Int        (EXTI->PR & 0x00000100)
#define AC_SYNC_Set        (EXTI->IMR |= 0x00000100)
#define AC_SYNC_Reset      (EXTI->IMR &= ~0x00000100)
#define AC_SYNC_Ack        (EXTI->PR |= 0x00000100)

#define AC_SYNC_Int_Rising          (EXTI->RTSR & 0x00000100)
#define AC_SYNC_Int_Rising_Set      (EXTI->RTSR |= 0x00000100)
#define AC_SYNC_Int_Rising_Reset    (EXTI->RTSR &= ~0x00000100)

#define AC_SYNC_Int_Falling          (EXTI->FTSR & 0x00000100)
#define AC_SYNC_Int_Falling_Set      (EXTI->FTSR |= 0x00000100)
#define AC_SYNC_Int_Falling_Reset    (EXTI->FTSR &= ~0x00000100)

#define OVERCURRENT_POS_Int        (EXTI->PR & 0x00000010)
#define OVERCURRENT_POS_Ack        (EXTI->PR |= 0x00000010)
#define OVERCURRENT_NEG_Int        (EXTI->PR & 0x00000020)
#define OVERCURRENT_NEG_Ack        (EXTI->PR |= 0x00000020)

// #define TIM16_HYSTERESIS    100
void EXTI4_15_IRQHandler(void)
{
#ifdef WITH_AC_SYNC_INT
    if (AC_SYNC_Int)
    {
        if (AC_SYNC_Int_Rising)
        {
            AC_SYNC_Int_Rising_Reset;
            AC_SYNC_Int_Falling_Set;

            SYNC_Rising_Edge_Handler();
#ifdef USE_LED_FOR_AC_PULSES
            LED_ON;
#endif
        }
        else if (AC_SYNC_Int_Falling)
        {
            AC_SYNC_Int_Falling_Reset;
            AC_SYNC_Int_Rising_Set;
            
            SYNC_Falling_Edge_Handler();
#ifdef USE_LED_FOR_AC_PULSES            
            LED_OFF;
#endif
        }
        AC_SYNC_Ack;
    }
#endif
    
#ifdef WITH_OVERCURRENT_SHUTDOWN
    if (OVERCURRENT_POS_Int)
    {
        DisablePreload_Mosfet_HighLeft;
        HIGH_LEFT(DUTY_NONE);
        //TODO: trabar el TIM3 aca!!!
        overcurrent_shutdown = 1;
        OVERCURRENT_POS_Ack;
    }

    if (OVERCURRENT_NEG_Int)
    {
        DisablePreload_Mosfet_HighRight;
        HIGH_RIGHT(DUTY_NONE);
        //TODO: trabar el TIM3 aca!!!
        overcurrent_shutdown = 2;
        OVERCURRENT_NEG_Ack;
    }
#endif
}

//--- end of file ---//
