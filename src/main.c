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

// -- Externals for synchro --------------------------------
#ifdef INVERTER_ONLY_SYNC_AND_POLARITY
volatile unsigned short timer_no_sync = 0;
volatile unsigned short delta_t1 = 0;
volatile unsigned short delta_t1_bar = 0;
volatile unsigned short delta_t2 = 0;
#endif

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


// #define USE_SIGNAL_SINUSOIDAL
// #define USE_SIGNAL_TRIANGULAR
// #define USE_SIGNAL_CURRENT_SIN_0_5_A

// #define USE_SIGNAL_VOLTAGE_200V

// #define USE_SIGNAL_MODIFIED_SIN

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
#define USE_SIGNAL_CURRENT_05_A

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



#ifdef USE_SIGNAL_VOLTAGE_185V
#define K_SIGNAL_PEAK_MULTIPLIER    435    //depende de cual es la medicion del sensor de tension
#endif

#ifdef USE_SIGNAL_VOLTAGE_220V
#define K_SIGNAL_PEAK_MULTIPLIER    517    //depende de cual es la medicion del sensor de tension
#endif

#ifdef USE_SIGNAL_SOFT_START_64V
#define K_SOFT_START_PEAK_MULTIPLIER    150
#endif

#ifdef USE_SIGNAL_SINUSOIDAL
unsigned short mem_signal [SIZEOF_SIGNAL] = {62,125,187,248,309,368,425,481,535,587,
                                             637,684,728,770,809,844,876,904,929,951,
                                             968,982,992,998,1000,998,992,982,968,951,
                                             929,904,876,844,809,770,728,684,637,587,
                                             535,481,425,368,309,248,187,125,62,0};
#endif
#ifdef USE_SIGNAL_TRIANGULAR
unsigned short mem_signal [SIZEOF_SIGNAL] = {40,80,120,160,200,240,280,320,360,400,
                                             440,480,520,560,600,640,680,720,760,800,
                                             840,880,920,960,1000,960,920,880,840,800,
                                             760,720,680,640,600,560,520,480,440,400,
                                             360,320,280,240,200,160,120,80,40,0};
#endif
#ifdef USE_SIGNAL_MODIFIED_SIN
unsigned short mem_signal [SIZEOF_SIGNAL] = {0,0,0,0,333,333,333,333,333,333,
                                             333,333,666,666,666,666,666,666,666,666,
                                             1000,1000,1000,1000,1000,1000,1000,1000,666,666,
                                             666,666,666,666,666,666,333,333,333,333,
                                             333,333,333,333,0,0,0,0,0,0};

#endif

unsigned short * p_current_ref;
unsigned short * p_voltage_ref;
pid_data_obj_t current_pid;
pid_data_obj_t voltage_pid;


// Module Functions ----------------------------------------
unsigned short CurrentLoop (unsigned short, unsigned short);
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
    
    LOW_LEFT(DUTY_NONE);
    HIGH_LEFT(DUTY_NONE);
    LOW_RIGHT(DUTY_NONE);
    HIGH_RIGHT(DUTY_NONE);
    EnablePreload_Mosfet_HighLeft;
    EnablePreload_Mosfet_HighRight;

    //ADC and DMA configuration
    AdcConfig();
    DMAConfig();
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    ADC1->CR |= ADC_CR_ADSTART;
    //end of ADC & DMA

    EXTIOn();

#ifdef HARD_TEST_MODE_STEP_RESPONSE_POSITIVE
    //Respuesta escalon 5ms lado positivo
    LOW_LEFT(DUTY_NONE);
    HIGH_RIGHT(DUTY_NONE);
    LOW_RIGHT(DUTY_ALWAYS);
    LED_OFF;            
    RELAY_ON;
    Wait_ms(100);

    while(1)
    {
        if (!STOP_JUMPER)
        {
            LED_ON;
            HIGH_LEFT(DUTY_100_PERCENT);
            Wait_ms(5);
            HIGH_LEFT(DUTY_NONE);
            LED_OFF;
            //espero que baje la salida al menos al 10%
            while (V_Sense > 100);
            Wait_ms(10);
            
        }
        else
            Wait_ms(500);

    }
#endif

