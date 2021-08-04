/*
 * NFS LED.c
 *
 * Created: 06/21/2021 09:55:05
 * Author : Galib
 */ 
#define F_CPU 1000000
#define D4 eS_PORTD4
#define D5 eS_PORTD5
#define D6 eS_PORTD6
#define D7 eS_PORTD7
#define RS eS_PORTC6
#define EN eS_PORTC7

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <time.h>
#include "lcd.h"
#include "24c64.c"

#define LEVEL_MAX 10

enum ProgramState{
	PS_StartScreen,
	PS_GameRunning,
	PS_UpdateLevelAndScore,
	PS_GameOver,
	PS_TESTING
};

enum ObstacleType{
	OT_Car,
	OT_Rectangle,
	OT_Plus,
	OT_RandomStuffIThought,
	OT_UpperTriangle,
	OT_LowerTriangle	
};

struct ObstacleInfo{
	int Row;
	int Column;
	int Index;
	
	enum ObstacleType Obstacletype;
};

volatile enum ProgramState programState;
volatile unsigned char LCD[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
volatile int LeftMoves, RightMoves;
volatile int TotalObstacleOnScreen = 0;
volatile int TestVariable = 0;
volatile int GoriberTimer = 0;
volatile int Whatever;
volatile int ObstacleSpeed;
volatile int ObstacleCounter = 0;
volatile int GoriberBoolean[] = {0,0,0};
volatile struct ObstacleInfo Obstacles[3];
//Game Necessities
volatile int Level;
volatile int Score;
volatile struct ObstacleInfo MainCar;
volatile int PreviousHighScore;
volatile int GoriberSaver;

void Testing(){
	char LevelMessage[] = "Testing Now...";
	char ScoreMessage[] = "Value: 00";
	
	//Read Value
	int Value = EEReadByte(0);
	ScoreMessage[8] = Value + '0';
	
	Lcd4_Set_Cursor(1,1);
	Lcd4_Write_String(LevelMessage);
	Lcd4_Set_Cursor(2,1);
	Lcd4_Write_String(ScoreMessage);
}

void SetLCDBits(int Row, int Column){
	int Temp = 1 << (Row-1);
	int Temp2 = 0xFF;
	Temp = Temp ^ Temp2;
	LCD[Column] = LCD[Column] & Temp;
}

void ResetLCDBits(int Row, int Column){
	int Temp = 1 << (Row-1);
	LCD[Column] = LCD[Column] | Temp;
}

void ShowStartScreen(){
	unsigned char NFSLED[] = {0b11000001, 0b11111011, 0b11110111, 0b11101111, 0b11000001, 0b11111111, 0b11000001, 0b11110101,
							  0b11110101, 0b11110101, 0b11111111, 0b11010001, 0b11010101, 0b11010101, 0b11000101, 0b11111111};
	int Counter;
	for(Counter = 0; Counter < 16; Counter++){
		PORTB = (Counter<<4);
		PORTA = NFSLED[Counter];
	}
}

void ShowNewHighScore(){
	char Message[] = "New High Score!";
	Lcd4_Set_Cursor(1,1);
	Lcd4_Write_String(Message);
	
	char NewScore[] = "High Score: 000";
	NewScore[14] = Score % 10 + '0';
	if(Score > 9){
		NewScore[13] = (Score % 100 - (Score % 10))/10 + '0';
		if(Score > 99){
			NewScore[12] = (Score - (Score % 100))/100 + '0';
		}
	}
	Lcd4_Set_Cursor(2,1);
	Lcd4_Write_String(NewScore);
}

void GameOver(){
	if(GoriberSaver == 0){
		if(PreviousHighScore < Score){
			EEWriteByte(0, Score);
			ShowNewHighScore();
			PORTD &= 0b11111100;
			PORTD |= 0b00000011;
		}
		GoriberSaver = 1;
	}
	unsigned char Oops[] = {0b11000001, 0b11011101, 0b11000001, 0b11111111, 0b11000001, 0b11011101, 0b11000001, 0b11111111,
							0b11000001, 0b11110101, 0b11110001, 0b11111111, 0b11010001, 0b11010101, 0b11010101, 0b11000101};
	int Counter;
	for(Counter = 0; Counter < 16; Counter++){
		PORTB = (Counter<<4);
		PORTA = Oops[Counter];
	}
}

void ShowLevelAndScore(int Level, int Score){
	char LevelMessage[] = "Level: 00";
	char ScoreMessage[] = "Score: 00";
	LevelMessage[8] = (Level%10) + '0';
	LevelMessage[7] = (Level/10) + '0';
	ScoreMessage[8] = (Score%10) + '0';
	ScoreMessage[7] = (Score/10) + '0';
	
	Lcd4_Set_Cursor(1,1);
	Lcd4_Write_String(LevelMessage);
	Lcd4_Set_Cursor(2,1);
	Lcd4_Write_String(ScoreMessage);
	//programState = PS_StartScreen;
}

void GenerateCar(){
	LCD[MainCar.Column] = 0xFF;
	LCD[MainCar.Column+1] = 0xFF;
	LCD[MainCar.Column+2] = 0xFF;
	LCD[MainCar.Column-1] = 0xFF;
	SetLCDBits(MainCar.Row, MainCar.Column);
	SetLCDBits(MainCar.Row+1, MainCar.Column-1);
	SetLCDBits(MainCar.Row-1, MainCar.Column-1);
	SetLCDBits(MainCar.Row+1, MainCar.Column+1);
	SetLCDBits(MainCar.Row-1, MainCar.Column+1);
	SetLCDBits(MainCar.Row, MainCar.Column+2);
}

struct ObstacleInfo GenerateObstacle(){
	int Type = rand()%4;
	struct ObstacleInfo obstacle;
	if(Type==0){
		obstacle.Column = 13;
		obstacle.Row = (rand()%5) + 2;
		obstacle.Obstacletype = OT_Car;
	}else if(Type==1){
		obstacle.Column = 13;
		obstacle.Row = (rand()%5) +2;
		obstacle.Obstacletype = OT_Rectangle;
	}else if(Type==2){
		obstacle.Column = 14;
		obstacle.Row = (rand()%5) + 2;
		obstacle.Obstacletype = OT_Plus;
	}else if(Type==3){
		obstacle.Column = 13;
		obstacle.Row = (rand()%5) + 2;
		obstacle.Obstacletype = OT_RandomStuffIThought;
	}
	
	obstacle.Index = ObstacleCounter;
	ObstacleCounter = (ObstacleCounter+1)%3;
	Obstacles[obstacle.Index] = obstacle;
	GoriberBoolean[obstacle.Index] = 1;
	return obstacle;
}

void GenerateObstacleFromType(struct ObstacleInfo obstacleinfo){
	if(obstacleinfo.Obstacletype==OT_Car){
		int Temp = obstacleinfo.Column+1;
		ResetLCDBits(obstacleinfo.Row, Temp);
		ResetLCDBits(obstacleinfo.Row+1, Temp-1);
		ResetLCDBits(obstacleinfo.Row-1, Temp-1);
		ResetLCDBits(obstacleinfo.Row+1, Temp+1);
		ResetLCDBits(obstacleinfo.Row-1, Temp+1);
		ResetLCDBits(obstacleinfo.Row, Temp+2);
		
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column+1);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column+1);
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column+2);
	}else if(obstacleinfo.Obstacletype==OT_Rectangle){
		int Temp = obstacleinfo.Column+1;
		
		ResetLCDBits(obstacleinfo.Row, Temp-1);
		ResetLCDBits(obstacleinfo.Row, Temp+1);
		ResetLCDBits(obstacleinfo.Row-1, Temp);
		ResetLCDBits(obstacleinfo.Row-1, Temp-1);
		ResetLCDBits(obstacleinfo.Row-1, Temp+1);
		ResetLCDBits(obstacleinfo.Row+1, Temp);
		ResetLCDBits(obstacleinfo.Row+1, Temp-1);
		ResetLCDBits(obstacleinfo.Row+1, Temp+1);
		
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column+1);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column+1);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column+1);
	}else if(obstacleinfo.Obstacletype==OT_Plus){
		int Temp = obstacleinfo.Column+1;
		
		ResetLCDBits(obstacleinfo.Row, Temp);
		ResetLCDBits(obstacleinfo.Row, Temp+1);
		ResetLCDBits(obstacleinfo.Row, Temp-1);
		ResetLCDBits(obstacleinfo.Row-1, Temp);
		ResetLCDBits(obstacleinfo.Row+1, Temp);
		
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column+1);
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column);
	}else if(obstacleinfo.Obstacletype==OT_RandomStuffIThought){
		int Temp = obstacleinfo.Column+1;
		
		ResetLCDBits(obstacleinfo.Row, Temp);
		ResetLCDBits(obstacleinfo.Row, Temp+2);
		ResetLCDBits(obstacleinfo.Row-1, Temp-1);
		ResetLCDBits(obstacleinfo.Row-1, Temp);
		ResetLCDBits(obstacleinfo.Row-1, Temp+1);
		ResetLCDBits(obstacleinfo.Row-1, Temp+2);
		ResetLCDBits(obstacleinfo.Row+1, Temp-1);
		ResetLCDBits(obstacleinfo.Row+1, Temp);
		ResetLCDBits(obstacleinfo.Row+1, Temp+1);
		ResetLCDBits(obstacleinfo.Row+1, Temp+2);
		
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row, obstacleinfo.Column+2);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column+1);
		SetLCDBits(obstacleinfo.Row-1, obstacleinfo.Column+2);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column-1);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column+1);
		SetLCDBits(obstacleinfo.Row+1, obstacleinfo.Column+2);
	}
}

