#include "../include/peripheral-pca9685.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>


// Registers
#define MODE1_REGISTER		0x00
#define MODE2_REGISTER		0x01
#define SUBADR1_REGISTER	0x02
#define SUBADR2_REGISTER	0x03
#define SUBADR3_REGISTER	0x04
#define ALLCALLADR_REGISTER	0x05
#define ALL_LED_ON_L_REGISTER   0xFA

#define LED_REGISTERS(i)	(0x06 + (i * 4))
#define LED_ON_L(i)		(LED_REGISTERS(i))
#define LED_ON_H(i)		(LED_REGISTERS(i) + 1)
#define LED_OFF_L(i)		(LED_REGISTERS(i) + 2)
#define LED_OFF_H(i)		(LED_REGISTERS(i) + 3)

#define PRE_SCALE_REGISTER	0xFE

// Registers values and masks
#define MODE1_ALLCALL		0x01
#define MODE1_SUB3		0x02
#define MODE1_SUB2		0x04
#define MODE1_SUB1		0x08
#define MODE1_SLEEP		0x10
#define MODE1_AI		0x20
#define MODE1_EXTCLK		0x40
#define MODE1_RESTART		0x80

#define MODE2_OUTNE_1		0x01
#define MODE2_OUTNE_Z		0x02
#define MODE2_OUTDRV		0x04
#define MODE2_OCH		0x08
#define MODE2_INVRT		0x10

// Default register values
#define DEFAULT_MODE1           0b00010001
#define DEFAULT_MODE2           0b00000100
#define DEFAULT_SUBADDR1        0b11100010
#define DEFAULT_SUBADDR2        0b11100100
#define DEFAULT_SUBADDR3        0b11101000
#define DEFAULT_ALLCALLADR      0b11100000
#define DEFAULT_LEDXX_OFF_H     0b00010000

#define DEFAULT_ALL_LED_ON_L    0b00000000
#define DEFAULT_ALL_LED_ON_H    0b00100000
#define DEFAULT_ALL_LED_OFF_L   0b00000000
#define DEFAULT_ALL_LED_OFF_H   0b00100000
#define DEFAULT_PRE_SCALE       0b00011110

static int write_regs(i2c_interface_t* i2c, uint8_t addr, uint8_t* buffer, uint8_t len) {
	const i2c_write_t to_write = {.buff=buffer, .len=len};
	if (i2c_write(i2c, addr, &to_write) != 0) {
		return -1;
	}

	return 0;
}

static int set_led(i2c_interface_t* i2c, uint8_t addr,  uint8_t index,
		uint8_t on_l, uint8_t on_h, uint8_t off_l, uint8_t off_h) {
	/*
	// Prevent MODE1 and MODE2 uncontrolled overwrite
	uint8_t* ptr = buffer;
	ptr = buffer;
	*ptr++ = MODE1_REGISTER;
	*ptr++ = MODE1_AI;
	*ptr++ = MODE2_OUTDRV;
	if (!write_regs(i2c, addr, ptr - buffer)) {
		return 0;
	}
	*/

	uint8_t buffer[] = {LED_REGISTERS(index), on_l, on_h, off_l, off_h};

	return write_regs(i2c, addr, buffer, sizeof(buffer));
}

static int pca9685_reset(i2c_interface_t* i2c, uint8_t addr) {
	uint8_t* to_write_buff = calloc(1+70, sizeof(uint8_t));
	if (to_write_buff == NULL) {
		return -1;
	}

	// Except MODE1, MODE2, SUBADDRX, ALLCALLADR and LEDXX_OFF_H, make all registers 0
	to_write_buff[0] = MODE1_REGISTER;
	to_write_buff[1] = DEFAULT_MODE1 | MODE1_AI;
	to_write_buff[2] = DEFAULT_MODE2;
	to_write_buff[3] = DEFAULT_SUBADDR1;
	to_write_buff[4] = DEFAULT_SUBADDR2;
	to_write_buff[5] = DEFAULT_SUBADDR3;
	to_write_buff[6] = DEFAULT_ALLCALLADR;
	for (uint8_t output = 0; output < PCA9685_NUM_OUTPUTS; output++) {
	        to_write_buff[10+output*4] = DEFAULT_LEDXX_OFF_H;
	}
	i2c_write_t to_write = {.buff=to_write_buff, .len=1+70};
	int i2c_ret = i2c_write(i2c, addr, &to_write);
	if (i2c_ret != 0) {
		goto reset_end;
	}

	// Now all the registres from 0xFA to 0xFE
	to_write_buff[0] = ALL_LED_ON_L_REGISTER;
	to_write_buff[1] = DEFAULT_ALL_LED_ON_L;
	to_write_buff[2] = DEFAULT_ALL_LED_ON_H;
	to_write_buff[3] = DEFAULT_ALL_LED_OFF_L;
	to_write_buff[4] = DEFAULT_ALL_LED_OFF_H;
	to_write_buff[5] = DEFAULT_PRE_SCALE;
	to_write.len = 1+5;
	i2c_ret = i2c_write(i2c, addr, &to_write);
	if (i2c_ret != 0) {
		goto reset_end;
	}

	// Finally, return MODE1 to default value
	to_write_buff[0] = MODE1_REGISTER;
	to_write_buff[1] = DEFAULT_MODE1;
	to_write.len = 1+1;
	i2c_ret = i2c_write(i2c, addr, &to_write);
	if (i2c_ret != 0) {
		goto reset_end;
	}

 reset_end:
	free(to_write_buff);
	return i2c_ret;
}

