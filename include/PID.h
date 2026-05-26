class PID {
    public:
        PID(float max_output, float Kp, float Ki, float Kd, float time_constant);
        float compute_gain(float current_inclination, float target_inclination, float current_gyro);

    private:
        const float max_output;
        const float Kp, Ki, Kd;
        float integral_sum, prev_gyro;
        float time_constant;
};