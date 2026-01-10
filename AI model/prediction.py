import time
import numpy as np
import joblib
import firebase_admin
from firebase_admin import credentials, db
from tensorflow import keras

# ==========================================
# 1. CONFIGURATION
# ==========================================

# REPLACE THIS with the UID of your "Correct" card (e.g., "A3B2C1D0")
# You can see this UID in your Serial Monitor or Firebase when you scan a card.
AUTHORIZED_UID = "YOUR_VALID_TAG_UID_HERE" 

# Firebase & AI Files
MODEL_PATH = 'traffic_ai_model.h5'
SCALER_X_PATH = 'scaler_X.pkl'
SCALER_Y_PATH = 'scaler_y.pkl'

# ==========================================
# 2. SETUP & INITIALIZATION
# ==========================================

print("Loading AI Model and Scalers...")
try:
    model = keras.models.load_model(MODEL_PATH)
    scaler_X = joblib.load(SCALER_X_PATH)
    scaler_y = joblib.load(SCALER_Y_PATH)
    print("AI Model loaded successfully!")
except Exception as e:
    print(f"Error loading AI files: {e}")
    exit()

# Connect to Firebase
# Replace with your actual database URL
DATABASE_URL = "https://hbro2-9682b-default-rtdb.firebaseio.com/" 

cred = credentials.Certificate("hbro2-9682b-firebase-adminsdk-9p2bu-105cc9bbde.json")
firebase_admin.initialize_app(cred, {
  "apiKey": "AIzaSyB1QQSoN_xKBIhOfMH-4y26A5z5XRU1ScE",
  "authDomain": "hbro2-9682b.firebaseapp.com",
  "databaseURL": "https://hbro2-9682b-default-rtdb.firebaseio.com",
  "projectId": "hbro2-9682b",
  "storageBucket": "hbro2-9682b.firebasestorage.app",
  "messagingSenderId": "580100574500",
  "appId": "1:580100574500:web:480381bb27357bf9fdf7b1",
  "measurementId": "G-2914BZMJB5"
  })
print("Connected to Firebase!")

# ==========================================
# 3. HELPER FUNCTIONS
# ==========================================

def predict_green_times(occupancy, q_main, q_entrance, q_exit):
    input_data = np.array([[occupancy, q_main, q_entrance, q_exit]])
    input_scaled = scaler_X.transform(input_data)
    pred_scaled = model.predict(input_scaled, verbose=0)
    pred_real = scaler_y.inverse_transform(pred_scaled)[0]
    
    t1 = max(5, int(pred_real[0])) # Main Start
    t2 = max(5, int(pred_real[1])) # Main End (Exit)
    t3 = max(5, int(pred_real[2])) # Entrance
    return t1, t2, t3

# ==========================================
# 4. MAIN CONTROL LOOP
# ==========================================

def main_loop():
    print("\nStarting AI Traffic & Access Control Loop...")
    print(f"Authorized UID set to: {AUTHORIZED_UID}")
    print("Press Ctrl+C to stop.\n")
    
    while True:
        try:
            # --- Fetch All Data ---
            # We fetch the root to get stats and logs in one go
            root_snapshot = db.reference().get()
            
            if root_snapshot:
                stats = root_snapshot.get('stats', {})
                logs = root_snapshot.get('logs', {})
                
                # ===========================
                # PART A: AI TRAFFIC LOGIC
                # ===========================
                count_main_in = stats.get('lane_1_count', 0)
                count_main_out = stats.get('lane_2_count', 0)
                count_entrance = stats.get('lane_3_count', 0)
                count_exit_ramp = stats.get('lane_4_count', 0)
                
                total_in = count_main_in + count_entrance
                total_out = count_main_out + count_exit_ramp
                current_occupancy = max(0, total_in - total_out)
                
                # Approximate Queues (using default 5 for now)
                t1, t2, t3 = predict_green_times(current_occupancy, 5, 5, 5)

                # ===========================
                # PART B: RFID ACCESS LOGIC
                # ===========================
                last_rfid_read = logs.get('rfid_entry', "")
                
                # Default State: Arrow RIGHT, Buzzer OFF
                arrow_cmd = "RIGHT"
                buzzer_cmd = False
                
                # Check if the scanned card matches the Authorized UID
                if last_rfid_read == AUTHORIZED_UID:
                    print(f"(!) AUTHORIZED CARD DETECTED: {last_rfid_read}")
                    arrow_cmd = "LEFT"
                    buzzer_cmd = True
                else:
                    if last_rfid_read != "":
                        print(f"(-) Unauthorized Card: {last_rfid_read}")
                
                # ===========================
                # PART C: UPLOAD DECISIONS
                # ===========================
                print(f"Traffic Update -> Occ: {current_occupancy} | T1: {t1}s, T2: {t2}s, T3: {t3}s")
                print(f"Access Update  -> Arrow: {arrow_cmd} | Buzzer: {'ON' if buzzer_cmd else 'OFF'}")

                updates = {
                    # Traffic Control
                    '/config/t1_green_duration': t1 * 1000,
                    '/config/t2_green_duration': t2 * 1000,
                    '/config/t3_green_duration': t3 * 1000,
                    
                    # Access Control (RFID)
                    '/control/arrow_led': arrow_cmd,
                    '/control/buzzer': buzzer_cmd
                }
                
                db.reference().update(updates)
                print(">> Firebase Updated.\n")
                
            else:
                print("Waiting for data...")

            time.sleep(2) # Faster update rate for RFID responsiveness

        except KeyboardInterrupt:
            print("\nStopping...")
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(2)

if __name__ == "__main__":
    main_loop()