int pca9685_init(i2c_interface_t* i2c, uint8_t addr) {
	uint8_t buffer[1 + 1];

	FAST_CREATE_I2C_WRITE(read_order_mode1_reg, MODE1_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_mode2_reg, MODE2_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_prescale_reg, PRE_SCALE_REGISTER);

	uint8_t mode1_reg = 0, mode2_reg, prescale_reg;
	i2c_read_t read_mode1_reg = {.buff=&mode1_reg, .len=1};
	i2c_read_t read_mode2_reg = {.buff=&mode2_reg, .len=1};
	i2c_read_t read_prescale_reg = {.buff=&prescale_reg, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_mode1_reg, &read_mode1_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_mode2_reg, &read_mode2_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_prescale_reg, &read_prescale_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	if ( (mode1_reg == MODE1_AI) && (mode2_reg == MODE2_OUTDRV) && (prescale_reg == 11) ) {
		errno = EALREADY;
		return 1;
	}


        i2c_ret = pca9685_reset(i2c, addr);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	buffer[0] = MODE1_REGISTER;
	buffer[1] = MODE1_SLEEP | MODE1_AI;
	i2c_ret = write_regs(i2c, addr, buffer, 2);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	buffer[0] = MODE2_REGISTER;
	buffer[1] = MODE2_OUTDRV;
	i2c_ret = write_regs(i2c, addr, buffer, 2);
	if (i2c_ret != 0) {
		return i2c_ret;
	}


	buffer[0] = PRE_SCALE_REGISTER;
	buffer[1] = 11; // PWM frequency 500Hz
	i2c_ret = write_regs(i2c, addr, buffer, 2);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	buffer[0] = MODE1_REGISTER;
	buffer[1] = MODE1_AI;
	i2c_ret = write_regs(i2c, addr, buffer, 2);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	errno = 0;
	return 0;
}

int pca9685_deinit(i2c_interface_t* i2c, uint8_t addr) {
	FAST_CREATE_I2C_WRITE(read_order_mode1_reg, MODE1_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_mode2_reg, MODE2_REGISTER);
	FAST_CREATE_I2C_WRITE(read_order_prescale_reg, PRE_SCALE_REGISTER);

	uint8_t mode1_reg, mode2_reg, prescale_reg;
	i2c_read_t read_mode1_reg = {.buff=&mode1_reg, .len=1};
	i2c_read_t read_mode2_reg = {.buff=&mode2_reg, .len=1};
	i2c_read_t read_prescale_reg = {.buff=&prescale_reg, .len=1};

	int i2c_ret = i2c_write_then_read(i2c, addr, &read_order_mode1_reg, &read_mode1_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_mode2_reg, &read_mode2_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}
	i2c_ret = i2c_write_then_read(i2c, addr, &read_order_prescale_reg, &read_prescale_reg);
	if (i2c_ret != 0) {
		return i2c_ret;
	}

	if ( (mode1_reg == DEFAULT_MODE1) && (mode2_reg == DEFAULT_MODE2) && (prescale_reg == DEFAULT_PRE_SCALE) ) {
		errno = EALREADY;
		return 1;
	}

        return pca9685_reset(i2c, addr);
}

int pca9685_set_out_on(i2c_interface_t* i2c, uint8_t addr, uint8_t index) {
	if (index >= PCA9685_NUM_OUTPUTS) {
		errno = EINVAL;
		return -1;
	}

	return set_led(i2c, addr, index, 0x00, 0x10, 0x00, 0x00);
}

int pca9685_set_out_off(i2c_interface_t* i2c, uint8_t addr, uint8_t index) {
	if (index >= PCA9685_NUM_OUTPUTS) {
		errno = EINVAL;
		return -1;
	}

	return set_led(i2c, addr, index, 0x00, 0x00, 0x00, 0x10);
}

int pca9685_set_out_pwm(i2c_interface_t* i2c, uint8_t addr, uint8_t index, uint16_t value) {
	return set_led(i2c, addr, index, 0x00, 0x00, value & 0xff, (value >> 8) & 0x0f);
}

int pca9685_set_all_digital(i2c_interface_t* i2c, uint8_t addr, uint16_t values) {
	uint8_t buffer[1 + 4*PCA9685_NUM_OUTPUTS];
	uint8_t* ptr = buffer;

	*ptr++ = LED_REGISTERS(0);

	for (int i = 0; i < PCA9685_NUM_OUTPUTS; ++i) {
		*ptr++ = 0x00;
		*ptr++ = (values & (1 << i)) ? 0x10 : 0x00;
		*ptr++ = 0x00;
		*ptr++ = (values & (1 << i)) ? 0x00 : 0x10;
	}

	return write_regs(i2c, addr, buffer, sizeof(buffer));
}

int pca9685_set_all_analog(i2c_interface_t* i2c, uint8_t addr, const uint16_t* values) {
	uint8_t buffer[1 + 4*PCA9685_NUM_OUTPUTS];
	uint8_t* ptr = buffer;

	*ptr++ = LED_REGISTERS(0);

	for (int i = 0; i < PCA9685_NUM_OUTPUTS; ++i) {
		*ptr++ = 0x00;
		*ptr++ = 0x00;
		*ptr++ = values[i] & 0xff;
		*ptr++ = (values[i] >> 8) & 0x0f;
	}

	return write_regs(i2c, addr, buffer, sizeof(buffer));
}