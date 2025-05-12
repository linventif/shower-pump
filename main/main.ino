// ==== BROCHES ====
const int floatAPin    = 2;   // flotteur bas
const int floatBPin    = 3;   // flotteur haut
const int relayPin     = 8;   // relais pompe
const int greenLEDPin  = 9;   // LED verte
const int yellowLEDPin = 10;  // LED jaune
const int redLEDPin    = 11;  // LED rouge

// ==== TEMPOS (ms) ====
const unsigned long SLEEP_BLINK_INTERVAL = 30UL * 1000;     // 30 s
const unsigned long SHOWER_TIMEOUT       = 5UL  * 60 * 1000; // 5 min
const unsigned long ERROR_TIMEOUT        = 2UL  * 60 * 1000; // 2 min
const unsigned long YELLOW_BLINK_INTERVAL= 1000;             // 1 s

// ==== ÉTATS ====
enum Mode { MODE_SLEEP, MODE_SHOWER };
Mode currentMode = MODE_SLEEP;
bool pumpState       = false;  // état du relais
bool blinkOn         = false;  // pour clignotement sleep (LED verte)
bool errorState      = false;  // alerte blocage

// ==== TIMERS ====
unsigned long lastBlinkTime      = 0;  // sleep vert
unsigned long lastActivityTime   = 0;  // timeout douche→sleep
unsigned long highLevelStartTime = 0;  // début éventuelle erreur
unsigned long lastYellowBlinkTime= 0;  // clignotement jaune
bool yellowBlinkState            = false;

// pour front montant B
bool lastBState = false;

void setup() {
  pinMode(floatAPin,    INPUT_PULLUP);
  pinMode(floatBPin,    INPUT_PULLUP);
  pinMode(relayPin,     OUTPUT);
  pinMode(greenLEDPin,  OUTPUT);
  pinMode(yellowLEDPin, OUTPUT);
  pinMode(redLEDPin,    OUTPUT);

  // tout éteint au démarrage
  digitalWrite(relayPin,     LOW);
  digitalWrite(greenLEDPin,  LOW);
  digitalWrite(yellowLEDPin, LOW);
  digitalWrite(redLEDPin,    LOW);

  lastBlinkTime       = millis();
  lastActivityTime    = millis();
  lastYellowBlinkTime = millis();
}

void loop() {
  unsigned long now = millis();
  bool A_haut = digitalRead(floatAPin)  == HIGH;
  bool B_haut = digitalRead(floatBPin)  == HIGH;

  // front montant sur B → reset erreur en douche
  if (currentMode == MODE_SHOWER && B_haut && !lastBState) {
    highLevelStartTime = now;
    errorState         = false;
  }
  lastBState = B_haut;

  if (currentMode == MODE_SLEEP) {
    // --- MODE VEILLE ---
    // 1) Drainage si A haut & B bas
    if (A_haut && !B_haut) {
      if (!pumpState) {
        pumpState = true;
        digitalWrite(relayPin, HIGH);
      }
    } else if (pumpState) {
      pumpState = false;
      digitalWrite(relayPin, LOW);
    }

    // 2) Clignotement LED verte
    if (now - lastBlinkTime >= SLEEP_BLINK_INTERVAL) {
      blinkOn = !blinkOn;
      digitalWrite(greenLEDPin, blinkOn);
      lastBlinkTime = now;
    }

    // 3) LEDs autres éteintes
    digitalWrite(yellowLEDPin, LOW);
    digitalWrite(redLEDPin,    LOW);

    // 4) Passage en mode douche si A+B hauts
    if (A_haut && B_haut) {
      currentMode      = MODE_SHOWER;
      lastActivityTime = now;
      // reset clignotement sleep
      blinkOn = false;
      digitalWrite(greenLEDPin, LOW);
    }
  }
  else {
    // --- MODE DOUCHE ---
    // 1) Détection montée entre A et B (avant pompe)
    bool rising = (A_haut && !B_haut && !pumpState);

    // 2) Gestion pompe
    if (!pumpState) {
      if (A_haut && B_haut) {
        pumpState = true;
        digitalWrite(relayPin, HIGH);
        lastActivityTime = now;
        // démarrage lance clignotement jaune
        lastYellowBlinkTime = now;
        yellowBlinkState    = true;
      }
    } else {
      if (!A_haut) {
        pumpState = false;
        digitalWrite(relayPin, LOW);
        lastActivityTime = now;
        // stop clignotement jaune
        yellowBlinkState = false;
        digitalWrite(yellowLEDPin, LOW);
      }
    }

    // 3) Vérif. blocage
    if (!errorState
        && A_haut && B_haut
        && !pumpState
        && (now - highLevelStartTime >= ERROR_TIMEOUT)) {
      errorState = true;
    }

    // 4) Affichage LEDs (priorité)
    if (errorState) {
      digitalWrite(redLEDPin,    HIGH);
      digitalWrite(greenLEDPin,  LOW);
      digitalWrite(yellowLEDPin, LOW);
    }
    else if (rising) {
      // montée → jaune fixe
      digitalWrite(yellowLEDPin, HIGH);
      digitalWrite(greenLEDPin,  LOW);
      digitalWrite(redLEDPin,    LOW);
    }
    else if (pumpState) {
      // pompe ON → jaune clignote 1 Hz
      if (now - lastYellowBlinkTime >= YELLOW_BLINK_INTERVAL) {
        yellowBlinkState = !yellowBlinkState;
        lastYellowBlinkTime = now;
      }
      digitalWrite(yellowLEDPin, yellowBlinkState);
      digitalWrite(greenLEDPin,  LOW);
      digitalWrite(redLEDPin,    LOW);
    }
    else {
      // attente (A bas ou autre) → LED verte fixe
      digitalWrite(greenLEDPin,  HIGH);
      digitalWrite(yellowLEDPin, LOW);
      digitalWrite(redLEDPin,    LOW);
    }

    // 5) Timeout retour veille
    if (now - lastActivityTime >= SHOWER_TIMEOUT) {
      // remise à zéro
      currentMode = MODE_SLEEP;
      pumpState   = false;
      errorState  = false;
      digitalWrite(relayPin,     LOW);
      digitalWrite(greenLEDPin,  LOW);
      digitalWrite(yellowLEDPin, LOW);
      digitalWrite(redLEDPin,    LOW);
      // réarmement blink sleep
      lastBlinkTime = now;
      blinkOn       = false;
    }
  }
}
