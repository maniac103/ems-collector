/*
 * Buderus EMS frame grabber
 *
 * receives data from the EMS via UART, validates them,
 * adds proper framing and sends those frames out via UART
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "common.h"

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#define RECV_BUF_SIZE	    64
#define TRANSMIT_BUF_SIZE   250

#define BAUD     9600
#define UBRR_VAL (F_CPU / 16 / BAUD - 1)

#define PACKET_LED_TIME	    100 /* ms */
#define FAULT_LED_TIME	    200 /* ms */

#define FRAME_TYPE_DATA  0
#define FRAME_TYPE_STATS 1

static uint8_t recv_buffer[RECV_BUF_SIZE];
static uint8_t recv_pos = 0;
static uint8_t recv_valid = 0;

static volatile uint8_t tx_buffer[TRANSMIT_BUF_SIZE];
static volatile uint8_t tx_write_pos = 0;
static volatile uint8_t tx_read_pos = 0;

/* each tick is 10ms, we want to transfer once per minute */
#define TX_STATS_COUNTDOWN_START 6000

static volatile uint8_t need_tx_stats = 0;
static uint16_t tx_stats_countdown = TX_STATS_COUNTDOWN_START;
static rx_stats_t rx_stats;

static uint8_t calc_checksum(const uint8_t *buffer, uint8_t size)
{
    uint8_t crc = 0, d;

    for (uint8_t i = 0; i < size; i++) {
	d = 0;
	if (crc & 0x80) {
	    crc ^= 0xc;
	    d = 1;
	}
	crc <<= 1;
	crc &= 0xfe;
	crc |= d;
	crc = crc ^ buffer[i];
    }

    return crc;
}

static void copy_to_tx_buffer(uint8_t data)
{
    tx_buffer[tx_write_pos++] = data;
    if (tx_write_pos >= TRANSMIT_BUF_SIZE) {
	tx_write_pos = 0;
    }
}

static void start_tx_frame(uint8_t type)
{
    /* start-of-frame */
    copy_to_tx_buffer(0xaa);
    copy_to_tx_buffer(0x55);
    /* type */
    copy_to_tx_buffer(type);
}

static void add_tx_data(uint8_t *data, uint8_t len)
{
    uint8_t csum = 0;

    copy_to_tx_buffer(len);
    for (uint8_t i = 0; i < len; i++) {
	copy_to_tx_buffer(data[i]);
	csum ^= data[i];
    }
    /* checksum -> running XOR */
    copy_to_tx_buffer(csum);
}

static uint8_t enough_tx_space(uint8_t needed)
{
    uint8_t space;

    needed += 5; /* frame start, type, len, csum */
    if (tx_write_pos >= tx_read_pos) {
	space = TRANSMIT_BUF_SIZE - (tx_write_pos - tx_read_pos);
    } else {
	space = tx_read_pos - tx_write_pos;
    }

    return space >= needed;
}

static void enable_led(uint8_t fault)
{
    uint16_t time;

    if (fault) {
	/* enable fault LED, disable packet LED */
	PORTB = _BV(PB1);
	time = FAULT_LED_TIME;
    } else {
	/* enable packet LED, disable fault LED */
	PORTB = _BV(PB2);
	time = PACKET_LED_TIME;
    }

    TIMSK |= _BV(OCIE1A);
    TCNT1 = 0;
    TCCR1B = _BV(CS12) | _BV(CS10); /* timer clk = 8MHz / 1024 -> 128µs */
    OCR1A = time << 3; /* 8 * 128µs -> ~1ms */
}

ISR(SIG_OUTPUT_COMPARE1A)
{
    /* disable LEDs */
    PORTB = 0;
    /* disable timer */
    TCCR1B = 0;
}

