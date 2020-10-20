//---------------------------------------------
// ## @Author: Med
// ## @Editor: Emacs - ggtags
// ## @TAGS:   Global
// ## @CPU:    TEST PLATFORM FOR FIRMWARE
// ##
// #### TESTS.C ###############################
//---------------------------------------------

// Includes Modules for tests --------------------------------------------------
#include "gen_signal.h"
#include "pwm_defs.h"
#include "dsp.h"

#include <stdio.h>
#include <math.h>

// Types Constants and Macros --------------------------------------------------
#define KI_SIGNAL_PEAK_MULTIPLIER    2792

// Externals -------------------------------------------------------------------


// Globals ---------------------------------------------------------------------
unsigned short reference [SIZEOF_SIGNAL] = { 0 };
unsigned short duty_high_left [SIZEOF_SIGNAL] = { 0 };
unsigned short duty_high_right [SIZEOF_SIGNAL] = { 0 };
unsigned short vinput[SIZEOF_SIGNAL] = { 0 };
float vinput_applied[SIZEOF_SIGNAL] = { 0 };
float voutput[SIZEOF_SIGNAL] = { 0 };
unsigned short voutput_adc[SIZEOF_SIGNAL] = { 0 };

// Module Functions to Test ----------------------------------------------------
void TEST_DspModule (void);

float Plant_Out (float);
void Plant_Step_Response (void);
void Plant_Step_Response_Duty (void);

unsigned short Adc12BitsConvertion (float );
void HIGH_LEFT (unsigned short duty);
void LOW_RIGHT (unsigned short duty);

void HIGH_RIGHT (unsigned short duty);
void LOW_LEFT (unsigned short duty);

//Auxiliares
void ShowVectorFloat (char *, float *, unsigned char);
void ShowVectorUShort (char *, unsigned short *, unsigned char);
void ShowVectorInt (char *, int *, unsigned char);

void PrintOK (void);
void PrintERR (void);


// Module Functions ------------------------------------------------------------
int main (int argc, char *argv[])
{
    printf("Simple module tests\n");
    TEST_DspModule ();
    

    //pruebo un step de la planta
    // Plant_Step_Response();

    //pruebo un step de la planta pero con duty y vinput
    //la tension de entrada es tan alta que incluso con duty_max = 10000
    //tengo errores del 2%
    // Plant_Step_Response_Duty();
    
    


//     float calc = 0.0;
//     for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
//     {
//         calc = sin (3.1415 * i / SIZEOF_SIGNAL);
//         calc = 350 - calc * 311;
//         vinput[i] = (unsigned short) calc;
//     }

//     for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
//     {
//     }

//     // ShowVectorUShort("\nVector reference:\n", reference, SIZEOF_SIGNAL);
//     // ShowVectorUShort("\nVector voltage input:\n", vinput, SIZEOF_SIGNAL);
// #ifdef TEST_ON_ACPOS    
//     ShowVectorUShort("\nVector duty_high_left:\n", duty_high_left, SIZEOF_SIGNAL);
// #endif
// #ifdef TEST_ON_ACNEG
//     ShowVectorUShort("\nVector duty_high_right:\n", duty_high_right, SIZEOF_SIGNAL);
// #endif

//     ShowVectorFloat("\nVector vinput_applied:\n", vinput_applied, SIZEOF_SIGNAL);
//     ShowVectorFloat("\nVector plant output:\n", voutput, SIZEOF_SIGNAL);

//     ShowVectorUShort("\nVector plant output ADC:\n", voutput_adc, SIZEOF_SIGNAL);

//     int error [SIZEOF_SIGNAL] = { 0 };
//     for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
//         error[i] = reference[i] - voutput_adc[i];

//     ShowVectorInt("\nPlant output error:\n", error, SIZEOF_SIGNAL);
//     ShowVectorUShort("\nVector reference:\n", reference, SIZEOF_SIGNAL);

    return 0;
}


