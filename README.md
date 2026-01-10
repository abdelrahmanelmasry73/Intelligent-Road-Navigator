# Intelligent Road Navigator 
## Gharbiya STEM - Team 19310 - 2025/2026 

## Methods 
The prototype has passed thrrough seven main steps:

  In the first step, the ESP32 microcontroller is connected along 2 breadboards, and the other
components are connected to its GPIOs. A 5V adaptor is used to power the system. There are also two
expanders used called PCF8574 i/o expansion module. Those expanders are connected to the ESP32
through {21, 22} GPIOs. The first with 0x20 address and the second with 0x21.

  In the second step, sensors are implemented in the ELEC: ultrasonic (hc-sr04) sensors are classified to
trig and echo pins. TRIG pins are connected to {12, 13, 25, 26} GPIOs. ECHO pins are connected to
{34, 35, 36, 39} GPIOs with a voltage driver with resistors at each echo pin. This voltage driver is done
by putting 1kΩ resistor between ECHO and GPIOs and a 2.2kΩ between the GPIOs and the ground.
Then, infrared obstacle avoidance motion sensors are connected to {27, 16, 17, 32} GPIOs.

  In the third step, the emergency and alarm system is installed. The main device used in that system is
the RIFID RC522. It is connected to GPIO {4, 19, 23, 18, 5}. Then the second thing the buzzer which
responsible for the alarm which is connected to P1 of the second expansion module in series with 1kΩ
resistor and through a MOSFET transistor.

  In the fourth step, all the LEDs are connected. First the traffic lights, they are connected to {P0, P1,
P2, P3, P4, P5, P6, P7, P0} of the first and second expansion modules. Second, the arrow led which is
classified into three LEDs. Each one is connected through a MOSFET transistor, 220Ω and 10kΩ. The
LEDs were connected to {33, 2, 14}
All connections mentioned above are shown in **fig (1)**.

In the fifth step, the control system of the project is programmed into the
ESP32 using the Arduino IDE. After uploading the code, the ESP32 is
connected to the nearest Wi-Fi network. The ESP32 continuously collects all
the real-time data from all sensors.

After the data is gathered, the ESP32 calculates the number of cars on the
road, the average distance between them, and If there is any emergency car or
not. These values are put in a JSON object and sent to the AI server through
an HTTP POST request after it get stored in a fire base. 
When the server gives the prediction either open or close, there is an
emergency or not, the ESP32 control all the sensors and parts to implement
that prediction.

  In the sixth step, the artificial-intelligence model responsible for predicting
is developed and trained. First, a dataset is created containing examples of
different road situations. Including the number of cars, the average distance
between them, and whether an emergency vehicle was present or not. The numbers of these values in the dataset is put randomly at the begining and then the dataset is sent to an application called Jupyter
This app is responsible for training the AI model according to the given dataset and conditions you need given in the python code. All the
mentioned IDE codes, server, and the AI model can be found in the main branche above. 

he final step, all the ELEC is installed. It is classified into two parts. The first is the sensors, LEDs,
buzzer, and the RIFID device as shown in **fig (2)**. This part is installed on the road because it’s
responsible for collecting the data and giving signs to the cars. The second part is the wires, breadboard
and all of its components, PCF8574, and ESP32. This part is housed below the road to make the road
organized and not to interrupt it as shown in **fig (3)**.

## Figure 1 

<img width="1046" height="814" alt="Screenshot 2025-11-26 200151" src="https://github.com/user-attachments/assets/c94a66fa-a182-4a15-b4d0-e9718143eb0a" />

## Figure 2

![5843816925274770340](https://github.com/user-attachments/assets/b7d7a408-f106-4cce-8f62-77a3c737ae02)

## Figure 3


