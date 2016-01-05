// include the SoftwareSerial library so you can use its functions:
#include <SoftwareSerial.h>
#include <IRremote.h>
#include <BlindsSwitch.h>

const int Screen_Control_SerialRX = -1;
const int Screen_Control_SerialTX = 7;

const int Blinds_Switch_TX = 4;
const unsigned int Blinds_Switch_RemoteID = 0x92;

const int Pin13LED = 13;

const int TriggerIn = 2;
const int IRRecv_pin = 3;

SoftwareSerial Screen_Control_Serial(Screen_Control_SerialRX, Screen_Control_SerialTX);
BlindsSwitch Blinds_Switch(Blinds_Switch_TX);

IRrecv irrecv(IRRecv_pin);
decode_results IR_results;

#define DUMP_IR
#ifdef DUMP_IR
int dump_ir = 0;
#endif

void setup()
{
  int i;

  pinMode(Screen_Control_SerialTX, OUTPUT);
  Screen_Control_Serial.begin(2400);

  pinMode(Pin13LED, OUTPUT);
  digitalWrite(Pin13LED, LOW);
  
  pinMode(TriggerIn, INPUT);

  Serial.begin(9600);

  Blinds_Switch.enableTransmit(Blinds_Switch_TX);

  irrecv.enableIRIn(); // Start the receiver
}

#define SCREEN_UP_WAITING        1
#define SCREEN_UP_TRIGGER_SEEN   2
#define SCREEN_DOWN_WAITING      3
#define SCREEN_DOWN_TRIGGER_SEEN 4

int debounce_start_time = 0;

int state = SCREEN_UP_WAITING;

const int DEBOUNCE_TIME = 250;

const byte ScreenDown[] = {
  0xff, 0xee, 0xee, 0xee, 0xee};
const byte ScreenUp[]   = {
  0xff, 0xee, 0xee, 0xee, 0xdd};
const byte ScreenStop[] = {
  0xff, 0xee, 0xee, 0xee, 0xcc};

#ifdef DUMP_IR
void dump(decode_results *results) {
  int count = results->rawlen;
  
  // Check if the buffer overflowed
  if (results->overflow) {
    Serial.println("IR code too long. Edit IRremoteInt.h and increase RAWLEN");
    return;
  }
  
  switch (results->decode_type) {
    default:
    case UNKNOWN:      Serial.print("UNKNOWN");       break ;
    case NEC:          Serial.print("NEC");           break ;
    case SONY:         Serial.print("SONY");          break ;
    case RC5:          Serial.print("RC5");           break ;
    case RC6:          Serial.print("RC6");           break ;
    case DISH:         Serial.print("DISH");          break ;
    case SHARP:        Serial.print("SHARP");         break ;
    case JVC:          Serial.print("JVC");           break ;
    case SANYO:        Serial.print("SANYO");         break ;
    case MITSUBISHI:   Serial.print("MITSUBISHI");    break ;
    case SAMSUNG:      Serial.print("SAMSUNG");       break ;
    case LG:           Serial.print("LG");            break ;
    case WHYNTER:      Serial.print("WHYNTER");       break ;
    case AIWA_RC_T501: Serial.print("AIWA_RC_T501");  break ;
    case PANASONIC:    Serial.print("PANASONIC");     break ;
    case DENON:        Serial.print("Denon");         break ;
   }
   // Panasonic has an Address
   if (results->decode_type == PANASONIC) {
    Serial.print(results->address, HEX);
    Serial.print(":");
   }
   Serial.print(results->value, HEX);
   Serial.print(" (");
   Serial.print(results->bits, DEC);
   Serial.println(" bits)");
   return;
   
    Serial.print("Timing[");
    Serial.print(results->rawlen-1, DEC);
    Serial.println("]: ");
  
    for (int i = 1;  i < results->rawlen;  i++) {
      unsigned long  x = results->rawbuf[i] * USECPERTICK;
      if (!(i & 1)) {  // even
        Serial.print("-");
        if (x < 1000)  Serial.print(" ") ;
        if (x < 100)   Serial.print(" ") ;
        Serial.print(x, DEC);
      } else {  // odd
        Serial.print("     ");
        Serial.print("+");
        if (x < 1000)  Serial.print(" ") ;
        if (x < 100)   Serial.print(" ") ;
        Serial.print(x, DEC);
        if (i < results->rawlen-1) Serial.print(", "); //',' not needed for last one
      }
      if (!(i % 8))  Serial.println("");
    }
    Serial.println("");                    // Newline
  
}
#endif

