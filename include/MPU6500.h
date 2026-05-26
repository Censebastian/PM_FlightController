#pragma once
#include <Arduino.h>
#include <SPI.h>

#define PIN_SCK  20  
#define PIN_MOSI 19  
#define PIN_MISO 14
#define PIN_NCS  18  
#define PIN_INT  15  

#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_CONFIG2 0x1D
#define REG_INT_ENABLE    0x38
#define REG_ACCEL_XOUT_H  0x3B
#define REG_PWR_MGMT_1    0x6B
#define REG_USER_CTRL     0x6A

struct input_imu_data {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
};

struct imu_data {
    float ax, ay, az;
    float gx, gy, gz;
};

void on_mpu_interrupt();
void write_register(uint8_t reg, uint8_t data);
void init_SPI();
void read_imu_data();
void calculate_sensor_bias();
void debug_imu();
void print_imu_data(imu_data data);
imu_data prepare_imu_data();
imu_data get_imu_data();
bool is_data_ready();
