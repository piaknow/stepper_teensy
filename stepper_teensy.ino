volatile const int st = 4; //モータドライバのSTEPピン
volatile const int pos = 6; //z相フィードバック

volatile int seed1; //乱数用1
volatile int seed2; //乱数用2

volatile int stepon = false; //ステップパルスの状態
volatile float speedd = 0.001; //回転速度
volatile float dspeedd = 0; //速度変化量 phase5の減速時に使用 減速前の速度に関わらず回転角度を揃えるため
volatile unsigned int stepinterval = 1/speedd; //ステップパルスのオン間隔
volatile unsigned int totaltime = 0; //回転開始からの時間
volatile unsigned int startinterval = 0; //回転の間隔
volatile int randnumber = 0; //タイミングずらす用の乱数
volatile unsigned int delaytimes = 0; //何周遅らせるか
volatile unsigned int ztime = 0; //z相デバウンス用
volatile unsigned int lastztime = 0; //z相デバウンス用
//volatile unsigned int dztime = 0; //デバッグ用　z相HIGHのタイミングのランダムさをmicrosで見る
volatile int started = false; //phase 0の初回を判断
//volatile int calltimes = 0; //デバッグ用　z相HIGHの回数

volatile int stepcount = 0; //ステップ数　1周12800
volatile int stepcount2 = 0; //z相HIGH後のステップ数
volatile int phasee = 0; //回転のフェーズ

volatile int firstz; //初回回転時に定位置についたかどうかを判定
volatile int i = 0; //firstzの後にステップする回数

volatile int rotations = 0; //周回数を記録

IntervalTimer steptimer;

void setup() {
  pinMode(st, OUTPUT);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  seed1 = analogRead(A2);
  seed2 = analogRead(A4);
  randomSeed(seed1*seed2);

  firstz = digitalRead(pos); //z相の状態を取得　
  
  while(firstz == true){ //初回回転　
                         //最初からz相HIGHかつ電源が入っていなかった場合、z相後の1800ステップが失われてスタート位置がずれてしまうので、
                         //一旦LOWになるまでモーターを回す
    digitalWrite(st, !digitalRead(st));
    firstz = digitalRead(pos);
    delayMicroseconds(300);
  }
  
  while(firstz == false){ //初回回転2
    digitalWrite(st, !digitalRead(st));
    firstz = digitalRead(pos);
    delayMicroseconds(300);
  }

  for(i = 0; i < 3600; i++){
    if(i < 3599){
      digitalWrite(st, !digitalRead(st));
    }
    else{
      digitalWrite(st, LOW);
    }
    delayMicroseconds(200);
  }
  
  attachInterrupt(pos, stamp, RISING);
  steptimer.begin(stephandler, 1000000);

}

void loop() {
  while(true){
    
  }
}

void stephandler(){
  if(stepon == true){       // TURN OFF PULSE
    //time2 = micros();  //デバッグ用 現在時刻の記録
    //Serial.print("OFF ");
    //Serial.println(time2);
    digitalWrite(st, LOW); //LOW期間はelse文の中で決まっている
    stepon = false;
    steptimer.update(10); //次のパルスON期間を決める
  }
  else if(rotations <= 1030){                      // START NEW PULSE 設定した周回数に達したら終了
    //time2 = micros();  //デバッグ用 現在時刻の記録
    //Serial.print("ON ");
    //Serial.println(time2);
    digitalWrite(st, HIGH); //HIGH期間はif文の中で決まっている (10μs)
    stepon = true;
    steptimer.update(stepinterval - 10); //パルスONの10を引く
    totaltime += stepinterval; //回転開始から次のsteponまでの時間を更新
    stepcount ++;

    switch(phasee){
      
      case 0: //加速
        if(started == false){
          totaltime = 0;
          started = true;
          rotations += 1; //rotations回目の周回をこれから始める
        }
        speedd = 0.001 + stepcount * 0.00004;
        if(speedd - 0.002 > 0){
          speedd = 0.002;
          phasee = 1;
        }
        stepinterval = 1 / speedd;
        break;
        
      case 1: //棒を押す回転
        if(stepcount - 2000 > 0){
          phasee = 2;
        }
        break;
        
      case 2: //加速
        speedd += 0.00002;
        if(speedd - 0.03 > 0){
          speedd = 0.03;
          phasee = 3;
        }
        stepinterval = 1/speedd;
        break;
        
      case 3: //原点に戻る速い回転
        break;
        
      case 4: //z相HIGH後の回転　z相HIGHで触発
        if(stepcount - stepcount2 > 0){ //800マイクロステップで次
          phasee = 5;
          dspeedd = (speedd - 0.001) / 1000; 
          stepcount2 = stepcount + 1000; //1000マイクロステップで止まるようにする
        }
        break;
        
      case 5: //減速  チャタリング防止の時間測定のため、減速後はz相HIGHの区域を抜けたい
        speedd -= dspeedd;
        if(speedd - 0.001 < 0){
          speedd = 0.001;
        }
        if(stepcount - stepcount2 > 0){ //1000マイクロステップで次
          phasee = 6;
        }
        stepinterval = 1/speedd;
        break;
        
      case 6: //減速後、原点付近で待機
        randnumber = random(0,10001);
        startinterval = 1796045 - randnumber; //開始の間隔を決める
        delaytimes = (totaltime + 50000) / startinterval; //totaltimeがstartinterval - 50msより長いとき、開始を1周遅らせる
        stepinterval = startinterval * (1 + delaytimes) - totaltime; //待ち時間 totaltimeがstartintervalより大きくてはならない
        /*
        Serial.println(delaytimes);
        //Serial.println(ztime);
        //Serial.println(ztime - dztime);
        Serial.println(totaltime);
        Serial.println(startinterval);
        Serial.println(stepinterval);
        //Serial.println(calltimes);
        Serial.println();
        */
        phasee = 0;
        stepcount = 0;
        started = false;
        break;
    }
  }
}

void stamp(){ //z相HIGHで触発
  //dztime = ztime; //デバッグ用
  ztime = micros();
  if(ztime - lastztime > 100000){
    lastztime = ztime;
    phasee = 4;
    stepcount2 = stepcount + 800;
  }
  //phasee = 4;
  //calltimes += 1; //デバッグ用　z相HIGHの回数
}
