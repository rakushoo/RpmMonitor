/*
 *  RPM ����c�[�� 4sec�T���v�����O�A1sec�X�V
 */

/* �ėp�w�b�_ */
#include <avr/io.h>
#include <avr/interrupt.h>


#define uint8_t unsigned char
#define uint32_t unsigned long

/*****
 ***  �T���v�����O����
 *****/
#define RPM_SAMPLING_SEC  4


/* PB0: ���[�^�[���͗p�|�[�g */
#define MTR_PORT  PORTB
#define MTR_DDR   DDRB

/* PB1-PB4: ���p�|�[�g */
#define DIG_PORT  PORTB
#define DIG_DDR   DDRB
#define DIG_BASE  1
#define DIG_1  (DIG_BASE+0)
#define DIG_2  (DIG_BASE+1)
#define DIG_3  (DIG_BASE+2)
#define DIG_4  (DIG_BASE+3)

/* PC0-PC6, PD0-PD1: �Z�O�����g�p�|�[�g */
#define SEG_DDR0  DDRC
#define SEG_DDR1  DDRD
#define SEG_PORT0 PORTC
#define SEG_PORT1 PORTD



/* ��]������Ԋu 60��1[sec] */
#define LED_INTRVL  60


/* �O���ϐ���` */
volatile uint16_t wait_time   = 0;
volatile uint16_t count = 0;
volatile uint16_t sec_cnt = 0;
volatile uint32_t rpm = 0;   /* �ŏI�I�ɂ͊��荞�݂ɂ��Z�o */
/* 7seg�ɕ\��������f�[�^�p�z��-4���� */
uint8_t  vram0[4];
uint8_t  vram1[4];

/***************************************************
 *  �^�C�}�[����
 ***************************************************/

#if 0
#define F_CPU 1000000   /* CPU �N���b�N���g��[Hz] */
#define PRESC 64        /* ������F�v���X�P�[���l */
#define PITCH 100       /* �ݒ肵�������荞�ݎ��g��[Hz]=10msec�Ɉ�� */
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
    /* �^�C�}�[2���荞�ݐݒ� */
    TCCR2A = 0;
    TCCR2B = 0x04;     /* 64���� (1KHz)  *//* 60�񊄂荞�݂Ŗ�1[sec] */
    TCNT2  = 0;        /* �J�E���^������ */
    TIMSK2 = 1<<TOIE2; /* �I�[�o�[�t���[���荞�݋��� */
}



/***************************************************
 *  �^�C�}�[1�ɂ��p���X�J�E���^�[
 ***************************************************/


ISR( TIMER1_CAPT_vect )
{
	count++;
}

void palsetimer_init( void )
{
	MTR_DDR = 0b11111110;   /* �|�[�gPB0����͐ݒ� */

	TCCR1B  = 0x42;         /* ICNC1:�ߊl�N�����͒[�𗧂��オ��ɐݒ�, CS1:8���� */
	TIMSK1  = _BV(ICIE1);   /* �^�C�}1�C���v�b�g�L���v�`�����荞�݋��� */
}




/***************************************************
 *  7�Z�OLED�\������
 ***************************************************/

/* �Z�O�����g�p�e�[�u�� */
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
 * 7seg �\������ 
 *
 */

void dig_init( void )
{
	/* �o�̓��[�h�ɐݒ� */
	DIG_DDR   =   ((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));
	SEG_DDR0  = 0x3F;
	SEG_DDR1  = 0xFF;
	/* �������@PIN�o�� LOW */
	DIG_PORT  &= ~((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));  /* &= 0x0F */
	SEG_PORT0 = 0x00;
	SEG_PORT1 = 0x00;
}


/* 
 * �@�\: 7seg����莞�ԕ\��������
 *
 * ����:
 *   LED_INTRVL �Ŏw�肳���J�E���g���̎��Ԃ���4��LED���_�C�i�~�b�N�_������B
 * ���̂Ƃ��A���[�^�[���͂�RPM�Z�o�̂��߁A1[sec]�Œ�Ƃ��Ă���
 *
 * ����:
 *   DIG->PORT�̏��ŏo�͂����Ȃ��Ɛ������_�����Ȃ��B
 *   �ꌅ�_����A��U�������Ă��玟�̌��Ɉڂ�Ȃ��ƁA
 * �O�̌���segment���������܂܂ɂȂ��Ă��܂����ۂ����������B
 *
 */
