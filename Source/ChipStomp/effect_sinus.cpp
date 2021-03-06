/*
	SINUS effect
	Adds a waveform to the output.
	The same really as the Sinus except that it adds to rather
	than multiplying the incoming signal.
*/

#include <PLIB.h>
#include "effect_sinus.h"

//******** Private macros ********//

#define FEATURECOUNT 2  // Note : Default feature is 0 : It does nothing
#define WAVE_LEN 0x03ff  // Number of samples
#define AMP_MAX 0xffff
#define AMP_MIN 0x0000
#define STEP_MAX 0x03ff
#define STEP_MIN 0x0001
#define POSITION_MAX 0x0003ffff  
#define BASEFREQ SAMPLERATE / WAVE_LEN  // 43hz for 1024 samples @ 44.1khz


//******** Private function declarations ********//

void sinus_nextFeature();
void sinus_adjustFeature(int16_t value);
uint8_t sinus_toggleOnOff();
int32_t sinus_effectISR(int32_t value);
void sinus_report();
float sinus_getHz();
void sinus_freq_adjust(int16_t value);
void sinus_amp_adjust(int16_t value);

//******** Private variables ********//

// Internal state variables
typedef struct {
    uint32_t position; // 24bits of position and 8bits of fraction
    uint16_t amplitude;
    uint16_t step;
} settings_t;
static settings_t settings = {
		0
	,	AMP_MAX / 2
	, 55
};
enum features_t {SAFE, AMP, FREQ};
static const char *featurenames[] = {"Safe", "Amplitude","Frequency"};

//******** Global variables ********//

// This struct is exposed globally via extern in the header
Effect_t effect_Sinus = {
		"Sinus"
	, 0
	, 0
	, sinus_nextFeature
	, sinus_adjustFeature
	, sinus_toggleOnOff
	, sinus_effectISR
	, sinus_report
};

//******** Function definitions ********//

// This is where the effect is actually processed
int32_t sinus_effectISR(int32_t value){
	uint16_t idx, amp;
	int16_t sine1, sine2;
	int32_t result;
	uint8_t frac;

  // create a variable frequency and amplitude sinewave.
  // since we will be moving through the lookup table at
  // a variable frequency, we wont always land directly
  // on a single sample.  so we will average between the
  // two samples closest to us.  this is called interpolation.
  // step through the table at rate determined by step
  // use upper byte of step value to set the rate
  // and have an offset of 1 so there is always an increment.
  // settings.position  += 1 + (mod1_value >> 8);
  settings.position  += settings.step;
  // if we've gone over the table boundary -> loop back
  settings.position  &= POSITION_MAX; // this is a faster way doing the table
                          // wrap around, which is possible
                          // because our table is a multiple of 2^n.
                          // otherwise you would do something like:
                          // if (settings.position  >= 1024*256) {
                          //   settings.position  -= 1024*256;
                          // }
  // Get the index part
  idx = (settings.position >> 8);
  
  // Retrieve first sine sample sample
  sine1 = g_sinewave[idx];
  idx++; // go to next sample
  // TODO: Check this is actually correct. I'm not sure STEP_MAX is the right value to be using here.
  idx &= STEP_MAX; // check if we've gone over the boundary.
                   // we can do this because its a multiple of 2^n,
                   // otherwise it would be:
                   // if (idx  >= 1024) {
                   //   idx  = 0; // reset to 0
                   // }
  // get second sample and put it in sine2 
  sine2  = g_sinewave[idx];
  
  // interpolate between samples
  // multiply each sample by the fractional distance
  // to the actual settings.position  value
  frac = (uint8_t)(settings.position & 0x000000ff); // fetch the lower 8b
  // scale sample 2
  sine2 = (sine2 * frac) >> 8;
  // scale sample 1
  sine1 = (sine1 * (0xff - frac )) >> 8;
  
  // add samples together to get an average
  // our resultant sinewave is now in sine2 
  sine2 += sine1;
  sine2 = (sine2 * settings.amplitude) >> 16;
  //return sine2;
  
  result = value + sine2;
  return result;
}

// Cycles my features
void sinus_nextFeature(){
	if(FEATURECOUNT <= 1) return;
	if(effect_Sinus.featureIdx < FEATURECOUNT) {
		effect_Sinus.featureIdx++;
	}else{
		// Skip the safe feature
		effect_Sinus.featureIdx = 1;
	}
}

// Turns me on or off
uint8_t sinus_toggleOnOff(){
	if(effect_Sinus.state) effect_Sinus.state = 0;
	else effect_Sinus.state = 1;
}

// Adjust the value of the current feature
// Receives the encoder delta
void sinus_adjustFeature(int16_t value){
	// Currently hard coded to alter the step
	features_t feat = (features_t)effect_Sinus.featureIdx;
	switch(feat){
		case AMP:{
			sinus_amp_adjust((uint16_t)value*0xff);	
			break;
		}
		case FREQ:{
			sinus_freq_adjust((uint16_t)value);	
			break;
		}
	}
}

// Alters the current Sinus step value by value (+ or -)
// Step value determines frequency
// use an int32 for result to make boundry checking easy
// Clamps result to within min/max
void sinus_freq_adjust(int16_t value){
	int32_t result = settings.step + value;
	if(result > STEP_MAX){
		result = STEP_MAX;
	}else if(result < STEP_MIN){
		result = STEP_MIN;
	}
	settings.step = (uint16_t)result;
}

// Alters the current Sinus amplitude value by value (+ or -)
// Clamps result to within min/max
void sinus_amp_adjust(int16_t value){
	int32_t result = settings.amplitude + value;
	if(result > AMP_MAX){
		result = AMP_MAX;
	}else if(result < AMP_MIN){
		result = AMP_MIN;
	}
	settings.amplitude = (uint16_t)result;
}

// Sends a string of my state to stdout
void sinus_report(){
	features_t feat = (features_t)effect_Sinus.featureIdx;
	// Write to screen
	if(feat == AMP){
		display.setTextColor(0);
		display.fillRect(0,DISP_FEAT_Y,DISP_FEAT_W,13,1);
	}else{
		display.setTextColor(1);
	}
	display.print("Amp ");
  display.print(percentage(settings.amplitude, AMP_MAX, AMP_MIN), 2);
	display.print("%");

  if(feat == FREQ){
		display.setTextColor(0);
		display.fillRect(0,DISP_FEAT_Y+14,DISP_FEAT_W,13,1);
	}else{
		display.setTextColor(1);
	}
	display.setCursor(DISP_FEAT_INDENT,DISP_FEAT_Y+14);
  display.print("Freq ");
  display.print(sinus_getHz(), 2);
	display.print("Hz");
}

// Returns the Sinus frequency calculated from the step
float sinus_getHz(){
	uint16_t step = settings.step; // This is volatile. Get it once
	uint8_t idx;
	uint8_t frac;
	float result;
	idx = (step >> 8);
	frac = (uint8_t)(step & 0x00ff);
	result = (float)(frac * BASEFREQ)/256;
	result += (float)(idx * BASEFREQ);
	return result;
}