void TEST_DspModule (void)
{
    printf("Testing dsp module: ");
    
    pid_data_obj_t pid1;

    pid1.kp = 128;
    pid1.ki = 0;
    pid1.kd = 0;
    PID_Flush_Errors(&pid1);

    pid1.setpoint = 100;
    pid1.sample = 100;
    short d = 0;
    
    d = PID(&pid1);
    if (d != 0)
    {
        PrintERR();
        printf("expected 0, d was: %d\n", d);
    }

    pid1.sample = 99;
    d = PID(&pid1);
    if (d != 1)
    {
        PrintERR();
        printf("expected 1, d was: %d\n", d);
    }

    pid1.sample = 0;
    d = PID(&pid1);
    if (d != 100)
    {
        PrintERR();
        printf("expected 100, d was: %d\n", d);
    }

    pid1.kp = 0;
    pid1.ki = 64;
    PID_Flush_Errors(&pid1);

    pid1.setpoint = 100;
    pid1.sample = 0;

    d = PID(&pid1);
    if (d != 50)
    {
        PrintERR();
        printf("expected 50, d was: %d\n", d);
    }

    d = PID(&pid1);
    if (d != 100)
    {
        PrintERR();
        printf("expected 100, d was: %d\n", d);
    }

    PrintOK();
}


///////////////////////////////////////////////
// Cosas que tienen que ver con las seniales //
///////////////////////////////////////////////
#define DUTY_NONE    0
#define DUTY_100_PERCENT    2000
#define DUTY_ALWAYS    (DUTY_100_PERCENT + 1)
/////////////////////////////////////////////
// Cosas que tienen que ver con mediciones //
/////////////////////////////////////////////
#define INDEX_TO_MIDDLE    47
#define INDEX_TO_FALLING    156
#define INDEX_TO_REVERT    204
    

unsigned short I_Sense_Pos = 0;
unsigned short I_Sense_Neg = 0;
unsigned short last_output = 0;

unsigned short d = 0;


unsigned char cntr_high_left = 0;
void HIGH_LEFT (unsigned short duty)
{
    float out = 0.0;
    float input = 0.0;
    
    duty_high_left[cntr_high_left] = duty;

    //aplico el nuevo duty a la planta
    input = vinput[cntr_high_left] * duty;
    input = input / DUTY_100_PERCENT;
    vinput_applied[cntr_high_left] = input;

    voutput[cntr_high_left] = Plant_Out(input);
    voutput_adc[cntr_high_left] = Adc12BitsConvertion(voutput[cntr_high_left]);
    last_output = voutput_adc[cntr_high_left];
    
    cntr_high_left++;
}

unsigned char cntr_high_right = 0;
void HIGH_RIGHT (unsigned short duty)
{
    float out = 0.0;
    float input = 0.0;
    
    duty_high_right[cntr_high_right] = duty;

    //aplico el nuevo duty a la planta
    input = vinput[cntr_high_right] * duty;
    input = input / DUTY_100_PERCENT;
    vinput_applied[cntr_high_right] = input;

    voutput[cntr_high_right] = Plant_Out(input);
    voutput_adc[cntr_high_right] = Adc12BitsConvertion(voutput[cntr_high_right]);
    last_output = voutput_adc[cntr_high_right];
    
    cntr_high_right++;
}


unsigned short duty_low_right [SIZEOF_SIGNAL] = { 0 };
unsigned char cntr_low_right = 0;
void LOW_RIGHT (unsigned short duty)
{
    duty_low_right[cntr_low_right] = duty;
    cntr_low_right++;
}


unsigned short duty_low_left [SIZEOF_SIGNAL] = { 0 };
unsigned char cntr_low_left = 0;
void LOW_LEFT (unsigned short duty)
{
    duty_low_left[cntr_low_left] = duty;
    cntr_low_left++;
}



//b[3]: [0.] b[2]: [0.02032562] b[1]: [0.0624813] b[0]: [0.01978482]
//a[3]: 1.0 a[2]: 0.011881459485754697 a[1]: 0.014346828516393684 a[0]: -0.9474935161284341
float output = 0.0;
float output_z1 = 0.0;
float output_z2 = 0.0;
float output_z3 = 0.0;
float input_z1 = 0.0;
float input_z2 = 0.0;
float input_z3 = 0.0;

