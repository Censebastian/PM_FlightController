#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include "MPU6500.h"
#include "PID.h"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Remote Control Interface</title>
    <style>
        * {
            box-sizing: border-box;
            user-select: none;
            -webkit-user-select: none;
        }
        body {
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: space-between;
            align-items: center;
            height: 100vh;
            background-color: #1a1a1a;
            color: #ffffff;
            font-family: sans-serif;
            touch-action: none; /* Critical for multitouch */
            overflow: hidden;
        }

        .label {
            margin-bottom: 15px;
            font-size: 14px;
            letter-spacing: 1px;
            text-transform: uppercase;
            color: #888;
            text-align: center;
        }

        /* Joystick Styles */
        #joystick-wrapper {
            margin-left: 10vw;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        #joystick-container {
            width: 160px;
            height: 160px;
            background: rgba(255, 255, 255, 0.1);
            border: 2px solid rgba(255, 255, 255, 0.2);
            border-radius: 50%;
            position: relative;
            display: flex;
            justify-content: center;
            align-items: center;
            touch-action: none;
        }
        #joystick-knob {
            width: 60px;
            height: 60px;
            background: #3498db;
            border-radius: 50%;
            position: absolute;
            cursor: grab;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
            pointer-events: none;
        }

        /* Throttle Styles */
        #throttle-wrapper {
            margin-right: 10vw;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        #throttle-container {
            width: 60px;
            height: 200px;
            background: rgba(255, 255, 255, 0.1);
            border: 2px solid rgba(255, 255, 255, 0.2);
            border-radius: 30px;
            position: relative;
            touch-action: none;
        }
        #throttle-knob {
            width: 56px;
            height: 60px;
            background: #e74c3c;
            border-radius: 30px;
            position: absolute;
            left: 0;
            transform: translateY(140px); 
            cursor: grab;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
            pointer-events: none;
        }
    </style>
