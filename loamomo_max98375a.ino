#include "BluetoothSerial.h"
#include <LittleFS.h> 
#include <map>
#include <vector>

// Thư viện Audio I2S
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorWAV.h" 
#include "AudioOutputI2S.h"

// --- CẤU HÌNH KIỂM THỬ (TEST CONFIGURATION) ---
// Đặt là 1 (TRUE) để chạy chế độ kiểm thử đọc tất cả file WAV từ 1 đến 21 trong Flash.
// Đặt là 0 (FALSE) để chạy chế độ Bluetooth bình thường.
#define ENABLE_FILE_TEST 0 
// Số lượng file tối đa để kiểm thử (Mã file cao nhất là 21)
#define MAX_TEST_TRACK 21 


// --- 1. KHAI BÁO I2S (MAX98357A) ---
// Chân kết nối I2S (Dựa trên kết nối phần cứng tiêu chuẩn)
#define I2S_BCLK 26  // Bit Clock
#define I2S_LRC 27   // Word Clock (Left/Right Clock)
#define I2S_DOUT 25  // Data Out

// *** QUẢN LÝ BỘ NHỚ ỔN ĐỊNH ***
// Khai báo đối tượng Audio là đối tượng toàn cục (Sử dụng constructor mặc định)
AudioOutputI2S audioOut; 
AudioFileSourceLittleFS audioFile;
AudioGeneratorWAV audioWAV;

// --- 2. TRẠNG THÁI PHÁT LẠI (PLAYLIST QUEUE) ---
std::vector<int> current_play_list; // Danh sách mã file WAV (ví dụ: 21, 10, 7)
int current_track_index = 0;       // Vị trí track đang phát hiện tại

// --- HẰNG SỐ VÀ WORD MAP ---
// Danh sách các từ (dạng C++ char array)
const char* UNITS[] = {"", "nghin", "trieu", "ty", "nghin ty"};
const char* DIGITS[] = {"khong", "mot", "hai", "ba", "bon", "nam", "sau", "bay", "tam", "chin"};

// BẢN ĐỒ TỪ-MÃ SỐ FILE ĐÃ ĐƯỢC CẬP NHẬT THEO THỨ TỰ THẬT TẾ CỦA BẠN:
// 1-Một, 2-Hai, ..., 10-Không, 11-Mười, 16-Đồng, 21-Bạn đã nhận được.
std::map<String, int> word_map = {
    // SỐ ĐẾM (0-9)
    {"khong", 10}, // File 10: Không
    {"mot", 1},    // File 1: Một
    {"hai", 2},    // File 2: Hai
    {"ba", 3},     // File 3: Ba
    {"bon", 4},    // File 4: Bốn
    {"nam", 5},    // File 5: Năm
    {"sau", 6},    // File 6: Sáu
    {"bay", 7},    // File 7: Bảy
    {"tam", 8},    // File 8: Tám
    {"chin", 9},   // File 9: Chín
    
    // ĐƠN VỊ CƠ BẢN
    {"muoif", 11}, // File 11: Mười (Dùng cho 10-19)
    {"tram", 12},  // File 12: Trăm
    {"nghin", 13}, // File 13: Nghìn
    {"trieu", 14}, // File 14: Triệu
    {"ty", 15},    // File 15: Tỷ
    {"dong", 16},  // File 16: Đồng
    
    // SỐ ĐẶC BIỆT VÀ TỪ NỐI
    {"le", 17},    // File 17: Lẻ (linh)
    {"muoi", 18},  // File 18: Mươi (Dùng cho 20, 30,...)
    {"mots", 19},  // File 19: Mốt (Dùng cho 21, 31,...)
    {"lam", 20},   // File 20: Lăm (Dùng cho 25, 35,...)
    
    // TỪ DẪN
    {"Bandanhanduoc", 21} // File 21: Bạn đã nhận được
};

// Hàm khai báo
std::vector<int> number_to_file_codes(long num);
std::vector<int> read_group_of_three(int part);

// --- 3. KHAI BÁO BLUETOOTH SERIAL ---
BluetoothSerial SerialBT;
String receivedData = "";

// --- 4. HÀM ĐIỀU CHỈNH ÂM LƯỢNG I2S ---
/**
 * @brief Điều chỉnh Gain (khuếch đại) của I2S.
 * @param gainValue: Giá trị từ 0.0 (Im lặng) đến 1.0 (Tối đa).
 */
