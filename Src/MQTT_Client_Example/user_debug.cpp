/*
@Date  : 8/11/2021
@Author: Tan Dung, Tran
@Brief : debug utils
 */


#include <stdarg.h>
#include <stdio.h>

// hw dependencies
#include "Arduino.h"
#define UART_DEBUG  Serial.print
#define UART_DEBUGX Serial.println
#define BAUDRATE_DEBUG 115200
// Sem/mutex lock
// SemaphoreHandle_t dbgSem_id;

void user_debug_init(void)
{
    // Serial.begin(BAUDRATE_DEBUG);
}

/* ---------------- For ITM debug ---------------- */

//#define ITM_Port8(n)    (*((volatile unsigned char *)(0xE0000000+4*n)))
//#define ITM_Port16(n)   (*((volatile unsigned short*)(0xE0000000+4*n)))
//#define ITM_Port32(n)   (*((volatile unsigned long *)(0xE0000000+4*n)))

//#define DEMCR           (*((volatile unsigned long *)(0xE000EDFC)))
//#define TRCENA          0x01000000

//struct __FILE { int handle; /* Add whatever needed */ };
//FILE __stdout;
//FILE __stdin;

// Retarget to ITM debug
/*
int fputc(int ch, FILE *f) {
//  if (DEMCR & TRCENA) {
//    while (ITM_Port32(0) == 0);
//    ITM_Port8(0) = ch;
//  }
//  return(ch);
	return( ITM_SendChar(ch ) );
}
*/


// Retarget to serial 
/*
int fputc(int ch, FILE *f) {
	UART_DMA_TX_SendByte(&huart3_dma,(uint8_t)ch);
	return(ch);
}
*/


// Using serial with DMA optimize
static  uint8_t  buffer[256];
static void uart_printf(const char *fmt, ...)
{
	uint32_t len;
	va_list vArgs;
	va_start(vArgs, fmt);
	len = vsprintf((char *)&buffer[0], (char const *)fmt, vArgs);
	va_end(vArgs);
    for (size_t i = 0; i < len; i++)
    {
        UART_DEBUG(buffer[i]);
    }	
}
// Du`.. fat hien ra chan ly :D
static void uart_vprintf(const char *fmt, va_list vArgs)
{
	uint32_t len;
	len = vsprintf((char *)&buffer[0], (char const *)fmt, vArgs);
	for (size_t i = 0; i < len; i++)
    {
        UART_DEBUG(buffer[i]);
    }	
}

/* ############### Actual debug redirect ################# */
#define __debug_printf(fmt, ...) uart_printf(fmt, __VA_ARGS__)
#define __debug_vprintf(fmt, vArgs) uart_vprintf(fmt, vArgs)

/* ------------------- MAIN DEBUG I/O -------------- */
void user_debug_print(int level, const char* module, int line, const char* fmt, ...)
{
  // osSemaphoreWait(dbgSem_id , osWaitForever);

	#if 0
	switch(level)
	{
		default:
		case 1:
		__printf("[ERROR]");
		break;
		case 2:
		__printf("[WARN]");
		break;
		case 3:
		__printf("[INFO]");
		break;
		case 4:
		__printf("[DBG]");
		break;
	}
	#endif
	
  	if (level == 1)
		{
			UART_DEBUGX("[ERROR]");
		}
  	else if (level == 2){
			UART_DEBUGX("[WARN]");
		}
		
 	// __debug_printf("%d@%s: ", line, module);
  	__debug_printf("->%s: ", module);
	
	va_list     vArgs;		    
	va_start(vArgs, fmt);
	__debug_vprintf((char const *)fmt, vArgs);
	va_end(vArgs);
	
  	// osSemaphoreRelease(dbgSem_id);
}

void dbg_error(const char* module, int line, int ret)
{
  // osSemaphoreWait(dbgSem_id , osWaitForever);
  __debug_printf("[RC] Module %s - Line %d : Error %d\n", module, line, ret);
  // osSemaphoreRelease(dbgSem_id);
}

void user_debug_print_exact(const char* fmt, ...)
{
  // osSemaphoreWait(dbgSem_id , osWaitForever);

	va_list     vArgs;		    
	va_start(vArgs, fmt);
	__debug_vprintf((char const *)fmt, vArgs);
	va_end(vArgs);
	
  // osSemaphoreRelease(dbgSem_id);
}

