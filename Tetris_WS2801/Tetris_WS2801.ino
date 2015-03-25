/*
*
* BASED ON:
*
*
* LEDTetrisNeckTie.c
*
* Created: 6/21/2013 
*  Author: Bill Porter
*    www.billporter.info
*
*   AI player code by: Mofidul Jamal
*
*    Code to run a wicked cool LED Tetris playing neck tie. Details:              
*         http://www.billporter.info/2013/06/21/led-tetris-tie/
*
*This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
*To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/ or
*send a letter to Creative Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
*
*/ 



/* Notes
Analog 2 is pin 2
LED is on pin 0
RGB LEDS data is on pin 1
*/

//#include <MemoryFree.h>;

#include <Wire.h>
//#include <WiiChuck.h>

//#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Adafruit_WS2801.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

//Constants for LED strip.  Hardware SPI can be used by commenting out these defines.
//#define LEDDATAPIN 2
//#define LEDCLKPIN 3

#define INLEFT 3
#define INRIGHT 4
#define INROTCCW 5
#define INROTCW 2

//Strips are assumed to be zig-zagged horizontally by default. Uncomment VERT_STRIPS to reverse this behavior
//#define VERT_STRIPS

#define    FIELD_WIDTH 10
#define    FIELD_HEIGHT 10
#define    LEDS FIELD_HEIGHT * FIELD_WIDTH

#define    NUMBEROFRBGLEDSATEND 3 // normal LED's are RGB not RBG!

//constants and initialization
#define COUNTERCLOCKWISE  0
#define DOWN  1
#define RIGHT  2
#define LEFT  3
#define CLOCKWISE 4
#define NOTHING 5
#define NUMBEROFMOVES 6

#define FULL 128
#define WHITE 0xFF
#define OFF 0

//Display Settings
//const short    field_start_x    = 1;
//const short    field_start_y    = 1;
//const short           preview_start_x    = 13;
//const short    preview_start_y    = 1;
//const short    delimeter_x      = 11;
//gameplay Settings
//const bool    display_preview    = 1;
#define tick_delay 600 //game speed
#define max_level 9
#define bounce_delay 50
//weight given to the highest column for ai
#define HIGH_COLUMN_WEIGHT 5
//weight given to the number of holes for ai
#define HOLE_WEIGHT 3

unsigned long  next_tick = 0;
unsigned long bounce_tick = 0;
const PROGMEM uint16_t bricks[][4] = {
  {
    0b0100010001000100,      //1x4 cyan
    0b0000000011110000,
    0b0100010001000100,
    0b0000000011110000
  },
  {
    0b0000010011100000,      //T  purple
    0b0000010001100100,
    0b0000000011100100,
    0b0000010011000100
  },
  {
    0b0000011001100000,      //2x2 yellow
    0b0000011001100000,
    0b0000011001100000,
    0b0000011001100000
  },
  {
    0b0000000011100010,      //L orange
    0b0000010001001100,
    0b0000100011100000,
    0b0000011001000100
  },
  {
    0b0000000011101000,      //inverse L blue
    0b0000110001000100,
    0b0000001011100000,
    0b0000010001000110
  },
  {
    0b0000100011000100,      //S green
    0b0000011011000000,
    0b0000100011000100,
    0b0000011011000000
  },
  {
    0b0000010011001000,      //Z red
    0b0000110001100000,
    0b0000010011001000,
    0b0000110001100000
  },
  {
    0b0000111010101110,      // doughnut
    0b0000111010101110,
    0b0000111010101110,
    0b0000111010101110
  },
  {
    0b0000101001001010,      // X
    0b0000101001001010,
    0b0000101001001010,
    0b0000101001001010,
  }
};
uint8_t brick_count = sizeof(bricks)/sizeof(bricks[0]);
#define EXTRABRICKS 2

//8 bit RGB colors of blocks
//RRRBBBGG
const PROGMEM uint8_t brick_colors[]={
  0b00011111, //cyan
  0b10010000, //purple
  0b11100011, //yellow
  0b11100001, //orange?
  0b00011100, //blue
  0b00000011, //green
  0b11100000, //red
  0b11100111, //pale yellow
  0b01011001, //pale purple
};

