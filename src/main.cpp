// IMP - Měření vzdálenosti ultrazvukovým senzorem
// Filip Botlo
// xbotlo01
// 29.11.2024


#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Servo.h>

// Konfiguracia LCD
LiquidCrystal lcd(11, 10, 4, 5, 6, 7);

// Konfiguracia ultrazvukoveho senzora
#define TRIG_PIN 3
#define ECHO_PIN 2
#define MAX_DISTANCE 200 
#define BAR_LENGTH 16    // Dlzka baru na LCD
#define SERVO_PIN 13   

// Konfiguracia tlacidla
#define BUTTON_PIN 12
int buttonState = 0;
int prevButtonState = 0;
int mode = 0;  // Mody: 0 = Vzdialenost, 1 = Bary, 2 = Rychlost, 3 = Servo Sweep
bool useInches = false; // Priznak pre zobrazenie v palcoch

// Casovanie
unsigned long lastButtonPressTime = 0; // Cas posledneho stlacenia tlacidla
const unsigned long doublePressDelay = 300; // Maximalny cas pre dvojite stlacenie (v ms)

Servo myServo;

// Premenne pre servo sweep
int servoAngle = 0;           // Aktualny uhol serva
int servoStep = 1;            // Velkost kroku pre sweep


// Prototypy funkcii
unsigned int measureDistance();
void displayBars(unsigned int distance);
float convertToInches(unsigned int distance);

void setup() {
  lcd.begin(16, 2);

  // Priama konfiguracia BUTTON_PIN ako vstup s pull-up odporom
  DDRD &= ~(1 << BUTTON_PIN); 
  PORTD |= (1 << BUTTON_PIN);
  
  // Konfiguracia pinov ultrazvukoveho senzora
  DDRD |= (1 << TRIG_PIN); 
  DDRD &= ~(1 << ECHO_PIN);

  myServo.attach(SERVO_PIN);

  lcd.print("Vzdialenost:");
}

void loop() {
  buttonState = digitalRead(BUTTON_PIN);
  unsigned long currentTime = millis();

  // Kontrola stlacenia tlacidla
  if (buttonState == LOW && prevButtonState == HIGH) {
    if (currentTime - lastButtonPressTime < doublePressDelay) {
      useInches = !useInches; // Prepnutie medzi palcami a centimetrami
      lcd.clear();
      lcd.print("Zmena jednotky:");
      lcd.setCursor(0, 1);
      if (useInches) {
        lcd.print("Palce");
      } else {
        lcd.print("Centimetre");
      }
      delay(1000);
      lcd.clear();
      if (mode == 0) {
        lcd.print("Vzdialenost:");
      } else if (mode == 1) {
        lcd.print("Bary:");
      } else if (mode == 2) {
        lcd.print("Rychlost:");
      } else if (mode == 3) {
        lcd.print("Servo + Dist:");
      }
    } else {
      mode = (mode + 1) % 4;  // Prepnutie medzi modmi (0, 1, 2, 3)
      lcd.clear();
      if (mode == 0) {
        lcd.print("Vzdialenost:");
      } else if (mode == 1) {
        lcd.print("Bary:");
      } else if (mode == 2) {
        lcd.print("Rychlost:");
      } else if (mode == 3) {
        lcd.print("Radar:");
      }
    }
    lastButtonPressTime = currentTime; // Ulozi cas posledneho stlacenia tlacidla
  }

  prevButtonState = buttonState;

  // Mod merania vzdialenosti
  if (mode == 0) {
    delay(50);
    unsigned long distance = measureDistance();
    lcd.setCursor(0, 1);
    lcd.print("  ");
    
    // Zobrazenie vzdialenosti v spravnych jednotkach
    if (useInches) {
      float distanceInInches = convertToInches(distance);
      lcd.print(distanceInInches);
      lcd.print(" in   ");
    } else {
      lcd.print(distance);
      lcd.print(" cm   ");
    }
  } 
  // Mod zobrazenia barov
  else if (mode == 1) {
    delay(50);
    unsigned int distance = measureDistance();
    displayBars(distance);
  }
  // Mod merania rychlosti
  else if (mode == 2) {
    delay(50);
    unsigned long distance = measureDistance();
    unsigned long currentMeasurementTime = millis();

    // Vypocet rychlosti (v cm/s)
    static unsigned long lastDistance = 0; // Ulozenie hodnoty medzi volaniami
    static unsigned long lastMeasurementTime = currentMeasurementTime; // Ulozenie casu medzi volaniami
    unsigned long timeDifference = currentMeasurementTime - lastMeasurementTime; // Rozdiel casov v ms

    // Zobrazenie rychlosti v spravnych jednotkach
    lcd.setCursor(0, 1);
    if (timeDifference > 0) {
        float speed = (distance - lastDistance) / (timeDifference / 1000.0);
        if (useInches) {
            speed = convertToInches(speed);
        }

        lcd.print("  ");
        lcd.print(speed);
        if (useInches) {
            lcd.print(" in/s");
        } else {
            lcd.print(" cm/s");
        }

        // Rozlisenie medzi priblizovanim a vzdialovanim
        lcd.setCursor(0, 0);
        if (distance < lastDistance) {
            lcd.print("Priblizujem   ");
        } else if (distance > lastDistance) {
            lcd.print("Vzdaluju sa   ");
        } else {
            lcd.print("Zastavenie    ");
        }
    }

    lastDistance = distance; 
    lastMeasurementTime = currentMeasurementTime;
  }
  // Radarovy mod
  else if (mode == 3) {
    delay(15); 
    unsigned long distance = measureDistance();

    // Pohyb serva
    myServo.write(servoAngle); // Nastavi servo na aktualny uhol
    servoAngle += servoStep;
    if (servoAngle <= 0 || servoAngle >= 180) {
      servoStep = -servoStep;  // Zmena smeru na hraniciach
    }

    lcd.setCursor(0, 1); 
    lcd.print("  "); 
    if (useInches) {
      float distanceInInches = convertToInches(distance); 
      lcd.print(distanceInInches);
      lcd.print(" in   ");
    } else {
      lcd.print(distance);
      lcd.print(" cm   ");
    }
  }
}

unsigned int measureDistance() {
  PORTD &= ~(1 << TRIG_PIN);  // TRIG_PIN je LOW
  delayMicroseconds(2);
  PORTD |= (1 << TRIG_PIN);   // TRIG_PIN HIGH
  delayMicroseconds(10);      // HIGH na 10 mikrosekund
  PORTD &= ~(1 << TRIG_PIN);  // TRIG_PIN spat na LOW

  long duration = 0;

  while (!(PIND & (1 << ECHO_PIN))); // Caka na HIGH signal na ECHO_PIN
  while (PIND & (1 << ECHO_PIN)) {
    duration++;
    delayMicroseconds(1);
  }

  unsigned int distance = duration * 0.034 / 2;
  if (distance > MAX_DISTANCE) {
    return MAX_DISTANCE;
  }
  return distance;
}

void displayBars(unsigned int distance) {
  int numBars = BAR_LENGTH * (1 - (distance / (float)MAX_DISTANCE));
  if (numBars < 0) numBars = 0;
  if (numBars > BAR_LENGTH) numBars = BAR_LENGTH;

  lcd.setCursor(0, 1);        // Nastavi kurzor na druhy riadok LCD
  for (int i = 0; i < BAR_LENGTH; i++) {
    if (i < numBars) {
      lcd.print("#");         // Vypíše # pre aktuálnu vzdialenosť
    } else {
      lcd.print(" ");
    }
  }
}

float convertToInches(unsigned int distance) {
  return distance / 2.54;
}