</head>
<body>

    <div id="joystick-wrapper">
        <div class="label">Direction</div>
        <div id="joystick-container">
            <div id="joystick-knob"></div>
        </div>
    </div>

    <div id="throttle-wrapper">
        <div class="label">Throttle</div>
        <div id="throttle-container">
            <div id="throttle-knob"></div>
        </div>
    </div>

    <script>
        // --- 1. State Management (0 to 511 bounds) ---
        const controlState = {
            joystickX: 256, // Center is ~256
            joystickY: 256, // Center is ~256
            throttle: 0     // Bottom is 0
        };

        // --- 2. Multitouch Logic ---
        
        // JOYSTICK
        const joyContainer = document.getElementById('joystick-container');
        const joyKnob = document.getElementById('joystick-knob');
        const joyRadius = joyContainer.offsetWidth / 2;
        const joyMaxRadius = joyRadius - (joyKnob.offsetWidth / 2);
        
        let joyPointerId = null;

        function updateJoystick(clientX, clientY) {
            const rect = joyContainer.getBoundingClientRect();
            const centerX = rect.left + joyRadius;
            const centerY = rect.top + joyRadius;

            let deltaX = clientX - centerX;
            let deltaY = clientY - centerY;
            const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);

            // Cap at edge
            if (distance > joyMaxRadius) {
                const ratio = joyMaxRadius / distance;
                deltaX *= ratio;
                deltaY *= ratio;
            }

            joyKnob.style.transform = `translate(${deltaX}px, ${deltaY}px)`;
            
            // Normalize from -1.0 to 1.0
            const normX = deltaX / joyMaxRadius;
            const normY = -deltaY / joyMaxRadius; // Invert so UP is positive
            
            // Map to 0-511 range
            controlState.joystickX = Math.round(((normX + 1) / 2) * 511);
            controlState.joystickY = Math.round(((normY + 1) / 2) * 511);
        }

        joyContainer.addEventListener('pointerdown', (e) => {
            if (joyPointerId !== null) return;
            joyPointerId = e.pointerId;
            joyContainer.setPointerCapture(joyPointerId);
            joyKnob.style.transition = 'none';
            updateJoystick(e.clientX, e.clientY);
        });

        joyContainer.addEventListener('pointermove', (e) => {
            if (joyPointerId === e.pointerId) updateJoystick(e.clientX, e.clientY);
        });

        function releaseJoystick(e) {
            if (joyPointerId === e.pointerId) {
                joyPointerId = null;
                joyKnob.style.transition = 'transform 0.2s ease-out';
                joyKnob.style.transform = `translate(0px, 0px)`;
                // Snap back to 511 range center
                controlState.joystickX = 256; 
                controlState.joystickY = 256;
            }
        }
        joyContainer.addEventListener('pointerup', releaseJoystick);
        joyContainer.addEventListener('pointercancel', releaseJoystick);


        // THROTTLE
        const throttleContainer = document.getElementById('throttle-container');
        const throttleKnob = document.getElementById('throttle-knob');
        const throttleMaxTravel = throttleContainer.offsetHeight - throttleKnob.offsetHeight;
        
        let throttlePointerId = null;

        function updateThrottle(clientY) {
            const rect = throttleContainer.getBoundingClientRect();
            let y = clientY - rect.top - (throttleKnob.offsetHeight / 2);

            // Clamp between bounds
            y = Math.max(0, Math.min(y, throttleMaxTravel));
            throttleKnob.style.transform = `translateY(${y}px)`;
            
            // Percentage (0.0 to 1.0) where 0 is bottom and 1.0 is top
            const percentage = 1 - (y / throttleMaxTravel);
            
            // Map to 0-511 range
            controlState.throttle = Math.round(percentage * 511);
        }

        throttleContainer.addEventListener('pointerdown', (e) => {
            if (throttlePointerId !== null) return;
            throttlePointerId = e.pointerId;
            throttleContainer.setPointerCapture(throttlePointerId);
            updateThrottle(e.clientY);
        });

        throttleContainer.addEventListener('pointermove', (e) => {
            if (throttlePointerId === e.pointerId) updateThrottle(e.clientY);
        });

        function releaseThrottle(e) {
            if (throttlePointerId === e.pointerId) throttlePointerId = null;
        }
        throttleContainer.addEventListener('pointerup', releaseThrottle);
        throttleContainer.addEventListener('pointercancel', releaseThrottle);


        // --- 3. WebSocket Binary Transmission ---
        const WS_URL = 'ws://drone.local/ws'; 
        const SEND_INTERVAL_MS = 30; 
        let ws;

        function connectWebSocket() {
            ws = new WebSocket(WS_URL);
            ws.binaryType = 'arraybuffer';

            ws.onopen = () => console.log('WebSocket Connected');
            ws.onerror = (error) => console.error('WebSocket Error:', error);
            ws.onclose = () => {
                console.log('WebSocket Disconnected. Reconnecting...');
                setTimeout(connectWebSocket, 2000);
            };
        }

        connectWebSocket();

        // Broadcast loop
        setInterval(() => {
            if (ws && ws.readyState === WebSocket.OPEN) {
                // 3 values * 2 bytes (16-bit integer) = 6 bytes total
                const buffer = new ArrayBuffer(6);
                const view = new DataView(buffer);

                // Set values as 16-bit Unsigned Integers (0-65535 capacity covers 0-511 easily)
                // The 'true' argument enforces Little-Endian format. Change to 'false' if your hardware expects Big-Endian.
                view.setUint16(0, controlState.joystickX, true); // Byte 0 & 1
                view.setUint16(2, controlState.joystickY, true); // Byte 2 & 3
                view.setUint16(4, controlState.throttle, true);  // Byte 4 & 5

                const binaryPayload = new Uint8Array(buffer);
                ws.send(binaryPayload);
            }
        }, SEND_INTERVAL_MS);

    </script>
</body>
</html>
)rawliteral";

enum attitude_indexes
{
    PITCH,
    ROLL,
    THROTTLE
};

enum motors
{
    FL,
    FR,
    BL,
    BR,
};

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const float MAX_INCLINATION = 45.0f;
const float THROTTLE_INTERPOLATION_RATE = 0.1;
const float lpf_alpha = 0.5;
const float comp_alpha = 0.98;

const uint8_t PWM_FREQUENCY = 250;
const uint8_t PWM_RESOLUTION = 12;
const uint8_t pin_FL = 0, pin_FR = 1, pin_BL = 2, pin_BR = 3;

const uint16_t MAX_REMOTE_INPUT = 511;
const uint16_t NEUTRAL_INPUT = 255;
const uint16_t MAX_INPUT_THROTTLE = 700;
const uint16_t MAX_PID_THROTTLE = 250;
const uint16_t MAX_THROTTLE_ERROR = 1;
const uint16_t MAX_MOTOR_OUTPUT = 2048;
const uint16_t MIN_MOTOR_OUTPUT = 1024;

const uint32_t TARGET_TIMER_PERIOD = 1000000;
const uint32_t INPUT_TIMER_PERIOD_US = 35000;
const uint32_t CL_TIMER_PERIOD_US = 4000;
const uint32_t MICROSECONDS_IN_SECONDS = 1000000;

const float time_constant = (float)CL_TIMER_PERIOD_US / MICROSECONDS_IN_SECONDS;