unsigned char cntr_plant = 0;
float Plant_Out (float in)
{
    float output_b = 0.0;
    float output_a = 0.0;
    
    // output = 0. * input + 0.02032 * input_z1 + 0.06248 * input_z2 + 0.01978 * input_z3
    //     - 0.01188 * output_z1 - 0.01434 * output_z2 + 0.94749 * output_z3;

    if (cntr_plant > 2)
    {
        output_b = 0. * in + 0.02032 * input_z1 + 0.06248 * input_z2 + 0.01978 * input_z3;
        output_a = 0.01188 * output_z1 + 0.01434 * output_z2 - 0.94749 * output_z3;

        output = output_b - output_a;

        input_z3 = input_z2;
        input_z2 = input_z1;        
        input_z1 = in;

        output_z3 = output_z2;
        output_z2 = output_z1;        
        output_z1 = output;
        
    }
    else if (cntr_plant > 1)
    {
        output_b = 0. * in + 0.02032 * input_z1 + 0.06248 * input_z2;
        output_a = 0.01188 * output_z1 + 0.01434 * output_z2;
        output = output_b - output_a;

        input_z2 = input_z1;
        input_z1 = in;

        output_z2 = output_z1;
        output_z1 = output;
    }
    else if (cntr_plant > 0)
    {
        output_b = 0. * in + 0.02032 * input_z1;
        output_a = 0.01188 * output_z1;
        output = output_b - output_a;

        input_z2 = input_z1;
        input_z1 = in;

        output_z2 = output_z1;
        output_z1 = output;
    }
    else
    {
        output = 0. * in;
        
        input_z1 = in;

        output_z1 = output;
    }

    cntr_plant++;

    return output;
}


void Plant_Step_Response (void)
{
    printf("\nPlant Step Response\n");
    
    for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
    {
        vinput[i] = 1;
    }

    for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
    {
        voutput[i] = Plant_Out(vinput[i]);
    }
    

    ShowVectorUShort("\nVector voltage input:\n", vinput, SIZEOF_SIGNAL);
    ShowVectorFloat("\nVector plant output:\n", voutput, SIZEOF_SIGNAL);

}


void Plant_Step_Response_Duty (void)
{
    printf("\nPlant Step Response with duty and vinput\n");

    unsigned short d = 28;
    for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
    {
        vinput[i] = 350;
    }

    for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
    {
        vinput_applied[i] = vinput[i] * d;
        vinput_applied[i] = vinput_applied[i] / DUTY_100_PERCENT;
        voutput[i] = Plant_Out(vinput_applied[i]);
    }
    

    ShowVectorFloat("\nVector voltage input applied:\n", vinput_applied, SIZEOF_SIGNAL);
    ShowVectorFloat("\nVector plant output:\n", voutput, SIZEOF_SIGNAL);
    
    unsigned short adc_out [SIZEOF_SIGNAL] = { 0 };
    for (unsigned char i = 0; i < SIZEOF_SIGNAL; i++)
        adc_out[i] = Adc12BitsConvertion(voutput[i]);

    ShowVectorUShort("\nVector plant output ADC:\n", adc_out, SIZEOF_SIGNAL);
    
}


unsigned short Adc12BitsConvertion (float sample)
{
    if (sample > 0.0001)
    {
        sample = sample / 3.3;
        sample = sample * 4095;
        
        if (sample > 4095)
            sample = 4095;
    }
    else
        sample = 0.0;

    return (unsigned short) sample;
    
}


void ShowVectorFloat (char * s_comment, float * f_vect, unsigned char size)
{
    printf(s_comment);
    for (unsigned char i = 0; i < size; i+=8)
        printf("index: %03d - %f %f %f %f %f %f %f %f\n",
               i,
               *(f_vect+i+0),
               *(f_vect+i+1),
               *(f_vect+i+2),
               *(f_vect+i+3),
               *(f_vect+i+4),
               *(f_vect+i+5),
               *(f_vect+i+6),
               *(f_vect+i+7));
    
}


void ShowVectorUShort (char * s_comment, unsigned short * int_vect, unsigned char size)
{
    printf(s_comment);
    for (unsigned char i = 0; i < size; i+=8)
        printf("index: %03d - %d %d %d %d %d %d %d %d\n",
               i,
               *(int_vect+i+0),
               *(int_vect+i+1),
               *(int_vect+i+2),
               *(int_vect+i+3),
               *(int_vect+i+4),
               *(int_vect+i+5),
               *(int_vect+i+6),
               *(int_vect+i+7));
    
}


void ShowVectorInt (char * s_comment, int * int_vect, unsigned char size)
{
    printf(s_comment);
    for (unsigned char i = 0; i < size; i+=8)
        printf("index: %03d - %d %d %d %d %d %d %d %d\n",
               i,
               *(int_vect+i+0),
               *(int_vect+i+1),
               *(int_vect+i+2),
               *(int_vect+i+3),
               *(int_vect+i+4),
               *(int_vect+i+5),
               *(int_vect+i+6),
               *(int_vect+i+7));
    
}


void PrintOK (void)
{
    printf("\033[0;32m");    //green
    printf("OK\n");
    printf("\033[0m");    //reset
}


void PrintERR (void)
{
    printf("\033[0;31m");    //red
    printf("ERR\n");
    printf("\033[0m");    //reset
}
//--- end of file ---//


