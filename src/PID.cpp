#include "PID.h"

PID::PID(float max_output, float Kp, float Ki, float Kd, float time_constant)
    : max_output(max_output), Kp(Kp), Ki(Ki), Kd(Kd) {
    this->time_constant = time_constant;
}

float PID::compute_gain(float current_inclination, float target_inclination, float current_gyro) {
    float err = current_inclination - target_inclination;

    float proportional = Kp * err;

    integral_sum += Ki * err * time_constant;
    if (integral_sum > max_output / 2) {
        integral_sum = max_output / 2;
    } else if (integral_sum < -max_output / 2) {
        integral_sum = -max_output / 2;
    }

    float derivative = Kd * current_gyro;
    prev_gyro = current_gyro;

    float pid = proportional + integral_sum + derivative;

    if (pid > max_output) {
        return max_output;
    } else if (pid < -max_output) {
        return -max_output;
    }
    return pid;
}