# Project Odradek - Build Log

**A chronological record of the development process, including all the failures, burnt components, 4 AM research sessions and ofc lessons learned.**

---

## 🎮 The Start

### October 25, 2025

Finished playing Death Stranding 2. The game's tech design, especially the Odradek scanner definitly was something I loved. So that night I thought "I NEED ONE NOW" so.

From that moment on, it became an obsession, rather unhealthy one. I stayed up until 4 AM researching mechanisms and trying to figure out how to recreate the iconic flapping arms. The challenge: make it accurate to the game, but achievable with hobbyist resources.

**Initial mechanism research:**
- Stepper motor → too complex for coordinated movement
- Rack and pinion → over-engineered
- Screw mechanism → too slow
- Eccentric cam → promising but difficult to fabricate
- 4 individual motors → expensive and complicated
- **Solenoid actuator** → *Was it, few moving parts and had the exact moving action needed.*

The realization hit: simpler is better, a solenoid is just a coil and a moving part. That's it.

**The "claw method":**  
Each arm would have two mounting points - one stationary, one pushed by the solenoid. When the solenoid extends, the arms open. When it retracts, they close. Simple physics, maximum effect.

But visualizing how to make this practical? That kept me up the rest of the night.

---

## 📐 Parts Planning Hell

### October 27, 2025

Another 4 AM night. This time, focused on parts and wiring.

**The power problem:**  
Solenoids need serious current. 12V at 3A. If I connect that directly to the ESP32 or even a basic MOSFET without protection, it'll fry everything. Inductive kickback from the solenoid collapsing magnetic field = dead components.

**Solution: Flyback diodes.**

Researched different diode types, settled on fast-recovery flyback diodes to handle the inductive spike.

**The embedded power problem:**  
I wanted this to be a standalone device, not tethered to USB. That means the ESP32 needs clean, stable 5V from the 24V power supply. Enter: buck converters.

**Power architecture:**
```
24V 10A PSU
├── Buck Converter 1 (24V → 12V) → Solenoid
└── Buck Converter 2 (24V → 5V) → ESP32 (with 10µF + 0.1µF caps for stability)
└── Buck Converter 3 (24V → 5V) → LED Strips
```

**Protection:**  
Fuses everywhere. If something shorts, I want the fuse to blow, not the components.