float target_attitude[] = {0.0, 0.0};
float current_attitude[] = {0.0, 0.0};
float pid_pitch_val, pid_roll_val;

uint8_t imu_readings;

int16_t input[] = {255, 255, 0};
int16_t motor_output[4];
int16_t current_throttle = 0;

imu_data sum_data;
imu_data filtered_data;

PID pitch_pid = PID((float)MAX_PID_THROTTLE, 4.0, 0.2, 1.5, time_constant);
PID roll_pid = PID((float)MAX_PID_THROTTLE, 4.0, 0.2, 1.5, time_constant);
//4 0 1.5 no bias 

bool new_input;

volatile bool input_timer_flag = false;
volatile bool cl_timer_flag = false;

hw_timer_t *input_timer = NULL;
hw_timer_t *cl_timer = NULL;

void IRAM_ATTR on_input_timer()
{
    input_timer_flag = true;
}

void IRAM_ATTR on_cl_timer()
{
    cl_timer_flag = true;
}

void init_timers()
{
    input_timer = timerBegin(TARGET_TIMER_PERIOD);
    timerAttachInterrupt(input_timer, &on_input_timer);
    timerAlarm(input_timer, INPUT_TIMER_PERIOD_US, true, 0);

    cl_timer = timerBegin(TARGET_TIMER_PERIOD);
    timerAttachInterrupt(cl_timer, &on_cl_timer);
    timerAlarm(cl_timer, CL_TIMER_PERIOD_US, true, 0);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    (void)len;

    if (type == WS_EVT_CONNECT)
    {
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        // SET STATIONARY ATTITUDE
    }
    else if (type == WS_EVT_ERROR)
    {
        // SET STATIONARY ATTITUDE
    }
    else if (type == WS_EVT_PONG)
    {
        // SET STATIONARY ATTITUDE
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->final && info->index == 0 && info->len == len)
        { // complete/completed package
            if (len == sizeof(input))
            {
                memcpy(input, data, len);
                new_input = true;
            }
        }
    }
}