//You will need to modify this to translate fro x,y to a pixel number.
uint16_t computeAddress(int row, int col){
	uint16_t reversed = 0;
#ifdef VERT_STRIPS
	if (col%2 == 0) {
		reversed = 1;
	}
	uint16_t base = (col)*FIELD_HEIGHT;
	if (reversed) {
		base += FIELD_HEIGHT - 1;
	}
	uint16_t final = reverse == 1? base - row: base + row;
	}
#else
	if (row%2 == 0){
		reversed = 1;
	}
	uint16_t base = (row)*FIELD_WIDTH;
	if (reversed) {
		base += FIELD_WIDTH -1;
	}
	uint16_t final = reversed == 1 ? base - col: base + col;
#endif
	return final;
}

/*const unsigned short level_ticks_timeout[ max_level ]  = {
32,
28,
24,
20,
17,
13,
10,
8,
5
};*/
//const unsigned int score_per_level          = 10; //per brick in lv 1+
//const unsigned int score_per_line          = 300;


byte wall[FIELD_WIDTH][FIELD_HEIGHT];
//The 'wall' is the 2D array that holds all bricks that have already 'fallen' into place

bool aiCalculatedAlready = false;
bool useAi = true;
struct TAiMoveInfo{
  byte rotation;
  int positionX, positionY;
  int weight;
} aiCurrentMove;

struct TBrick{
  byte type; //This is the current brick shape. 
  byte rotation; //active brick rotation
  byte color; //active brick color
  int positionX, positionY; //active brick position
  byte pattern[4][4]; //2D array of active brick shape, used for drawing and collosion detection

} currentBrick;


//unsigned short  level        = 0;
//unsigned long  score        = 0;
//unsigned long  score_lines      = 0;

//WiiChuck

//WiiChuck chuck = WiiChuck();


// Define the RGB pixel array and controller functions, 
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(LEDS, LEDDATAPIN, NEO_GRB + NEO_KHZ800);
#ifdef LEDCLKPIN
Adafruit_WS2801 strip = Adafruit_WS2801(LEDS, LEDDATAPIN, LEDCLKPIN);
#else
Adafruit_WS2801 strip = Adafruit_WS2801(LEDS);
#endif

int idleBricks = 0;
#define MAX_IDLEBRICKS 4

int numberOfBricksSinceAI = 0;
int numberOfBricksInGame = 0;
int addExtraBrickCount = 20;  // limit before adding last mystry brick

void setup(){

  Serial.begin(115200); 
  Serial.println(F("Starting Arduino Tetris"));
  Serial.print(F("Numbers of Possible Bricks = "));
  Serial.println(sizeof(bricks)/sizeof(bricks[0]));
  brick_count = (sizeof(bricks)/sizeof(bricks[0])) - EXTRABRICKS; 

  Serial.print(F("Numbers of Bricks in Play = "));
  Serial.println(brick_count);

  strip.begin();

  //Pre-Operating Self Test of LED grid.
  Serial.println(F("Starting Pre Operating Self Test"));
  Serial.println(F("fade to Red"));
  fadeGrid(Color(0,0,0), Color(255,0,0), 8, 50);   // fade from off to Red
  Serial.println(F("fade to Green"));
  fadeGrid(Color(255,0,0), Color(0,255,0), 8, 50); // fade from Red to Green
  Serial.println(F("fade to Blue"));
  fadeGrid(Color(0,255,0), Color(0,0,255), 8, 50); // fade from Green to Blue
  Serial.println(F("fade to off"));
  fadeGrid(Color(0,0,255), Color(0,0,0), 8, 50);   // fade from Blue to Off
  Serial.println(F("Pre Operating Self Test Finished"));

  Serial.print(F("useAI mode = ")); 
  Serial.println(useAi); 
  
  pinMode(INLEFT, INPUT_PULLUP);
  pinMode(INRIGHT, INPUT_PULLUP);
  pinMode(INROTCCW, INPUT_PULLUP);
  pinMode(INROTCW, INPUT_PULLUP);
  
  //chuck.begin();
  //chuck.update();
  newGame();

}

void loop(){

  //screenTest();
  play();
  
}

