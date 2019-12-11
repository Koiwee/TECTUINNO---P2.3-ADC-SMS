#include <msp430.h>
#include <stdio.h>
#include <string.h>
#include "varios.h"
#include "gsm.h"
#include "logo_tnm.h"
#include "SDD1306.h"

void arranque(void);
void config_adc(void);
int calc_temp(void);
void enviar_msj(int);
void limpiar_ram(void);

int gradCent=0,x,i=0;
char dato;
char ram[50]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
char mensaje[40]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int main(void) {
    arranque();
    config_adc();

    enviar_com_AT("AT+CFUN=0\r\n"); //Desactivar funciones
    _delay_cycles(1000000*8);
    if(strstr(ram,"OK") == NULL){
        WDTCTL &= ~(WDTPW | WDTHOLD);
    }
    limpiar_ram();

    enviar_com_AT("AT+CFUN=1\r\n"); //Activar funciones
    _delay_cycles(1000000*8*5);
    if(strstr(ram,"OK") == NULL){
        WDTCTL &= ~(WDTPW | WDTHOLD);
    }
    limpiar_ram();

    x=0;
    while(x==0){
        enviar_com_AT("AT+CREG?\r\n"); // Conexión a la red
        _delay_cycles(1000000*8);
        if(strstr(ram,"+CREG: 0,1") != NULL){
            x=1;
        }
    }
    limpiar_ram();

    enviar_com_AT("AT+CMGF=1\r\n"); // Configuración de modo TEXTO para SMS
    _delay_cycles(1000000*8);
    limpiar_ram();

    __bis_SR_register(LPM0_bits);     // Entra al modo ahorro LPM0(se duerme el uC)

}

// proceso de interrupción DE tx DE la I2C  *****
#pragma vector = USCIAB0TX_VECTOR
__interrupt void USCIAB0TX_ISR(void){
    IFG2 &= ~UCB0TXIFG;  //Limpia bandera después una vez que entra a la interrupción
    __bic_SR_register_on_exit(LPM3_bits); // da la instrucción al procesador de mantenerse despierto
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
    while (!(IFG2&UCA0TXIFG));                // USCI_A0 TX buffer ready?
	dato = UCA0RXBUF;                    // TX -> RXed character
	ram[i] = dato;
	i++;

	if(dato=='K'){
        for(x=0;x<2;x++){
            P1OUT &=~(BIT0+BIT5);    // P2.1  ---__----  0000 0000
            _delay_cycles(100000*8); //	50mS
            P1OUT |=(BIT0+BIT5);     // P2.1  ----------  0000 0010
            _delay_cycles(100000*8); //	50mS
        }
	}
}

#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
{
  if(P2IFG & BIT3){
      gradCent = calc_temp();                               //Método para calcular temperatura
      enviar_msj(gradCent);                                 //Temperatura enviada como parámetro para envío de mensaje
  }
  P2IFG &= ~(BIT3+BIT2);                                    // P2.3 IFG cleared
}

void arranque(void){
    WDTCTL = WDTPW | WDTHOLD;	// amarrar al perro
	config_osc();
    ini_SDD1306();
    config_uart();
    config_sensores();
    P1DIR |=BIT0 + BIT5;
    P1OUT |=BIT5;  // OFF BUZZER

	P1OUT &=~(BIT0 + BIT5);
	_delay_cycles(400000*8);
	P1OUT |=BIT0 + BIT5;

 	comando_oled(0XC8);
    imagen(logotnm);
    _delay_cycles(4000000*8);
    limpia_oled(0);
    __bis_SR_register(GIE);       // Entra al modo ahorro LPM0(se duerme el uC)

}

void config_adc(void){
    ADC10CTL1 = INCH_10 + ADC10DIV_3;                   // Temp Sensor ADC10CLK/4
    ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON;
}

int calc_temp(void){
    long temp=0;
    int grad=0;

    ADC10CTL0 |= ENC + ADC10SC;             //Inicio del muestreo y conversión
    while (ADC10CTL1 & ADC10BUSY);
    temp = ADC10MEM;                        //Se guarda la medición en temp
    grad = ((temp - 673) * 423) / 1024;     //Temperatura se convierte a °C
    return grad;
}

void enviar_msj(int grad){
    limpiar_ram();
    sprintf(mensaje,"TECTUINNO Temp: %d Grados Celcius",grad);   //Armado de la cadena a enviar como contenido

    P1OUT &= ~(BIT0+BIT5);
    _delay_cycles(100000*8);
    P1OUT |= (BIT0+BIT5);
    _delay_cycles(100000*8);

    enviar_com_AT("AT+CMGS=\"1234567890\"\r\n");        //Configurar el número de teléfono
    _delay_cycles(1000000*8);
    enviar_com_AT(mensaje);                             //Envío del contenido del SMS
    while (!(IFG2&UCA0TXIFG));
    UCA0TXBUF=0x1A;                                     //Envío de Ctrl + Z
    _delay_cycles(1000000*8);
    x=0;
}

void limpiar_ram(void){
    for( x=0 ; x<50 ; x++ ){
        ram[x]=0;
    }
    i=0;
}
