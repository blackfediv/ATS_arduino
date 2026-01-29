#include "main.h"


void init_ports() {
    DDRB |= (1 << PB5) | (1 << PB1); // D9 and D13 as OUTPUT
    PORTB &= ~(1 << PB5 | 1 << PB1);

    PORTD &= ~(1 << PD2); // D2 as INPUT
    DDRD &= ~(1 << PD2);

    DDRD |= (1 << PD3); // D3 as OUTPUT
    PORTD &= ~(1 << PD3);
}


void init_timer_main() {
    TCCR0B = 0;
    TCCR0B |= (1 << CS01) | (1 << CS00); // Делитель 64, переполнение 1.024мc
    TIMSK0 |= (1 << TOIE0);              // Прерывание по переполнению
}


void beep(uint8_t on) {
    if (on == 1) {
        TCCR1A |= (1 << COM1A0);
        TCCR1A &= ~(1 << COM1A1);
    } else if (on == 0) {
        TCCR1A &= ~(1 << COM1A1);
        TCCR1A &= ~(1 << COM1A0);
    } else {
        TCCR1A = 1 << COM1A0;
        TCCR1C = 1 << FOC1A;
        TCCR1B = 1 << WGM12 | 1 << WGM13 | 0b001;
        ICR1 = ((float)F_CPU / (frequency * 2UL)) - 1;
    }
}


void i2c_callback(uint8_t state){
    if (state == TW_SR_STOP) { // Получили сообщение от главного модуля
        if (twi_buf[0] == '?') { // Если это запрос
            // Формируем ответ
            for (int j = 0; j < (sizeof(struct data_to_main)); j++) twi_buf[j] = *(dtm_p+j);
        } else if (twi_buf[0] == '!') { // Если это ответ
            for (int j = 0; j < (sizeof(struct data_to_main)); j++) dtm_p[j] = twi_buf[j+1];
        }
    }
    return;
}


ISR (TIMER0_OVF_vect) {
    t0_cntr++;
    if (is_time_ms(2)) {
        timer_main = 1;
        sys_time += 2;
    }
    if (is_time_ms(300)) timer_busy = !timer_busy;
    if (is_time_ms(4000) && !timer_ring) timer_ring = 1;
    if (is_time_ms(1000) && timer_ring) timer_ring = 0;
}


int main(void) {
    cli();
    init_ports();
    beep(5);
    beep(0);
    init_timer_main();
    twi_buf = twi_slave_init(MY_SYS_PORT, 255, TWI_SLAVE_MODE_RESET);
    twi_slave_connect_callback(i2c_callback);
    for (int x = 0; x < 16; x++) N_number[x] = '\0';
    sei();

    while (dtm.st > 0) {
        if (timer_main) {
            timer_main = 0;
            if (!last_st && ((PIND >> PD2) & 1)) {
                last_st = ((PIND >> PD2) & 1);
                handset = last_st;
                if (handset) last_0 = sys_time;
            } else if (last_st && !((PIND >> PD2) & 1)) {
                last_st = ((PIND >> PD2) & 1);
                handset = last_st;
                if (!handset) last_1 = sys_time;
            }

            if (dtm.st == 1) { // Режим ожидания
                PORTB &= ~(1 << PB5);  // Тушим лампочку состояния
                PORTD &= ~(1 << PD3);  // Выключаем реле звонка
                if (N_number[0] != '\0'){
                    for (int x = 0; x < 16; x++) N_number[x] = '\0';
                    beep_off();
                    num = 0;
                    i = 0;
                }
                if (handset) { // Если сняли трубку
                    dtm.st = 3;
                    timer_34 = sys_time;
                }
            } else if (dtm.st == 2) { // Входящий вызов
                if (!handset) {
                    if (timer_ring) {
                        PORTD |= (1 << PD3);
                    } else {
                        PORTD &= ~(1 << PD3);
                    }
                } else {
                    dtm.st = 7;
                }
            } else if (dtm.st == 3) { // Ответ станции при снятии трубки
                beep_on();
                PORTB |= (1 << PB5); // Зажигаем лампочку состояния
                if (!handset && (sys_time - timer_34 > 200)) { // Переход в режим набора номера с задержкой
                    dtm.st = 4;
                    beep_off();
                    last_0 = last_1;
                }
            } else if (dtm.st == 4) { // Режим набора номера
                if (!handset && (sys_time - last_1) > 1000) dtm.st = 1; // Трубка положена больше 1 секунд
                if (handset && !once) {
                    once = 1;
                    if ((last_0 - last_1) >= 40) { // Размыкание больше 40 мс
                        num++;
                    }
                }
                if (!handset && once) {
                    once = 0;
                    if (num > 0 && (last_1 - last_0) > 200) { // Замыкание линии больше 200 мс (межсерийный интервал)
                        if (num == 10) num = 0;
                        N_number[i] = num + '0';
                        num = 0;
                        i++;
                    }
                }
                if (N_number[0] > '0' && handset && once && (sys_time - last_0) > 2000) {
                    if (num == 10) num = 0;
                    N_number[i] = num + '0';
                    for (int l = 0; l < 16; l++) dtm_p[l+1] = N_number[l];
                    dtm.st = 5;
                    timer_ring = 1;
                    once = 0;
                }
            } else if (dtm.st == 6) { // Посылка вызова
                if (timer_ring) {
                    beep_on();
                } else {
                    beep_off();
                }
            } else if (dtm.st == 7) { // Соединение
                PORTD &= ~(1 << PD3);
                beep_off();
            } else if (dtm.st == 8) { // Занято
                if (timer_busy) {
                    beep_on();
                } else {
                    beep_off();
                }
            }

            if ((dtm.st > 2) && (dtm.st != 4) && !handset) dtm.st = 1;
        }
    }
}