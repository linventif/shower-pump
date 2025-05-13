// ==== BROCHES ====
const int floatAPin    = 2;   // flotteur bas
const int floatBPin    = 3;   // flotteur haut
const int relayPin     = 8;   // relais pompe
const int greenLEDPin  = 9;   // LED verte
const int yellowLEDPin = 10;  // LED jaune
const int redLEDPin    = 11;  // LED rouge

// ==== TEMPOS (ms) ====
const unsigned long SLEEP_BLINK_INTERVAL  = 1000;
const unsigned long SHOWER_TIMEOUT        = 60000;
const unsigned long ERROR_TIMEOUT         = 60000;
const unsigned long YELLOW_BLINK_INTERVAL = 1000;

// ==== ÉTATS ====
enum Mode { MODE_SLEEP, MODE_SHOWER };
Mode currentMode     = MODE_SHOWER;
bool pumpState       = false;
bool blinkOn         = false;
bool errorState      = false;
bool sleepDrainDone  = false;  // <--- nouveau flag
bool yellowBlinkState= false;
bool lastBState      = false;

// ==== TIMERS ====
unsigned long lastBlinkTime       = 0;
unsigned long lastActivityTime    = 0;
unsigned long highLevelStartTime  = 0;
unsigned long lastYellowBlinkTime = 0;

void setup() {
  pinMode(floatAPin,    INPUT_PULLUP);
  pinMode(floatBPin,    INPUT_PULLUP);
  pinMode(relayPin,     OUTPUT);
  pinMode(greenLEDPin,  OUTPUT);
  pinMode(yellowLEDPin, OUTPUT);
  pinMode(redLEDPin,    OUTPUT);

  // Tout éteint au démarrage
  digitalWrite(relayPin,     LOW);
  digitalWrite(greenLEDPin,  LOW);
  digitalWrite(yellowLEDPin, LOW);
  digitalWrite(redLEDPin,    LOW);

  unsigned long now = millis();
  lastBlinkTime       = now;
  lastActivityTime    = now;
  highLevelStartTime  = now;
  lastYellowBlinkTime = now;
}

void loop() {
  unsigned long now = millis();
  bool A_haut = digitalRead(floatAPin) == HIGH;
  bool B_haut = digitalRead(floatBPin) == HIGH;

  // Front montant sur B → reset erreur en mode douche
  if (currentMode == MODE_SHOWER && B_haut && !lastBState) {
    highLevelStartTime = now;
    errorState         = false;
  }
  lastBState = B_haut;

  // Dispatch selon le mode
  if (currentMode == MODE_SLEEP) {
    handleSleepMode(now, A_haut);
  } else {
    handleShowerMode(now, A_haut, B_haut);
  }

  // Mise à jour des LED (calcul local de rising)
  updateLEDs(now, A_haut, B_haut);
}

// ----- SLEEP MODE -----
void handleSleepMode(unsigned long now, bool A_haut) {
  if (!sleepDrainDone) {
    // 1× seule vidange : tant que A est haut, on pompe
    if (A_haut) {
      setPump(true);
    } else {
      // dès qu'il n'y a plus d'eau sur A → on arrête la pompe pour de bon
      setPump(false);
      sleepDrainDone = true;
    }
  }
  else {
    // Vidange faite → pompe toujours arrêtée
    setPump(false);
    // Si A remonte, on repart direct en douche
    if (A_haut) {
      currentMode      = MODE_SHOWER;
      lastActivityTime = now;
      highLevelStartTime = now;
    }
  }

  // Clignotement vert permanent en veille
  if (now - lastBlinkTime >= SLEEP_BLINK_INTERVAL) {
    blinkOn = !blinkOn;
    lastBlinkTime = now;
  }
}

// ----- SHOWER MODE -----
void handleShowerMode(unsigned long now, bool A_haut, bool B_haut) {
  bool rising = (A_haut && !B_haut && !pumpState);

  // Démarrage / arrêt pompe
  if (!pumpState && A_haut && B_haut) {
    setPump(true);
    lastActivityTime    = now;
    yellowBlinkState    = true;
    lastYellowBlinkTime = now;
  }
  else if (pumpState && !A_haut) {
    setPump(false);
    lastActivityTime = now;
    yellowBlinkState = false;
  }

  // Erreur si eau bloquée trop longtemps
  if (!errorState
      && A_haut && B_haut
      && !pumpState
      && (now - highLevelStartTime >= ERROR_TIMEOUT)) {
    errorState = true;
  }

  // Timeout douche → retour en veille
  if (now - lastActivityTime >= SHOWER_TIMEOUT) {
    currentMode     = MODE_SLEEP;
    errorState      = false;
    setPump(false);
    sleepDrainDone  = false;    // on réarme la vidange pour la prochaine veille
    blinkOn         = false;
    lastBlinkTime   = now;
  }
}

// ----- POMPE -----
void setPump(bool on) {
  pumpState = on;
  digitalWrite(relayPin, on ? HIGH : LOW);
}

// ----- LEDs -----
void updateLEDs(unsigned long now, bool A_haut, bool B_haut) {
  // Recalcul de rising pour LED jaune fixe
  bool rising = (A_haut && !B_haut && !pumpState);

  // Clignotement jaune si la pompe tourne
  if (pumpState && (now - lastYellowBlinkTime >= YELLOW_BLINK_INTERVAL)) {
    yellowBlinkState    = !yellowBlinkState;
    lastYellowBlinkTime = now;
  }

  // Vert : toujours allumé en douche, clignote en veille
  bool green  = (currentMode == MODE_SHOWER)
                || (currentMode == MODE_SLEEP && blinkOn);

  // Jaune : montée ou pompe en clignote
  bool yellow = rising || (pumpState && yellowBlinkState);

  // Rouge : erreur
  bool red    = errorState;

  digitalWrite(greenLEDPin,  green  ? HIGH : LOW);
  digitalWrite(yellowLEDPin, yellow ? HIGH : LOW);
  digitalWrite(redLEDPin,    red    ? HIGH : LOW);
}
