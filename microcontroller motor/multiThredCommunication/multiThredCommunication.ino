#include <Arduino.h>
#include <IntervalTimer.h>

IntervalTimer timer1, timer2;
volatile int sharedCounter = 0;
volatile bool lock = false;  // Jednoduchý semafor

void lockSemaphore() {
  while (lock);  // Čeká, dokud není zámek uvolněn
  lock = true;
}

void unlockSemaphore() {
  lock = false;
}

void task1() {
  lockSemaphore();
  sharedCounter++;
  Serial.print("Task 1: ");
  Serial.println(sharedCounter);
  unlockSemaphore();
}

void task2() {
  lockSemaphore();
  sharedCounter += 1;
  Serial.print("Task 2: ");
  Serial.println(sharedCounter);
  unlockSemaphore();
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  timer1.begin(task1, 1000000);
  timer2.begin(task2, 3000000);
}

void loop() {
  // Další logika
}