void init_motors()
{
    ledcAttach(pin_FL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttach(pin_FR, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttach(pin_BL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttach(pin_BR, PWM_FREQUENCY, PWM_RESOLUTION);

    ledcWrite(pin_FL, MIN_MOTOR_OUTPUT);
    ledcWrite(pin_FR, MIN_MOTOR_OUTPUT);
    ledcWrite(pin_BL, MIN_MOTOR_OUTPUT);
    ledcWrite(pin_BR, MIN_MOTOR_OUTPUT);

    delay(3000);
}

void setup()
{
    Serial.begin(115200);
 
    Serial.println("Starting Wi-Fi...");
    WiFi.softAP("drone");
 
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
 
    ws.onEvent(onEvent);
    server.addHandler(&ws);
 
    if (!MDNS.begin("drone"))
    {
        Serial.println("Error setting up MDNS responder!");
    }
    else
    {
        Serial.println("mDNS responder started at http://drone.local");
    }
 
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", index_html); });
 
    server.begin();
    Serial.println("HTTP server started");
 
    init_timers();
    init_SPI();
    init_motors();

    // delay(5000); // wait 10 seconds for gyro to heat up
    // calculate_sensor_bias();
}

void interpolate_throttle()
{
    if (abs(input[THROTTLE] - current_throttle) > MAX_THROTTLE_ERROR)
    {
        current_throttle += (input[THROTTLE] - current_throttle) * THROTTLE_INTERPOLATION_RATE;
    }
    else
    {
        current_throttle = input[THROTTLE];
    }
}

void loop()
{
    if (input_timer_flag)
    {
        target_attitude[PITCH] = 0.0;
        target_attitude[ROLL] = 0.0;
    }
    if (new_input)
    {
        new_input = false;
        timerRestart(input_timer);
        target_attitude[PITCH] = (float)(input[ROLL] - NEUTRAL_INPUT) / (MAX_REMOTE_INPUT - NEUTRAL_INPUT) * MAX_INCLINATION;
        target_attitude[ROLL] = (float)(input[PITCH] - NEUTRAL_INPUT) / (MAX_REMOTE_INPUT - NEUTRAL_INPUT) * MAX_INCLINATION;
    }
    input_timer_flag = false;

    if (is_data_ready())
    {
        imu_data scaled_data = get_imu_data();
        sum_data.ax += scaled_data.ax;
        sum_data.ay += scaled_data.ay;
        sum_data.az += scaled_data.az;
        sum_data.gx += scaled_data.gx;
        sum_data.gy += scaled_data.gy;
        sum_data.gz += scaled_data.gz;

        imu_readings++;
    }

    if (cl_timer_flag)
    {
        cl_timer_flag = false;
        interpolate_throttle();

        if (imu_readings != 0)
        {
            // lowpass
            filtered_data.ax = lpf_alpha * (sum_data.ax / imu_readings) + (1 - lpf_alpha) * filtered_data.ax;
            filtered_data.ay = lpf_alpha * (sum_data.ay / imu_readings) + (1 - lpf_alpha) * filtered_data.ay;
            filtered_data.az = lpf_alpha * (sum_data.az / imu_readings) + (1 - lpf_alpha) * filtered_data.az;
            filtered_data.gx = lpf_alpha * (sum_data.gx / imu_readings) + (1 - lpf_alpha) * filtered_data.gx;
            filtered_data.gy = lpf_alpha * (sum_data.gy / imu_readings) + (1 - lpf_alpha) * filtered_data.gy;
            filtered_data.gz = lpf_alpha * (sum_data.gz / imu_readings) + (1 - lpf_alpha) * filtered_data.gz;

            imu_readings = 0;
            sum_data = {0.0};
            // comp filter

            float gyro_pitch = current_attitude[PITCH] + filtered_data.gy * time_constant;
            float acc_pitch = atan2(filtered_data.ax, sqrt(filtered_data.az * filtered_data.az + filtered_data.ay * filtered_data.ay)) * (180.0 / PI);
            current_attitude[PITCH] = comp_alpha * gyro_pitch + (1 - comp_alpha) * acc_pitch;

            float gyro_roll = current_attitude[ROLL] + filtered_data.gx * time_constant;
            float acc_roll = atan2(filtered_data.ay, sqrt(filtered_data.az * filtered_data.az + filtered_data.ax * filtered_data.ax)) * (180.0 / PI);
            current_attitude[ROLL] = comp_alpha * gyro_roll + (1 - comp_alpha) * acc_roll;

            pid_pitch_val = pitch_pid.compute_gain(current_attitude[PITCH], target_attitude[PITCH], filtered_data.gy);
            pid_roll_val = roll_pid.compute_gain(current_attitude[ROLL], target_attitude[ROLL], filtered_data.gx);
        }

        // motor mixing
        motor_output[FL] = MIN_MOTOR_OUTPUT + current_throttle * (float)(MAX_MOTOR_OUTPUT - MIN_MOTOR_OUTPUT) / MAX_INPUT_THROTTLE - pid_pitch_val - pid_roll_val;
        motor_output[FL] = motor_output[FL] > MAX_MOTOR_OUTPUT ? MAX_MOTOR_OUTPUT : motor_output[FL];
        motor_output[FL] = motor_output[FL] < MIN_MOTOR_OUTPUT ? MIN_MOTOR_OUTPUT : motor_output[FL];

        motor_output[FR] = MIN_MOTOR_OUTPUT + current_throttle * (float)(MAX_MOTOR_OUTPUT - MIN_MOTOR_OUTPUT) / MAX_INPUT_THROTTLE - pid_pitch_val + pid_roll_val;
        motor_output[FR] = motor_output[FR] > MAX_MOTOR_OUTPUT ? MAX_MOTOR_OUTPUT : motor_output[FR];
        motor_output[FR] = motor_output[FR] < MIN_MOTOR_OUTPUT ? MIN_MOTOR_OUTPUT : motor_output[FR];

        motor_output[BL] = MIN_MOTOR_OUTPUT + current_throttle * (float)(MAX_MOTOR_OUTPUT - MIN_MOTOR_OUTPUT) / MAX_INPUT_THROTTLE + pid_pitch_val - pid_roll_val;
        motor_output[BL] = motor_output[BL] > MAX_MOTOR_OUTPUT ? MAX_MOTOR_OUTPUT : motor_output[BL];
        motor_output[BL] = motor_output[BL] < MIN_MOTOR_OUTPUT ? MIN_MOTOR_OUTPUT : motor_output[BL];

        motor_output[BR] = MIN_MOTOR_OUTPUT + current_throttle * (float)(MAX_MOTOR_OUTPUT - MIN_MOTOR_OUTPUT) / MAX_INPUT_THROTTLE + pid_pitch_val + pid_roll_val;
        motor_output[BR] = motor_output[BR] > MAX_MOTOR_OUTPUT ? MAX_MOTOR_OUTPUT : motor_output[BR];
        motor_output[BR] = motor_output[BR] < MIN_MOTOR_OUTPUT ? MIN_MOTOR_OUTPUT : motor_output[BR];

        ledcWrite(pin_FL, motor_output[FL]);
        ledcWrite(pin_FR, motor_output[FR]);
        ledcWrite(pin_BL, motor_output[BL]);
        ledcWrite(pin_BR, motor_output[BR]);
    }
}