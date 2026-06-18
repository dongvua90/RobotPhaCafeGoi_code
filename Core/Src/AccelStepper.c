
#include "AccelStepper.h"


void AccelStepper_init(struct AccelStepperData * AccelMotor,TIM_HandleTypeDef htim,uint16_t startPosition, uint32_t maxSpeed,uint32_t maxAccel){
  //khoi tao thu vien
	AccelMotor->_currentPos = 0;
	AccelMotor->_targetPos = 0;
	AccelMotor->_speed = 0.0f;
	AccelMotor->_maxSpeed = 1.0f;
	AccelMotor->_acceleration = 0.0f;
	AccelMotor->_sqrt_twoa = 1.0f;
	AccelMotor->_stepInterval = 0;
	AccelMotor->_lastStepTime = 0;
	AccelMotor->_n = 0;
	AccelMotor->_c0 = 0.0f;
	AccelMotor->_cn = 0.0f;
	AccelMotor->_cmin = 1.0f;
	AccelMotor->_direction = DIRECTION_CCW;
	setMaxSpeed(AccelMotor,maxSpeed);  
	setAcceleration(AccelMotor,maxAccel);
	AccelMotor->_currentPos = startPosition;  //setup vi tri ban dau
	AccelMotor->_targetPos=AccelMotor->_currentPos;
	computeNewSpeed(AccelMotor);  //tinh toan buoc dau tien
	run(AccelMotor);          
  if(AccelMotor->usingTimChanelN==ON) 	
		HAL_TIMEx_PWMN_Start_IT(&htim,AccelMotor->TIM_CHANEL); 
	else
	HAL_TIM_PWM_Start_IT(&htim,AccelMotor->TIM_CHANEL);  //khoi dong TIMER tao xung STEP
	HAL_GPIO_WritePin(AccelMotor->GPIO_PORT_Enable,AccelMotor->GPIO_PIN_Enable,GPIO_PIN_SET);
}

void setMaxSpeed(struct AccelStepperData * AccelMotor,float speed)
{
    if (speed < 0.0f)
       speed = -speed;
    if (AccelMotor->_maxSpeed != speed)
    {
	AccelMotor->_maxSpeed = speed;
	AccelMotor->_cmin = 1000000.0f / speed;
	// Recompute _n from current speed and adjust speed if accelerating or cruising
	if (AccelMotor->_n > 0)
	{
	    AccelMotor->_n = (long)((AccelMotor->_speed * AccelMotor->_speed) / (2.0f * AccelMotor->_acceleration)); // Equation 16
	    computeNewSpeed(AccelMotor);
	}
    }
}
void setAcceleration(struct AccelStepperData * AccelMotor,float acceleration)
{
    if (acceleration == 0.0f)
	return;
    if (acceleration < 0.0f)
      acceleration = -acceleration;
    if (AccelMotor->_acceleration != acceleration)
    {
	// Recompute _n per Equation 17
	AccelMotor->_n = AccelMotor->_n * (AccelMotor->_acceleration / acceleration);
	// New c0 per Equation 7, with correction per Equation 15
	AccelMotor->_c0 = 0.676f * sqrt(2.0f / acceleration) * 1000000.0f; // Equation 15
	AccelMotor->_acceleration = acceleration;
	//computeNewSpeed(AccelMotor);
    }
}

