#include <Servo.h>
#include <math.h>

IntervalTimer myTimer;
IntervalTimer myTimer2;
IntervalTimer myTimer3;


int debug = 0;

char moveCommand = 0;

int moveSpeed = 0;
int moveSteering = 0;
int moveLidar = 0;

int moveLidarPwm = 122;

int init_angle = 95; //center the vehicle's front axle
int init_speed = 90; //center the vehicle's velocity 
int mode = 0; //manual - 0, autonomous - 1

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

bool lock_mode = false;

float speed = 0.0;
float steer = 0.0;
float speedPWM = 0.15;
float steerPWM = 0.15;


volatile unsigned long startHigh = 0;
volatile unsigned long startAll = 0;
volatile unsigned long pulseHigh = 0;

volatile unsigned long startHigh2 = 0;
volatile unsigned long startAll2 = 0;
volatile unsigned long pulseHigh2 = 0;

/* 
 *interupts
*/

void measureSpeed() {
  if (digitalRead(speedPinInput) == HIGH) {
    // Serial.print("all speed");
    unsigned long all = micros()- startAll; 
    // Serial.println(micros()- startAll);
    float res = (float)pulseHigh/ (float)all;
    // Serial.println(res,5);
    set_speedPWM(res);

    // Serial.println();
    startHigh = micros();
    startAll = micros();
    // Serial.println("high");
  }else{
    // Serial.println("low");
    pulseHigh = micros() - startHigh;
    // Serial.print("high");
    // Serial.println(pulseHigh);

  } 
}

void measureSteer() {
  if (digitalRead(steeringPinInput) == HIGH) {
    // Serial.print("all steer");
    unsigned long all2 = micros()- startAll2; 
    // Serial.println(micros()- startAll);
    float res2 = (float)pulseHigh2/ (float)all2;
    set_steerPWM(res2);

    // Serial.println(res2,5);
    startHigh2 = micros();
    startAll2 = micros();
    // Serial.println("high");
  }else{
    // Serial.println("low");
    pulseHigh2 = micros() - startHigh2;
    // Serial.print("high");
    // Serial.println(pulseHigh);

  } 
}

void setup() {
  Serial.begin(115200);

  // Serial.println("start steer");
  // steering
  pinMode(steeringPinOutput, OUTPUT);
  pinMode(steeringPinInput, INPUT);

  // Serial.println("start speed");
  //speed
  pinMode(speedPinOutput, OUTPUT);
  pinMode(speedPinInput, INPUT);

  //notification led -> mode
  pinMode(led, OUTPUT);


   
  pinMode(pwmLidarPin, OUTPUT);
  analogWrite(pwmLidarPin, moveLidarPwm);

  // Serial.println("start timers");
  prepare_motor();

  // set_autonomous_mode();
  servoVelocity.attach(speedPinOutput); //prepare for autonomous control
  servoSteering.attach(steeringPinOutput); //prepare for autonomous control
  myTimer.begin(read_communcation, 100000);
  // myTimer2.begin(manual_control, 1000000);
  // myTimer3.begin(check_controller, 100000);
  attachInterrupt(digitalPinToInterrupt(speedPinInput), measureSpeed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(steeringPinInput), measureSteer, CHANGE);
  // attachInterrupt(digitalPinToInterrupt(speedPinInput), measureLow, FALLING);
  set_mode(0);

  // servoVelocity.write(110);
  // write_speed(110);
  // servoSteering.write(75); 



}


void loop() {
  int currMode =  get_mode();
  if(currMode == 0){ //manual
    // Serial.println("manual");

    //get speed and steer PWM
    float speed_pwm = get_speedPWM();
    float steer_pwm = get_steerPWM();
    // String set_values2= "v"+String(speed_pwm)+"s"+ String(steer_pwm);
    // Serial.println(set_values2);
    
    //compute angle
    float angle_steer = (float)(steer_pwm - 0.15) * 20 * 45;
    float ms_speed = (speed_pwm - 0.15) * 20 * 1.3;
    // Serial.println("angle steer " +String(angle_steer));
    // Serial.println("speed in ms " +String(ms_speed));
    
    //write to motors
    write_speed(ms_speed);
    write_steer(angle_steer);
    
    //publish odometry
    // String set_values= "v"+String(ms_speed)+"s"+ String(angle_steer/57.2958);
    // Serial.println(set_values);
  }

  else if(currMode == 1){ //autonomous
    // Serial.println("autonomous");

    //get angle and speed
    float new_speed = get_speed();
    float new_steer = get_steer();
    
    //write to motors
    write_speed(new_speed);
    write_steer(new_steer);
  
  }

  // write_steer(-40.0);

  delay(10);
}

