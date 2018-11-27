#include <xc.h>
#include <p18f4550.h>
#include <xlcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <delays.h>
#include <plib/usart.h>

//#pragma WDT CONFIG = OFF
#pragma config PBADEN = OFF
#define _XTAL_FREQ 48000000
#define COMANDO_RECEBIDO 1

#define IDLE 0			//estado da máquina
#define VERIFICACRC 1	//estado da maquina
#define COMRECEBIDO 2	//estado da máquina
#define ENVIARESPOSTA 3	//estado da máquina
#define ENVIAACK 4		//estado da máquina
#define ENVIANACK 5		//estado da máquina

#define ALTERACAO '2' //0x32 	//estado de comando
#define LEITURA '1' //0x31 //estado de comando

#define ACK 	0x05	//estado de comando aceito
#define NACK 	0x15	//estado de comando recusado

#define FREQ 48000000    // Frequency = 12MHz
#define baud 9600
#define spbrg_value (((FREQ/64)/baud)-1)    // Refer to the formula for Baud rate calculation in Description tab
#define POLY    0x8408	//constante de calculo de crc

#define comando_versao 	'0'//0x30
#define comando_hora 	'1'//0x31
#define comando_data	'2'//0x32
#define comando_alarme	'3'//0x33

unsigned char serial_data;
unsigned char BUFFCOM[7], BUFFRESP[7];
char estado = IDLE; // estado idle
char versao = 1;//0x31;
char revisao = 2;//0x32;
char tela_atual = 1;
char contador_interrupcao;
char contador_um_segundo;
char buffer[16];
char hora = 0, minuto = 0, segundo = 0;
char dia = 04, mes = 06;
int ano = 2018;
char desp_hora = 5, desp_min = 10;
char contador_alarme = 0;
char despertou = 0;
int i;


//Variáveis para utilizar o AD
unsigned int ADResult;
float temperatura = 0;
char calcula_temp = 0;

float constante_ad = 5.0/1023;

/*
void init_ADC(void){        //Configure ADC with 3 analog channels
OpenADC(ADC_FOSC_64 &              // ADC_FOSC_64:      Clock de conversão do A/D igual a
                        //               FAD = FOSC/64 = 48MHz/64 = 750kHz
                        //               Desta Forma, TAD=1/FAD = 1,33us.
         ADC_RIGHT_JUST &            // ADC_RIGHT_JUST:    Resultado da conversão ocupará os
                        //               bits menos significativos dos regis-
                        //               tradores ADRESH e ADRESL.
         ADC_12_TAD,               // ADC_12_TAD:              Determina o tempo de conversão de uma
                        //               palavra de 10-bits. Neste caso será
                        //               igual a 12*TAD = 12*1,33us = 16us.
         
         ADC_CH1 &          // ADC_CH0:         Atua sobre os bits CHS3:CHS0 do ADCON0
                        //               para selecionar o canal no qual será
                        //               realizada a conversão. Neste caso o AN0.
         ADC_INT_OFF &       // ADC_INT_OFF:      Habilita ou Desabilita a interrupção de términio de
                        //               conversão.
         ADC_REF_VDD_VSS,    //ADC reference voltage from VDD & VSS
         ADC_1ANA);         // BITs PCFG3:PCFG0:        Configura os pinos  AN1(RA1) e AN0(RA0) como Entradas Analógicas
}
*/

void calcula_temperatura(){
    temperatura = (ADResult/15)*constante_ad;
}

void tela_1(){
    SetDDRamAddr(0x00);
    putrsXLCD("Hora: ");
    sprintf(buffer, "%d:%d:%d", hora, minuto, segundo);
    putrsXLCD(buffer);
    SetDDRamAddr(0x40);
    putrsXLCD("Data: ");
    sprintf(buffer, "%d/%d/%d", dia, mes, ano);
    putrsXLCD(buffer);
    return;
//    SetDDRamAddr(0x00);
//    sprintf(buffer, "%d/%d/%d/%d", BUFFCOM[0], BUFFCOM[1], BUFFCOM[2], BUFFCOM[3]);
//    putrsXLCD(buffer);
//    SetDDRamAddr(0x40);
//    sprintf(buffer, "%d/%d/%d", BUFFCOM[4], BUFFCOM[5], BUFFCOM[6]);
//    putrsXLCD(buffer);
//    
}