//tests pixels
void screenTest(){
  for( int i = 0; i < FIELD_WIDTH; i++ )
  {
    for( int k = 0; k < FIELD_HEIGHT; k++ )
    {
      wall[i][k] = 7;
      drawGame();
      delay(500);
    }
  }
}

//plays the game!
// globals, sorry (@RCK)
int ct = 0;
void play(){
	ct++; // increment our tick counter

	if (millis() > bounce_tick) {
		byte command = getCommand();

		if ( command != 4 ) {
			bounce_tick = millis() + bounce_delay;
		}
		/* To account for an oversensitive thumbstick,
		   we want to introduce a timer that prevents
		   commands from being processed too frequently. @RCK */

		// if we're not on the AI, and there is a real command
		// but < 7 loops have gone by, pretend it didn't happen.
		if (!useAi && command < 4 && ct < 7) {
			Serial.print(F("SKIPPED"));
			Serial.println(ct);
		} else { // ok, we can process this command.

			// reset the tick counter if it's a command
			if (command < 4)
				ct = 0;

			// process the command
			if ( command == COUNTERCLOCKWISE ) {
  			Serial.println(F("ROTATE 90"));
				bounce_tick = millis() + bounce_delay*5;
				if ( checkRotate( 1 ) == true ) {
					rotate( 1 );
				}
			} else if ( command == CLOCKWISE ) {
  			Serial.println(F("ROTATE -90"));
				bounce_tick = millis() + bounce_delay*5;
				if ( checkRotate( 0 ) == true ) {
					rotate( 0 );
				}
			} else if ( command == RIGHT ) {
				if ( checkShift( -1, 0 ) == true ) {
					Serial.println(F("SHIFT RIGHT"));
					shift( -1, 0 );
				}
			} else if ( command == LEFT ) {
				if ( checkShift( 1, 0 ) == true ) {
					Serial.println(F("SHIFT LEFT"));
					shift( 1, 0 );
				}
			} else if ( command == DOWN ) {
  			Serial.print(F("D"));
				moveDown();
			}
		}
	}
	if ( millis() > next_tick ) {
		next_tick = millis()+tick_delay;
		moveDown();
	}
	drawGame();
}


//returns how high the wall goes 
int getHighestColumn(){
  int columnHeight = 0;
  //count
  int maxColumnHeight = 0;
  for(int j = 0; j < FIELD_WIDTH; j++)
  {
    columnHeight = 0;
    for(int k = FIELD_HEIGHT-1; k!=0; k--)
    {
      if(wall[j][k] != 0)
      {
        columnHeight = FIELD_HEIGHT - k;
        //Serial.print(k);
        //Serial.println(F(" is k"));
        //delay(100);
      }
    }
    if(columnHeight > maxColumnHeight)
      maxColumnHeight = columnHeight;
  }
  return maxColumnHeight;
}

//counts the number of given holes for the ai calculation
int getHoleCount(){
  int holeCount = 0;
  for(int j = 0; j < FIELD_WIDTH; j++)
  {
    for(int k = currentBrick.positionY + 2; k < FIELD_HEIGHT; k++)
    {
      if(wall[j][k] == 0)
        holeCount++;
    }
  }
  return holeCount;
}

//determines if a full line is possible given the current wall (for ai)
bool getFullLinePossible()
{
  int lineCheck;
  for(byte i = 0; i < FIELD_HEIGHT; i++)
  {
    lineCheck = 0;
    for(byte k = 0; k < FIELD_WIDTH; k++)
    {
      if( wall[k][i] != 0)  
        lineCheck++;
    }
    
    if(lineCheck == FIELD_WIDTH)
    {
      return true;
    }
  }
  return false;
}

