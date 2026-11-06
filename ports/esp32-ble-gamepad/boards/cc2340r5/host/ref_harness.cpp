// Runs the UNMODIFIED function bodies of BleGamepad.cpp (extracted by script into
// gen/ref_funcs.inc) and the unmodified BleGamepadConfiguration.cpp on the host,
// with the BLE parts stubbed, and prints MAP and RPT1..4 for the JOBSPEC sequence.
#include "Arduino.h"
#include "../lib/BleGamepadConfiguration.h"
struct Cap { uint8_t buf[64]; size_t n; void setValue(uint8_t*d,size_t l){memcpy(buf,d,l);n=l;} void notify(){} };
struct Serial_t { void println(const char*s){puts(s);} } Serial;
#define NIMBLE_LOGE(...) ((void)0)
#define LOG_TAG 0
#define BLE_GAMEPAD_DEBUG 0
#define USAGE_PAGE(x) 0x05
#define USAGE(x) 0x09
#define xTaskCreate(...) ((void)0)
static inline int16_t clampAxis(int16_t v){return v;}
class RefGamepad {
public:
  BleGamepadConfiguration configuration;
  uint8_t tempHidReportDescriptor[512]; int hidReportDescriptorSize=0; uint8_t hidReportSize=0;
  uint8_t genericButtonPaddingBits=0, genericSpecialButtonPaddingBits=0, numOfButtonBytes=0;
  uint8_t _specialButtonPositions[POSSIBLESPECIALBUTTONS];
  uint8_t _buttons[16]={0}; uint8_t _specialButtons=0;
  int16_t _x=0,_y=0,_z=0,_rX=0,_rY=0,_rZ=0,_slider1=0,_slider2=0,_rudder=0,_throttle=0,_accelerator=0,_brake=0,_steering=0;
  int16_t _gX=0,_gY=0,_gZ=0,_aX=0,_aY=0,_aZ=0; signed char _hat1=0,_hat2=0,_hat3=0,_hat4=0;
  bool enableOutputReport=false, enableFeatureReport=false, enableSInput=false;
  uint16_t outputReportLength=0, featureReportLength=0;
  std::string deviceName="x", deviceManufacturer="y";
  Cap *inputGamepad = new Cap();
  void buildSInputDescriptor(){} void buildXInputDescriptor(){} void sendReport(){}
  static void taskServer(void*){}
  void buildGenericDescriptor(); void sendGenericReport(); void press(uint8_t); void release(uint8_t);
  uint8_t specialButtonBitPosition(uint8_t); void pressSpecialButton(uint8_t); void releaseSpecialButton(uint8_t);
  void pressStart(); void releaseStart(); void pressSelect(); void releaseSelect();
  void setAxes(int16_t,int16_t,int16_t,int16_t,int16_t,int16_t,int16_t,int16_t); void setHat1(signed char);
  void resetButtons(); void begin(BleGamepadConfiguration*);
};
#include "gen/ref_funcs.inc"
static void hex(const char*tag,const uint8_t*b,size_t n){printf("%s len=%zu ",tag,n);for(size_t i=0;i<n;i++)printf("%02x",b[i]);printf("\n");}
int main(int argc,char**argv){
  bool ss = argc>1 && argv[1][0]=='1';          // start+select on, as the JOBSPEC text says
  BleGamepadConfiguration cfg; cfg.setAutoReport(false);
  if(ss){cfg.setIncludeStart(true);cfg.setIncludeSelect(true);}
  RefGamepad g; g.begin(&cfg);
  hex("MAP",g.tempHidReportDescriptor,g.hidReportDescriptorSize);
  g.sendGenericReport(); hex("RPT1",g.inputGamepad->buf,g.inputGamepad->n);
  g.press(BUTTON_5); g.press(BUTTON_16); g.pressStart();
  g.setAxes(32767,32767,32767,32767,32767,32767,32767,32767); g.setHat1(HAT_DOWN_RIGHT);
  g.sendGenericReport(); hex("RPT2",g.inputGamepad->buf,g.inputGamepad->n);
  g.release(BUTTON_5); g.releaseStart(); g.setHat1(HAT_CENTERED); g.setAxes(0,0,0,0,0,0,0,0);
  g.sendGenericReport(); hex("RPT3",g.inputGamepad->buf,g.inputGamepad->n);
  g.press(BUTTON_1); g.press(BUTTON_8); g.press(BUTTON_9); g.pressSelect();
  g.setAxes(-32768,-1,1,255,256,-256,12345,-12345); g.setHat1(HAT_UP_LEFT);
  g.sendGenericReport(); hex("RPT4",g.inputGamepad->buf,g.inputGamepad->n);
}
