//=====================================================================//
/*!	@file
	@brief	R8C ADC メイン
	@author	平松邦仁 (hira@rvf-rc45.net)
*/
//=====================================================================//
#include "main.hpp"
#include "system.hpp"
#include "clock.hpp"
#include "port.hpp"
#include "common/format.hpp"

static void wait_(uint16_t n)
{
	while(n > 0) {
		asm("nop");
		--n;
	}
}

static timer_b timer_b_;
static uart0 uart0_;

void putch_(char ch) {
	uart0_.putch(ch);
}

static adc adc_;

extern "C" {
	const void* variable_vectors_[] __attribute__ ((section (".vvec"))) = {
		(void*)brk_inst_,   nullptr,	// (0)
		(void*)null_task_,  nullptr,	// (1) flash_ready
		(void*)null_task_,  nullptr,	// (2)
		(void*)null_task_,  nullptr,	// (3)

		(void*)null_task_,  nullptr,	// (4) コンパレーターB1
		(void*)null_task_,  nullptr,	// (5) コンパレーターB3
		(void*)null_task_,  nullptr,	// (6)
		(void*)null_task_,  nullptr,	// (7) タイマＲＣ

		(void*)null_task_,  nullptr,	// (8)
		(void*)null_task_,  nullptr,	// (9)
		(void*)null_task_,  nullptr,	// (10)
		(void*)null_task_,  nullptr,	// (11)

		(void*)null_task_,  nullptr,	// (12)
		(void*)null_task_,  nullptr,	// (13) キー入力
		(void*)null_task_,  nullptr,	// (14) A/D 変換
		(void*)null_task_,  nullptr,	// (15)

		(void*)null_task_,  nullptr,	// (16)
		(void*)uart0_.send_task, nullptr,   // (17) UART0 送信
		(void*)uart0_.recv_task, nullptr,   // (18) UART0 受信
		(void*)null_task_,  nullptr,	// (19)

		(void*)null_task_,  nullptr,	// (20)
		(void*)null_task_,  nullptr,	// (21) /INT2
		(void*)null_task_,  nullptr,	// (22) タイマＲＪ２
		(void*)null_task_,  nullptr,	// (23) 周期タイマ

		(void*)timer_b_.trb_task,  nullptr,	// (24) タイマＲＢ２
		(void*)null_task_,  nullptr,	// (25) /INT1
		(void*)null_task_,  nullptr,	// (26) /INT3
		(void*)null_task_,  nullptr,	// (27)

		(void*)null_task_,  nullptr,	// (28)
		(void*)null_task_,  nullptr,	// (29) /INT0
		(void*)null_task_,  nullptr,	// (30)
		(void*)null_task_,  nullptr,	// (31)
	};
}

int main(int argc, char *argv[])
{
	using namespace device;

// クロック関係レジスタ・プロテクト解除
	PRCR.PRC0 = 1;

// 高速オンチップオシレーターへ切り替え(20MHz)
// ※ F_CLK を設定する事（Makefile内）
	OCOCR.HOCOE = 1;
	wait_(1000);
	SCKCR.HSCKSEL = 1;
	CKSTPR.SCKSEL = 1;

	// タイマーＢ初期化
	{
		uint8_t ir_level = 2;
		timer_b_.start_timer(60, ir_level);
	}

	// UART の設定 (P1_4: TXD0[in], P1_5: RXD0[in])
	// ※シリアルライターでは、RXD 端子は、P1_6 となっているので注意！
	{
		PMH1E.P14SEL2 = 0;
		PMH1.P14SEL = 1;
		PMH1E.P15SEL2 = 0;
		PMH1.P15SEL = 1;
		uint8_t ir_level = 1;
		uart0_.start(19200, ir_level);
	}

	uart0_.puts("Start R8C ADC\n");

	// ADC の設定（CH1のサイクルモード）
	// port1.b1 の A/D 変換
	{
		PD1.B1 = 0;
		adc_.setup(true, device::adc_io::cnv_type::chanel1, 0);
		adc_.start();
	}

	// L チカ・メイン
	PD1.B0 = 1;
	uint8_t cnt = 0;
	uint32_t nnn = 0;
	while(1) {
		timer_b_.sync();
		++cnt;
		if(cnt >= 30) {
			cnt = 0;
			if(adc_.get_state()) {
				uint16_t v = adc_.get_value(1);
				utils::format("(%d): %1.2:8y[V], %d\n")
					% static_cast<uint32_t>(nnn)
					% static_cast<uint32_t>(((v + 1) * 10) >> 3)
					% static_cast<uint32_t>(v);
				adc_.start();
			}
			++nnn;
		}

		if(cnt < 10) P1.B0 = 1;
		else P1.B0 = 0;

		if(uart0_.length()) {  // UART のレシーブデータがあるか？
			char ch = uart0_.getch();
			uart0_.putch(ch);
		}
	}
}