void computeNewSpeed(struct AccelStepperData * AccelMotor)  //tinh toan 1 toc do moi
{
  long distanceTo = distanceToGo(AccelMotor); // +ve is clockwise from curent location - duong neu cung chieu kim dong ho tu vi tri hien tai

  long stepsToStop = (long)((AccelMotor->_speed * AccelMotor->_speed) / (2.0f * AccelMotor->_acceleration)); // Equation 16 - phuong trinh 16 theo giai thuat By David Austin

  if (distanceTo == 0 && stepsToStop <= 1){
	// We are at the target and its time to stop -  da den vi tri muc tieu va bay gio dung lai
	  AccelMotor->_stepInterval = 0;
	  AccelMotor->_speed = 0.0f;
	  AccelMotor->_n = 0;
	  return;
  }

  if (distanceTo > 0){
	  // We are anticlockwise from the target  -  chung ta dang o vi tri nguoc chieu kim dong ho 
	  // Need to go clockwise from here, maybe decelerate now  -  can quay theo chieu kim dong ho, co th giam toc ngay
	  if (AccelMotor->_n > 0){
	    // Currently accelerating, need to decel now? Or maybe going the wrong way?
			// hien tai dang tang toc,can giam toc bay ngay? hoac co the di sai huong
	    if ((stepsToStop >= distanceTo) || AccelMotor->_direction == DIRECTION_CCW)
		     AccelMotor->_n = -stepsToStop; // Start deceleration - bat dau giam toc
	  }
	  else if (AccelMotor->_n < 0){
	    // Currently decelerating, need to accel again? - hien tai dang giam toc ,can tang toc lai
	    if ((stepsToStop < distanceTo) && AccelMotor->_direction == DIRECTION_CW)
		     AccelMotor->_n = -AccelMotor->_n; // Start accceleration  //bat dau tang toc
	  }
  }
  else if (distanceTo < 0){
	// We are clockwise from the target - chung ta dang o vi tri cung chieu kim dong ho
	// Need to go anticlockwise from here, maybe decelerate - can quay theo chieu nguoc lai ,co the giam toc
	   if (AccelMotor->_n > 0){
	      // Currently accelerating, need to decel now? Or maybe going the wrong way?
			  // hien dang tang toc,can giam toc ngay? hoac dang di sai huong?
	      if ((stepsToStop >= -distanceTo) || AccelMotor->_direction == DIRECTION_CW)
		        AccelMotor->_n = -stepsToStop; // Start deceleration - bat dau giam toc
	   }
	   else if (AccelMotor->_n < 0){
	       // Currently decelerating, need to accel again?
			   // hien dang giam toc, can tang toc lai?
	       if ((stepsToStop < -distanceTo) && AccelMotor->_direction == DIRECTION_CCW)
		        AccelMotor->_n = -AccelMotor->_n; // Start accceleration  //bat dau tang toc
	   }
  }
    // Need to accelerate or decelerate - can tang toc hoac giam toc
  if (AccelMotor->_n == 0){
	   // First step from stopped  -  buoc dau tien tu vi tri stop
	   AccelMotor->_cn = AccelMotor->_c0;
	   AccelMotor->_direction = (distanceTo > 0) ? DIRECTION_CW : DIRECTION_CCW;
  }
	else{
	   // Subsequent step. Works for accel (n is +_ve) and decel (n is -ve).
		 //buoc tiep theo. hoat dong cho tang toc (n la duong) va giam toc (n la am)
	   AccelMotor->_cn = AccelMotor->_cn - ((2.0f * AccelMotor->_cn) / ((4.0f * AccelMotor->_n) + 1)); // Equation 13 - phuong trinh 13 
	   AccelMotor->_cn = max(AccelMotor->_cn, AccelMotor->_cmin);
  }
  AccelMotor->_n++;
  AccelMotor->_stepInterval = AccelMotor->_cn;
  AccelMotor->_speed = 1000000.0f / AccelMotor->_cn;
  if (AccelMotor->_direction == DIRECTION_CCW) AccelMotor->_speed = -AccelMotor->_speed;
}

long distanceToGo(struct AccelStepperData * AccelMotor)  //tra lai khoang cach giua vi tri muc tieu va hien tai
{
    return AccelMotor->_targetPos - AccelMotor->_currentPos;
}

void run(struct AccelStepperData * AccelMotor)
{
	if(AccelMotor->isStop){
		switch(AccelMotor->TIM_CHANEL){
			case TIM_CHANNEL_1: AccelMotor->USER_TIMER->CCR1=0; break;
			case TIM_CHANNEL_2: AccelMotor->USER_TIMER->CCR2=0; break;
			case TIM_CHANNEL_3: AccelMotor->USER_TIMER->CCR3=0; break;
			case TIM_CHANNEL_4: AccelMotor->USER_TIMER->CCR4=0; break;
		}
		return;
	}
  if(runSpeed(AccelMotor)){
	computeNewSpeed(AccelMotor);
	if(AccelMotor->_stepInterval==0){
			switch(AccelMotor->TIM_CHANEL){
				case TIM_CHANNEL_1: AccelMotor->USER_TIMER->CCR1=0; break;
				case TIM_CHANNEL_2: AccelMotor->USER_TIMER->CCR2=0; break;
				case TIM_CHANNEL_3: AccelMotor->USER_TIMER->CCR3=0; break;
				case TIM_CHANNEL_4: AccelMotor->USER_TIMER->CCR4=0; break;
			}
//			AccelMotor->sumComplete++;
	}else{
	    AccelMotor->USER_TIMER->ARR=(uint16_t)AccelMotor->_stepInterval;
//		if((distanceToGo(AccelMotor)>3) || (distanceToGo(AccelMotor)>-3)) AccelMotor->sumComplete =0;
	}
  }else if((distanceToGo(AccelMotor)>0) || (distanceToGo(AccelMotor)<0)){
		computeNewSpeed(AccelMotor);
		if(AccelMotor->_stepInterval==0){
				switch(AccelMotor->TIM_CHANEL){
				case TIM_CHANNEL_1: AccelMotor->USER_TIMER->CCR1=0; break;
				case TIM_CHANNEL_2: AccelMotor->USER_TIMER->CCR2=0; break;
				case TIM_CHANNEL_3: AccelMotor->USER_TIMER->CCR3=0; break;
				case TIM_CHANNEL_4: AccelMotor->USER_TIMER->CCR4=0; break;
			}
//				AccelMotor->sumComplete++;
		}
	  else{
	      AccelMotor->USER_TIMER->ARR=(uint16_t)AccelMotor->_stepInterval;
//			  if((distanceToGo(AccelMotor)>3) || (distanceToGo(AccelMotor)>-3)) AccelMotor->sumComplete =0;
		}
	}
//	if(AccelMotor->sumComplete>50) AccelMotor->isComplete=true;
}