byte previousCommand = NOTHING;
//gets commands according to ai state
byte getCommand(){
  /*
    return COUNTERCLOCKWISE;
    return LEFT;
    return RIGHT;
    return DOWN;
    */
//  if(Serial.available())
//  {
//    switch(Serial.read())
//    {
//      case ' ': return COUNTERCLOCKWISE;
//      case 'a': return LEFT;
//      case 's': return RIGHT;
//    }
//  }
  bool btnLeft = digitalRead(INLEFT) == LOW;
  bool btnRight = digitalRead(INRIGHT) == LOW;
  bool btnRotCCW = digitalRead(INROTCCW) == LOW;
  bool btnRotCW = digitalRead(INROTCW) == LOW;
  byte result = NOTHING;
  if(btnLeft && (previousCommand != LEFT)) result = LEFT; 
  if(!btnLeft && (previousCommand == LEFT)) previousCommand = NOTHING; 
  if(btnRight && (previousCommand != RIGHT)) result = RIGHT;
  if(!btnRight && (previousCommand == RIGHT)) previousCommand = NOTHING;
  if(btnRotCCW && (previousCommand != COUNTERCLOCKWISE)) result = COUNTERCLOCKWISE;
  if(!btnRotCCW && (previousCommand == COUNTERCLOCKWISE)) previousCommand = NOTHING;
  if(btnRotCW && (previousCommand != CLOCKWISE)) result = CLOCKWISE;
  if(!btnRotCW && (previousCommand == CLOCKWISE)) previousCommand = NOTHING;
  if(result != NOTHING) previousCommand = result;
  return result;
}

//checks if the next rotation is possible or not.
bool checkRotate( bool direction )
{
  rotate( direction );
  bool result = !checkCollision();
  rotate( !direction );

  return result;
}

//checks if the current block can be moved by comparing it with the wall
bool checkShift(short right, short down)
{
  shift( right, down );
  bool result = !checkCollision();
  shift( -right, -down );

  return result;
}

// checks if the block would crash if it were to move down another step
// i.e. returns true if the eagle has landed.
bool checkGround()
{
  shift( 0, 1 );
  bool result = checkCollision();
  shift( 0, -1 );
  return result;
}

// checks if the block's highest point has hit the ceiling (true)
// this is only useful if we have determined that the block has been
// dropped onto the wall before!
bool checkCeiling()
{
  for( int i = 0; i < 4; i++ )
  {
    for( int k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[i][k] != 0)
      {
        if( ( currentBrick.positionY + k ) < 0 )
        {
          return true;
        }
      }
    }
  }
  return false;
}

//checks if the proposed movement puts the current block into the wall.
bool checkCollision()
{
  int x = 0;
  int y =0;

  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if( currentBrick.pattern[i][k] != 0 )
      {
        x = currentBrick.positionX + i;
        y = currentBrick.positionY + k;

        if(x >= 0 && y >= 0 && wall[x][y] != 0)
        {
          //this is another brick IN the wall!
          return true;
        }
        else if( x < 0 || x >= FIELD_WIDTH )
        {
          //out to the left or right
          return true;
        }
        else if( y >= FIELD_HEIGHT )
        {
          //below sea level
          return true;
        }
      }
    }
  }
  return false; //since we didn't return true yet, no collision was found
}

//updates the position variable according to the parameters
void shift(short right, short down)
{
  currentBrick.positionX += right;
  currentBrick.positionY += down;
}

// updates the rotation variable, wraps around and calls updateBrickArray().
// direction: 1 for clockwise (default), 0 to revert.
void rotate( bool direction )
{
  if( direction == 1 )
  {
    if(currentBrick.rotation == 0)
    {
      currentBrick.rotation = 3;
    }
    else
    {
      currentBrick.rotation--;
    }
  }
  else
  {
    if(currentBrick.rotation == 3)
    {
      currentBrick.rotation = 0;
    }
    else
    {
      currentBrick.rotation++;
    }
  }
  updateBrickArray();
}

void moveDown(){
  Serial.print(F("."));
  if( checkGround() )
  {
    #ifdef DEGUB
  	Serial.print(F("checkGround()"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif

    addToWall();
    #ifdef DEGUB
  	Serial.print(F("addToWall()"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif

    drawGame();
    #ifdef DEGUB
  	Serial.print(F("drawGame()"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif
    
    if( checkCeiling() )
    {
    #ifdef DEGUB
  	Serial.print(F("Ceiling Found"));
  	Serial.print(F("bounce_tick = "));
  	Serial.print(bounce_tick);
  	Serial.print(F(" millis() = "));
  	Serial.println(millis());
  	#endif
      gameOver();
    }
    else
    {
      while( clearLine() )
      {
        //scoreOneUpLine();
      }
      nextBrick();
      //scoreOneUpBrick();
    }
  }
  else
  {
    //grounding not imminent
    shift( 0, 1 );
  }
  //scoreAdjustLevel();
  //ticks = 0;
}

//put the brick in the wall after the eagle has landed.
void addToWall()
{
  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[i][k] != 0){
        wall[currentBrick.positionX + i][currentBrick.positionY + k] = currentBrick.color;
        
      }
    }
  }
}

//removes brick from wall, used by ai algo
void removeFromWall(){
  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[i][k] != 0){
        wall[currentBrick.positionX + i][currentBrick.positionY + k] = 0;
        
      }
    }
  }
}

