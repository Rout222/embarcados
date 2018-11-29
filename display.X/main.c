#include <xc.h>
#include <p18f4550.h>
#include <xlcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <delays.h>
#include <plib/usart.h>


typedef struct {
    char h;
    char m;
    char s;
} tempo;

typedef struct {
    tempo inicio;
    tempo fim;
} alarme;

typedef struct{
    int potencia;
    int dia;
} consumoDia;

typedef struct{
    consumoDia consumoDia[30];
} consumoMes;

char semanaPrint[7]           = {'d', 's', 't', 'q', 'q', 's', 's'};
int  semanaAlarme[7]          = {  0,  0,   0,    0,   0,   0,   0};

tempo horaAtual;
consumoMes energia;
//#pragma WDT CONFIG = OFF
#pragma config PBADEN = OFF
#define _XTAL_FREQ 48000000
#define COMANDO_RECEBIDO 1

#define IDLE 0			//estado da m�quina
#define VERIFICACRC 1	//estado da maquina
#define COMRECEBIDO 2	//estado da m�quina
#define ENVIARESPOSTA 3	//estado da m�quina
#define ENVIAACK 4		//estado da m�quina
#define ENVIANACK 5		//estado da m�quina

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


#define CONST_TELAINICIAL  1
#define CONST_CONSUMO 2
#define CONST_CONFIGURACAO 3
#define CONST_ALARME 4


unsigned char serial_data;
unsigned char BUFFCOM[7], BUFFRESP[7];
char estado = IDLE; // estado idle
char versao = 1;//0x31;
char revisao = 2;//0x32;
char tela_atual = CONST_TELAINICIAL;
char contador_interrupcao;
char contador_um_segundo;
char mensagemL1[16];
char mensagemL2[16];
char dia = 04, mes = 06;
int ano = 2018;
char desp_hora = 5, desp_min = 10;
char contador_alarme = 0;
char despertou = 0;
int i;
int funcao = 0;
int tela = 1;
int telaAntiga;
char primeiraExec = 1;



//Vari�veis para utilizar o AD
unsigned int ADResult;
float temperatura = 0;
char calcula_temp = 0;

float constante_ad = 5.0/1023;

/*
void init_ADC(void){        //Configure ADC with 3 analog channels
OpenADC(ADC_FOSC_64 &              // ADC_FOSC_64:      Clock de convers�o do A/D igual a
                        //               FAD = FOSC/64 = 48MHz/64 = 750kHz
                        //               Desta Forma, TAD=1/FAD = 1,33us.
         ADC_RIGHT_JUST &            // ADC_RIGHT_JUST:    Resultado da convers�o ocupar� os
                        //               bits menos significativos dos regis-
                        //               tradores ADRESH e ADRESL.
         ADC_12_TAD,               // ADC_12_TAD:              Determina o tempo de convers�o de uma
                        //               palavra de 10-bits. Neste caso ser�
                        //               igual a 12*TAD = 12*1,33us = 16us.
         
         ADC_CH1 &          // ADC_CH0:         Atua sobre os bits CHS3:CHS0 do ADCON0
                        //               para selecionar o canal no qual ser�
                        //               realizada a convers�o. Neste caso o AN0.
         ADC_INT_OFF &       // ADC_INT_OFF:      Habilita ou Desabilita a interrup��o de t�rminio de
                        //               convers�o.
         ADC_REF_VDD_VSS,    //ADC reference voltage from VDD & VSS
         ADC_1ANA);         // BITs PCFG3:PCFG0:        Configura os pinos  AN1(RA1) e AN0(RA0) como Entradas Anal�gicas
}
*/

void init_XLCD(){
    OpenXLCD(FOUR_BIT&LINES_5X7);
    while(BusyXLCD());
    WriteCmdXLCD(0x06);
    WriteCmdXLCD(0x0C);
} 

void calcula_temperatura(){
    temperatura = (ADResult/15)*constante_ad;
}


void escreveMensagem(char *l1,char *l2){
    putrsXLCD(l1);
    SetDDRamAddr(0x40);
    putrsXLCD(l2);
    
}

void limpaTela(){
    init_XLCD();
}


