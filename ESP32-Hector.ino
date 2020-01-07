/*

  ESP32-Hector is placed under the MIT license

  Copyleft (c+) 2020 tobozo 

  Permission is hereby granted, free of charge, to any person obtaining a copy
  ociated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  This project is heavily inspired from the work of Gerard Ferrandez http://codepen.io/ge1doot/details/eWzQBm/
  who developed and implemented the function for HTML5 Canvas.

  Sound implementation follows @Bitluni's ULP example: https://github.com/bitluni/ULPSoundESP32/tree/master/ULPSoundMonoSample
  but is also heavily based on the Audio library from this repo: https://github.com/charlierobson/lasertag/

  Sound loop is taken from "Les histoires d'amour finissent mal" by "Rita Mistouko"
  and was provided by Gardie-Le-Gueux https://soundcloud.com/gardie-le-gueux
  Complete track : https://www.youtube.com/watch?v=ln0VwCqMkcA

 */


#include <M5Stack.h> // https://github.com/tobozo/ESP32-Chimera-Core
#include <M5StackUpdater.h> // https://github.com/tobozo/M5Stack-SD-Updater
#include "ULPAudio.h"

#define SIZE 150
#define STEP 2
#define GRID_SIZE SIZE/STEP
#include "lookup_tables.h"

#define tft M5.Lcd

TFT_eSprite sprite = TFT_eSprite( &tft );
TFT_eSprite headersprite = TFT_eSprite( &tft );
TFT_eSprite gfx = TFT_eSprite(&tft); // Sprite object for graphics write

SFX* sfx;

// Accel and gyro data
int32_t  gx, gy;

// sizing the 3d animation
static float size = SIZE;
static float step = STEP*1.2;
static float doublestep = STEP*2;
static float speed = 0.15; // for color rotation
static float tsize = 0.85*size;
static float halfsize = size * 0.5;
static float zoom = 1.33;
static float halfzoom = 0.5 * zoom;
static float k = 0;
static float romcosav, romsinav, romcosah, romsinah;
static float stepdirection = 1;
static float brightnessfactor;
static float z;

static int num = GRID_SIZE;
static int totalpixels = 0;
static int fps = 0;

uint16_t wtf[SIZE]; // this is a strange bug, can't get rid of this without breaking the demo

#define PEAK_SIZE 320
static int16_t peak[PEAK_SIZE*2]; // depth cache, for 'solid' visual effect
static int16_t lastpeak[PEAK_SIZE*2]; // depth cache, for 'solid' visual effect

static int16_t pathindex, scan_x, scan_y;
static int16_t txlast;
static int16_t tylast;
static int16_t scroll_x = 0; // Keep track of the scrolled position, this is where the origin 
static int16_t scroll_y = 0; // (top left) of the gfx Sprite will be

static uint16_t screenWidth;
static uint16_t screenHeight;
static uint16_t screenHalfWidth;
static uint16_t screenHalfHeight;
static uint16_t spritePosX;
static uint16_t spritePosY;
static uint16_t spriteWidth;
static uint16_t spriteHeight;

static uint8_t maxrangecolor = 255;
static uint8_t minrangecolor = 0;
static uint8_t basecolor;
static uint8_t green;
static uint8_t red;
static uint8_t blue;
static uint8_t segmentpos = 0;
static uint8_t lastsegmentpos = 0;

static unsigned long framecount = 0; // count fps
static unsigned long lastviewmodechange = millis();
static unsigned long viewmodechangetime = 5000;
static unsigned long lastscroll = millis();
static unsigned long scrolltick = 60;
static unsigned long sampleloopsegment;

static uint32_t fstart = 0;

uint16_t *gfxPtr; // Pointer to start of graphics sprite memory area
uint16_t *scrollPtr; // Pointer to start of graphics sprite memory area
uint16_t *mainPtr; // Pointer to start of graphics sprite memory area

bool sound = true;

struct Coords {
  int16_t x = -1;
  int16_t y = -1;
  uint16_t color = 0;
};

static Coords HectorGrid[GRID_SIZE+1][GRID_SIZE+1];

enum DisplayStyle {
  DISPLAY_GRID,
  DISPLAY_SOLID,
  DISPLAY_ZEBRA,
  DISPLAY_CHECKBOARD
};

enum WaveStyle {
  DRIP_WAVE,
  SIN_WAVE
};


enum ButtonState {
  BUTTON_PAUSE,
  BUTTON_PLAY,
  BUTTON_SPEAKER_ON,
  BUTTON_SPEAKER_OFF
};

