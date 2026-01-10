import pandas as pd
import numpy as np
import random

# ==========================================
# 1. CONFIGURATION
# ==========================================
NUM_SAMPLES = 5000         # Size of the dataset
MAX_CAPACITY = 60          # Physical limit of the road
TARGET_RATIO = 0.60        # We want to keep load under 60%
TARGET_CARS = int(MAX_CAPACITY * TARGET_RATIO) # 36 cars

# Traffic Light Constraints (in seconds)
MIN_GREEN = 10
MAX_GREEN = 60

# ==========================================
# 2. GENERATION LOGIC
# ==========================================
data = []

for _ in range(NUM_SAMPLES):
    # --- Generate Random Inputs (Sensor Readings) ---
    # Randomly simulate current cars on road (0 to 65 to simulate occasional overcrowding)
    current_cars = random.randint(0, 65) 
    
    # Random queue lengths waiting at the lights (0 to 30 cars waiting)
    q_main_in = random.randint(0, 30)   # Waiting at Traffic 1
    q_entrance = random.randint(0, 20)  # Waiting at Traffic 3
    q_main_out = random.randint(0, 30)  # Waiting at Traffic 2 (to leave)

    # --- Calculate Optimal Green Times (Expert Logic) ---
    
    # Initialize base times
    t1_green = MIN_GREEN
    t2_green = MIN_GREEN
    t3_green = MIN_GREEN

    # LOGIC 1: Congestion Control (The 60% Rule)
    if current_cars >= TARGET_CARS:
        # ROAD IS FULL: Throttle inputs, Maximize output
        
        # Penalize inputs heavily based on how far over we are
        overload_factor = (current_cars - TARGET_CARS)
        
        # Inputs (T1 & T3) get minimum time mostly
        t1_green = MIN_GREEN
        t3_green = MIN_GREEN
        
        # Output (T2) gets maximum time to drain the road
        t2_green = MAX_GREEN
        
    else:
        # ROAD IS FREE: Balance based on demand (Queue Lengths)
        
        # Calculate 'Pressure' relative to total demand
        total_demand = q_main_in + q_entrance + q_main_out
        if total_demand == 0: total_demand = 1 # Avoid div by zero
        
        # Allocate time proportionally to queue length
        # We assume a total cycle budget roughly between 60s-120s
        cycle_budget = 90 
        
        t1_green = int((q_main_in / total_demand) * cycle_budget)
        t3_green = int((q_entrance / total_demand) * cycle_budget)
        t2_green = int((q_main_out / total_demand) * cycle_budget)

    # --- Final Constraints & Noise ---
    # Clamp values between Min and Max
    t1_green = np.clip(t1_green, MIN_GREEN, MAX_GREEN)
    t2_green = np.clip(t2_green, MIN_GREEN, MAX_GREEN)
    t3_green = np.clip(t3_green, MIN_GREEN, MAX_GREEN)

    # Add slight randomness (Noise) so the AI learns to generalize, not just memorize rules
    # We alter the time by -2 to +2 seconds randomly
    t1_green += random.randint(-2, 2)
    t2_green += random.randint(-2, 2)
    t3_green += random.randint(-2, 2)

    # Re-clamp after noise
    t1_green = np.clip(t1_green, MIN_GREEN, MAX_GREEN)
    t2_green = np.clip(t2_green, MIN_GREEN, MAX_GREEN)
    t3_green = np.clip(t3_green, MIN_GREEN, MAX_GREEN)

    # Append to dataset
    data.append([current_cars, q_main_in, q_entrance, q_main_out, t1_green, t2_green, t3_green])

# ==========================================
# 3. EXPORT
# ==========================================
columns = [
    'Current_Occupancy', 
    'Queue_Main_In', 
    'Queue_Entrance', 
    'Queue_Main_Out', 
    'Green_T1', 
    'Green_T2', 
    'Green_T3'
]

df = pd.DataFrame(data, columns=columns)

# Save to CSV
csv_filename = 'smart_traffic_dataset.csv'
df.to_csv(csv_filename, index=False)

print(f"Dataset generated successfully with {NUM_SAMPLES} samples!")
print(f"Saved as: {csv_filename}")
print("-" * 30)
print("Sample Data:")
print(df.head(10))

# Optional: Correlation check to see if logic holds
print("\nCorrelation Matrix (Check relationships):")
print(df.corr()[['Green_T1', 'Green_T2', 'Green_T3']])