void tela_2(){
    SetDDRamAddr(0x05);
    putrsXLCD("Alarme");
    SetDDRamAddr(0x46);
    sprintf(buffer, "%d:%d", desp_hora, desp_min);
    putrsXLCD(buffer);
    return;
}

void tela_3(){
    SetDDRamAddr(0x03);
    putrsXLCD("HELLOWORLD");
    SetDDRamAddr(0x45);
    sprintf(buffer, "%d.%d", versao, revisao);
    putrsXLCD(buffer);
    return;
}
void tela_4(){
    SetDDRamAddr(0x03);
    putrsXLCD("TESTEer");
    SetDDRamAddr(0x46);
    sprintf(buffer, "%d C", temperatura);
    putrsXLCD(buffer);
    return;
}

void limpa_tela(){
    SetDDRamAddr(0x00);
    putrsXLCD("                ");
    SetDDRamAddr(0x40);
    putrsXLCD("                ");
    return;
}

void telas(char troca_tela){
    if(troca_tela == 1){
        if(tela_atual == 4){
            limpa_tela();
            tela_atual = 1;
            tela_1();
        }else if(tela_atual == 1){
            limpa_tela();
            tela_atual = 2;
            tela_2();
        }else if(tela_atual == 2){
            limpa_tela();
            tela_atual = 3;
            tela_3();
        }else{
            limpa_tela();
            tela_atual = 4;
            tela_4();
        }
    }else{
        if(tela_atual == 1) tela_1();
        if(tela_atual == 2) tela_2();
        if(tela_atual == 3) tela_3();
        if(tela_atual == 4) tela_4();
    }
}

void troca_telas(){
  if(PORTDbits.RD0){
        __delay_ms(150);
        telas(1);
        while(PORTDbits.RD0){}
    }
}

void trata_despertador(){
    if(PORTDbits.RD1){
        __delay_ms(150);
        if(desp_hora == 24){
            desp_hora = 0;
        }else{
            desp_hora+=1;
        }
        despertou = 0;
        telas(0);
        while(PORTDbits.RD1);
    }
    if(PORTDbits.RD2){
        __delay_ms(150);
        if(desp_min == 60){
            desp_min = 0;
        }else{
            desp_min+=1;
        }
        despertou = 0;
        telas(0);
        
        while(PORTDbits.RD2);
    }   
}

void trata_data(){
    if(hora == 24){
        dia+=1;
        hora = 0;
        return;
    }
    if(mes == 2 && dia == 28){
        mes+=1;
        dia = 1;
        return;
    }
    if(mes == 12 && dia == 30){
        ano+=1;
        mes = 1;
        dia = 1;
        return;
    }
    if(dia == 30){
        mes+=1;
        dia = 1;
        return;
    }
}

void trata_hora(){
    if((segundo) == 60){
        minuto += 1;
        segundo = 0;
        return;
    }
    if((minuto) == 60){
        hora+=1;
        minuto = 0;
        return;
    }
}

void despertar(){
    if((hora) == desp_hora && (minuto) == desp_min && despertou == 0){
        PORTAbits.RA0 = 1;
        contador_alarme += 1;
    }
    if(contador_alarme >= 15){
        PORTAbits.RA0 = 0;
        despertou = 1;
    }
}

void init_XLCD(){
    OpenXLCD(FOUR_BIT&LINES_5X7);
    while(BusyXLCD());
    WriteCmdXLCD(0x06);
    WriteCmdXLCD(0x0C);
} 

void interrupt low_priority pic_isr(void){
    if(TMR0IF){
        TMR0 = 28021;
        ///MUDEI AQUI
        INTCONbits.TMR0IF = 0;
        contador_interrupcao++;
        if(contador_interrupcao == 20){
            contador_um_segundo += 1;
            contador_interrupcao = 0;
            segundo+=1;
        }
    }
    
    if(PIR1bits.RCIF == 1){      
        while(!RCIF); // Wait until RCIF gets low
            BUFFCOM[i]= RCREG;// Retrieve data from reception register
        i++;
        if(i>6){
            PIR1bits.RCIF = 0; // clear rx flag
            i = 0;
            estado = VERIFICACRC;
           // enviaByteTeste(VERIFICACRC);
        }
    }
}


