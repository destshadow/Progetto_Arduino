#include <DmxSimple.h>

void setup() {
  Serial.begin(250000); // Inizializza la comunicazione seriale
  DmxSimple.usePin(3); // Specifica il pin per l'uscita DMX
  DmxSimple.maxChannel(4); // Configura il numero massimo di canali DMX

}

void loop() {
  if (Serial.available() > 0) {
    // Legge i dati ricevuti
    String input = Serial.readStringUntil('\n');
    //Serial.println(input);

    // Trova la posizione del separatore ':'
    int separatore = input.indexOf(':');
    if (separatore != -1) {
      // Estrai canale e valore come sottostringhe
      int canale = input.substring(0, separatore).toInt();
      int valore = input.substring(separatore + 1).toInt();

      // Imposta il valore DMX per il canale ricevuto
      if (canale > 0 && canale <= 512) {
        DmxSimple.write(canale, valore);
        //Serial.print("Canale ");
        //Serial.print(canale);
        //Serial.print(" impostato a ");
        //Serial.println(valore);
      }
    }
  }
}
