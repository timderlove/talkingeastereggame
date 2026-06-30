#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <driver/i2s.h>
#include <math.h>
#include "bunny_audio.h" 

/* * ALVIN BUNNY: HIDE & SEEK EDITION
 * Features: Button state control, Mario Coin Synth, Fart Timers, Volume Control
 */

// --- 1. PIN DEFINITIONS ---
#define I2S_BCLK_PIN 3  
#define I2S_LRCK_PIN 4  
#define I2S_DATA_PIN 5  
#define I2S_NUM I2S_NUM_0 

#define TRIG_PIN 2      
#define ECHO_PIN 6      
#define BUTTON_PIN 10    // The new push button pin

#define ONBOARD_LED 8   

// Distance Thresholds (Hide & Seek Game)
#define WARM_DISTANCE_CM 120.0  // "You're never gonna find me booger"
#define HOT_DISTANCE_CM 40.0    // "Hey stinky getting hot"

bool is_audio_playing = false; 
bool game_active = false;       // Determines if the egg is playing
bool is_picked_up = false;      // Toggled by the button

// --- 2. NETWORKING CONFIG ---
uint8_t BROADCAST_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const char* EGG_ID = "EGG_ALPHA"; 

typedef struct struct_message {
    char sender[10];
    char event[20];
} struct_message;

struct_message outgoingMessage;
struct_message incomingMessage;
esp_now_peer_info_t peerInfo;

// --- 3. I2S AUDIO ENGINE ---
void initI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 24000, 
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_DATA_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_set_clk(I2S_NUM, 24000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

// Play raw voice data with a Software Volume Limiter
void playAudioArray(const unsigned char* audio_data, size_t audio_len) {
    if (audio_len <= 44) return; // Prevent crashes on empty files
    
    is_audio_playing = true; 
    digitalWrite(ONBOARD_LED, LOW); 

    size_t offset = 44; // Skip WAV header
    size_t bytes_left = audio_len - offset;
    const unsigned char* current_ptr = audio_data + offset;

    // A small buffer to hold our volume-adjusted audio
    const int BUFFER_SAMPLES = 256; 
    int16_t out_buffer[BUFFER_SAMPLES];

    while (bytes_left > 0) {
        size_t samples_to_process = (bytes_left / 2 > BUFFER_SAMPLES) ? BUFFER_SAMPLES : (bytes_left / 2);

        for (size_t i = 0; i < samples_to_process; i++) {
            // Reconstruct the 16-bit audio sample (Little Endian)
            int16_t sample = (int16_t)((current_ptr[i * 2 + 1] << 8) | current_ptr[i * 2]);
            
            // --- VOLUME CONTROL IS HERE ---
            // Dividing by 4 sets the volume to 25% (Safe for small LiPo batteries)
            // If it still crashes, change this to: out_buffer[i] = sample / 8; (12.5% volume)
            out_buffer[i] = sample / 4; 
        }

        size_t bytes_to_write = samples_to_process * 2;
        size_t bytes_written = 0;
        i2s_write(I2S_NUM, out_buffer, bytes_to_write, &bytes_written, portMAX_DELAY);
        
        current_ptr += bytes_to_write;
        bytes_left -= bytes_to_write;
    }
    
    i2s_zero_dma_buffer(I2S_NUM); 
    digitalWrite(ONBOARD_LED, HIGH); 
    
    delay(100); 
    is_audio_playing = false; 
}

// Generate pure mathematical tones for sound effects
void playTone(float freq, int duration_ms) {
    is_audio_playing = true;
    size_t bytes_written;
    int samples = (24000 * duration_ms) / 1000;
    int16_t sample[2];
    float phase = 0;
    float phase_increment = (2.0 * PI * freq) / 24000.0;
    
    for (int i=0; i<samples; i++) {
        // Simple volume envelope to prevent clicking
        float envelope = 1.0;
        if (i < 100) envelope = i / 100.0; 
        if (i > samples - 100) envelope = (samples - i) / 100.0; 
        
        // Lowered synth volume to match the 25% voices
        int16_t val = (int16_t)(sin(phase) * 500 * envelope); 
        sample[0] = val; 
        sample[1] = val; 
        i2s_write(I2S_NUM, &sample, sizeof(sample), &bytes_written, portMAX_DELAY);
        phase += phase_increment;
        if (phase >= 2.0 * PI) phase -= 2.0 * PI;
    }
    i2s_zero_dma_buffer(I2S_NUM);
    is_audio_playing = false;
}

// THE MARIO COIN SOUND (Note B5 then Note E6)
void playCoinSound() {
    playTone(987.77, 100);  
    playTone(1318.51, 400); 
}

// GAME START TONE (Happy chord progression)
void playStartTone() {
    playTone(523.25, 150);  // C5
    playTone(659.25, 150);  // E5
    playTone(783.99, 150);  // G5
    playTone(1046.50, 400); // C6
}

// --- 4. SENSING ENGINE ---
float getDistance() {
    if (is_audio_playing) return 999.0;

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 35000); 
    if (duration == 0 || duration > 30000) return 999.0;
    return duration * 0.0343 / 2;
}