void setAudioGain(float gainValue) {
    if (gainValue < 0.0) gainValue = 0.0;
    if (gainValue > 1.0) gainValue = 1.0;
    
    audioOut.SetGain(gainValue);
    
    Serial.print("I2S Volume set to Gain: ");
    Serial.println(gainValue);
}

// --- 5. HÀM PHÁT ÂM THANH NON-BLOCKING ---

/**
 * @brief Bắt đầu phát một track WAV duy nhất (Non-blocking).
 * @param tracknum: Mã số file WAV (ví dụ: 21 sẽ tìm file /021_G711.org_.wav)
 */
void startTrackPlayback(int tracknum) {
    // 1. Dừng track cũ (nếu đang phát)
    if (audioWAV.isRunning()) {
        audioWAV.stop();
    }

    // TẠO CHUỖI TÊN FILE THEO CẤU TRÚC: /XXX_G711.org_.wav
    char fileNameBuffer[32]; // Buffer để chứa tên file
    
    // %03d đảm bảo tracknum luôn có 3 chữ số, đệm bằng số 0
    snprintf(fileNameBuffer, sizeof(fileNameBuffer), 
             "/%03d_G711.org_.wav", tracknum); 
    
    String filePath = fileNameBuffer; 
    
    
    // 2. Mở file (Sử dụng đối tượng toàn cục)
    if (audioFile.isOpen()) {
        audioFile.close(); // Đảm bảo file cũ đã đóng
    }
    audioFile.open(filePath.c_str());
    
    if (!audioFile.isOpen()) {
        Serial.print("ERROR: File not found: ");
        Serial.println(filePath);
        // Chuyển sang track tiếp theo nếu file bị lỗi
        if (current_track_index < current_play_list.size()) {
            current_track_index++;
        }
        return;
    }
    
    Serial.print("Starting playback: ");
    Serial.println(filePath);
    
    // 3. Bắt đầu giải mã WAV (Lúc này thư viện sẽ tự động đặt Sample Rate 8KHz)
    audioWAV.begin(&audioFile, &audioOut);
}

// --- KHAI BÁO TRƯỚC HÀM KIỂM THỬ ---
void startFlashFileTest();

/**
 * @brief Callback được gọi NGAY SAU KHI audioWAV.loop() phát hiện track đã kết thúc.
 */
void audio_finished_callback() {
    Serial.println("Track finished. Checking next...");
    current_track_index++;

    if (current_track_index < current_play_list.size()) {
        // Có track tiếp theo, bắt đầu phát
        int next_code = current_play_list[current_track_index];
        startTrackPlayback(next_code);
    } else {
        // Đã phát xong toàn bộ list
        Serial.println("Finished playlist. Ready for next BT command.");
        current_play_list.clear(); // Xóa danh sách
        current_track_index = 0;

        #if ENABLE_FILE_TEST == 1
        // Nếu đang ở chế độ Test, lặp lại playlist sau khi kết thúc
        Serial.println("--- TEST MODE: Restarting Playlist ---");
        startFlashFileTest(); // Gọi lại hàm test để lặp lại
        #endif
    }
}

// --- 6. HÀM KIỂM THỬ ĐỌC FILE TỪ FLASH ---
void startFlashFileTest() {
    Serial.println("\n--- STARTING FILE TEST (WAV 1 to 21) ---");
    // 1. Dừng mọi thứ đang chạy
    if (audioWAV.isRunning()) {
        audioWAV.stop();
    }
    current_play_list.clear();

    // 2. Tạo playlist từ 1 đến MAX_TEST_TRACK
    for (int i = 1; i <= MAX_TEST_TRACK; i++) {
        current_play_list.push_back(i);
    }

    // 3. Bắt đầu phát track đầu tiên
    if (current_play_list.size() > 0) {
        current_track_index = 0;
        startTrackPlayback(current_play_list[current_track_index]);
    } else {
        Serial.println("ERROR: Test playlist is empty.");
    }
}


