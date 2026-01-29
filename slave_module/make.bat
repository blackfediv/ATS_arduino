set sys_port=%1

avr-gcc -Wall -Os -mmcu=atmega328p -DMY_SYS_PORT=%sys_port% -c main.c -o main.o
avr-gcc -Wall -Os -mmcu=atmega328p -c libs/uart.c -o uart.o
avr-gcc -Wall -Os -mmcu=atmega328p -c libs/slave.c -o slave.o

avr-gcc -Wall -Os -mmcu=atmega328p main.o slave.o uart.o -o main.elf
avr-objcopy -j .text -j .data -O ihex main.elf main_%sys_port%.hex
del main.elf slave.o main.o uart.o