// --- 5. ESP-NOW V3 ---
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {}
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    if (is_audio_playing || !game_active) return;
    playAudioArray(audio_converse, audio_converse_len);
}

// --- 6. SETUP & LOOP ---
void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // Wire button to GPIO 10 and GND
    pinMode(ONBOARD_LED, OUTPUT);
    digitalWrite(ONBOARD_LED, HIGH);
    
    initI2S();
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_send_cb(OnDataSent);
        esp_now_register_recv_cb(OnDataRecv);
        memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
        peerInfo.channel = 0;  
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
    
    Serial.println("Alvin Bunny Online. Press button to start game!");
}

void loop() {
    static bool last_hot_state = false;
    static bool last_warm_state = false;
    static bool last_btn_state = HIGH;
    static float last_distance = 0;
    static unsigned long last_dist_change = millis();
    
    // --- 1. BUTTON LOGIC ---
    bool btn_state = digitalRead(BUTTON_PIN);
    if (btn_state == LOW && last_btn_state == HIGH) {
        delay(50); // Debounce
        
        if (!game_active) {
            // START THE GAME
            game_active = true;
            is_picked_up = false;
            playStartTone();
            last_dist_change = millis(); // Reset fart timer
        } else {
            // TOGGLE HIDE/FOUND
            if (!is_picked_up) {
                is_picked_up = true;
                playCoinSound(); // Mario Coin!
                playAudioArray(audio_picked_up, audio_picked_up_len);
                
                // Tell other eggs we got found
                strcpy(outgoingMessage.event, "picked_up");
                esp_now_send(BROADCAST_MAC, (uint8_t *) &outgoingMessage, sizeof(outgoingMessage));
            } else {
                is_picked_up = false;
                playAudioArray(audio_put_down, audio_put_down_len);
                last_dist_change = millis(); // Reset fart timer
            }
        }
    }
    last_btn_state = btn_state;

    // --- 2. SENSOR LOGIC (Only runs when hiding) ---
    if (game_active && !is_picked_up) {
        float dist = getDistance(); 
        
        if (dist != 999.0) {
            // Track if people are moving for the Fart Timer
            if (abs(dist - last_distance) > 5.0) {
                last_distance = dist;
                last_dist_change = millis();
            }

            // Hide & Seek Insults
            if (dist < HOT_DISTANCE_CM && !last_hot_state) {
                playAudioArray(audio_hot, audio_hot_len);
                last_hot_state = true;
                last_warm_state = true;
                last_dist_change = millis(); // Reset timer after talking
            } 
            else if (dist >= HOT_DISTANCE_CM && dist < WARM_DISTANCE_CM && !last_warm_state) {
                playAudioArray(audio_warm, audio_warm_len);
                last_warm_state = true;
                last_hot_state = false;
                last_dist_change = millis();
            } 
            else if (dist >= WARM_DISTANCE_CM) {
                last_hot_state = false;
                last_warm_state = false;
            }

            // FART NOISE: 10 Seconds of silence and no movement
            if (millis() - last_dist_change > 10000) {
                playAudioArray(audio_fart, audio_fart_len);
                last_dist_change = millis(); // Reset the timer so it farts again in 10s
            }
        }
    }

    delay(50); 
}