// --- SETUP ---
void setup() {
    Serial.begin(115200);
    // DÒNG DEBUG MỚI: Báo hiệu code đã bắt đầu chạy Serial.begin()
    Serial.println("STARTING..."); 
    
    // ĐỘ TRỄ ĐỂ ĐẢM BẢO BAUD RATE ỔN ĐỊNH VÀ BẠN CÓ THỂ ĐỌC DÒNG STARTING...
    delay(2000); 

    SerialBT.begin("ESP32_BT_VND_Reader");

    // 1. Khởi tạo LittleFS VÀ KIỂM TRA LỖI.
    if (!LittleFS.begin(false)) { // Thử mount mà không format trước
        Serial.println("LittleFS Mount Failed. Attempting to FORMAT and Reboot...");
        // Nếu mount thất bại, thực hiện format toàn bộ phân vùng dữ liệu.
        LittleFS.end(); // Kết thúc phiên trước nếu có
        
        // Thử format
        if (LittleFS.format()) {
            Serial.println("SUCCESS: LittleFS has been fully FORMATTED. Rebooting now...");
        } else {
            Serial.println("CRITICAL ERROR: LittleFS FORMAT failed. Check Partition Scheme.");
        }
        
        // Luôn khởi động lại để áp dụng LittleFS mới hoặc để báo lỗi.
        delay(3000);
        ESP.restart();
    }
    
    // Nếu mount thành công (hoặc mount lại sau khi format thành công)
    if (!LittleFS.begin(true)) {
         Serial.println("CRITICAL ERROR: LittleFS cannot be mounted. Check Partition Scheme/Flash.");
         // Dừng ở đây nếu không thể sử dụng Flash
         while(true) { delay(1000); }
    }
    Serial.println("LittleFS Ready.");

    // 2. Khởi tạo I2S cho MAX98357A
    // Set Pinout rõ ràng, sử dụng đối tượng audioOut đã khai báo.
    audioOut.SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audioOut.SetOutputModeMono(true); 
    
    // 3. Cài đặt Âm lượng (Gain ĐÃ TĂNG lên 1.0 - mức tối đa)
    setAudioGain(1.0); 

    Serial.println("System Initialized: Bluetooth & I2S Ready.");

    // 4. CHẠY CHẾ ĐỘ KIỂM THỬ NẾU ĐƯỢC KÍCH HOẠT
    #if ENABLE_FILE_TEST == 1
    startFlashFileTest();
    #endif
}

// --- LOOP ---
void loop() {
    // --- A. DUY TRÌ LUỒNG AUDIO (NON-BLOCKING - RẤT QUAN TRỌNG) ---
    // Hàm này phải được gọi liên tục và NHANH NHẤT CÓ THỂ để stream data.
    if (audioWAV.isRunning()) {
        // Nếu audioWAV.loop() trả về FALSE, có nghĩa là track đã phát xong.
        if (!audioWAV.loop()) {
            audioWAV.stop();
            audio_finished_callback(); // Tự động gọi track tiếp theo
        }
    }
    // Chú ý: Nếu không gọi audioWAV.loop() thì nhạc sẽ bị ngắt hoặc không phát.

    // --- B. NHẬN VÀ XỬ LÝ LỆNH TỪ BLUETOOTH ---
    // Chỉ xử lý BT nếu không ở chế độ TEST hoặc playlist đã xong
    #if ENABLE_FILE_TEST == 0
    while (SerialBT.available()) {
        char incomingChar = SerialBT.read();
        receivedData += incomingChar;

        if (incomingChar == '\n') { 
            receivedData.trim();
            Serial.print("Received BT: ");
            Serial.println(receivedData);

            int keywordPos = receivedData.indexOf("tien:");
            
            if (keywordPos != -1) { 
                // *** Đảm bảo dừng mọi thứ trước khi tạo playlist mới ***
                if (audioWAV.isRunning()) {
                    audioWAV.stop();
                }
                
                int startPos = keywordPos + String("tien:").length();
                String tempStr = receivedData.substring(startPos);
                tempStr.trim();
                
                int endPos = tempStr.indexOf(' '); 
                if (endPos == -1) {
                    endPos = tempStr.length();
                }
                
                String numStr = tempStr.substring(0, endPos);
                long so_tien = numStr.toInt(); 
                
                // --- XỬ LÝ LOGIC PHÁT I2S (CHUYỂN SANG PLAYLIST) ---
                if (so_tien > 0) {
                    Serial.println("BT Command: Creating and starting playlist...");
                    
                    // 1. Tạo playlist mới
                    std::vector<int> tien_codes = number_to_file_codes(so_tien);
                    
                    // 2. Chèn "Bandanhanduoc" (21) vào đầu và "dong" (16) vào cuối
                    current_play_list.clear();
                    current_play_list.push_back(word_map["Bandanhanduoc"]); // Bandanhanduoc (21)
                    current_play_list.insert(current_play_list.end(), tien_codes.begin(), tien_codes.end());
                    current_play_list.push_back(word_map["dong"]);  // dong (16)

                    // 3. Bắt đầu phát track đầu tiên (Non-blocking)
                    current_track_index = 0;
                    startTrackPlayback(current_play_list[current_track_index]);
                    
                    SerialBT.println("Phát âm thanh đang được xử lý."); 
                } 
                else if (numStr.equals("0")) {
                    SerialBT.println("Số tiền là 0 đồng.");
                }
                else {
                    SerialBT.println("Lỗi: Số tiền không hợp lệ.");
                }
            } else {
                SerialBT.println("Lệnh không hợp lệ. Không tìm thấy 'tien:'.");
            }
            
            receivedData = ""; // Reset buffer
        }
    }
    #endif // ENABLE_FILE_TEST

    // --- C. CHUYỂN TIẾP TỪ SERIAL MONITOR QUA BLUETOOTH (Tùy chọn) ---
    // Chỉ hoạt động nếu không ở chế độ TEST
    #if ENABLE_FILE_TEST == 0
    if (Serial.available()) {
        SerialBT.write(Serial.read());
    }
    #endif
}