struct ButtonCoords {
  uint16_t x;
  uint16_t y;
};


struct UIButton {
  ButtonCoords coords;
  uint8_t width;
  uint8_t height;
  
  void render(ButtonState state=BUTTON_PAUSE ) {
    uint16_t x = coords.x;
    uint16_t y = coords.y;
    byte anglebox = height / 4;
    byte anglesign = height / 15;
    byte pheight = (height - 2) - height/5;
    byte pwidth = width/5;
    byte posX1 = x +  (((width-2)/2)/2);
    byte posX2 = x + ((((width-2)/2)/2) +1)*2;
    byte posY = y + ((height-2)/2 - pheight/2);
    uint16_t TFT_GRAY = tft.color565( 64, 64, 64 );
    uint16_t TFT_DARKGRAY = tft.color565( 32, 32, 32 );
    uint16_t TFT_LIGHTGRAY = tft.color565( 128, 128, 128 );

    tft.drawRoundRect( x,    y,   width, height, anglebox, TFT_GRAY );
    tft.drawRoundRect( x,    y,   width-1, height-1, anglebox, TFT_LIGHTGRAY );

    tft.fillRoundRect( x+1,  y+1, width-2, height-2, anglebox, TFT_DARKGRAY );

    switch( state ) {
      case BUTTON_PLAY:
        tft.fillTriangle( posX1, posY, posX2+pwidth, posY+pheight/2, posX1, posY+pheight, TFT_LIGHTGRAY );
      break;
      case BUTTON_PAUSE:
        tft.fillRoundRect( posX1, posY, pwidth, pheight, anglesign, TFT_LIGHTGRAY );
        tft.fillRoundRect( posX2, posY, pwidth, pheight, anglesign, TFT_LIGHTGRAY );
      break;
      case BUTTON_SPEAKER_ON:
        pwidth/=2;
        posX1+=pwidth/2;
        posX2+=pwidth/2;
        tft.fillRoundRect( posX1, posY+pheight/4, pwidth*2, (pheight/2)+1, anglesign, TFT_LIGHTGRAY );
        tft.fillTriangle( posX1, posY+pheight/2, posX2+pwidth, posY, posX2+pwidth, posY+pheight, TFT_LIGHTGRAY );
      break;
      case BUTTON_SPEAKER_OFF:
        pwidth/=2;
        posX1+=pwidth/2;
        posX2+=pwidth/2;
        tft.fillRoundRect( posX1, posY+pheight/4, pwidth*2, (pheight/2)+1, anglesign, TFT_GRAY );
        tft.fillTriangle( posX1, posY+pheight/2, posX2+pwidth, posY, posX2+pwidth, posY+pheight, TFT_GRAY );
      break;
    }

  }
};


WaveStyle waveStyle = DRIP_WAVE;
WaveStyle oldWaveStyle = SIN_WAVE;
DisplayStyle displayStyle = DISPLAY_GRID;


ButtonCoords PlayPauseButtonCoords = { 42, 212 };
ButtonCoords SpeakerButtonCoords   = { 132, 212 };

UIButton PlayPauseButton = { PlayPauseButtonCoords, 50, 28 };
UIButton SpeakerButton   = { SpeakerButtonCoords, 50, 28 };


static void ulpLoop(void * pvParameters) {
  sfx->begin();
  sfx->playSound(HECTOR_SOUND);
  bool lastsoundstate = sound;
  while(1) {
    if( lastsoundstate != sound ) {
      sfx->toggleMute();
      lastsoundstate = sound;
    }
    if( sound ) {
      sfx->flush();
    }
  }
}


// pointer to sine wave function
float (*surfaceFunction)(float x, float y, float k);

