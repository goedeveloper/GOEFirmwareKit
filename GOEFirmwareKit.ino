/*
 * Input/Output files
 * INPUT1 : Nurture Condition File(config.txt)
 *          Setting file for the Plant Device
 * INPUT2 : Communication Setting File(comm.txt) 
 * OUTPUT1: Record File(record.txt)
 * OUTPUT2: Error Log(errlog.txt)
 *
 * Device Censor
 * 1. Tempurture(LM61CIZ)
 *    {AI2-Vout}
 * 2. Lux(S9648-100)
 *    {AI1-Anode}
 * 3. Water Level(2SC1815)
 *    {DO9-Correcta}
 *
 * Device
 * 1. LED
 *    {D17(A3)}
 * 2. WATER PUMP
 *    {D18(A4)}
 * 3. AIR PUMP
 *    {D19(A5)}
 * 4. FAN 
 * 5. LCD(SD1602HULD)
 *    {DO2-DB7, DO3-DB6, DO4-DB5, DO5-DB4, DO7-E, DO8-RS}
 * 6. SD Card
 *    {DO6-DAT2, DO11-CMD, DO13-CLK, DO12-DAT0}
 */

// include the library code
#include <Arduino.h>
#include <LiquidCrystal.h>
//#include <MsTimer2.h>         //Timer
#include <SD.h>
#include <ctype.h>
#include "types.h"


//FILE I/O
#define FILE_IN_CONFIG "config.txt"
#define FILE_IN_COMM "comm.txt"
#define FILE_OUT_RECORD "record.txt"
#define FILE_OUT_ERRLOG "errlog.txt"


//Arduino Pin Setting
//Digital Pins Map
const int HLD_001 = 0;   //DO0(HOLD)
const int HLD_002 = 1;   //DO1(HOLD)
const int LCD_DB7 = 2;   //DO2(LCD用)
const int LCD_DB6 = 3;   //DO3(LCD用)
const int SDC_DT2 = 4;   //DO4(SCCard) *CABLE SELECT PIN
const int LCD_DB4 = 5;   //DO5(LCD用)
const int LCD_DB5 = 6;   //DO6(LCD用)
const int LCD_EBL = 7;   //DO7(LCD用)
const int LCD_RS_ = 8;   //DO8(LCD用)
const int SEN_WTP = 9;   //DO9(Water Level Censor)
const int SDC_010 = 10;  //DO10(SDCard用) * Hold for SD Card -> CHIPSELECT
const int SDC_CMD = 11;  //DO11(SCCard用)
const int SDC_DT0 = 12;  //DO12(SCCard用)
const int SDC_CLK = 13;  //DO13(SCCard用)
const int LED_CTL = 17;  //LED(Analog->Digital)
const int WTP_CTL = 18;  //Water Pump(Analog->Digital)
const int ARP_CTL = 19;  //Airation(Analog->Digital)
//Analog Pins Map
const int SEN_LUX = 1;  //A01(Lux Censor)
const int SEN_CES = 2;  //A02(Tempurture Censor)



struct CONTROLCONFIG CC;
struct RECORD REC = {0,0,0,0};

//Initialize LCD
LiquidCrystal lcd(LCD_RS_, LCD_EBL, LCD_DB4, 
                  LCD_DB5, LCD_DB6, LCD_DB7);

/*
 * Initialization
 */
void setup() {
  
	//Init SmartPlant
	boolean ret = initializeSmartPlant();
	if (!ret) { 
		raiseError();
		return;
	}

	//Sett Config file
	setControlConfig();
	delay(1000);
	CC = getControlConfig();
	delay(1000);

	int x = 0;
	do {
		circuitChecker();
		x++;
	} while (x > 1000);

}

void loop() {

    return;
  
	int n = 0;
	do {
		if (directController() != 0) {
			raiseError();
		} else {
			break;
		}
		n++;
	} while (n < 3);

	//Start Control
	if (!stateChecker(&REC)) {
		stopPlant();
		raiseError();
		return;
	}

	addRecord(&REC);

	delay(CC.measinterval);
}

