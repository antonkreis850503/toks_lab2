#pragma once

#include <string>
#include <cstdint>
#include <Windows.h>
#include <fileapi.h>

namespace serialport {
	class SerialPort {
		HANDLE comPort = NULL;

		HANDLE readingFinished = NULL;

		HANDLE writtingFinished = NULL;

		bool asyncModeEnabled = false;

		bool secondFlagRecieved = false;

		std::wstring portName;

		int baudRate;
	public:
		bool comInit(const std::wstring port, int baud, bool asyncModeEnabled);

		void comDisconnect();

		bool comWrite(const uint8_t* data, uint32_t dataSize);

		bool comRead(uint8_t* data, uint32_t dataSize);

		bool comWriteAsync(const uint8_t* data, uint32_t dataSize);

		bool comWritePacket(const uint8_t* data, uint32_t dataSize);

		void waitForReadingFinished();

		void waitForWrittingFinished();

		bool comReadAsync(uint8_t* data, uint32_t dataSize);

		bool comReadPacket(uint8_t* data, uint32_t dataSize);

		bool comSetBaudRate(uint32_t baud);
	};
}