//uses the currentBrick_type and rotation variables to render a 4x4 pixel array of the current block
// from the 2-byte binary reprsentation of the block
void updateBrickArray()
{
  unsigned int data = pgm_read_word(&(bricks[ currentBrick.type ][ currentBrick.rotation ]));
  for( byte i = 0; i < 4; i++ )
  {
    for( byte k = 0; k < 4; k++ )
    {
      if(bitRead(data, 4*i+3-k))
      currentBrick.pattern[k][i] = currentBrick.color; 
      else
      currentBrick.pattern[k][i] = 0;
    }
  }
}
//clears the wall for a new game
void clearWall()
{
  for( byte i = 0; i < FIELD_WIDTH; i++ )
  {
    for( byte k = 0; k < FIELD_HEIGHT; k++ )
    {
      wall[i][k] = 0;
    }
  }
}

// find the lowest completed line, do the removal animation, add to score.
// returns true if a line was removed and false if there are none.
bool clearLine()
{
  int line_check;
  for( byte i = 0; i < FIELD_HEIGHT; i++ )
  {
    line_check = 0;

    for( byte k = 0; k < FIELD_WIDTH; k++ )
    {
      if( wall[k][i] != 0)  
      line_check++;
    }

    if( line_check == FIELD_WIDTH )
    {
      flashLine( i );
      for( int  k = i; k >= 0; k-- )
      {
        for( byte m = 0; m < FIELD_WIDTH; m++ )
        {
          if( k > 0)
          {
            wall[m][k] = wall[m][k-1];
          }
          else
          {
            wall[m][k] = 0;
          }
        }
      }

      return true; //line removed.
    }
  }
  return false; //no complete line found
}

//randomly selects a new brick and resets rotation / position.
void nextBrick(){
  Serial.print(F("Next Brick"));
  Serial.print(F(", useAI mode = ")); 
  Serial.print(useAi); 
  Serial.print(F(", idleBricks = ")); 
  Serial.println(idleBricks); 
  Serial.print(F("bounce_tick = "));
  Serial.print(bounce_tick);
  Serial.print(F(" millis() = "));
  Serial.println(millis());

  numberOfBricksInGame++;
  Serial.print(F("numberOfBricksInGame = "));
  Serial.println(numberOfBricksInGame);

  if (!useAi) {
    numberOfBricksSinceAI++;
    if (numberOfBricksSinceAI > (addExtraBrickCount * 1.5)) {
      brick_count = sizeof(bricks)/sizeof(bricks[0]); 
    } else if (numberOfBricksSinceAI > addExtraBrickCount) {
      brick_count = sizeof(bricks)/sizeof(bricks[0])  - (EXTRABRICKS / 2); 
    } else {
      brick_count = (sizeof(bricks)/sizeof(bricks[0])) - EXTRABRICKS; 
    }
  } else {
    numberOfBricksSinceAI = 0;
    brick_count = (sizeof(bricks)/sizeof(bricks[0])) - EXTRABRICKS; 
  }
  Serial.print(F("numberOfBricksSinceAI = "));
  Serial.println(numberOfBricksSinceAI);

  Serial.print(F("Numbers of Bricks in Play = "));
  Serial.println(brick_count);
	
  if (!useAi) {
    idleBricks++;
    if (idleBricks > MAX_IDLEBRICKS) {
      useAi = !useAi;
      Serial.print(F("MAX_IDLEBRICKS of ")); 
      Serial.print(MAX_IDLEBRICKS); 
      Serial.println(F(" exceeded")); 
      Serial.println(F("Swithing to AI mode!")); 
      idleBricks = 0;
    }
    Serial.print(F("idle manual bricks = ")); 
    Serial.println(idleBricks);
  }
  currentBrick.rotation = 0;
  currentBrick.positionX = round(FIELD_WIDTH / 2) - 2;
  currentBrick.positionY = -3;


  currentBrick.type = random( 0, brick_count );

  currentBrick.color = pgm_read_byte(&(brick_colors[ currentBrick.type ]));

  aiCalculatedAlready = false;

  updateBrickArray();

  //displayPreview();
}

