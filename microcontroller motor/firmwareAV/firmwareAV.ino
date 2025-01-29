#include <Servo.h>
int debug = 0;

char moveCommand = 0;

int moveSpeed = 0;
int moveSteering = 0;
int moveLidar = 0;

int moveLidarPwm = 122;

int init_angle = 95; //center the vehicle's front axle
int init_speed = 90; //center the vehicle's velocity 
int flagMode = 0; //manual - 0, autonomous - 1

int pwmLidarPin = 6;

int steeringPinOutput = 2;
int steeringPinInput = 3;
int led = 13;

int speedPinOutput = 9;
int speedPinInput = 8;

int countSteering = 0;
int counterVelocity = 0;

float offsetDutyCycle = 0.15;
int afterChangeMode = 3;

Servo servoVelocity;
Servo servoSteering;

void setup() {
  // steering
  pinMode(steeringPinOutput, OUTPUT);
  pinMode(steeringPinInput, INPUT);

  //speed
  pinMode(speedPinOutput, OUTPUT);
  pinMode(speedPinInput, INPUT);

  //notification led -> mode
  pinMode(led, OUTPUT);

  Serial.begin(115200); //can try 230400, 460800
   
  pinMode(pwmLidarPin, OUTPUT);
  analogWrite(pwmLidarPin, moveLidarPwm);
}

