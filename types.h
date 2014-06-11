#include <Arduino.h>

struct CONTROLCONFIG {
  int ces_dev;       //温度偏差 ex) ±5%
  int ces_thd;       //温度しきい値 threshold
  int lux_thd;       //照度しきい値
  int led_stat;      //消灯開始時間
  int lec_fins;      //消灯終了時間
  int pumping_t;     //水位低下時のポンプ起動時間
  int measinterval;  //計測用時間間隔
  int dictinterval;  //通信用時間間隔
  int recdinterval;  //記録用時間間隔
};

//計測データ記録用構造体
struct RECORD {
  unsigned long run_t;  //時間(SPが起動してからの時間)
//  unsigned led_t;     //LED起動時間
//  unsigned aro_t;     //エアレーション起動時間
//  unsigned wtp_t;     //ポンプ起動時間
  int ces;              //温度(celisius)
  int lux;              //照度
  int wlv;              //水位レベル
};