//effect, flashes the line at the given y position (line) a few times.  
void flashLine( int line ){

  bool state = 1;
  for(byte i = 0; i < 6; i++ )
  {
    for(byte k = 0; k < FIELD_WIDTH; k++ )
    {  
      if(state)
      wall[k][line] = 0b11111111;
      else
      wall[k][line] = 0;
      
    }
    state = !state;
    drawWall();
    updateDisplay();
    delay(200);
  }

}


//draws wall only, does not update display
void drawWall(){
  for(int j=0; j < FIELD_WIDTH; j++){
    for(int k = 0; k < FIELD_HEIGHT; k++ )
    {
      draw(wall[j][k],FULL,j,k);
    }
    
  }

}

//'Draws' wall and game piece to screen array 
void drawGame()
{

  //draw the wall first
  drawWall();

  //now draw current piece in play
  for( int j = 0; j < 4; j++ )
  {
    for( int k = 0; k < 4; k++ )
    {
      if(currentBrick.pattern[j][k] != 0)
      {
        if( currentBrick.positionY + k >= 0 )
        {
          draw(currentBrick.color, FULL, currentBrick.positionX + j, currentBrick.positionY + k);
          //field[ positionX + j ][ p osition_y + k ] = currentBrick_color;
        }
      }
    }
  }
  updateDisplay();
}

//takes a byte color values an draws it to pixel array at screen x,y values.
// Assumes a Down->UP->RIGHT->Up->Down->etc (Shorest wire path) LED strips display.
//new brightness value lets you dim LEDs w/o changing color.
/*
void draw(byte color, signed int brightness, byte x, byte y){
  
  unsigned short address=0;
  byte r,g,b;
  
  //flip y for new tie layout. remove if your strips go up to down
  y = (FIELD_HEIGHT-1) - y;
  
  //calculate address
  if(x%2==0) //even row
  address=FIELD_HEIGHT*x+y;
  else //odd row
  address=((FIELD_HEIGHT*(x+1))-1)-y;
  
  if(color==0 || brightness < 0){
    strip.setPixelColor(address, 0);
  }
  else{
    //calculate colors, map to LED system
    b=color&0b00000011;
    g=(color&0b00011100)>>2;
    r=(color&0b11100000)>>5;
    
    //make sure brightness value is correct
    brightness=constrain(brightness,0,FULL);
    
    Fixed_setPixelColor(address, map(r,0,7,0,brightness), map(g,0,7,0,brightness), map(b,0,3,0,brightness));

  }
  
}
*/
void draw(byte color, signed int brightness, byte x, byte y){
  byte r,g,b;
  //flip y for new tie layout. remove if your strips go up to down
  y = (FIELD_HEIGHT-1) - y;
  
  //calculate address
  //if(x%2==0) //even row
  //address=FIELD_HEIGHT*x+y;
  //else //odd row
  //address=((FIELD_HEIGHT*(x+1))-1)-y;
  uint16_t address = computeAddress(y,x);
  if(color==0 || brightness < 0){
    strip.setPixelColor(address, 0);
  }
  else{
    //calculate colors, map to LED system
    b=color&0b00000011;
    g=(color&0b00011100)>>2;
    r=(color&0b11100000)>>5;

    //make sure brightness value is correct
    brightness=constrain(brightness,0,FULL);
    
    Fixed_setPixelColor(address, map(r,0,7,0,brightness), map(g,0,7,0,brightness), map(b,0,3,0,brightness));

  }
  
}  
//obvious function
void gameOver()
{
  Serial.println(F("GAME Over"));
//  Serial.println(F("Free RAM = ")); //F function does the same and is now a built in library, in IDE > 1.0.0
//  Serial.println(freeMemory(), DEC);  // print how much RAM is available.
  Serial.print(F("bounce_tick = "));
  Serial.print(bounce_tick);
  Serial.print(F(" millis() = "));
  Serial.println(millis());

  int y;
  for ( y = 0; y < FIELD_HEIGHT; y++ ) {
	  colorRow(Color(255, 0, 0), y);
	  strip.show();
	  delay(80);
  }
  fadeGrid(Color(255, 0, 0), Color(255,255,255),0, 100);
  fadeGrid(Color(255,255,255), Color(0,0,0), 8, 200);
  delay(1500);
  //dissolveGrid(5, 250);

  newGame();

}