#ifdef HARD_TEST_MODE_STEP_RESPONSE_NEGATIVE
    //Respuesta escalon 5ms lado negativo
    LOW_RIGHT(DUTY_NONE);
    HIGH_LEFT(DUTY_NONE);
    LOW_LEFT(DUTY_ALWAYS);
    LED_OFF;            
    RELAY_ON;
    Wait_ms(100);

    while(1)
    {
        if (!STOP_JUMPER)
        {
            LED_ON;
            HIGH_RIGHT(DUTY_100_PERCENT);
            Wait_ms(5);
            HIGH_RIGHT(DUTY_NONE);
            LED_OFF;
            //espero que baje la salida al menos al 10%
            while (V_Sense > 100);
            Wait_ms(10);
            
        }
        else
            Wait_ms(500);

    }
#endif
    
#ifdef INVERTER_MODE_VOLTAGE_FDBK
    // Initial Setup for PID Controller
    PID_Small_Ki_Flush_Errors(&voltage_pid);    

    voltage_pid.kp = 5;    //3.2
    voltage_pid.ki = 32;      //0.016
    voltage_pid.kd = 16;      //10.32
    unsigned short d = 0;

#ifdef ONLY_ONE_KB817
    //the place for save duty cycles
    unsigned short d_saved[SIZEOF_SIGNAL] = { 0 };