/*
 * Definições das funções de comunicação serial do programa
 * Todas as implementações estão abaixo da main.
 */

unsigned short crc16(char *data_p, unsigned short length);
void calculaCRCBUFFRESP();
void trataComando();
void verificaCRCBUFFCOM();
void maquinaEstado();
void enviaByte(char BYTE);
void tx_data();


void main(void) {
    T0CONbits.TMR0ON = 0; //desabilita Timer
    INTCONbits.TMR0IE = 1; //Habilita interruoÃ§Ã£o TRM0
    INTCONbits.TMR0IF = 0; //Limpa flag de interrupÃ§Ã£o
    T0CONbits.T08BIT = 0; //Temporizador/contador de 16Bits
    T0CONbits.PSA = 0;
    
    T0CONbits.T0PS0 = 1;    //Configuração do pre-scaler
    T0CONbits.T0PS1 = 1;
    T0CONbits.T0PS2 = 0;
    
    T0CONbits.T0CS = 0; //desabilita clock interno
    TMR0 = 32768; //zera o contador da interrupÃ§Ã£o
    T0CONbits.TMR0ON = 1; //habilita o timer 0
    INTCONbits.GIEH = 1; //Global interrupt enable - high
    INTCONbits.GIEL = 1; //global interrupt enable - low


    TRISA = 0x00000000;
    init_XLCD();
    
    SPBRG = spbrg_value;                                // Fill the SPBRG register to set the Baud Rate
    //RCSTA.SPEN=1;                                     // To activate Serial port (TX and RX pins)
   // TXSTA.TXEN=1;                                     // To enable transmission
   // RCSTA.CREN=1;                                     // To enable continuous reception   
    RCSTA = 0b10010000; // 0x90 (SPEN RX9 SREN CREN ADEN FERR OERR RX9D)
    TXSTA = 0b00100000; // 0x20 (CSRC TX9 TXEN SYNC - BRGH TRMT TX9D)
    TRISCbits.RC6 = 0; //TX pin set as output
    TRISCbits.RC7 = 1; //RX pin set as input
    //compare with the table above
    RCIF = 0; //reset RX pin flag
    RCIP = 0; //Not high priority
    RCIE = 1; //Enable RX interrupt
    PEIE = 1; //Enable pheripheral interrupt (serial port is a pheripheral)
    
    //putrsXLCD("Despertador");
    //SetDDRamAddr(0x40);
    //putrsXLCD("v01.00");
    while(1){
        if(contador_um_segundo == 1){
            contador_um_segundo = 0;
            trata_hora();
            trata_data();
            despertar();
            telas(0);
            maquinaEstado();
        }
        if(tela_atual == 2){
            trata_despertador();
        }
        troca_telas();
    }
}

void DelayFor18TCY(void){
    Delay10TCYx(120);
}
 
void DelayPORXLCD(void){
    Delay1KTCYx(180);
    return;
}
 
void DelayXLCD(void){
    Delay1KTCYx(60);
    return;
}


unsigned short crc16(char *data_p, unsigned short length){
      unsigned char i;
      unsigned int data;
      unsigned int crc = 0xffff;
      if (length == 0)
            return (~crc);
      do{
            for (i=0, data=(unsigned int)0xff & *data_p++; i < 8; i++, data >>= 1){
                  if ((crc & 0x0001) ^ (data & 0x0001))
                        crc = (crc >> 1) ^ POLY;
                  else  crc >>= 1;
            }
      } while (--length);

      crc = ~crc;
      data = crc;
      crc = (crc << 8) | (data >> 8 & 0xff);
      return (crc);
}


void calculaCRCBUFFRESP(){
	unsigned int crc = crc16(BUFFCOM, 5);
	unsigned char crc1 = crc >> 8;
	unsigned char crc2 = crc & 0xFF;
	memcpy(&BUFFCOM[5], crc1, sizeof(BUFFCOM[5]));
	memcpy(&BUFFCOM[6], crc2, sizeof(BUFFCOM[6]));
	estado = ENVIARESPOSTA;
}