void dig_drive( void )
{
	int dig_num = 0;

	/* PORT���� */
	DIG_PORT  &= ~((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));  /* &= 0xF0 */
	SEG_PORT0 = 0x00;
	SEG_PORT1 = 0x00;

	/* �o�̓��[�h */
	DIG_DDR  |= (1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4);
	SEG_DDR0 = 0xFF;
	SEG_DDR1 = 0xFF;

	/* ���̊ԂɃ^�C�}�[�J�E���g�����s */
	while( wait_time < LED_INTRVL )
	{
		DIG_PORT = ( 1<<(dig_num+DIG_BASE) ); /* �����o�� */
		SEG_PORT0 = vram0[dig_num];           /* �Z�O�����g�\���p�����[�^�Z�b�g */
		SEG_PORT1 = vram1[dig_num];

		if( dig_num++ >= 4 )
		//if( dig_num >= 4 )
		{
			dig_num = 0;
		}

		SEG_PORT0 = 0x0;  /* ��U���� */
		SEG_PORT1 = 0x0;
	}
		
	wait_time = 0;          /* �^�C�}�[�N���A */

	/* ���� */
	DIG_DDR &= ~((1<<DIG_1) | (1<<DIG_2) | (1<<DIG_3) | (1<<DIG_4));  /* &= 0xF0 */
	SEG_DDR0 = 0x00;
	SEG_DDR1 = 0x00;

}


/*
 * �@�\: RPM ���Z�O�����g�e�[�u���ɐݒ�
 *
 * ����:
 *   ���\���s���ʒu�̎d�l
 *    ex)  1    2    3    4    : 1234��\��������ꍇ
 *        Pin2 Pin1 Pin0 Pin3
 *
 * ����:
 *   vram0 �Z�O�����ga-f�p
 *   vram1 �Z�O�����gg,h�p
 * 
 * �w�i:
 *   PB0�|�[�g: 1���ڕ\��->�L���v�`�����͂Ƃ�������PB4�֕ύX
 *   PC6�|�[�g: Reset�|�[�g�Ƃ��Ďg�����ߕύXPD0�֕ύX
 *
 */
void dig_putrpm( void )
{
	uint32_t rpm_tmp = rpm;

	/*    1�̈� */
	vram0[3] = digtable0[ rpm_tmp % 10 ];
	vram1[3] = digtable1[ rpm_tmp % 10 ];
	rpm_tmp /= 10;

	/*   10�̈� */
	vram0[0] = digtable0[ rpm_tmp % 10 ];
	vram1[0] = digtable1[ rpm_tmp % 10 ];
	rpm_tmp /= 10;

	/*  100�̈� */
	vram0[1] = digtable0[ rpm_tmp % 10 ];
	vram1[1] = digtable1[ rpm_tmp % 10 ];
	rpm_tmp /= 10;

	/* 1000�̈� */ 
	vram0[2] = digtable0[ rpm_tmp ];
	vram1[2] = digtable1[ rpm_tmp ];

	dig_drive();
}



/************************************
 ***
 *** ���C���֐��i�����RPM��񑪒�j
 ***
 ************************************/

int main( void )
{
    uint16_t mtr_cnt[4] = {0,0,0,0};
	
	cli();

	palsetimer_init(); /* �^�C�}1�C���v�b�g�L���v�`���ݒ� */
	timer2_init();     /* �^�C�}2���荞�ݐݒ� */
	dig_init();        /* �o�̓|�[�g�w��A������ */
    sei();             /* SREG �̐ݒ� - ���荞�ݗL�� */

	rpm   = 4321;
	count = 0;
	wait_time = 0;                /* �^�C�}�[�N���A */

	while(1)
	{
		dig_putrpm();             /* �O�� RPM ��1sec�\���B���̊ԂɃ��[�^�[�J�E���g�B */

		/* RPM ���Z�o
		 * �����l���A5�p���X������1��]�̂��߁A
		 * rpm = mtr_cnt �~ 60 �� (5*�T���v�����Osec)
		 */
		mtr_cnt[sec_cnt] = count;
		count = 0;                /* 1sec�̃J�E���g�J�n */
		rpm = (mtr_cnt[0] + mtr_cnt[1] + mtr_cnt[2] + mtr_cnt[3]) * (60/(5*RPM_SAMPLING_SEC));
		
		sec_cnt++;
		sec_cnt &= 0x3;           /* 0-3���J��Ԃ� */

	}


}