#endif
    
    while (1)
    {
        switch (ac_sync_state)
        {
        case START_SYNCING:
            //Check voltage and polarity
            // if ((Voltage_is_Good()) && Polarity_is_Good())
            LED_OFF;            
            RELAY_ON;
            timer_standby = 200;
            EnablePreload_Mosfet_HighLeft;
            EnablePreload_Mosfet_HighRight;
            ac_sync_state++;
            break;

        case WAIT_RELAY_TO_ON:
            if (!timer_standby)
            {
                p_voltage_ref = sin_half_cycle;
                ac_sync_state = FIRST_SOFT_START_CYCLE;
                d = 0;
                HIGH_RIGHT(DUTY_NONE);
                LOW_LEFT(DUTY_NONE);
                LOW_RIGHT(DUTY_ALWAYS);
            }
            break;

        case FIRST_SOFT_START_CYCLE:    //GEN_POS
            if (sequence_ready)
            {
                sequence_ready_reset;

                //Adelanto la senial de tension
                //el d depende de la tension de entrada (que no puedo medir!!!)
                if (p_voltage_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    unsigned int calc = *p_voltage_ref * K_SOFT_START_PEAK_MULTIPLIER;
                    calc = calc >> 10;
                    HIGH_LEFT(calc);
                    p_voltage_ref++;
                }
                else
                {
                    p_voltage_ref = sin_half_cycle;
                    ac_sync_state = SECOND_SOFT_START_CYCLE;
                    HIGH_LEFT(DUTY_NONE);
                    LOW_RIGHT(DUTY_NONE);
                    LOW_LEFT(DUTY_ALWAYS);
                }
            }
            break;

        case SECOND_SOFT_START_CYCLE:    //GEN_NEG
            if (sequence_ready)
            {
                sequence_ready_reset;

                if (p_voltage_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    unsigned int calc = *p_voltage_ref * K_SOFT_START_PEAK_MULTIPLIER;
                    calc = calc >> 10;
                    HIGH_RIGHT(calc);
                    p_voltage_ref++;
                }
                else
                {
                    //termino soft-start doy paso al PID
                    ac_sync_state = WAIT_FOR_FIRST_SYNC;
                }
            }
            break;            
            
        case WAIT_FOR_FIRST_SYNC:
            ac_sync_state = WAIT_CROSS_NEG_TO_POS;
            d = 0;
            PID_Small_Ki_Flush_Errors(&voltage_pid);                    

            HIGH_RIGHT(DUTY_NONE);
            LOW_LEFT(DUTY_NONE);
            LOW_RIGHT(DUTY_ALWAYS);
            break;
        
        case GEN_POS:
            if (sequence_ready)
            {
                sequence_ready_reset;

                //Adelanto la seniale de tension
                //el d depende de cual deba ajustar
                if (p_voltage_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    //ajusto el valor de la senoidal a lo que mide el sensor
                    //de salida
                    unsigned int calc = *p_voltage_ref * K_SIGNAL_PEAK_MULTIPLIER;
                    calc = calc >> 10;
                    
                    d = VoltageLoop ((unsigned short) calc, V_Sense);
                    HIGH_LEFT(d);
                    p_voltage_ref++;
#ifdef ONLY_ONE_KB817
                    //guardo los d del ciclo positivo
                    unsigned char d_index = (unsigned char) (p_voltage_ref - sin_half_cycle);
                    d_saved[d_index] = d;
#endif
                }
                else
                {
                    ac_sync_state = WAIT_CROSS_POS_TO_NEG;
                    PID_Small_Ki_Flush_Errors(&voltage_pid);                    
                    
                    HIGH_LEFT(DUTY_NONE);
                    LOW_RIGHT(DUTY_NONE);
                    LOW_LEFT(DUTY_ALWAYS);
                }
            }
            break;            

        case WAIT_CROSS_POS_TO_NEG:
            //me quedo esperando en 0 un sample
            if (sequence_ready)
            {
                sequence_ready_reset;
                ac_sync_state = GEN_NEG;
                p_voltage_ref = sin_half_cycle;
                
#ifdef USE_LED_FOR_MAIN_POLARITY
                LED_OFF;
#endif
            }
            break;
            
        case GEN_NEG:
            if (sequence_ready)
            {
                sequence_ready_reset;

                //Adelanto la seniale de tension,
                //el d depende de cual deba ajustar
                if (p_voltage_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    //loop de tension
#ifdef ONLY_ONE_KB814
                    //ajusto el valor de la senoidal a lo que mide el sensor
                    //de salida
                    unsigned int calc = *p_voltage_ref * K_SIGNAL_PEAK_MULTIPLIER;
                    calc = calc >> 10;
                    
                    d = VoltageLoop ((unsigned short) calc, V_Sense);
                    HIGH_RIGHT(d);
#endif
#ifdef ONLY_ONE_KB817
                    //uso el valor guardado en el ciclo positivo
                    unsigned char d_index = (unsigned char) (p_voltage_ref - sin_half_cycle);
                    HIGH_RIGHT(d_saved[d_index]);
#endif
                    p_voltage_ref++;
                }
                else
                {
                    ac_sync_state = WAIT_CROSS_NEG_TO_POS;
                    PID_Small_Ki_Flush_Errors(&voltage_pid);                    

                    HIGH_RIGHT(DUTY_NONE);
                    LOW_LEFT(DUTY_NONE);
                    LOW_RIGHT(DUTY_ALWAYS);
                }
            }
            break;

        case WAIT_CROSS_NEG_TO_POS:
            //me quedo esperando en 0 un sample
            if (sequence_ready)
            {
                sequence_ready_reset;
                ac_sync_state = GEN_POS;
                p_voltage_ref = sin_half_cycle;
                
#ifdef USE_LED_FOR_MAIN_POLARITY
                LED_ON;
#endif
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

        default:
            ac_sync_state = START_SYNCING;
            break;
            
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
#endif    // INVERTER_MODE_VOLTAGE_FDBK


#ifdef INVERTER_MODE_CURRENT_FDBK
    // Initial Setup for PID Controller
    PID_Small_Ki_Flush_Errors(&current_pid);

    current_pid.kp = 5;
    current_pid.ki = 32;
    current_pid.kd = 16;
    unsigned short d = 0;

    while (1)
    {
        switch (ac_sync_state)
        {
        case START_SYNCING:
            //Check voltage and polarity
            // if ((Voltage_is_Good()) && Polarity_is_Good())
            LED_OFF;            
            RELAY_ON;
            timer_standby = 200;
            EnablePreload_Mosfet_HighLeft;
            EnablePreload_Mosfet_HighRight;
            ac_sync_state++;
            break;

        case WAIT_RELAY_TO_ON:
            if (!timer_standby)
            {
                p_voltage_ref = mem_signal_soft_start;
                ac_sync_state = FIRST_SOFT_START_CYCLE;
                d = 0;
                HIGH_RIGHT(DUTY_NONE);
                LOW_LEFT(DUTY_NONE);
                LOW_RIGHT(DUTY_ALWAYS);
            }
            break;

        case FIRST_SOFT_START_CYCLE:    //GEN_POS
            if (sequence_ready)
            {
                sequence_ready_reset;

                //Adelanto la senial de tension
                //el d depende de la tension de entrada (que no puedo medir!!!)
                if (p_voltage_ref < &mem_signal_soft_start[(SIZEOF_SIGNAL - 1)])
                {
                    HIGH_LEFT(*p_voltage_ref);
                    p_voltage_ref++;
                }
                else
                {
                    p_voltage_ref = mem_signal_soft_start;
                    ac_sync_state = SECOND_SOFT_START_CYCLE;
                    HIGH_LEFT(DUTY_NONE);
                    LOW_RIGHT(DUTY_NONE);
                    LOW_LEFT(DUTY_ALWAYS);
                }
            }
            break;

        case SECOND_SOFT_START_CYCLE:    //GEN_NEG
            if (sequence_ready)
            {
                sequence_ready_reset;

                if (p_voltage_ref < &mem_signal_soft_start[(SIZEOF_SIGNAL - 1)])
                {
                    HIGH_RIGHT(*p_voltage_ref);
                    p_voltage_ref++;
                }
                else
                {
                    //termino soft-start doy paso al PID
                    ac_sync_state = WAIT_FOR_FIRST_SYNC;
                }
            }
            break;            
            
        case WAIT_FOR_FIRST_SYNC:
            ac_sync_state = WAIT_CROSS_NEG_TO_POS;
            d = 0;
            PID_Small_Ki_Flush_Errors(&current_pid);

            HIGH_RIGHT(DUTY_NONE);
            LOW_LEFT(DUTY_NONE);
            LOW_RIGHT(DUTY_ALWAYS);
            break;
        
        case GEN_POS:
            if (sequence_ready)
            {
                sequence_ready_reset;

                //Adelanto la seniale de corriente,
                //el d depende de cual deba ajustar
                if (p_current_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    //loop de corriente
                    d = CurrentLoop (*p_current_ref, I_Sense_Pos);
                    HIGH_LEFT(d);
                    p_current_ref++;
                }
                else
                {
                    ac_sync_state = WAIT_CROSS_POS_TO_NEG;
                    PID_Small_Ki_Flush_Errors(&current_pid);
                    
                    HIGH_LEFT(DUTY_NONE);
                    LOW_RIGHT(DUTY_NONE);
                    LOW_LEFT(DUTY_ALWAYS);
                }
            }
            break;            

        case WAIT_CROSS_POS_TO_NEG:
            //me quedo esperando en 0 un sample
            if (sequence_ready)
            {
                sequence_ready_reset;
                ac_sync_state = GEN_NEG;
                p_current_ref = sin_half_cycle;
                
#ifdef USE_LED_FOR_MAIN_POLARITY
                LED_OFF;
#endif
            }
            break;
            
        case GEN_NEG:
            if (sequence_ready)
            {
                sequence_ready_reset;

                //Adelanto la senial de corriente,
                //el d depende de cual deba ajustar
                if (p_current_ref < &sin_half_cycle[(SIZEOF_SIGNAL - 1)])
                {
                    //loop de corriente
                    d = CurrentLoop (*p_current_ref, I_Sense_Neg);
                    HIGH_RIGHT(d);
                    p_current_ref++;
                }
                else
                {
                    ac_sync_state = WAIT_CROSS_NEG_TO_POS;
                    PID_Small_Ki_Flush_Errors(&current_pid);

                    HIGH_RIGHT(DUTY_NONE);
                    LOW_LEFT(DUTY_NONE);
                    LOW_RIGHT(DUTY_ALWAYS);
                }
            }
            break;

        case WAIT_CROSS_NEG_TO_POS:
            //me quedo esperando en 0 un sample
            if (sequence_ready)
            {
                sequence_ready_reset;
                ac_sync_state = GEN_POS;
                p_current_ref = sin_half_cycle;
                
#ifdef USE_LED_FOR_MAIN_POLARITY
                LED_ON;
#endif
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

        default:
            ac_sync_state = START_SYNCING;
            break;
            
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
#endif    // INVERTER_MODE_CURRENT_FDBK
    
    
#ifdef GRID_TIED_ONLY_SYNC_AND_POLARITY

    while (1)
    {
        switch (ac_sync_state)
        {
        case START_SYNCING:
            //Check voltage and polarity
            // if ((Voltage_is_Good()) && Polarity_is_Good())
            LED_OFF;            
            ac_sync_state++;
            break;

        case WAIT_RELAY_TO_ON:
            ac_sync_state++;
            break;

        case WAIT_FOR_FIRST_SYNC:
            if (SYNC_Sync_Now())
            {
                if (SYNC_Last_Polarity_Check() == POLARITY_POS)
                {
                    ac_sync_state = GEN_NEG;
#ifdef USE_LED_FOR_MAIN_POLARITY
                    LED_OFF;
#endif
                }
                else if (SYNC_Last_Polarity_Check() == POLARITY_NEG)
                {
                    ac_sync_state = GEN_POS;
#ifdef USE_LED_FOR_MAIN_POLARITY                    
                    LED_ON;
#endif
                }
                else    //debe haber un error en synchro
                    ac_sync_state = START_SYNCING;
                
                SYNC_Sync_Now_Reset();
            }
            break;
        
        case GEN_POS:
            if (SYNC_Sync_Now())
            {
                ac_sync_state = WAIT_CROSS_POS_TO_NEG;
                SYNC_Sync_Now_Reset();
            }

            //TODO: poner timeout para salir aca o revisar POLARITY_UNKNOWN
            break;

        case WAIT_CROSS_POS_TO_NEG:
            if (SYNC_Last_Polarity_Check() == POLARITY_POS)
            {
                ac_sync_state = GEN_NEG;
#ifdef USE_LED_FOR_MAIN_POLARITY                
                LED_OFF;
#endif                
            }
            else    //debe haber un error de synchro
                ac_sync_state = START_SYNCING;

            break;
            
        case GEN_NEG:
            if (SYNC_Sync_Now())
            {
                ac_sync_state = WAIT_CROSS_NEG_TO_POS;
                SYNC_Sync_Now_Reset();
            }
            
            //TODO: poner timeout para salir aca o revisar POLARITY_UNKNOWN            
            break;

        case WAIT_CROSS_NEG_TO_POS:
            if (SYNC_Last_Polarity_Check() == POLARITY_NEG)
            {
                ac_sync_state = GEN_POS;
#ifdef USE_LED_FOR_MAIN_POLARITY                
                LED_ON;
#endif
            }
            else    //debe haber un error de synchro
                ac_sync_state = START_SYNCING;
                    
            break;

        default:
            ac_sync_state = START_SYNCING;
            break;
            
        }

        SYNC_Update_Sync();
        SYNC_Update_Polarity();

        if (SYNC_Cycles_Cnt() > 100)
        {
            SYNC_Cycles_Cnt_Reset();
            sprintf(s_lcd, "d_t1_bar: %d d_t2: %d pol: %d st: %d vline: %d\n",
                    delta_t1_bar,
                    delta_t2,
                    SYNC_Last_Polarity_Check(),
                    ac_sync_state,
                    SYNC_Vline_Max());
            Usart1Send(s_lcd);            
        }
    }
#endif     // GRID_TIED_ONLY_SYNC_AND_POLARITY
    
    
    //--- Grid Tied Mode ----------
#ifdef GRID_TIED_FULL_CONECTED
    
    while (1)
    {
        switch (ac_sync_state)
        {
        case START_SYNCING:
            RELAY_ON;
            timer_standby = 200;
            ac_sync_state = WAIT_RELAY_TO_ON;
            break;

        case WAIT_RELAY_TO_ON:
            if (!timer_standby)
            {
                SYNC_Sync_Now_Reset();
                TIM17->CNT = 0;
                TIM17->ARR = 9800;
                TIM17Enable();
                ac_sync_state = WAIT_FOR_FIRST_SYNC;
            }
            break;

        case WAIT_FOR_FIRST_SYNC:
            if (SYNC_Sync_Now())
            {
                SYNC_Sync_Now_Reset();
                ac_sync_state = WAIT_CROSS_NEG_TO_POS;
                ChangeLed(LED_GENERATING);

                HIGH_RIGHT(DUTY_NONE);
                LOW_LEFT(DUTY_NONE);
                TIM16->CNT = 0;
            }
            break;
        
        case GEN_POS:
            if (SYNC_Sync_Now())
            {
                SYNC_Sync_Now_Reset();
                ac_sync_state = WAIT_CROSS_POS_TO_NEG;
                TIM16->CNT = 0;
                
                HIGH_LEFT(DUTY_NONE);
                LOW_RIGHT(DUTY_NONE);

#ifdef USE_LED_FOR_MAIN_POLARITY
                LED_OFF;
#endif
            }
#ifdef INVERTER_MODE_PURE_SINUSOIDAL
            else
            {
                if (TIM16->CNT >= 200)
                {
                    TIM16->CNT = 0;
                    //aca la senial (el ultimo punto) terminaria en 0
                    if (p_signal < &mem_signal[(SIZEOF_SIGNAL - 1)])
                        p_signal++;

                    HIGH_LEFT(*p_signal);
                }
            }
#endif    // INVERTER_MODE_PURE_SINUSOIDAL
            break;

        case WAIT_CROSS_POS_TO_NEG:
            if (TIM16->CNT >= 200)
            {
                LOW_LEFT(DUTY_ALWAYS);
#ifndef INVERTER_MODE_PURE_SINUSOIDAL
                HIGH_RIGHT(DUTY_ALWAYS);
#else
                TIM16->CNT = 0;
                p_signal = mem_signal;
                HIGH_RIGHT(*p_signal);
#endif
                ac_sync_state = GEN_NEG;
            }
            break;
            
        case GEN_NEG:
            if (SYNC_Sync_Now())
            {
                SYNC_Sync_Now_Reset();
                ac_sync_state = WAIT_CROSS_NEG_TO_POS;
                TIM16->CNT = 0;

                HIGH_RIGHT(DUTY_NONE);
                LOW_LEFT(DUTY_NONE);

#ifdef USE_LED_FOR_MAIN_POLARITY
                LED_ON;
#endif
            }
#ifdef INVERTER_MODE_PURE_SINUSOIDAL
            else
            {
                if (TIM16->CNT >= 200)
                {
                    TIM16->CNT = 0;
                    //aca la senial (el ultimo punto) terminaria en 0
                    if (p_signal < &mem_signal[(SIZEOF_SIGNAL - 1)])
                        p_signal++;

                    HIGH_RIGHT(*p_signal);
                }
            }
#endif    // INVERTER_MODE_PURE_SINUSOIDAL            
            break;

        case WAIT_CROSS_NEG_TO_POS:
            if (TIM16->CNT >= 200)
            {
                LOW_RIGHT(DUTY_ALWAYS);
#ifndef INVERTER_MODE_PURE_SINUSOIDAL
                HIGH_LEFT(DUTY_ALWAYS);
#else
                TIM16->CNT = 0;
                p_signal = mem_signal;
                HIGH_LEFT(*p_signal);
#endif
                ac_sync_state = GEN_POS;
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
                ChangeLed(LED_OVERCURRENT_POS);
            else
                ChangeLed(LED_OVERCURRENT_NEG);

            timer_standby = 10000;
            overcurrent_shutdown = 0;
            ac_sync_state = OVERCURRENT_ERROR;
        }

#ifdef USE_LED_FOR_MAIN_STATES
        UpdateLed();
#endif
    }
    
#endif    // INVERTER_MODE
    
    return 0;
}

//--- End of Main ---//

unsigned short CurrentLoop (unsigned short setpoint, unsigned short new_sample)
{
    short d = 0;
    
    current_pid.setpoint = setpoint;
    current_pid.sample = new_sample;
    d = PID_Small_Ki(&current_pid);
                    
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
    
    // //cuenta de a 1 minuto
    // if (secs > 59999)	//pasaron 1 min
    // {
    // 	minutes++;
    // 	secs = 0;
    // }
    // else
    // 	secs++;
    //
    // if (minutes > 60)
    // {
    // 	hours++;
    // 	minutes = 0;
    // }


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

void EXTI4_15_IRQHandler(void)
{
#ifdef WITH_AC_SYNC_INT
    if (AC_SYNC_Int)
    {
        if (AC_SYNC_Int_Rising)
        {
            //reseteo tim
            delta_t2 = TIM16->CNT;
            TIM16->CNT = 0;
            AC_SYNC_Int_Rising_Reset;
            AC_SYNC_Int_Falling_Set;

            SYNC_Rising_Edge_Handler();
        }
        else if (AC_SYNC_Int_Falling)
        {
            delta_t1 = TIM16->CNT;
            AC_SYNC_Int_Falling_Reset;
            AC_SYNC_Int_Rising_Set;
            // ac_sync_int_flag = 1;
            
            SYNC_Falling_Edge_Handler();
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

//------ EOF -------//