void trataComando(){
    int iterator;
	//verifica se e comando de leitura ou alteracao	
	switch(BUFFCOM[0]){
		case LEITURA:
			memcpy(&BUFFRESP[0], &BUFFCOM[0], sizeof(BUFFCOM[0]));
			memcpy(&BUFFRESP[1], &BUFFCOM[1], sizeof(BUFFCOM[1]));
			switch(BUFFCOM[1]){
				//consulta a versao e revisao do sistema
				case comando_versao:
                    memcpy(&BUFFRESP[2], &versao, sizeof(versao));
                    memcpy(&BUFFRESP[3], &revisao, sizeof(revisao));
                    memcpy(&BUFFRESP[4], 0x0, sizeof(char));
					//BUFFRESP[2] = versao;
					//BUFFRESP[3] = revisao;
					//BUFFRESP[4] = 0xFF; //posição vazia
					break;			
				
				//consulta a hora atual do sistema
				case comando_hora:
					BUFFRESP[2] = hora;
					BUFFRESP[3] = minuto;
					BUFFRESP[4] = segundo;
					break;

				//consulta a data atual do sistema
				case comando_data:
					BUFFRESP[2] = dia;
					BUFFRESP[3] = mes;
					BUFFRESP[4] = ano;
					break;

				//consulta a data atual do alarme
				case comando_alarme:
					BUFFRESP[2] = desp_hora;
					BUFFRESP[3] = desp_min;
					BUFFRESP[4] = 0xFF;
					break;
				
				default:
					break;
			}	
			break;

		case ALTERACAO:
			for(iterator = 0; iterator < 5; iterator++){
				memcpy(&BUFFRESP[iterator], &BUFFCOM[iterator], sizeof(BUFFCOM[iterator]));
            }
			// verifica o que deve ser lido
			switch(BUFFCOM[1]){
				//altera versao e revisao
				case comando_versao:		
					versao  = BUFFCOM[2]-48;
					revisao = BUFFCOM[3]-48;
					break;			
				
				//altera hora
				case comando_hora:
					hora = (BUFFCOM[2]-48);
					minuto = (BUFFCOM[3]-48);
					segundo = (BUFFCOM[4]-48);
					break;
				
				//altera data
				case comando_data:
					dia = BUFFCOM[2]-48;
					mes = BUFFCOM[3]-48;
					ano = BUFFCOM[4]-48;
					break;
				
				//altera hora do alarme
				case comando_alarme:
					desp_hora = BUFFCOM[2]-48;
					desp_min = BUFFCOM[3]-48;
					//segundo_alarme = BUFFCOM[4];
				default:
					break;
			}
		break;		
	}
	calculaCRCBUFFRESP();	
}



void verificaCRCBUFFCOM(){
	char CRC_OK;
	unsigned int crc = crc16(BUFFCOM, 5);
	unsigned char crc1 = crc >> 8;
	unsigned char crc2 = crc & 0xFF;
    
	if(crc1 == BUFFCOM[5] && BUFFCOM[6] == crc2) CRC_OK = 1;
	else CRC_OK = 0;
    
	if(CRC_OK == 1) estado = ENVIAACK;
	else estado = ENVIANACK;
}

void maquinaEstado(){
	switch(estado){
		case VERIFICACRC:
			verificaCRCBUFFCOM();
			break;
		
		case COMRECEBIDO:
			trataComando();
			break;
		
		case ENVIARESPOSTA:
			tx_data();
			break;	
		
		case ENVIAACK:
			enviaByte(ACK);
			break;
		
		case ENVIANACK:
			enviaByte(NACK);
			break;
		
        case IDLE:
            break;
		default:
			break;
	}
}

void enviaByte(char BYTE){
	while(!TXIF);                            // Wait until RCIF gets low
	TXREG = BYTE;

	if(BYTE==ACK)
		estado = COMRECEBIDO;
	else
		estado = IDLE;
}

//criar uma função para enviar para o Tx todo o byte que eu recebo no RX
//envia buffer de resposta pela serial
void tx_data(){
	int cont;
	for(cont=0;cont<7;cont++){
    	while(!TXIF);                            // Wait until RCIF gets low
    	TXREG = BUFFRESP[cont];
	}
	estado = IDLE;
}