//clean up, reset timers, scores, etc. and start a new round.
void newGame()
{
  numberOfBricksInGame = 0;
  Serial.print(F("New GAME"));
  Serial.print(F(", useAI mode = ")); 
  Serial.println(useAi); 
	Serial.print(F("bounce_tick = "));
	Serial.print(bounce_tick);

	Serial.print(F(" millis() = "));
	Serial.println(millis());
  aiCalculatedAlready = false;
  //  level = 0;
  // ticks = 0;
  //score = 0;
  //score_lines = 0;
  //last_key = 0;
  bounce_tick = millis() + bounce_delay;
  Serial.print(F("bounce_tick = "));
  Serial.print(bounce_tick);
  clearWall();

  nextBrick();
}

//Update LED strips
void updateDisplay(){

  strip.show();
  
  
}
uint32_t Color(byte r, byte g, byte b) {
	uint32_t c;
	c = r;
	c <<= 8;
	c |= g;
	c <<= 8;
	c |= b;
	return c;
}
void colorGrid(uint32_t color) {
	int i;
	for (i=0; i < strip.numPixels(); i++) {
		Fixed_setPixelColor(i, (uint8_t *) &color);
	}
}

void colorRow(uint32_t color, int row) {
	int x;

	for ( x = 0; x < FIELD_WIDTH; x++ ) {
		Fixed_setPixelColor(computeAddress(row, x), (uint8_t *)  &color);
	}
}

void fadeGrid(uint32_t s_color, uint32_t e_color, uint16_t pause, float steps) {
	float s_color_r = (( s_color >> 16 ) & 0xFF);
	float s_color_g = (( s_color >> 8 ) & 0xFF);
	float s_color_b = ( s_color & 0xFF );

	float e_color_r = (( e_color >> 16 ) & 0xFF);
	float e_color_g = (( e_color >> 8 ) & 0xFF);
	float e_color_b = ( e_color & 0xFF );
	uint32_t currentColor = s_color;
	long i;
	for ( i = 0; i <= steps; i++) {
		//currentColor = map(i, 0, steps, min(s_color, e_color), max(s_color, e_color) );
		//colorGrid(currentColor);

		//Serial.println((s_color_r + ((e_color_r - s_color_r) / steps)*i));
		colorGrid(Color((s_color_r + ((e_color_r - s_color_r) / steps)*i),(s_color_g + ((e_color_g - s_color_g) / steps)*i),(s_color_b + ((e_color_b - s_color_b) / steps)*i)));
		strip.show();
		delay(pause);
	}
}

void dissolveGrid(uint16_t pause, uint16_t steps) {
	int i;
	for (i = 0; i<steps; i++) {
		strip.setPixelColor(random(0, strip.numPixels()), 0);
		strip.show();
		delay(pause);
	}
}

// Pixel LED's on End may not be RGB, but rather RBG.
void Fixed_setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  if (n > strip.numPixels() - NUMBEROFRBGLEDSATEND - 1) {
    strip.setPixelColor(n, r, b, g);
  } else {
    strip.setPixelColor(n, r, g, b);
  }
}

void Fixed_setPixelColor(uint16_t n, uint8_t *c) {
  union fourbyte {
    uint32_t lword;
    uint8_t  byte[4];
  } d;

   if (n > strip.numPixels() - NUMBEROFRBGLEDSATEND - 1) {
     d.byte[2] = c[2]; // Red
     d.byte[1] = c[0]; // Blue
     d.byte[0] = c[1]; // Green
     strip.setPixelColor(n, d.lword);
   } else {
     strip.setPixelColor(n, *(uint32_t *) c);
   }
}