// --- HÀM LOGIC XỬ LÝ SỐ (GIỮ NGUYÊN) ---

// Hàm phụ trợ để xử lý 3 chữ số (0-999) và trả về list mã file
std::vector<int> read_group_of_three(int part) {
    std::vector<int> codes;
    if (part == 0) return codes;

    int hundreds = part / 100;
    int tens_place = (part % 100) / 10;
    int units_place = part % 10;
    
    // 1. Hàng trăm
    if (hundreds > 0) {
        codes.push_back(word_map[DIGITS[hundreds]]); // mot, hai, ba...
        codes.push_back(word_map["tram"]); // tram
    }

    // 2. Hàng chục
    if (tens_place == 0) {
        // Xử lý "linh" (le)
        if (hundreds > 0 && units_place > 0) {
            codes.push_back(word_map["le"]); // le (linh)
        }
    } else if (tens_place == 1) {
        // Xử lý "mười" (10-19)
        codes.push_back(word_map["muoif"]); // muoif (mười)
    } else { // tens_place >= 2
        codes.push_back(word_map[DIGITS[tens_place]]); // hai, ba, bon...
        codes.push_back(word_map["muoi"]); // muoi (File 18 - mươi)
    }

    // 3. Hàng đơn vị (và các trường hợp đặc biệt: mốt, lăm)
    if (units_place > 0) {
        if (units_place == 1 && tens_place >= 2) {
            // Trường hợp "mốt" (ví dụ: 21) - File 19
            codes.push_back(word_map["mots"]);
        } else if (units_place == 5 && tens_place >= 1) {
            // Trường hợp "lăm" (ví dụ: 15, 25) - File 20
            codes.push_back(word_map["lam"]);
        } else if (units_place != 0) {
             // Xử lý các số còn lại (1, 2, 3, 4, 6, 7, 8, 9)
             // Lưu ý: Cần đảm bảo 1, 5 không bị thêm 2 lần
             if (!(units_place == 1 && tens_place >= 2) && !(units_place == 5 && tens_place >= 1)) {
                 codes.push_back(word_map[DIGITS[units_place]]);
             }
        }
    }

    return codes;
}

// Hàm chính chuyển số sang list mã file
std::vector<int> number_to_file_codes(long num) {
    std::vector<int> final_file_list;
    if (num == 0) {
        final_file_list.push_back(word_map["khong"]);
        return final_file_list;
    }
    
    if (num < 0) {
        // Xử lý số âm nếu cần
        num = -num;
    }

    int group_index = 0;
    
    while (num > 0) {
        int part = num % 1000;
        
        if (part > 0) {
            std::vector<int> part_codes = read_group_of_three(part);
            
            // Thêm đơn vị (nghìn, triệu, tỷ)
            if (group_index > 0 && part_codes.size() > 0) {
                // Kiểm tra nếu nhóm trước là 000 thì không đọc đơn vị (ví dụ: 1 triệu 0 nghìn)
                // Tuy nhiên, logic này phức tạp, tạm thời cứ đọc đơn vị nếu part > 0
                final_file_list.insert(final_file_list.begin(), word_map[UNITS[group_index]]);
            }
            
            // Thêm mã số của nhóm 3 chữ số vào đầu list
            final_file_list.insert(final_file_list.begin(), part_codes.begin(), part_codes.end());
        }
        
        num /= 1000;
        group_index++;
    }

    return final_file_list;
}