/*
 * Measure and Activate Device
 */
boolean stateChecker(struct RECORD *Rec) {

	Rec->run_t = millis();
	Rec->ces = readCes();
	Rec->lux = readLux();
	Rec->wlv = readWlv();

	if (Rec->ces < 0 || Rec->ces > 45) {
		return false;
	}

	if (Rec->lux < 0 || Rec->wlv < 0) {
		return false;
	}

	ledActivator(Rec->lux);
	wtpActivator(Rec->wlv);
	arpActivator(Rec->wlv);

	return true;
}

void circuitChecker() {

  Serial.print("Temparture Testing...: ");
  Serial.print(readCes());
  Serial.print("\n");
  lcdController("Temp Test...", String(readCes()));
  delay(1000);
 
  Serial.print("Lux Testing...: ");
  Serial.print(readLux());
  Serial.print("\n");
  lcdController("Lux Test...", String(readLux()));
  delay(1000);
  
  Serial.print("Water Level Testing...: ");
  Serial.print(readWlv());
  Serial.print("\n");
  lcdController("WL Test...", String(readWlv()));
  delay(1000);

  Serial.print("CC.ces_thd...: ");
  Serial.print(CC.ces_thd);
  Serial.print("\n");
  delay(1000);


//	ledActivator(readLux());
	wtpActivator(readWlv());
	arpActivator(readWlv());


}

void lcdController(String title, String value){
  // set the cursor to column 0, line 1
  lcd.begin(16, 2);
  lcd.print(title);
  
//  lcd.setCursor(0, 2);
  lcd.setCursor(0, 1);
  lcd.print(value);
}

/*
 * Smart Plantの初期化
 */
boolean initializeSmartPlant() {

  	Serial.begin(9600);

	lcd.begin(16, 2);

	pinMode(SEN_WTP, INPUT);    //Water Censor
	pinMode(WTP_CTL, OUTPUT);   //Water Pump(DO18)	
	pinMode(ARP_CTL, OUTPUT);   //Airation(DO19)	
/*
	pinMode(LED_CTL, OUTPUT);   //LEDの(DO17)
*/
	//Initialize SD Card
	pinMode(SDC_010, OUTPUT);
	Serial.print("SD read/write start....\n");
	if (!SD.begin(SDC_DT2)) {
		Serial.print("Failed: SD.begin()\n");
		return false;
	}
	Serial.println("Card is Initialized.");

	delay(1000);
	return true;
}

/*
 * getControlConfig
 */
CONTROLCONFIG getControlConfig() {
	CONTROLCONFIG cc;
	File f = SD.open(FILE_IN_CONFIG, FILE_READ);
	char charcter;
	String value = "";
	int num[9] = {0,0,0,0,0,0,0,0,0};
	int n = 0;

	Serial.print("start:getControlConfig..\n");
	while (f.available()) {
		charcter = f.read();
		if(isdigit(charcter)){
			value.concat(charcter);
		}else if(charcter == ','){
			num[n] = convertToInt(value);
			value = "";
			n++;
		}else if(charcter == '\n'){
			break;
		} 
	};
	f.close();
	Serial.print("end:\n");

	cc.ces_dev = num[0];       //温度偏差 ex) ±5%
	cc.ces_thd = num[1];       //温度しきい値 threshold
	cc.lux_thd = num[2];       //照度しきい値
	cc.led_stat = num[3];      //消灯開始時間
	cc.lec_fins = num[4];      //消灯終了時間
	cc.pumping_t = num[5];     //水位低下時のポンプ起動時間
	cc.measinterval = num[6];  //計測用時間間隔
	cc.dictinterval = num[7];  //通信用時間間隔
	cc.recdinterval = num[8];  //記録用時間間隔

	Serial.print("ces: ");
	Serial.print(cc.ces_thd);
	Serial.print("\n");

	return cc;
}


/*
 * getControlConfig
 */