void GoriberLevelUp(){
	ObstacleSpeed-=50;
}

void AnimateObstacle(){
	int Counter;
	if(GoriberTimer >= ObstacleSpeed){
		for(Counter=0; Counter<3; Counter++){
			if(GoriberBoolean[Counter] == 1){
				if(Obstacles[Counter].Column != -3){
					Obstacles[Counter].Column--;
				}
				else{
					GoriberBoolean[Counter] = 0;
					Score++;
					if(Score%3==0 && Level < LEVEL_MAX){
						Level++;
						GoriberLevelUp();
					}
					ShowLevelAndScore(Level, Score);
				}
			}
		}
		GoriberTimer = 0;
		Whatever++;
	}
}

void ShowGameScreen(){
	int Counter;
	for(Counter = 0; Counter < 16; Counter++){
		PORTB = (Counter<<4);
		PORTA = LCD[Counter];
		_delay_ms(3);
		GoriberTimer += 3;
	}
}

void MoveLeft(){
	if(LeftMoves != 0){
		MainCar.Row--;
		LeftMoves--;
		RightMoves++;
	}
}

void MoveRight(){
	if(RightMoves != 0){
		MainCar.Row++;
		RightMoves--;
		LeftMoves++;
	}
}

int HasCollided(struct ObstacleInfo mainCar, struct ObstacleInfo obstacle){
	int XDistance = abs(mainCar.Column - obstacle.Column);
	int YDistance = abs(mainCar.Row - obstacle.Row);
	
	if(obstacle.Obstacletype==OT_Car){
		if((XDistance == 3 || XDistance == 1) && YDistance == 2){
			return 0;
		}else if(XDistance == 3 && YDistance == 0){
			return 0;	
		}else if(XDistance < 4 && YDistance < 3){
			return 1;
		}else{
			return 0;
		}
	}else if(obstacle.Obstacletype==OT_Plus){
		if((XDistance == 2 || XDistance == 0) && YDistance == 2){
			return 0;
		}else if(XDistance < 3 && YDistance < 3){
			return 1;
		}else{
			return 0;
		}
	}else if(obstacle.Obstacletype==OT_RandomStuffIThought){
		if(XDistance == 3 && YDistance == 0){
			return 0;
		}else if(XDistance == 3 && YDistance == 2){
			return 0;
		}else if(XDistance < 4 && YDistance < 3){
			return 1;
		}else{
			return 0;
	}
	}else if(obstacle.Obstacletype==OT_Rectangle){
		if(XDistance == 3 && YDistance == 2){
			return 0;
		}else if(XDistance < 4 && YDistance < 3){
			return 1;
		}else{
			return 0;
		}
	}
	return 0;
}