void loop() {
  //read when serial port is attached and avabile new comand
  if (Serial.available() > 0) {
    moveCommand = Serial.read();
    //Debug
    if (debug == 1) {
      Serial.print("Move: ");
      Serial.println(moveCommand);
    }
    
    //set mode
    if (moveCommand == 'm') {
      char str[2];
      str[0] = Serial.read();
      str[1] = '\0';
      int mode = atoi(str);
      if (debug == 1) {
        Serial.print("Mode: ");
        Serial.println(1);
      }
      if(mode == 1){
        if(debug == 1){
          Serial.println("Setting autonomous mode");
        }
        set_autonomous_mode();
      }else if (mode == 0){
        if(debug == 1){
          Serial.println("Setting manual mode");
        }
        set_manual_mode();
      }

    //command
    } else if (moveCommand == 'c') {
      char strVelocity[4];
      strVelocity[0] = Serial.read();
      strVelocity[1] = Serial.read();
      strVelocity[2] = Serial.read();
      strVelocity[3] = '\0';
      moveSpeed = atoi(strVelocity);
      //Debug
      if (debug == 1) {
        Serial.print("Velocity: ");
        Serial.println(moveSpeed);
      }
      char strSteering[4];
      strSteering[0] = Serial.read();
      strSteering[1] = Serial.read();
      strSteering[2] = Serial.read();
      strSteering[3] = '\0';
      moveSteering = atoi(strSteering);
      //Debug
      if (debug == 1) {
        Serial.print("Steering: ");
        Serial.println(moveSteering);
      }

      //set speed
      if (moveSpeed > 0) {
        servoVelocity.write(100 + moveSpeed * 0.45); //convert to PWM duty cycle
      } else if (moveSpeed == 0) {
        servoVelocity.write(init_speed);
      } else if (moveSpeed < 0) {
        servoVelocity.write(85 - abs(moveSpeed) * 0.45); //convert to PWM duty cycle
      }

      //set velocity
      if (moveSteering > 0) {
        servoSteering.write(init_angle - moveSteering * 0.45); //convert to PWM duty cycle
      } else if (moveSteering == 0) {
        servoSteering.write(init_angle);
      } else if (moveSteering < 0) {
        servoSteering.write(init_angle - moveSteering * 0.45); //convert to PWM duty cycle
      }

    //set lidar rotation speed   
    }else if (moveCommand == 'l') {
      char strLidar[3];
      strLidar[0] = Serial.read();
      strLidar[1] = Serial.read();
      strLidar[2] = '\0';

      moveLidar = atoi(strLidar);
      moveLidarPwm = int(moveLidar*255/100); 

      if (debug == 1) {
        Serial.print("Lidar: ");
        Serial.println(moveLidarPwm);
      }
      analogWrite(pwmLidarPin, moveLidarPwm);

    //error on serial port or not avabile comand
    }else {
      moveCommand = 'g';
    }
  }

  //write command to servos with manual control
  if (flagMode == 0) {
    // steering
    int valsteering = digitalRead(steeringPinInput); //read value from controler
    digitalWrite(steeringPinOutput, valsteering); //write value to servo

    // speed
    int valSpeed = digitalRead(speedPinInput); //read value from controler
    digitalWrite(speedPinOutput, valSpeed); // write value to H-bridge
  
  //control via computer
  }
  else{
    //measure the pwm on steering controler
    unsigned long highTime = pulseIn(steeringPinInput, HIGH);
    unsigned long lowTime = pulseIn(steeringPinInput, LOW);
    unsigned long cycleTime = highTime + lowTime;
    float dutyCycle = (float)highTime / float(cycleTime);
    if(debug == 1){
      Serial.print("Stearing: ");
      Serial.println(dutyCycle);
    }

    //how many times there is change in steering
    if (0.02 < abs(dutyCycle-offsetDutyCycle)) {
      countSteering++;
    } else{
      countSteering = 0;
    }
    
    //if measure 3times change in steering set the manual mode (becouse of interferences)      
    if (countSteering > afterChangeMode) {
      countSteering = 0;
      set_manual_mode();
      if (debug == 1) {
        Serial.println("set to manual cotrol duto steering");
      }
    }

    //how many times there is change in velocity
    highTime = pulseIn(speedPinInput, HIGH);
    lowTime = pulseIn(speedPinInput, LOW);
    cycleTime = highTime + lowTime;
    dutyCycle = (float)highTime / float(cycleTime);
    if(debug == 1){
      Serial.print("Velocity: ");
      Serial.println(dutyCycle);
    }

    //if measure 3times velocity in steering set the manual mode (becouse of interferences)  
    if (0.02 < abs(dutyCycle-offsetDutyCycle)) {
      counterVelocity++;
    }else{
      counterVelocity = 0; 
    }

    if( counterVelocity > afterChangeMode){
      counterVelocity = 0; 
      set_manual_mode();
      if (debug == 1) {
        Serial.println("Set to manual control duto speed");
      }
    }

      //1000 -> to participate only valid values
//    if (moveSpeed > 0 && moveSpeed <= 100) {
//      servoVelocity.write(100 + moveSpeed * 0.45); //convert to PWM duty cycle
//      moveSpeed = 1000;
//    } else if (moveSpeed == 0) {
//      servoVelocity.write(init_speed);
//      moveSpeed = 1000;
//    } else if (moveSpeed >= -100 && moveSpeed < 0) {
//      servoVelocity.write(85 - abs(moveSpeed) * 0.45); //convert to PWM duty cycle
//      moveSpeed = 1000;
//    }

//    //1000 -> to participate only valid values
//    if (moveSteering > 0 && moveSteering <= 100) {
//      servoSteering.write(init_angle - moveSteering * 0.45); //convert to PWM duty cycle
//      moveSteering = 1000;
//    } else if (moveSteering == 0) {
//      servoSteering.write(init_angle);
//      moveSteering = 1000;
//    } else if (moveSteering >= -100 && moveSteering < 0) {
//      servoSteering.write(init_angle - moveSteering * 0.45); //convert to PWM duty cycle
//      moveSteering = 1000;
//    }
  }
}

void set_manual_mode(void){
  flagMode = 0;
  moveSpeed = 0;
  servoVelocity.write(init_speed); //center servo
  servoSteering.write(init_angle); //center servo
  servoVelocity.detach(); //detach servo from autonomous mode
  servoSteering.detach(); //detach servo from autonomous mode
  pinMode(speedPinOutput, OUTPUT); //prepare for manual control
  pinMode(steeringPinOutput, OUTPUT); //prepare for manual control
  digitalWrite(led, LOW); //turn off LED
}

void set_autonomous_mode(void){
  flagMode = 1;
  servoVelocity.attach(speedPinOutput); //prepare for autonomous control
  servoSteering.attach(steeringPinOutput); //prepare for autonomous control
  digitalWrite(led, HIGH); //turn on LED
}