void loop()
{

  // Level conveter changes polarity of signal, invert it here
  int trigger_value = !digitalRead(TriggerIn);
  digitalWrite(Pin13LED, trigger_value);

  switch (state)
  {
  case SCREEN_UP_WAITING:
    // If trigger is seen, log the start time and transition 
    // to the debounce state      
    if (trigger_value)
    {
      debounce_start_time = millis();
      state = SCREEN_UP_TRIGGER_SEEN;
      Serial.println("Leaving 1 for 2");
    }
    break;
  case SCREEN_UP_TRIGGER_SEEN:
    if (!trigger_value)
    {
      // just a glitch - not long enough to be a true
      // signal. go back to waiting
      state = SCREEN_UP_WAITING;
      Serial.println("returning to 1");
    }
    else if ((millis() - debounce_start_time) >= DEBOUNCE_TIME)
    {
      // steady trigger for enough time - this is a
      // real signal.  send the screen down command
      Screen_Control_Serial.write(ScreenDown, sizeof(ScreenDown));
      state = SCREEN_DOWN_WAITING;
      Serial.println("Leaving 2 for 3");
    }
    break;
  case SCREEN_DOWN_WAITING:
    // If trigger is no longer seen, log the start time 
    // and transition to the debounce state      
    if (!trigger_value)
    {
      debounce_start_time = millis();
      state = SCREEN_DOWN_TRIGGER_SEEN;
      Serial.println("Leaving 3 for 4");
    }
    break;
  case SCREEN_DOWN_TRIGGER_SEEN:
    if (trigger_value)
    {
      // Trigger not missing for a long enough
      // time - go back to waiting
      state = SCREEN_DOWN_WAITING;
      Serial.println("Returning to 3");
    }
    else if ((millis() - debounce_start_time) >= DEBOUNCE_TIME)
    {
      // Trigger cleared lnog enough - send screen back up
      Screen_Control_Serial.write(ScreenUp, sizeof(ScreenUp));
      state = SCREEN_UP_WAITING;
      Serial.println("Leaving 4 for 1");
    }
    break;
  default:
    // return to a known good state just in case
    state = SCREEN_UP_WAITING;
    break;
  }

  if (irrecv.decode(&IR_results)) {
#ifdef DUMP_IR
    if (dump_ir)
      dump(&IR_results);
#endif  
    irrecv.disableIRIn();
    switch (IR_results.value)
    {
    case 0x8C7328D7: // PLAY            
#ifdef DUMP_IR
      dump_ir = 1 - dump_ir;
      Serial.println("PLAY");
#endif
      break;
    case 0x8C7308F7: // STOP            
#ifdef DUMP_IR
      Serial.println("STOP");
#endif
      break;
    case 0x8C738877: // PAUSE/STILL     
#ifdef DUMP_IR
      Serial.println("PAUSE/STILL");
#endif
      break;
    case 0x8C73C837: // FF_>>           
#ifdef DUMP_IR
      Serial.println("FF_>>");
#endif
      break;
    case 0x8C7348B7: // <<_REW          
#ifdef DUMP_IR
      Serial.println("<<_REW");
#endif
      break;
    case 0x8C73A857: // RECORD          
#ifdef DUMP_IR
      Serial.println("RECORD");
#endif
      break;
    case 0x8C73DA25: // OPERATE         
#ifdef DUMP_IR
      Serial.println("POWER");
#endif
      break;
    case 0x8C7320DF: // 1               
#ifdef DUMP_IR
      Serial.println("1");
#endif
      Screen_Control_Serial.write(ScreenUp, sizeof(ScreenUp));
      Serial.println("screen up"); 
      break;
    case 0x8C73A05F: // 2
#ifdef DUMP_IR
      Serial.println("2");
#endif
      break;
    case 0x8C73609F: // 3               
#ifdef DUMP_IR
      Serial.println("3");
#endif
      Serial.println("shades up");  
      Blinds_Switch.AllUp(Blinds_Switch_RemoteID);
      break;
    case 0x8C73E01F: // 4
#ifdef DUMP_IR
      Serial.println("4");
#endif
      Screen_Control_Serial.write(ScreenStop, sizeof(ScreenStop));
      Serial.println("screen stop");     
      break;
    case 0x8C7330CF: // 5               
#ifdef DUMP_IR
      Serial.println("5");
#endif
      break;
    case 0x8C73B04F: // 6
#ifdef DUMP_IR
      Serial.println("6");
#endif
      Serial.println("shades stop");  
      Blinds_Switch.AllStop(Blinds_Switch_RemoteID);
      break;
    case 0x8C73708F: // 7               
#ifdef DUMP_IR
      Serial.println("7");
#endif
      Screen_Control_Serial.write(ScreenDown, sizeof(ScreenDown));
      Serial.println("screen down");     
      break;
    case 0x8C73F00F: // 8
#ifdef DUMP_IR
      Serial.println("8");
#endif
      break;
    case 0x8C7338C7: // 9               
#ifdef DUMP_IR
      Serial.println("9");
#endif
      Serial.println("shades down");   
      Blinds_Switch.AllDown(Blinds_Switch_RemoteID);
      break;

    case 0x8C73807F: // CHANNEL_UP
#ifdef DUMP_IR
      Serial.println("CHANNEL_UP/ARROW_UP");
#endif
      break;
    case 0x8C7340BF: // CHANNEL_DOWN    
#ifdef DUMP_IR
      Serial.println("CHANNEL_DOWN/ARROW_DOWN");
#endif
      break;
    case 0x8C7329D6: // ARROW_RIGHT  
#ifdef DUMP_IR
      Serial.println("ARROW_RIGHT");
#endif
      break;
    case 0x8C73A9D6: // ARROW_LEFT  
#ifdef DUMP_IR
      Serial.println("ARROW_LEFT");
#endif
      break;
    case 0x8C7310EF: // VTR_V           
#ifdef DUMP_IR
      Serial.println("VIDEO");
#endif
      break;
    case 0x8C738A75: // TV_MONITOR      
#ifdef DUMP_IR
      Serial.println("TV_MONITOR");
#endif
      break;
    case 0x8C730AF5: // KEY             
#ifdef DUMP_IR
      Serial.println("KEY");
#endif
      break;
    case 0x8C739867: // INDEX           
#ifdef DUMP_IR
      Serial.println("INDEX");
#endif
      break;
    case 0x8C7322DD: // RESET           
#ifdef DUMP_IR
      Serial.println("RESET");
#endif
      break;
    case 0x8C73C23D: // MEMORY          
#ifdef DUMP_IR
      Serial.println("MEMORY");
#endif
      break;
    case 0x8C73F807: // ATR             
#ifdef DUMP_IR
      Serial.println("ATR");
#endif
      break;
    case 0x8C73629D: // CHECK           
#ifdef DUMP_IR
      Serial.println("MENU/GUIDE");
#endif
      break;
    case 0x8C7352AD: // CL              
#ifdef DUMP_IR
      Serial.println("EXIT");
#endif
      break;
    case 0x8C73B847: // O/AV            
#ifdef DUMP_IR
      Serial.println("O/AV");
#endif
      break;
    case 0x8C73D22D: // OK              
#ifdef DUMP_IR
      Serial.println("OK/SELECT");
#endif
      break;
#if 0
    case 0xC2D0: // power  
      dump_ir = 1 - dump_ir;    
      break;
    case 0xC230: // play                     
      break;
    case 0xC260: // forward                  
      break;
    case 0xC2E0: // backward                 
      break;
    case 0xC2C0: // stop                     
      break;
    case 0xC2B0: // pause                    
      break;
    case 0xC2EC: // menu                     
      break;
    case 0xC23C: // ok                       
      break;
    case 0xC26C: // zero4x                   
      break;
    case 0xC233: // itr                      
      break;
    case 0xC298: // pr_up                    
      break;
    case 0xC218: // pr_dn                    
      break;
    case 0xC228: // right                    
      break;
    case 0xC2A8: // left                     
      break;
    case 0xC220: // eject                    
      break;
    case 0xC284: // no_1   
      Screen_Control_Serial.write(ScreenUp, sizeof(ScreenUp));
      Serial.println("screen up"); 
      break;
    case 0xC244: // no_2                     
      break;
    case 0xC2C4: // no_3  
      Serial.println("shades up"); 
      Blinds_Switch.AllUp(Blinds_Switch_RemoteID);
      break;
    case 0xC224: // no_4  
      Screen_Control_Serial.write(ScreenStop, sizeof(ScreenStop));
      Serial.println("screen stop");     
      break;
    case 0xC2A4: // no_5                     
      break;
    case 0xC264: // no_6   
      Serial.println("shades stop");  
      Blinds_Switch.AllStop(Blinds_Switch_RemoteID);
      break;
    case 0xC2E4: // no_7  
      Screen_Control_Serial.write(ScreenDown, sizeof(ScreenDown));
      Serial.println("screen down");     
      break;
    case 0xC214: // no_8                     
      break;
    case 0xC294: // no_9  
      Serial.println("shades down"); 
      Blinds_Switch.AllDown(Blinds_Switch_RemoteID);
      break;
    case 0xC2CC: // no_0                     
      break;
    case 0xC098: // tvpr_up
      break;
    case 0xC018: // tvpr_dn
      break;
#endif
    }
    irrecv.enableIRIn(); // Receive the next value

  }
}