int GoriberCollider(){
	int Counter;
	for(Counter=0; Counter<3; Counter++){
		if(HasCollided(MainCar, Obstacles[Counter])){
			return 1;
		}
	}
	return 0;
}

void RunGame(){
	int Counter = 0;
	while(programState == PS_GameRunning){
		if(GoriberCollider()){
			programState = PS_GameOver;
		}
		GenerateCar();
		if(Whatever == 10){
			GenerateObstacle();
			Whatever = 0;
		}
		for(Counter=0; Counter<3; Counter++){
			if(GoriberBoolean[Counter]){
				GenerateObstacleFromType(Obstacles[Counter]);
			}
		}
		ShowGameScreen();
		AnimateObstacle();
	}
}

ISR(INT2_vect){
	if(programState == PS_StartScreen){
		programState = PS_GameRunning;
	}else if(programState == PS_GameOver){
		programState = PS_GameRunning;
	}else if(programState == PS_TESTING){
		TestVariable++;
		EEWriteByte(0, TestVariable);
	}
}

ISR(INT1_vect){
	MoveLeft();
}

ISR(INT0_vect){
	MoveRight();
}

int main(void)
{
	srand(time(0));
	DDRA = 0xFF;
	DDRB = 0b11111000;
	DDRC = 0xFF;
	DDRD = 0b11110011;
	int Counter;
	
	//Interrupt Necessities
	PORTB = PORTB | (1<<2);
	GICR = (1<<INT2);
	GICR = GICR | (1<<INT1);
	GICR = GICR | (1<<INT0);
	MCUCSR = MCUCSR | 0b00000000;
	MCUCR = MCUCR | 00001000;
	MCUCR = MCUCR & 11111011;
	sei();
	
	//Initializations
	Lcd4_Init(); //LCD 
	EEOpen(); //EEPROM
	for(Counter=0; Counter<16; Counter++){
		LCD[Counter] = 0xFF;
	}
	
	MainCar.Column = 1;
	MainCar.Row = 4;
	MainCar.Obstacletype = OT_Car;
	
	//We Will store Highest Score On Address 0 of ROM
	programState = PS_StartScreen;
	//EEWriteByte(0, 0);
	if(EEReadByte(0) == 0xFF){
		EEWriteByte(0, 0);
	}
	PreviousHighScore = EEReadByte(0);
	LeftMoves = 2;
	RightMoves = 3;
	Level = 1;
	Score = 0;
	Whatever = 10;
	ObstacleSpeed = 600;
	GoriberSaver = 0;
	ShowLevelAndScore(Level, Score);
	while (1) 
    {
		if(programState == PS_StartScreen){
			ShowStartScreen();
		}
		if(programState == PS_GameOver){
			GameOver();
		}
		if(programState == PS_GameRunning){
			RunGame();
		}
		if(programState == PS_TESTING){
			Testing();
		}
    }
}