static void init_10ms_tick()
{
    TIMSK |= _BV(OCIE2);
    /* 80 * 128µs ~= 10ms */
    OCR2 = 80;
    /* CTC, timer clk = 8 MHz / 1024 -> 128µs */
    TCCR2 = _BV(WGM21) | _BV(CS22) | _BV(CS21) | _BV(CS20);
}

ISR(SIG_OUTPUT_COMPARE2)
{
    if (!need_tx_stats) {
	tx_stats_countdown--;
	if (!tx_stats_countdown) {
	    tx_stats_countdown = TX_STATS_COUNTDOWN_START;
	    need_tx_stats = 1;
	}
    }
}

ISR(SIG_UART_RECV)
{
    uint8_t status, data;

    while (1) {
	status = UCSRA;

	if (!(status & _BV(RXC))) {
	    break;
	}

	data = UDR;

	if (recv_valid) {
	    if (status & _BV(FE)) {
		/* frame error -> end of frame byte */

		rx_stats.total_bytes += recv_pos;

		/* ignore 1-byte-long frames */
		if (recv_pos > 1) {
		    /* strip CRC */
		    recv_pos--;

		    uint8_t crc = calc_checksum(recv_buffer, recv_pos);
		    if (crc == recv_buffer[recv_pos]) {
			/* checksum valid, check whether there's enough
			 * room in the TX buffer */

			/* need frame start + len + payload */
			if (enough_tx_space(recv_pos)) {
			    rx_stats.good_packets++;
			    rx_stats.good_bytes += recv_pos + 1;
			    start_tx_frame(FRAME_TYPE_DATA);
			    add_tx_data(recv_buffer, recv_pos);
			    enable_led(0);
			} else {
			    rx_stats.dropped_packets++;
			    rx_stats.dropped_bytes += recv_pos + 1;
			}
		    } else {
			enable_led(1);
			rx_stats.bad_packets++;
		    }
		} else {
		    rx_stats.onebyte_packets++;
		}
	    } else {
		if (recv_pos < RECV_BUF_SIZE) {
		    recv_buffer[recv_pos++] = data;
		}
	    }
	}

	if (status & _BV(FE)) {
	    /* after frame end, the upcoming data is valid */
	    recv_pos = 0;
	    recv_valid = 1;
	}
    }

    if (need_tx_stats && enough_tx_space(sizeof(rx_stats_t))) {
	start_tx_frame(FRAME_TYPE_STATS);
	/* we can do that, because
	 * a) we're running on an 8-bit machine and thus the struct is
	 *    implicitly packed
	 * b) we want to transfer data as little endian, which
	 *    incidently matches AVR endianess
	 */
	add_tx_data((uint8_t *) &rx_stats, sizeof(rx_stats));
	rx_stats.id++;
	need_tx_stats = 0;
    }
}

static void uart_init(void)
{
    UBRRH = (UBRR_VAL >> 8) & 0xff;
    UBRRL = UBRR_VAL & 0xff;

    UCSRA = 0;
    UCSRB = _BV(RXCIE) | _BV(RXEN) | _BV(TXEN);
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
}

static void uart_write(uint8_t ch)
{
    while (!(UCSRA & _BV(UDRE)));
    UDR = ch;
}

int main(void)
{
    uint8_t has_data;

    /* enable watchdog */
    WDTCR = _BV(WDCE) | _BV(WDE) | _BV(WDP2) | _BV(WDP1) | _BV(WDP0);

    /* enable LED outputs */
    PORTB = 0;
    DDRB = _BV(DDB1) | _BV(DDB2);

    uart_init();
    init_10ms_tick();
    sei();

    while (1) {
	cli();
	has_data = tx_write_pos != tx_read_pos;
	sei();

	if (has_data) {
	    uart_write(tx_buffer[tx_read_pos]);

	    cli();
	    tx_read_pos++;
	    if (tx_read_pos >= TRANSMIT_BUF_SIZE) {
		tx_read_pos = 0;
	    }
	    sei();
	}

	wdt_reset();
    }

    return 0;
}
