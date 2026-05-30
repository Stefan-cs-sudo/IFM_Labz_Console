# IFM_Labz_Console

A collaborative project built during the **2-Month ifm Challenge**. We designed and developed a custom mobile gaming experience powered by two physical hardware controllers that communicate wirelessly and via serial connection with an Android smartphone.

This project bridges the gap between hardware and software by turning two popular microcontrollers into physical game controllers for a custom-built mobile game. 

* **The Goal:** Build an interactive, responsive gaming system from scratch within a tight two-month timeline.
* **The Result:** A fully functional mobile game controlled simultaneously by two distinct hardware devices acting as player consoles.

---

## System Architecture

The project is split into two main ecosystems: **Hardware (Consoles)** and **Software (Mobile Game)**.

###  Hardware Consoles (Arduino)
The controllers handle player input and transmit data to the mobile device. The firmware was built using the **Arduino IDE**.

* **NodeMCU-32S:** Acts as Console 1, utilizing its onboard capabilities for wireless communication.
* **Arduino Nano:** Acts as Console 2, offering a compact, reliable hardware interface,this console was given to us by the organizers of the challenge.
*  ** The Fusion representation of the console **  : https://a360.co/4dywyMw 
*  **And a kicad representation of the PCB:
![Kicad](hPCB .png)


###  Mobile Application (Android Studio)
The game itself runs on an Android device and processes inputs from both controllers in real-time.

* **Language:** Java
* **Environment:** Android Studio
* **Features:** Multi-device input handling, real-time game state updates, and a responsive mobile UI.


##  Tech Stack

 Component | Technology / Hardware | Language / IDE 

 **Console 1** | NodeMCU-32S | Arduino IDE (C/C++) 
 **Console 2** | Arduino Nano | Arduino IDE (C/C++) 
 **Mobile Game** | Android Smartphone | Java / Android Studio 

---

## 👥 Authors

* [Deac Stefan](https://github.com/Stefan-cs-sudo))
* [Popa Maria](https://github.com/popam482)

***
