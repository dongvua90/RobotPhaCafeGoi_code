
#ifndef AccelStepper_h
#define AccelStepper_h

#include "main.h"
#include <math.h>
#include "stdbool.h"

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

enum Direction{
    DIRECTION_CCW=0,
    DIRECTION_CW=1
};

enum boolean{
	ON =1,
	OFF=0,
	TRUE=1,
	FALSE=0
};


#define SEGMENT_BUFFER_SIZE 6

typedef struct {  
  uint8_t direction;
  uint16_t step_interval;
} st_block_t;


struct AccelStepperData {
	long           _targetPos;     // Steps
	float          _speed;         // Steps per second
	float          _maxSpeed;
	float          _acceleration;
	float          _sqrt_twoa;     // Precomputed sqrt(2*_acceleration)
	unsigned long  _stepInterval;
	unsigned long  _lastStepTime;
	long _n;	        //bo dem buoc de tinh toan toc do    
	float _c0;        // Initial step size in microseconds
	float _cn;        // Last step size in microseconds
	float _cmin;      // at max speed// Min step size in microseconds based on maxSpeed
	char _direction;  // 1 == CW
	long _currentPos; // Steps
	int16_t sumComplete;
	bool isComplete;
	bool isStop;
	char enable;
		//IO 
	GPIO_TypeDef* GPIO_PORT_Dir;
	//GPIO_TypeDef* GPIO_PORT_Pulse;
	GPIO_TypeDef* GPIO_PORT_Enable;
	uint16_t GPIO_PIN_Dir;
	//uint16_t GPIO_PIN_Pulse;
	uint16_t GPIO_PIN_Enable;
	TIM_TypeDef * USER_TIMER;
	uint32_t TIM_CHANEL;
	char usingTimChanelN;
};



void AccelStepper_init(struct AccelStepperData * AccelMotor,TIM_HandleTypeDef htim,uint16_t startPosition, uint32_t maxSpeed,uint32_t maxAccel);
void setMaxSpeed(struct AccelStepperData * AccelMotor,float speed);
void setAcceleration(struct AccelStepperData * AccelMotor,float acceleration);
void computeNewSpeed(struct AccelStepperData * AccelMotor); 
long distanceToGo(struct AccelStepperData * AccelMotor); 
void run(struct AccelStepperData * AccelMotor);
char runSpeed(struct AccelStepperData * AccelMotor);
void step(struct AccelStepperData * AccelMotor);
char isRunning(struct AccelStepperData * AccelMotor);
void setCurentPos(struct AccelStepperData * AccelMotor,long position);
void moveTo(struct AccelStepperData * AccelMotor,long absoluted);
void enableStepper(struct AccelStepperData * AccelMotor,enum boolean onOff);
void setSpeed(struct AccelStepperData * AccelMotor,float speed);

#endif