**The shopping spree:**  
That night, I ordered **€260+ worth of parts**:
- Pack of ESP32 boards (because I knew I'd kill at least one lol)
- 2x 12V 3A 60N solenoid actuators
- 24V 10A power supply
- Multiple buck converters
- Flyback diodes
- MOSFETs (logic-level)
- Capacitors (ceramic and electrolytic)
- Resistors (assorted values)
- Fuses (various ratings)
- 18 AWG power cable
- WS2812B LED strips
- Capacitive touch sensor
- Piezo buzzer

---

## ✏️ Design Phase

### October 28, 2025

Sketched mechanical designs. How would the arms attach? How would the solenoid mount? What materials could I use?

Rough CAD concepts. Lots of erasing. Lots of "this won't work."

### October 30, 2025

Spent the entire day in Death Stranding 2's photo mode, pausing the game every time the Odradek appeared on screen. Screenshot after screenshot. I needed accurate reference material for the final design for CAD.

The goal: make it *feel* like the real thing

---

## 📦 Parts Arrival & Early Progress

### November 2, 2025

**Parts arrived.**

The pile of components on my desk looked kinda scary since I don't have much experience in this.

### November 3, 2025 - Audio Tuning Hell

Decided to tackle the hardest part first: **the sound.**

The Odradek's chirp is so robotic. Harmonic, layered and all that. so I went ingame and used tons of youtube too to sample the sounds.

**Method:**  
Loaded Death Stranding audio into a spectrogram analyzer. Identified the dominant frequencies. Started with these values:
- 7900 Hz (high overtone)
- 7800 Hz
- 6016 Hz
- 5763 Hz
- 5300 Hz
- 3400 Hz (low fundamental)

**The process:**  
3 hours of tweaking PWM output, cycling through frequencies rapidly to create the layered effect. Test, compare to game audio, adjust, repeat.

**Result: Roughly 88% match !!**  

Not perfect, but close enough.

### November 4, 2025

Got the startup sequence working. LED fade-in, solenoid test pulse, audio chirp.

Added basic weather beeping functionality - placeholder logic, but it worked.

---

## 🔥 The Day of Pain #1

### November 17, 2025

**The buck converter arrived.**

I didn't have a proper connector for the power supply adapter, so I improvised with thin Arduino jumper wires from a friend. 

**Mistake #1: Thin wires + 24V 10A = bad idea.**

**Mistake #2: I touched the bare conductors while connecting them to the buck converter.**

**Mistake #3: Reversed polarity.**

**What happened:**
- Instant heat. The wire was burning my hand.
- An ember shot from the cable to my left hand.
- Smell of burning plastic.
- Buck converter: **dead.**

**Cost: €23**

I sat there staring at the fried converter pissed,

Took a break. Went full theoretical for the next few weeks, just researching and planning.

---

## 🔥 The Day of Pain #2

### December 14, 2025

**Bought a new buck converter.**  
**Bought a proper female power adapter.**  
**Soldered proper 18 AWG cable.**

This time: methodical. Double-checked polarity. Used a multimeter. Slow and careful so I don't burn my hands again.

**SUCCESS !!** The buck converter powered on, output stable 12V after tweaking.

Connected the solenoid. Tuned the voltage. **It worked !!**

Feeling confident, I wired up the full circuit: ESP32 → MOSFET → Solenoid, with flyback diode for protection.

Powered it on.

**Then the bastard blew, had the polarity wrong on the Diode.**

**I accidentally touched the MOSFET input and output pins while it was powered on trying to diagnose voltage outputs with multimeter...**

**What happened:**
- The diode fried instantly, thing made tons of smoke and had a quick burst of fire.
- The MOSFET overheated and stopped working.
- My room filled with the smell of burning electronics - plastic, silicone and probably cancer.

Panicking, I yanked the power cable. The converter survived, thankfully. But the MOSFET and diode were toast :(

**Lesson learned:** Triple-check diode polarity. Never touch live circuits. Work deliberately and DEFINITLY not quickly.

Replaced the MOSFET and diode. Corrected the circuit. Moved on.

---

## 🔥 The Day of Pain #3

### December 18, 2025 (Morning)

Finally got the solenoid working reliably with the ESP32. Added a small bright LED for status indication.

Ran a test sequence.

**The ESP32 froze.**

Then it got real hot. I touched it to check.
Shit burned my thumb, very annoying >:(

The ESP32 was fried from improper resistor values on the LED circuit (somehow worked later after I replaced this one with a new ESP32. But something was STILL off)

Thankfully I have bought a pack of ESP32s.

Swapped in a backup. Corrected the resistor values. Added the LED again. This time it worked without overheating

Got the startup sequence fully working - LED, buzzer, solenoid all working in tandem !!

### December 18, 2025 (Evening) - The Day of Fortune :D

Started programming the 255 RGB LEDs. This was the fun part - the playground phase if I can call it that.

**Experimentation:**
- Hover over capacitive touch sensor → flash effect
- Ripple animations spreading across the strip
- Breathing effect (default mode, Bridges blue color)
- Smooth color morphing between states


**Weather integration:**  
Found Open-Meteo's free weather API. Clean, no API key required, perfect for my broke ass LOL.

Started implementing the 30-minute rain warning logic. This got complex quickly - parsing JSON, calculating time windows, triggering alerts. Lots of trial and error. Lots of bugs.

Learned about watchdog timers to prevent ESP32 freezing (which I REGRET, IT NEVER WORKS !!!!). Learned about dual-core task management. Learned WAY more about HTTP requests and DNS than I expected.

---

## 🎨 Web Interface Development

### December 20, 2025

**Playground day.**

Explored different UI aesthetics:
- Futuristic (too generic and bald lol)
- Modern flat design (BORING)
- Cyberpunk neon (trying too hard and doesn't match DS1/DS2)
- **Retro Death Stranding 1 terminal aesthetic (perfect)**

Settled on:
- Black background
- Bridges blue accents
- Dark blue, bright blue highlights
- CRT scanline effect
- Monospace fonts
- Minimal, functional layout

Dove deep into web development. Learned:
- **AJAX** for live updates without page refresh
- **CSS animations** for the scanline effect
- **Responsive design** so it works on phones

Spent the next week refining. Every detail mattered - button hover states, loading animations, smooth transitions, all of em >:)

### December 23, 2025

Added complex weather warning sequences to the web UI. Created a test page at the bottom to trigger animations manually during development.

---

## 🐱 The Cat Incident

### Late December 2025

My cat decided the LED strip looked fun. Clawed through it. **Destroyed 132 LEDs, trust me I tried soldering but shit STILL BROKE.**

Went from 255 working LEDs to 123. Had to update the code temporarily while waiting for replacement strips.

Lesson learned lmao Keep electronics away from curious cats

---

## ✅ Completion

### December 30, 2025

**Web UI complete. Firmware stable-ish. Hardware functional.**

The Odradek was alive.

It warns me 30 minutes before rain. It chirps. The LEDs breathe. The solenoid pulses.

**Still missing:**  
The physical enclosure. The arm mechanism. The shell.

I have CAD concepts, but fabrication is the next challenge. For now, the internals work perfectly.

---

## 📊 Final Statistics

**Total Development Time:** ~2 months  
**Late Night Sessions:** Too many to count.  
**Total Cost:** €230-300+  
**Components Destroyed:**
- 1x Buck converter (€23)
- 2x MOSFETs
- 2x Flyback diodes
- 1x ESP32
- 132 LEDs thanks Taco >:(

**Injuries:**
- Burnt hand (wire heating)
- Ember burn (left hand)
- Thumb light burn (hot ESP32)

**Lines of Code:** ~1800+ (Arduino) + ~800+ (HTML/CSS/JS)

---

## 🧠 Key Lessons Learned

### Hardware
1. **Always check polarity !!** Twice. Then a third time.
2. **Flyback diodes are a NEED** for inductive loads.
3. **Use proper gauge wire** for high current power.
4. **Capacitors on power rails** prevent voltage spikes and noise.
5. **Fuses save projects.** They're cheap insurance.
6. **Never touch live circuits.** Just don't.

---

## 🚀 What's Next

**Immediate:**
- Replace damaged LED strips (ordered)
- Update code for 255 LEDs
- Test long-term stability

**Future:**
- Design and 3D print the enclosure
- Implement the arm mechanism (claw method)
- Add second solenoid for symmetry
- Integrate into Home Assistant

---

## 📸 Progress Photos

*(Photos would go here if available - burnt components, work-in-progress shots, etc.)*

**Current status:** Functional but no final enclosure  
**Next update:** When I finish the shell

---

**- Madis O.**  
**NullSec Hardware Projects**  
**[@nullopses](https://instagram.com/nullopses)**