void telas(){

    switch(tela){
        case  CONST_TELAINICIAL :
            limpaTela();
            sprintf(mensagemL1,"[%c %c %c %c %c %c %c]", semanaPrint[0], semanaPrint[1], semanaPrint[2], semanaPrint[3], semanaPrint[4], semanaPrint[5], semanaPrint[6]);
            sprintf(mensagemL2,"Horario : %d:%d:%d",horaAtual.h , horaAtual.m, horaAtual.s);   
            escreveMensagem(mensagemL1,mensagemL2);
        break;

        case  CONST_CONSUMO :
            limpaTela();
            sprintf(mensagemL1,"CONSUMO : ");
            sprintf(mensagemL2,"f*        %d W",energia.consumoDia[dia].potencia);   
            escreveMensagem(mensagemL1,mensagemL2);

        break;
        
        case  CONST_CONFIGURACAO :
            limpaTela();
            sprintf(mensagemL1,"CONFIGURACAO : ");
            sprintf(mensagemL2,"f* on/off_sem/hor_rel");   
            escreveMensagem(mensagemL1,mensagemL2);
        break;
        
        case  CONST_ALARME :
            limpaTela();
            sprintf(mensagemL1,"ALARME : ");
            sprintf(mensagemL2,"f* hora_liga/hora_desl.");   
            escreveMensagem(mensagemL1,mensagemL2);

        break;

        default :
            limpaTela();
            sprintf(mensagemL1,"Defaltoss");
            sprintf(mensagemL2,"tela %i", tela);   
            escreveMensagem(mensagemL1,mensagemL2);

    }
}


void troca_telas(){
    if(PORTDbits.RD0){
        __delay_ms(150);

        tela += tela;
        if(tela>4){tela = 1;}
        

        while(PORTDbits.RD0){};
        
    }

}



void trata_data(){
    if(horaAtual.h == 24){
        dia+=1;
        horaAtual.h = 0;
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
    if((horaAtual.s) == 60){
        horaAtual.m += 1;
        horaAtual.s = 0;
        return;
    }
    if((horaAtual.m) == 60){
        horaAtual.h+=1;
        horaAtual.m = 0;
        return;
    }
}

void despertar(){
    if((horaAtual.h) == desp_hora && (horaAtual.m) == desp_min && despertou == 0){
        PORTAbits.RA0 = 1;
        contador_alarme += 1;
    }
    if(contador_alarme >= 15){
        PORTAbits.RA0 = 0;
        despertou = 1;
    }
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
            horaAtual.s+=1;
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
 * Defini��es das fun��es de comunica��o serial do programa
 * Todas as implementa��es est�o abaixo da main.
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
    INTCONbits.TMR0IE = 1; //Habilita interruoção TRM0
    INTCONbits.TMR0IF = 0; //Limpa flag de interrupção
    T0CONbits.T08BIT = 0; //Temporizador/contador de 16Bits
    T0CONbits.PSA = 0;
    
    T0CONbits.T0PS0 = 1;    //Configura��o do pre-scaler
    T0CONbits.T0PS1 = 1;
    T0CONbits.T0PS2 = 0;
    
    T0CONbits.T0CS = 0; //desabilita clock interno
    TMR0 = 32768; //zera o contador da interrupção
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
    
    //inicializador de structs
    horaAtual.h = 0x0;
    horaAtual.m = 0x0;
    horaAtual.s = 0x0;
    
   

    for (int b = 1; b<31; b++){
        energia.consumoDia[b].potencia = 0;
        energia.consumoDia[b].dia = 0;
    }

    
    while(1){
        if(contador_um_segundo == 1){
            contador_um_segundo = 0;
            trata_hora();
            trata_data();
            despertar();
            maquinaEstado();
            telas();
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
					//BUFFRESP[4] = 0xFF; //posi��o vazia
					break;			
				
				//consulta a hora atual do sistema
				case comando_hora:
					BUFFRESP[2] = horaAtual.h;
					BUFFRESP[3] = horaAtual.m;
					BUFFRESP[4] = horaAtual.s;
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
					horaAtual.h = (BUFFCOM[2]-48);
					horaAtual.m = (BUFFCOM[3]-48);
					horaAtual.s = (BUFFCOM[4]-48);
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

//criar uma fun��o para enviar para o Tx todo o byte que eu recebo no RX
//envia buffer de resposta pela serial
void tx_data(){
	int cont;
	for(cont=0;cont<7;cont++){
    	while(!TXIF);                            // Wait until RCIF gets low
    	TXREG = BUFFRESP[cont];
	}
	estado = IDLE;
}