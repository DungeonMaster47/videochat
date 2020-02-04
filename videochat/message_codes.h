#pragma once

namespace message_codes {
	constexpr unsigned char WAIT = 0xF0;
	constexpr unsigned char READY = 0xF1;
	constexpr unsigned char TEXT = 0xF2;
	constexpr unsigned char FRAME = 0xF3;
	constexpr unsigned char DISCONNECTED = 0xF4;
}