void prepare_motor(){
  unsigned long start = micros();
  while((micros()-start)<2000000){
    int valSpeed = digitalRead(speedPinInput); //read value from controler
    digitalWrite(speedPinOutput, valSpeed); // write value to H-bridge
    // Serial.println("Forwarding manual");
  }
}

void check_controller(){
    // Serial.println("start check control");
    // unsigned long highTime = 2;
    // unsigned long lowTime = 8;

    //how many times there is change in velocity
    // unsigned long highTime = pulseIn(speedPinInput, HIGH);
    // unsigned long lowTime = pulseIn(speedPinInput, LOW);
    // unsigned long cycleTime = highTime + lowTime;
    // float dutyCycle = (float)highTime / (float)(cycleTime);
    // set_speedPWM(dutyCycle);
    
    // if(debug == 1){
    //   Serial.print("Velocity: ");
    //   Serial.println(dutyCycle,5);
    // }

    //how many times there is change in velocity
    // highTime = pulseIn(steeringPinInput, HIGH);
    // lowTime = pulseIn(steeringPinInput, LOW);
    // cycleTime = highTime + lowTime;
    // dutyCycle = (float)highTime / (float)(cycleTime);
    
    // if(debug == 1){
    //   Serial.print("Steering: ");
    //   Serial.println(dutyCycle,5);
    // }
    // set_steerPWM(dutyCycle);
    float dutyCycleSteer = get_steerPWM();

    //how many times there is change in steering
    if (0.015 < abs(dutyCycleSteer-offsetDutyCycle)) {
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

    float dutyCycleSpeed = get_speedPWM();
    //if measure 3times velocity in steering set the manual mode (becouse of interferences)  
    if (0.015 < abs(dutyCycleSpeed-offsetDutyCycle)) {
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
}


void read_communcation() {
    // Serial.println("read communication...");

    if (Serial.available()) {  // Pokud jsou data k dispozici
        String line = Serial.readStringUntil('\n');  // Čtení až po Enter
        Serial.print("read line: ");
        Serial.println(line);
        // return;
        
        // read first character
        if (line[0] == 'm')
          receive_mode(line);
        else if (line [0] == 'c')
          receive_command(line);
        else if (line[0] == 'l')
          receive_lidar(line);
    }
}

// void manual_control(){
//   // Serial.println("start manual control");

//   if (get_mode() == 0) {
//     // steering
//     int valsteering = digitalRead(steeringPinInput); //read value from controler
//     digitalWrite(steeringPinOutput, valsteering); //write value to servo

//     // speed
//     int valSpeed = digitalRead(speedPinInput); //read value from controler
//     digitalWrite(speedPinOutput, valSpeed); // write value to H-bridge
  
//   }
// }



void receive_mode(String text){
  Serial.println("receive mode");

  int new_mode = text.substring(1,2).toInt();
  if(new_mode == 1){
    if(debug == 1){
      Serial.println("Setting autonomous mode");
    }
    set_autonomous_mode();
  }
  else if (new_mode == 0){
    if(debug == 1){
      Serial.println("Setting manual mode");
    }
    set_manual_mode();
  }
}

void receive_command(String text){
  Serial.print("receive command: ");  

  int pos = text.indexOf(';');  // Najde pozici oddělovače
  if (pos != -1) {
      //speed
      String speedStr = text.substring(1, pos);     
      float speed = speedStr.toFloat();
      // Serial.print("speed str: ");  
      // Serial.println(speedStr);
      // Serial.print("speed: ");  
      // Serial.println(speed);
      set_speed(speed);

      //steering
      String steerStr = text.substring(pos + 1); 
      float steer = steerStr.toFloat();
      steer = steer * 57.2958;
      // Serial.print("steer str: ");  
      // Serial.println(steerStr);
      // Serial.print("steer: ");  
      // Serial.println(steer);
      set_steer(steer);
  }else{
    Serial.println("ERROR: delimiter not found");
  }
  // Serial.println(text);
  // String speedStr = text.substring(1, 5);
  // float speed = speedStr.toFloat();
  // Serial.print("speed str: ");  
  // Serial.println(speedStr);
  // Serial.print("speed: ");  
  // Serial.println(speed);
  // set_speed(speed);

  // String steerStr = text.substring(5, 10);
  // float steer = steerStr.toFloat();
  // Serial.print("steer str: ");  
  // Serial.println(steerStr);
  // Serial.print("steer: ");  
  // Serial.println(steer);
  // set_steer(steer);
}

void receive_lidar(String text){
  Serial.println("receive lidar");
  String speedStr = text.substring(1, 2);
  int speed = speedStr.toInt();
  set_lidar(speed);
}

void write_speed(float input){
  // servoVelocity.write(70);
  // return;
  // Serial.println("write speed"+ String(input));
  // float newSpeed;
  // servoVelocity.write(input);
  // return; 

  // if (input > 0.1){
  //   newSpeed = -100.17 * pow(input,3) + 298.45 * pow(input,2) - 247.17 * input + 172.48;
  //   Serial.println("newSpeed"+ String(newSpeed) +"forward");
  //   servoVelocity.write(newSpeed);
    
  // }
  if (input <= 0.3 && input >= -0.1){
    servoVelocity.write(init_speed); 
    // Serial.println("newSpeed"+ String(init_speed) +"stop");
  }
  else {
    float newSpeed = 3.29 * pow(input,3) + 0.73 * pow(input,2) + 28.7 * input + 90.74;
    // newSpeed = -25.92 * pow(input,3) -77.1 * pow(input,2) - 34.84 * input + 75.64;
    // Serial.println("newSpeed"+ String(newSpeed));
    servoVelocity.write(newSpeed);
  }

    // servoVelocity.write(-25.92 * pow(input,3) - 77.1 * pow(input,2) - 34.84 * input + 75.64); 

}

void write_steer(float steer){
  // Serial.println("write steer" + String(steer));
  // servoSteering.write(110); 
  // return; 


  servoSteering.write(-0.98 * steer + 95.1); 
}

void set_lidar(int speed){

  int speedPwm = int(speed*255/100); 

  if (debug == 1) {
    Serial.print("Lidar: ");
    Serial.println(speedPwm);
  }

  analogWrite(pwmLidarPin, speedPwm);

}

void set_manual_mode(void){

  set_mode(0);

  set_speed(0);
  set_steer(0);
  // moveSpeed = 0;
  write_speed(0);
  write_steer(0);
  // servoVelocity.write(init_speed); //center servo
  // servoSteering.write(init_angle); //center servo
  // servoVelocity.detach(); //detach servo from autonomous mode
  // servoSteering.detach(); //detach servo from autonomous mode
  // pinMode(speedPinOutput, OUTPUT); //prepare for manual control
  // pinMode(steeringPinOutput, OUTPUT); //prepare for manual control
  digitalWrite(led, LOW); //turn off LED
}

void set_autonomous_mode(void){
  set_mode(1);
  // servoVelocity.attach(speedPinOutput); //prepare for autonomous control
  // servoSteering.attach(steeringPinOutput); //prepare for autonomous control
  digitalWrite(led, HIGH); //turn on LED
}



/*
  getter and setters
*/
void set_speed(float input){
  speed = input;
}

void set_steer(float input){
  steer= input;
}

float get_speed(){
  return speed;
}

float get_steer(){
  return steer;
}


void set_speedPWM(float speed){
  if(speed< 0.09 || speed >0.21){
    // Serial.println("ERROR: invalid speed PWM");
    return;
  }
  if(get_mode()==1 && (speed > 0.16 || speed < 0.14)){
    set_manual_mode();
  }
  speedPWM = speed;
}

float get_speedPWM(){
  return speedPWM;
}

void set_steerPWM(float steer){
  if(steer< 0.09 || steer >0.21){
    // Serial.println("ERROR: invalid steer PWM");
    return;
  }
  if(get_mode()==1 && (steer > 0.16 || steer < 0.14)){
    set_manual_mode();
  }
  steerPWM = steer;
}

float get_steerPWM(){
  return steerPWM;
}

void set_mode(int value){
  if (value < 0 || value > 1){
    Serial.println("ERROR: invalid mode" );
    return;
  }
  mode = value;
}

int get_mode(void){
  return mode;
}
