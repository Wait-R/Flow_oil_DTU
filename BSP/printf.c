#include "printf.h"
#include "stdio.h"
#include "usart.h"

#if 1
#if (__ARMCC_VERSION >= 6010050)            /* 使用AC6编译器时 */
__asm(".global __use_no_semihosting\n\t");  /* 声明不使用半主机模式 */
__asm(".global __ARM_use_no_argv \n\t");    /* AC6下需声明main函数为无参数格式*/

#else
/* 使用AC5模式时要定义__FILE 和 不使用半主机模式 */
#pragma import(__use_no_semihosting)

struct __FILE
{
    int handle;
    /* Whatever you require here. If the only file you are using is */
    /* standard output using printf() for debugging, no file handling */
    /* is required. */
};

#endif

/* 不使用半主机要重定义_ttywrch\_sys_exit\_sys_command_string函数,以同时兼容AC5和AC6模式*/
int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

/*定义_sys_exit()避免半主机 */
void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}

/* FILE 在 stdio.h里定义 */
FILE __stdout;
#endif

/* 重定义fputc函数 */
int fputc(int ch, FILE *f) 
{		
	while ((USART1->SR & 0X40) == 0);               /* 等待上一个字符发送完成 */
	USART1->DR = (uint8_t)ch;                       /* 将要发送的字符 ch 写入到DR寄存器 */
	return ch;
}

void print_Array(uint8_t arr[],uint8_t size)
{
	for(int i = 0; i<size;i++)
	{
		printf("%c",arr[i]);
	}
	printf("\r\n");
}


