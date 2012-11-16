/*
 *  RPM 測定ツール 4secサンプリング、1sec更新
 */

/* 汎用ヘッダ */
#include <avr/io.h>
#include <avr/interrupt.h>


#define uint8_t unsigned char
#define uint32_t unsigned long

/*****
 ***  サンプリング時間
 *****/
#define RPM_SAMPLING_SEC  4


/* PB0: モーター入力用ポート */
#define MTR_PORT  PORTB
#define MTR_DDR   DDRB

/* PB1-PB4: 桁用ポート */
#define DIG_PORT  PORTB
#define DIG_DDR   DDRB
#define DIG_BASE  1
#define DIG_1  (DIG_BASE+0)
#define DIG_2  (DIG_BASE+1)
#define DIG_3  (DIG_BASE+2)
#define DIG_4  (DIG_BASE+3)

/* PC0-PC6, PD0-PD1: セグメント用ポート */
#define SEG_DDR0  DDRC
#define SEG_DDR1  DDRD
#define SEG_PORT0 PORTC
#define SEG_PORT1 PORTD



/* 回転数測定間隔 60で1[sec] */
#define LED_INTRVL  60


/* 外部変数定義 */
volatile uint16_t wait_time   = 0;
volatile uint16_t count = 0;
volatile uint16_t sec_cnt = 0;
volatile uint32_t rpm = 0;   /* 最終的には割り込みにより算出 */
/* 7segに表示させるデータ用配列-4桁分 */
uint8_t  vram0[4];
uint8_t  vram1[4];

/***************************************************
 *  タイマー処理
 ***************************************************/

#if 0
#define F_CPU 1000000   /* CPU クロック周波数[Hz] */
#define PRESC 64        /* 分周比：プリスケーラ値 */
#define PITCH 100       /* 設定したい割り込み周波数[Hz]=10msecに一回 */
#define INTRCOUNT (0x10000-((F_CPU / PRESC)/PITCH))
#endif


/* Timer-2 Overflow */
ISR( TIMER2_OVF_vect ) {
    wait_time ++;
}


void wait(uint16_t w){
    wait_time = 0;
    while (wait_time<w);
}


void timer2_init( void )
{
    /* タイマー2割り込み設定 */
    TCCR2A = 0;
    TCCR2B = 0x04;     /* 64分周 (1KHz)  *//* 60回割り込みで約1[sec] */
    TCNT2  = 0;        /* カウンタ初期化 */
    TIMSK2 = 1<<TOIE2; /* オーバーフロー割り込み許可 */
}



/***************************************************
 *  タイマー1によるパルスカウンター
 ***************************************************/


ISR( TIMER1_CAPT_vect )
{
	count++;
}

void palsetimer_init( void )
{
	MTR_DDR = 0b11111110;   /* ポートPB0を入力設定 */

	TCCR1B  = 0x42;         /* ICNC1:捕獲起動入力端を立ち上がりに設定, CS1:8分周 */
	TIMSK1  = _BV(ICIE1);   /* タイマ1インプットキャプチャ割り込み許可 */
}




/***************************************************
 *  7セグLED表示処理
 ***************************************************/

/* セグメント用テーブル */
/* segA-segF */
unsigned char digtable0[] =
{
	0b00111111, /* 0 */
	0b00000110, /* 1 */
	0b00011011, /* 2 */
	0b00001111, /* 3 */
	0b00100110, /* 4 */
	0b00101101, /* 5 */
	0b00111101, /* 6 */
	0b00100111, /* 7 */
	0b00111111, /* 8 */
	0b00101111  /* 9 */
};
/* segG,segH */
unsigned char digtable1[] =
{
	0b00, /* 0 */
	0b00, /* 1 */
	0b01, /* 2 */
	0b01, /* 3 */
	0b01, /* 4 */
	0b01, /* 5 */
	0b01, /* 6 */
	0b00, /* 7 */
	0b01, /* 8 */
	0b01  /* 9 */
};


/*
 * 7seg 表示処理 
 *
 */

void dig_init( void )
{
	/* 出力モードに設定 */
	DIG_DDR   =   ((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));
	SEG_DDR0  = 0x3F;
	SEG_DDR1  = 0xFF;
	/* 初期化　PIN出力 LOW */
	DIG_PORT  &= ~((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));  /* &= 0x0F */
	SEG_PORT0 = 0x00;
	SEG_PORT1 = 0x00;
}