// f(x,y) equation for sin wave
float sinwave(float x, float y, float k) {
  float r = 0.001 * ( rompow(x) + rompow(y) );
  return 100 * romcos(-k + r) / (2 + r);
}
// f(x,y) equation for water drip wave
float dripwave( float x, float y, float k ) {
  float r = 1.5*romsqrt( rompow(x) + rompow(y) );
  const float amplitude = 2.5;
  const float a = 200.0;
  const float b = (amplitude - fmod(k/3, amplitude))-amplitude/2;
  return /*100 **/ (a/(1+r)) * romcos((b/romlog(r+2))*r);
}
// projection
void project(float x, float y, float z/*, float ah, float av*/) {
  float x1 = x * romcosah - y * romsinah;
  float y1 = x * romsinah + y * romcosah;
  float z2 = z * romcosav - x1 * romsinav;
  if( (tsize - x1) == 0 ) return;
  float s = size / (tsize - x1);
  HectorGrid[scan_x][scan_y].x = screenHalfWidth - (zoom * (y1 * s));
  HectorGrid[scan_x][scan_y].y = screenHalfHeight - (zoom * (z2 * s));
}
// handle depth cache while line-drawing
void mySetPixel(int16_t x, int16_t y, uint16_t color) {
  if(peak[PEAK_SIZE+x]<=y) {
    peak[PEAK_SIZE+x] = y;
    totalpixels++;
    if( displayStyle == DISPLAY_GRID ) {
      sprite.drawPixel(x, screenHeight-y, color);
    }
  }
}
// draw helpers, just a copy of drawLine, calling mySetPixel() instead of setPixel()
// to handle depth, mainly here to compensate the lack of fill/stroke
void drawOverlap(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  totalpixels = 0;
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }
  if (x0 > x1) {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }
  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);
  int16_t err = dx / 2;
  int16_t ystep;
  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }
  for (; x0<=x1; x0++) {
    if( steep ) {
      mySetPixel(y0, x0, color);
    } else {
      mySetPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}
// depth-aware drawing/filling
void drawLineOverlap() {
  int16_t x0     = HectorGrid[pathindex][scan_y].x;
  int16_t x1     = HectorGrid[pathindex][scan_y-1].x;
  uint16_t color = HectorGrid[pathindex][scan_y].color;
  if( color == 0 ) return; // unset color
  int16_t y0 = peak[PEAK_SIZE+x0];
  int16_t y1 = lastpeak[PEAK_SIZE+x1];

  if( y0==-1 && x0==-1 ) return; // coords aren't set
  if( y1==-1 && x1==-1 ) return; // coords aren't set

  switch( displayStyle ) {
    case DISPLAY_GRID:
        sprite.drawLine( x0, screenHeight-y0, x1, screenHeight-y1, color );
        return;      
    break;
    case DISPLAY_SOLID:
      if(  pathindex%3 == 0 &&  scan_y%2 == 1
        || pathindex%2 == 1 &&  scan_y%2 == 0
      ) {
        return;
      }
    break;
    case DISPLAY_ZEBRA:
      if( scan_y%2 == 1-pathindex%2 ) return;
    break;
    case DISPLAY_CHECKBOARD:
      if(  pathindex%3 == 0 &&  scan_y%3 == 1
        || pathindex%3 == 1 &&  scan_y%3 == 2
        || pathindex%3 == 2 &&  scan_y%3 == 0
      ) {
        // render
      } else {
        return;
      };
    break;
  }
  if( pathindex < STEP ) return;
  int16_t x2 = HectorGrid[pathindex-STEP][scan_y].x;
  int16_t y2 = peak[PEAK_SIZE+x2];
  int16_t x3 = HectorGrid[pathindex-STEP][scan_y-1].x;
  int16_t y3 = lastpeak[PEAK_SIZE+x3];
  sprite.fillTriangle( x2, screenHeight-y2, x0, screenHeight-y0, x1, screenHeight-y1, color );
  sprite.fillTriangle( x2, screenHeight-y2, x1, screenHeight-y1, x3, screenHeight-y3, color );
}


void drawPath() {
  txlast = HectorGrid[0][scan_y].x;
  tylast = HectorGrid[0][scan_y].y;
  memcpy( lastpeak, peak, sizeof(peak) );
  for (pathindex = 0; pathindex < num; pathindex++) {
    drawOverlap(txlast, tylast, HectorGrid[pathindex][scan_y].x, HectorGrid[pathindex][scan_y].y, HectorGrid[pathindex][scan_y].color);
    if( totalpixels>0 && pathindex%STEP == 1 ) {
      drawLineOverlap();
    }
    txlast = HectorGrid[pathindex][scan_y].x;
    tylast = HectorGrid[pathindex][scan_y].y;
  }
}


void sinLoop() {
  // reset depth cache
  memset(peak,-1,sizeof(peak));
  memset(lastpeak,-1,sizeof(lastpeak));
  totalpixels = 0;

  k += speed;

  setupScale();

  float ah = ((-0.5 * gx + screenHalfWidth) / screenWidth);
  float av = 0.5 * gy / screenHeight;

  romcosav = romcos(av);
  romsinav = romsin(av);
  romcosah = romcos(ah);
  romsinah = romsin(ah);

  sprite.fillSprite( TFT_BLACK );

  scan_y = 0;

  for (float x = halfsize; x >= -halfsize; x -= doublestep) {

    blue = map( x, -halfsize, halfsize, minrangecolor, maxrangecolor );

    scan_x = 0;

    for (float y = -halfsize; y <= halfsize; y += step) {
      //float z = surface(x, y, k);
      float z = surfaceFunction(x, y, k);
      green = map( y, -halfsize, halfsize, minrangecolor, maxrangecolor );
      brightnessfactor = float(map(int(z), -50, 50, 100, 20))  / 100.0;
      red = maxrangecolor - (green-minrangecolor);
      green *= brightnessfactor;
      red   *= brightnessfactor;
      blue  *= brightnessfactor;

      HectorGrid[scan_x][scan_y].color = tft.color565(red, green, blue);
      project(x, y, z*1.2);
      scan_x++;
    }
    drawPath();
    scan_y++;
  }

  unsigned long nowmillis = millis();

  if(nowmillis - fstart >= 1000) {
    fps = (framecount * 1000) / (nowmillis - fstart);
    fstart = nowmillis;
    tft.setCursor( 240, 211 );
    tft.printf( "fps:   %2d", fps );
    framecount = 0;
  } else {
    framecount++;
  }

  if( sfx->samplelen > 0 ) {

    int32_t segment = nowmillis%sfx->samplelen;
    segmentpos = map( segment, 0, sfx->samplelen, 0, 16);
    if( lastsegmentpos != segmentpos ) {

      tft.setCursor( 240, 231 );
      tft.printf( "seg:   %2d", segmentpos );

      lastsegmentpos = segmentpos;
      if( segmentpos%2==0 ) {
        switch( displayStyle ) {
          case DISPLAY_SOLID:      displayStyle = DISPLAY_GRID; break;
          case DISPLAY_GRID:       displayStyle = DISPLAY_ZEBRA; break;
          case DISPLAY_ZEBRA:      displayStyle = DISPLAY_CHECKBOARD; break;
          case DISPLAY_CHECKBOARD: displayStyle = DISPLAY_SOLID; break;
        }
        tft.setCursor( 240, 221 );
        tft.printf( "style: %2d", displayStyle );
      }
      if( segmentpos == 1 ) { // sound loop reset
        switch( waveStyle ) {
          case DRIP_WAVE: waveStyle = SIN_WAVE; break;
          case SIN_WAVE: waveStyle = DRIP_WAVE; break;
        }
      }
      if( oldWaveStyle != waveStyle ) {
        switch( waveStyle ) {
          case SIN_WAVE:
            size = SIZE;
            step = STEP*1.5;
            setupScale();
            surfaceFunction = &sinwave; 
          break;
          case DRIP_WAVE: 
            setupScale();
            surfaceFunction = &dripwave; 
          break;
        }
        oldWaveStyle = waveStyle;
      }

    }
  }

  if( nowmillis > lastscroll + scrolltick ) {
    uint16_t chromar = map( 64*romsin(k), -64, 64, 128, 200);
    uint16_t chromag = map( 64*romsin(k*2), -64, 64, 128, 200);
    uint16_t chromab = map( 64*romsin(k*3), -64, 64, 128, 200);
    uint16_t gfxcolor = tft.color565( chromar, chromag, chromab );
    gfx.setBitmapColor(gfxcolor, TFT_BLACK);
    waveScroll(-3);
    headersprite.pushSprite( spritePosX , 0 );
    lastscroll = nowmillis;
  }

  sprite.pushSprite( spritePosX, spritePosY );
}


void resetCoords() {
  for( uint16_t x=0;x<GRID_SIZE+1;x++ ) {
    for( uint16_t y=0;y<GRID_SIZE+1;y++ ) {
      HectorGrid[x][y].x = -1;
      HectorGrid[x][y].y = -1;
      HectorGrid[x][y].color = 0;
    }
  }
}


void setupScale() {
  num = GRID_SIZE;
  doublestep = step*2;
  speed = 0.15; // for color rotation

  tsize = 0.85*size;
  halfsize = size * 0.5;
  zoom = 1.33;
  halfzoom = 0.5 * zoom;
  resetCoords();
}


void waveScroll( int dx ) {
  uint16_t width = gfx.width();
  uint16_t height = gfx.height();

  if (scroll_x < -width ) scroll_x += width;
  if (scroll_x > 0) scroll_x -= width;

  headersprite.fillSprite( TFT_BLACK );

  for (uint16_t x = 0; x < width; x++) {
    uint16_t xpos = (scroll_x+x)%(width);
    float xseq = (float)xpos / (float)headersprite.width() * (4*PI);
    int8_t margin = height/2 + (float)height/2.0 * romcos( xseq );
    int16_t offsetx = scroll_x + x + dx;
    //Serial.println( margin );
    for (uint16_t y = 0; y < height; y++) {
      if( offsetx > 0 && offsetx < headersprite.width()) {
        headersprite.drawPixel(offsetx,         y+margin, gfx.readPixel(x, y));
      }
      if( offsetx + width > 0 && offsetx + width < headersprite.width() ) {
        headersprite.drawPixel(offsetx + width, y+margin, gfx.readPixel(x, y));
      }
    }
  }
  scroll_x += dx;
}



static void drawLoop( void * param ) {
  while(1) {
    // TODO: change this to MPU6050 data
    gx = romsin(fmod(k*.15,2*PI))*/*amplitude*/150 /*offset*/+300;
    gy = -romsin(fmod(k*.25,2*PI))*/*amplitude*/50 /*offset*/-250;

    sinLoop();

    M5.update(); // read buttons

    if( M5.BtnA.wasPressed() ) {
      bool oldsound = sound;
      PlayPauseButton.render(BUTTON_PLAY);
      sound = false;
      while(true) {
        M5.update();
        if( M5.BtnA.wasPressed() ) {
          sound = oldsound;
          PlayPauseButton.render(BUTTON_PAUSE);
          break;
        }
        delay( 100 );
      }
    }
    if( M5.BtnB.wasPressed() ) {
      sound = !sound;
      if( sound ) {
        SpeakerButton.render(BUTTON_SPEAKER_ON);
      } else {
        SpeakerButton.render(BUTTON_SPEAKER_OFF);
      }
    }
    vTaskDelay(1);
  }

}




void setup() {
  M5.begin();
  tft.clear();

  if(digitalRead(BUTTON_A_PIN) == 0) {
    Serial.println("Will Load menu binary");
    updateFromFS( SD );
    ESP.restart();
  }

  if( tft.width() > tft.height() ) { // landscape
    screenWidth      = 300;
    screenHeight     = 180;
  } else { // portrait, not implemented, shit will happen
    log_n("Unimplemented orientation, expect problems");
    // scale (width*height must be inferior to 56k to fit in sram)
    screenWidth      = 240;
    screenHeight     = 230;
  }

  screenHalfWidth  = screenWidth/2;
  screenHalfHeight = screenHeight/2;
  spritePosX       = tft.width()/2 - screenHalfWidth;
  spritePosY       = tft.height()/2 - screenHalfHeight;

  sprite.setColorDepth( 16 );
  headersprite.setColorDepth( 1 );
  gfx.setColorDepth( 1 );

  if( psramInit() ) {
    // make sure the sprite doesn't use psram -> bad for the fps
    sprite.setPsram( false );
    headersprite.setPsram( false );
  }

  byte textsize = 2;

  gx = 256 + (k*20);
  gy = -305;

  tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
  tft.setTextSize( textsize );

  const char* scrollText = " . . . If silence is as deep as eternity, then speech is as shallow as time . . .  ";

  spriteWidth  = tft.textWidth( scrollText );
  spriteHeight = tft.fontHeight();

  mainPtr   = (uint16_t*)       sprite.createSprite( screenWidth, screenHeight );
  scrollPtr = (uint16_t*) headersprite.createSprite( screenWidth, spriteHeight*2 );
  gfxPtr    = (uint16_t*)          gfx.createSprite( spriteWidth, spriteHeight );

  gfx.setTextColor( TFT_WHITE, TFT_WHITE );
  gfx.setTextSize( textsize );
  gfx.fillSprite( TFT_BLACK );
  gfx.drawString( scrollText, 0, 0 );

  sprite.setTextColor( TFT_WHITE, TFT_WHITE );
  sprite.setTextSize( 1 );

  tft.setTextSize(1);
  tft.setTextColor( TFT_WHITE, TFT_BLACK );

  wtf[0] = 1; // <<< guru meditation : for some reason deleting this line makes the rendering fail, deleting the array too

  surfaceFunction = &sinwave;
  setupScale();

  fstart = millis()-1;

  PlayPauseButton.render(BUTTON_PAUSE);
  SpeakerButton.render(BUTTON_SPEAKER_ON);

  sfx = new SFX();
  xTaskCreatePinnedToCore(ulpLoop, "ulpLoop", 2048, NULL, 4, NULL, 1);

  xTaskCreatePinnedToCore(drawLoop, "drawLoop", 2048, NULL, 16, NULL, 0);

}





void loop() {
  vTaskSuspend(NULL);
}