char runSpeed(struct AccelStepperData * AccelMotor)
{
    // Dont do anything unless we actually have a step interval
	  // khong lam gi neu nhu gia tri _stepInterval = 0
  if (!AccelMotor->_stepInterval){
		switch(AccelMotor->TIM_CHANEL){
			case TIM_CHANNEL_1: AccelMotor->USER_TIMER->CCR1=0; break;
			case TIM_CHANNEL_2: AccelMotor->USER_TIMER->CCR2=0; break;
			case TIM_CHANNEL_3: AccelMotor->USER_TIMER->CCR3=0; break;
			case TIM_CHANNEL_4: AccelMotor->USER_TIMER->CCR4=0; break;
		}
//		AccelMotor->isComplete = 1;
		return 0;
	}
	
	if (AccelMotor->_direction == DIRECTION_CW){ // Clockwise    
	    AccelMotor->_currentPos += 1;
	  }
	else{
	    // Anticlockwise
	    AccelMotor->_currentPos -= 1;
	  }
	step(AccelMotor);  //tao 1 step pulse

	return 1;
}

void step(struct AccelStepperData * AccelMotor)
{
	if(AccelMotor->_direction) 
		HAL_GPIO_WritePin(AccelMotor->GPIO_PORT_Dir,AccelMotor->GPIO_PIN_Dir,GPIO_PIN_RESET); 
	else 
		HAL_GPIO_WritePin(AccelMotor->GPIO_PORT_Dir,AccelMotor->GPIO_PIN_Dir,GPIO_PIN_SET);
	if(AccelMotor->enable==1){
			HAL_GPIO_WritePin(AccelMotor->GPIO_PORT_Enable,AccelMotor->GPIO_PIN_Enable,GPIO_PIN_SET);
	}
	switch(AccelMotor->TIM_CHANEL){
			case TIM_CHANNEL_1: AccelMotor->USER_TIMER->CCR1=30; break;
			case TIM_CHANNEL_2: AccelMotor->USER_TIMER->CCR2=30; break;
			case TIM_CHANNEL_3: AccelMotor->USER_TIMER->CCR3=30; break;
			case TIM_CHANNEL_4: AccelMotor->USER_TIMER->CCR4=30; break;
		}
//	AccelMotor->isComplete = 0;
}

char isRunning(struct AccelStepperData * AccelMotor)
{
    return !(AccelMotor->_speed == 0.0f && AccelMotor->_targetPos == AccelMotor->_currentPos);
}
void setCurentPos(struct AccelStepperData * AccelMotor,long position){
 AccelMotor->_currentPos=position;
}

void moveTo(struct AccelStepperData * AccelMotor,long absoluted)
{
	AccelMotor->isStop = false;
	if(AccelMotor->_targetPos != absoluted){
	   AccelMotor->_targetPos = absoluted;
	   computeNewSpeed(AccelMotor);
//		AccelMotor->USER_TIMER->CNT=0;
  }
}
void setSpeed(struct AccelStepperData * AccelMotor,float speed)
{
    if (speed == AccelMotor->_speed)
        return;
    speed = constrain(speed, -AccelMotor->_maxSpeed, AccelMotor->_maxSpeed);
    if (speed == 0.0f)
	AccelMotor->_stepInterval = 0;
    else
    {
	AccelMotor->_stepInterval = fabs(1000000.0f / speed);
	AccelMotor->_direction = (speed > 0.0f) ? DIRECTION_CW : DIRECTION_CCW;
    }
    AccelMotor->_speed = speed;
}
void enableStepper(struct AccelStepperData * AccelMotor,enum boolean onOff){
	if(onOff==OFF) 
		HAL_GPIO_WritePin(AccelMotor->GPIO_PORT_Enable,AccelMotor->GPIO_PIN_Enable,GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(AccelMotor->GPIO_PORT_Enable,AccelMotor->GPIO_PIN_Enable,GPIO_PIN_RESET);
}