/* 
 * 機能: 7segを一定時間表示させる
 *
 * 動作:
 *   LED_INTRVL で指定されるカウント数の時間だけ4桁LEDをダイナミック点灯する。
 * このとき、モーター入力のRPM算出のため、1[sec]固定としている
 *
 * 注意:
 *   DIG->PORTの順で出力させないと正しく点灯しない。
 *   一桁点灯後、一旦消灯してから次の桁に移らないと、
 * 前の桁のsegmentが光ったままになってしまう現象が発生した。
 *
 */
void dig_drive( void )
{
	int dig_num = 0;

	/* PORT消灯 */
	DIG_PORT  &= ~((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));  /* &= 0xF0 */
	SEG_PORT0 = 0x00;
	SEG_PORT1 = 0x00;

	/* 出力モード */
	DIG_DDR  |= (1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4);
	SEG_DDR0 = 0xFF;
	SEG_DDR1 = 0xFF;

	/* この間にタイマーカウントが実行 */
	while( wait_time < LED_INTRVL )
	{
		DIG_PORT = ( 1<<(dig_num+DIG_BASE) ); /* 桁を出力 */
		SEG_PORT0 = vram0[dig_num];           /* セグメント表示パラメータセット */
		SEG_PORT1 = vram1[dig_num];

		if( dig_num++ >= 4 )
		//if( dig_num >= 4 )
		{
			dig_num = 0;
		}

		SEG_PORT0 = 0x0;  /* 一旦消灯 */
		SEG_PORT1 = 0x0;
	}
		
	wait_time = 0;          /* タイマークリア */

	/* 消灯 */
	DIG_DDR &= ~((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));  /* &= 0xF0 */
	SEG_DDR0 = 0x00;
	SEG_DDR1 = 0x00;

}


/*
 * 機能: RPM をセグメントテーブルに設定
 *
 * 注意:
 *   桁表示ピン位置の仕様
 *    ex)  1    2    3    4    : 1234を表示させる場合
 *        Pin2 Pin1 Pin0 Pin3
 *
 * 説明:
 *   vram0 セグメントa-f用
 *   vram1 セグメントg,h用
 * 
 * 背景:
 *   PB0ポート: 1桁目表示->キャプチャ入力としたためPB4へ変更
 *   PC6ポート: Resetポートとして使うため変更PD0へ変更
 *
 */
void dig_putrpm( void )
{
	uint32_t rpm_tmp = rpm;

	/*    1の位 */
	vram0[3] = digtable0[ rpm_tmp % 10 ];
	vram1[3] = digtable1[ rpm_tmp % 10 ];
	rpm_tmp /= 10;

	/*   10の位 */
	vram0[0] = digtable0[ rpm_tmp % 10 ];
	vram1[0] = digtable1[ rpm_tmp % 10 ];
	rpm_tmp /= 10;

	/*  100の位 */
	vram0[1] = digtable0[ rpm_tmp % 10 ];
	vram1[1] = digtable1[ rpm_tmp % 10 ];
	rpm_tmp /= 10;

	/* 1000の位 */ 
	vram0[2] = digtable0[ rpm_tmp ];
	vram1[2] = digtable1[ rpm_tmp ];

	dig_drive();
}



/************************************
 ***
 *** メイン関数（一周でRPM一回測定）
 ***
 ************************************/

int main( void )
{
    uint16_t mtr_cnt[4] = {0,0,0,0};
	
	cli();

	palsetimer_init(); /* タイマ1インプットキャプチャ設定 */
	timer2_init();     /* タイマ2割り込み設定 */
	dig_init();        /* 出力ポート指定、初期化 */
    sei();             /* SREG の設定 - 割り込み有効 */

	rpm   = 4321;
	count = 0;
	wait_time = 0;                /* タイマークリア */

	while(1)
	{
		dig_putrpm();             /* 前の RPM を1sec表示。その間にモーターカウント。 */

		/* RPM を算出
		 * 実測値より、5パルスあたり1回転のため、
		 * rpm = mtr_cnt × 60 ÷ (5*サンプリングsec)
		 */
		mtr_cnt[sec_cnt] = count;
		count = 0;                /* 1secのカウント開始 */
		rpm = (mtr_cnt[0] + mtr_cnt[1] + mtr_cnt[2] + mtr_cnt[3]) * (60/(5*RPM_SAMPLING_SEC));
		
		sec_cnt++;
		sec_cnt &= 0x3;           /* 0-3を繰り返す */

	}


}