boolean setControlConfig() {
	String str = "";
	str += String("5") + ",";
	str += String("25") + ",";
	str += String("100") + ",";
	str += String("2300") + ",";
	str += String("0700") + ",";
	str += String("5000") + ",";
	str += String("1000") + ",";
	str += String("1000") + ",";
	str += String("1000") + "\n";
/*
  int ces_dev;       //温度偏差 ex) ±5%
  int ces_thd;       //温度しきい値 threshold
  int lux_thd;       //照度しきい値
  int led_stat;      //消灯開始時間
  int lec_fins;      //消灯終了時間
  int pumping_t;     //水位低下時のポンプ起動時間
  int measinterval;  //計測用時間間隔
  int dictinterval;  //通信用時間間隔
  int recdinterval;  //記録用時間間隔
*/
	Serial.print("Start: setControlConfig...\n");

	if (SD.exists(FILE_IN_CONFIG)){
		Serial.print("file is removed...\n");
		SD.remove(FILE_IN_CONFIG);
	}

	File f = SD.open(FILE_IN_CONFIG, FILE_WRITE);
	if (f == false) {
		Serial.print("false:setconfig\n");
		return false;
	}
	Serial.print(str);
	f.print(str);
	Serial.print("End\n");

	f.close();
	return true;  
}


/*
 * Log Error
 */
boolean raiseError() {
	return true;
}

int convertToInt(String value){
	char charBuf[value.length()+1];
	value.toCharArray(charBuf, value.length()+1);
	return atoi(charBuf);
}

/*
 * Record Measurement Value
 */
boolean addRecord(struct RECORD *Rec ){
	String str = "";
	str += String(Rec->run_t) + ",";
	str += String(Rec->ces) + ",";
	str += String(Rec->lux) + ",";
	str += String(Rec->wlv) + "\n";
	File f = SD.open(FILE_OUT_RECORD, FILE_WRITE);
	if (f == false) {
		return false;
	}
	f.print(str);
	f.close();
	return true;
}


/*
 * Stop Plant
 */
void stopPlant() {
	digitalWrite(LED_CTL, LOW);
	digitalWrite(WTP_CTL, LOW);
	digitalWrite(ARP_CTL, LOW);
}

/*
 * Direct Contorol for Smart Plant
 */
boolean deviceindicater() {
	return true;
}

/*
 * Measure Tempurture
 */
int readCes() {
	int ans, ces, tvl;
	ans = analogRead(SEN_CES);            //Read Value from Analog Pin 0
	tvl = map(ans, 0, 1023, 0, 5000);     //Convert value considering Voltage
	ces = map(tvl, 300, 1600, -30, 100);  //Convert Vol to Tempurture
	return ces;
}

/*
 * Lux
 */
int readLux() {
	int ans;
	float lux, vlt, micamp;
	ans = analogRead(SEN_LUX);
	vlt = ((long)ans * 5000) / 1024;
	micamp = (vlt * 1000) / 1000;
	lux = micamp / (290/100);
	return lux;
}

/*
 * Read Water Level, Return the Value
 */
int readWlv() {
	return digitalRead(SEN_WTP);
}

/*
 * Direct Control for Smart Plant
 */
int directController(){
	//Read Direction Controller File
	int di; //= deviceindicator();
	switch (di) {
        /*
		case 1: dosomething; break;
		case 2: dosomething; break;
		default:
        */
	};
	return 0;
}

void dosomething(){

}

/*
 * Activate/Stop LED
 */
void ledActivator(int lux) {
	if (lux < CC.lux_thd) {
		digitalWrite(LED_CTL, HIGH);
	} else {
		digitalWrite(LED_CTL, LOW);
	}
}

/*
 * Activate/Stop WaterPump
 */
void wtpActivator(int waterlevel) {
	if (waterlevel == LOW) {
  		digitalWrite(WTP_CTL, HIGH);
		delay(5000);
		digitalWrite(WTP_CTL, LOW);
	}
}

/*
 * Activate/Stop Airation
 */
void arpActivator(int waterlevel) {
	if (waterlevel == HIGH) {
		digitalWrite(ARP_CTL, HIGH);
	} else {
		digitalWrite(ARP_CTL, LOW);
	}
}


