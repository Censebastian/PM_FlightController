#include "MPU6500.h"

const uint16_t CALIBRATION_STEPS = 500;
const uint32_t SPI_INIT_FREQUENCY = 1000000; 
const uint32_t SPI_COMM_FREQUENCY = 2000000; 

volatile bool imu_data_ready;
volatile input_imu_data raw_imu;

imu_data bias;

void IRAM_ATTR on_mpu_interrupt() {
    imu_data_ready = true;
}

void write_register(uint8_t reg, uint8_t data) {
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
    digitalWrite(PIN_NCS, LOW);
    SPI.transfer(reg & 0x7F); 
    SPI.transfer(data);
    digitalWrite(PIN_NCS, HIGH);
    SPI.endTransaction();
}

void init_SPI() {
    pinMode(PIN_NCS, OUTPUT);
    digitalWrite(PIN_NCS, HIGH);
    pinMode(PIN_INT, INPUT);

    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_NCS);

    write_register(REG_PWR_MGMT_1, 0x80);
    delay(100);
    write_register(REG_PWR_MGMT_1, 0x01);
    write_register(REG_USER_CTRL, 0x10);

    write_register(REG_GYRO_CONFIG, 0x18); 
    write_register(REG_ACCEL_CONFIG, 0x10); 

    write_register(REG_ACCEL_CONFIG2, 0x08); 

    write_register(REG_INT_ENABLE, 0x01); 

    attachInterrupt(digitalPinToInterrupt(PIN_INT), on_mpu_interrupt, RISING);
}

void read_imu_data() {
    SPI.beginTransaction(SPISettings(SPI_COMM_FREQUENCY, MSBFIRST, SPI_MODE3));
    digitalWrite(PIN_NCS, LOW);
    
    SPI.transfer(REG_ACCEL_XOUT_H | 0x80);

    uint8_t axH = SPI.transfer(0x00); uint8_t axL = SPI.transfer(0x00);
    uint8_t ayH = SPI.transfer(0x00); uint8_t ayL = SPI.transfer(0x00);
    uint8_t azH = SPI.transfer(0x00); uint8_t azL = SPI.transfer(0x00);
    
    SPI.transfer(0x00); SPI.transfer(0x00);

    uint8_t gxH = SPI.transfer(0x00); uint8_t gxL = SPI.transfer(0x00);
    uint8_t gyH = SPI.transfer(0x00); uint8_t gyL = SPI.transfer(0x00);
    uint8_t gzH = SPI.transfer(0x00); uint8_t gzL = SPI.transfer(0x00);

    digitalWrite(PIN_NCS, HIGH);
    SPI.endTransaction();

    raw_imu.ax = (axH << 8) | axL;
    raw_imu.ay = (ayH << 8) | ayL;
    raw_imu.az = (azH << 8) | azL;
    raw_imu.gx = (gxH << 8) | gxL;
    raw_imu.gy = (gyH << 8) | gyL;
    raw_imu.gz = (gzH << 8) | gzL;
}

imu_data prepare_imu_data() {
    imu_data data;
    const float acc_scalar = 1 / 4096.0;
    const float gyro_scalar = 1 / 16.4;

    data.ax = (float)raw_imu.ax * acc_scalar - bias.ax;
    data.ay = (float)raw_imu.ay * acc_scalar - bias.ay;
    data.az = (float)raw_imu.az * acc_scalar - bias.az;
    data.gx = (float)raw_imu.gx * gyro_scalar - bias.gx;
    data.gy = (float)raw_imu.gy * gyro_scalar - bias.gy;
    data.gz = (float)raw_imu.gz * gyro_scalar - bias.gz;

    return data;
}

void calculate_sensor_bias() {
    imu_data sum = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    for (int i = 0; i < CALIBRATION_STEPS;) {
        if (imu_data_ready) {
            imu_data_ready = false;
            read_imu_data();
            imu_data preped_data = prepare_imu_data();
            sum.ax += preped_data.ax; 
            sum.ay += preped_data.ay;
            sum.az += preped_data.az;
            sum.gx += preped_data.gx;
            sum.gy += preped_data.gy;
            sum.gz += preped_data.gz;
            i++;
        }
    }
    bias.ax = sum.ax / CALIBRATION_STEPS; 
    bias.ay = sum.ay / CALIBRATION_STEPS;
    bias.az = sum.az / CALIBRATION_STEPS - 1.0; // we don't want to cancel out the gravity
    bias.gx = sum.gx / CALIBRATION_STEPS;
    bias.gy = sum.gy / CALIBRATION_STEPS;
    bias.gz = sum.gz / CALIBRATION_STEPS;
}

imu_data get_imu_data() {
    read_imu_data();
    return prepare_imu_data();
}

bool is_data_ready() {
    if (!imu_data_ready) return false;
    imu_data_ready = false;
    return true;
}

void debug_imu() {
    if (is_data_ready()) {
        imu_data debug_data = get_imu_data();
        Serial.print("raw ax ");
        Serial.print(raw_imu.ax);
        Serial.print("raw ay ");
        Serial.print(raw_imu.ay);
        Serial.print("raw az ");
        Serial.print(raw_imu.az);
        Serial.print("raw gx ");
        Serial.print(raw_imu.gx);
        Serial.print("raw gy ");
        Serial.print(raw_imu.gy);
        Serial.print("raw gz ");
        Serial.println(raw_imu.gz);

        Serial.print("debug ax ");
        Serial.print(debug_data.ax);
        Serial.print("debug ay ");
        Serial.print(debug_data.ay);
        Serial.print("debug az ");
        Serial.print(debug_data.az);
        Serial.print("debug gx ");
        Serial.print(debug_data.gx);
        Serial.print("debug gy ");
        Serial.print(debug_data.gy);
        Serial.print("debug gz ");
        Serial.println(debug_data.gz);
        Serial.println("");
        delay(100);
    }
}

void print_imu_data(imu_data data) {
    Serial.print("imu ax ");
    Serial.print(data.ax);
    Serial.print("imu ay ");
    Serial.print(data.ay);
    Serial.print("imu az ");
    Serial.print(data.az);
    Serial.print("imu gx ");
    Serial.print(data.gx);
    Serial.print("imu gy ");
    Serial.print(data.gy);
    Serial.print("imu gz ");
    Serial.println(data.gz);
    Serial.println("");
}