#include "COMport.h"
#define HEADER_SIZE 11

using namespace serialport;

bool SerialPort::comInit(std::wstring port, int baud, bool asyncModeEnabled) {
	if (asyncModeEnabled) {
		std::wstring writtingFinishedName = port;
		std::wstring readingFinishedName = port;
		if (port.size()) {
			writtingFinishedName[0] = 'w';
			readingFinishedName[0] = 'r';
		}
		writtingFinished = CreateEvent(NULL, FALSE, FALSE, (const WCHAR*)writtingFinishedName.c_str());
		if (writtingFinished == NULL) {
			return false;
		}

		readingFinished = CreateEvent(NULL, FALSE, FALSE, (const WCHAR*)readingFinishedName.c_str());
		if (readingFinished == NULL) {
			CloseHandle(writtingFinished);
			writtingFinished = NULL;
			return false;
		}

		comPort = CreateFile((const WCHAR*)port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		if (comPort == INVALID_HANDLE_VALUE) {
			CloseHandle(readingFinished);
			CloseHandle(writtingFinished);
			writtingFinished = NULL;
			readingFinished = NULL;
			return false;
		}
	}
	else {
		comPort = CreateFile((const WCHAR*)port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (comPort == INVALID_HANDLE_VALUE) {
			return false;
		}
	}

	SetupComm(comPort, 1500, 1500);

	COMMTIMEOUTS commTimeouts;
	commTimeouts.ReadIntervalTimeout = 0xFFFFFFFF;
	commTimeouts.ReadTotalTimeoutMultiplier = 0;
	commTimeouts.ReadTotalTimeoutConstant = 0xFFFF;
	commTimeouts.WriteTotalTimeoutMultiplier = 0;
	commTimeouts.WriteTotalTimeoutConstant = 0xFFFF;
	if (!SetCommTimeouts(comPort, &commTimeouts)) {
		CloseHandle(comPort);
		comPort = NULL;
		this->asyncModeEnabled = false;
		if ((readingFinished != NULL) && (readingFinished != NULL)) {
			CloseHandle(readingFinished);
			readingFinished = NULL;
			CloseHandle(writtingFinished);
			writtingFinished = NULL;
		}
		return false;
	}

	DCB comParams;
	memset(&comParams, 0, sizeof(comParams));
	comParams.DCBlength = sizeof(comParams);
	GetCommState(comPort, &comParams);
	comParams.BaudRate = baud;
	comParams.ByteSize = 8;
	comParams.Parity = NOPARITY;
	comParams.StopBits = ONESTOPBIT;
	comParams.fAbortOnError = TRUE;
	comParams.fDtrControl = DTR_CONTROL_DISABLE;
	comParams.fRtsControl = RTS_CONTROL_TOGGLE;
	comParams.fBinary = TRUE;
	comParams.fParity = FALSE;
	comParams.fInX = comParams.fOutX = FALSE;
	comParams.XonChar = 0;
	comParams.XoffChar = 0xff;
	comParams.fErrorChar = FALSE;
	comParams.fNull = FALSE;
	comParams.fOutxCtsFlow = FALSE;
	comParams.fOutxDsrFlow = FALSE;
	comParams.XonLim = 128;
	comParams.XoffLim = 128;
	if (!SetCommState(comPort, &comParams)) {
		CloseHandle(comPort);
		comPort = NULL;
		this->asyncModeEnabled = false;
		if ((readingFinished != NULL) && (readingFinished != NULL)) {
			CloseHandle(readingFinished);
			readingFinished = NULL;
			CloseHandle(writtingFinished);
			writtingFinished = NULL;
		}
		return false;
	}

	this->asyncModeEnabled = asyncModeEnabled;
	return true;
}

void SerialPort::comDisconnect() {
	if (comPort != NULL) {
		CloseHandle(comPort);
		comPort = NULL;
		asyncModeEnabled = false;
		if ((readingFinished != NULL) && (readingFinished != NULL)) {
			CloseHandle(readingFinished);
			readingFinished = NULL;
			CloseHandle(writtingFinished);
			writtingFinished = NULL;
		}
	}
}

bool SerialPort::comWrite(const uint8_t* data, uint32_t dataSize) {
	if (asyncModeEnabled == 1) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	DWORD feedback;
	if (!WriteFile(comPort, data, dataSize, &feedback, 0) || feedback != dataSize) {
		return false;
	}

	return true;
}

//bool SerialPort::comWritePacket(const uint8_t* data, uint32_t dataSize) {
//	if (asyncModeEnabled == 1) {
//		return false;
//	}
//
//	if (comPort == 0) {
//		return false;
//	}
//
//
//	uint8_t* packet = new uint8_t[dataSize + 4];
//	//uint8_t packet[25];
//	uint32_t staffedDataSize = dataSize;
//	uint32_t staffedDataSizeField = staffedDataSize;
//	uint32_t dataSizeField = dataSize;
//
//	packet[0] = 0b01111110;
//	packet[1] = 2; //pc_2 address
//	packet[2] = 1; //pc_1 address
//	//packet[3] = dataSize + 4; //amount of fields
//
//	for (int8_t i = 3; i >= 0; --i) {
//		packet[3 + i] = dataSizeField;
//		dataSizeField = dataSizeField >> 8;
//	}
//
//	for (int8_t i = 3; i >= 0; --i) {
//		packet[7 + i] = staffedDataSizeField;
//		staffedDataSizeField = staffedDataSizeField >> 8;
//	}
//
//	for (size_t i = 0; i < dataSize; i++)
//	{
//		packet[11 + i] = data[i];
//	}
//
//	DWORD feedback;
//	if (!WriteFile(comPort, packet, staffedDataSize + 11, &feedback, 0) || (feedback != (dataSize + 4))) {
//		return false;
//	}
//
//	return true;
//}

bool SerialPort::comWritePacket(const uint8_t* data, uint32_t dataSize) {
	if (asyncModeEnabled == 1) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	//======================Staffing Area======================================================

	uint32_t bitArraySize = 8 * dataSize;
	uint8_t* bitArray = (uint8_t*)malloc(bitArraySize);
	uint32_t newSize;
	uint8_t binaryNumber[] = { 0,0,0,0,0,0,0,0 };
	uint8_t decimalNumber;
	uint8_t j = 0;
	uint32_t bitArrayPointer = 0;

	//==================Initial Bit Array====================
	for (uint32_t i = 0; i < dataSize; i++)
	{
		j = 0;
		decimalNumber = data[i];
		while (decimalNumber > 1) {
			binaryNumber[j] = decimalNumber % 2;
			decimalNumber /= 2;
			j++;
		}
		binaryNumber[j] = decimalNumber;

		for (uint8_t k = 0; k < 8; k++, bitArrayPointer++)
		{
			bitArray[bitArrayPointer] = binaryNumber[7 - k];
			binaryNumber[7 - k] = 0;
		}
	}

	//=================Staffing=============================


	for (uint32_t i = 0; i < bitArraySize - 4; i++)
	{
		if (bitArray[i] && bitArray[i + 1] && bitArray[i + 2] && bitArray[i + 3] && bitArray[i + 4]) {
			bitArraySize++;
			bitArray = (uint8_t*)realloc(bitArray, bitArraySize);
			for (uint32_t j = bitArraySize - 1; j >= i + 6; j--)
			{
				bitArray[j] = bitArray[j - 1];
			}
			bitArray[i + 5] = 0;
		}
	}

	//===============Convert into bytes===================

	newSize = bitArraySize / 8;
	if (bitArraySize % 8) {
		newSize++;
	}

	uint8_t* newBuffer = (uint8_t*)malloc(newSize);

	int remainder = bitArraySize % 8;
	if (remainder) {
		remainder = 8 - remainder;
		bitArraySize += remainder;
		bitArray = (uint8_t*)realloc(bitArray, bitArraySize);
		for (uint8_t i = 0; i < remainder; i++)
		{
			bitArray[bitArraySize - 1 - i] = 0;
		}
	}


	for (uint32_t i = 0; i < bitArraySize; i++)
	{
		decimalNumber = 0;
		for (uint8_t j = 0; j < 8; j++, i++)
		{
			decimalNumber += bitArray[i] * pow(2, 7 - j);
		}

		i--;
		newBuffer[i / 8] = decimalNumber;

	}
	
	//=================End of staffing area====================================================

	uint8_t* packet = new uint8_t[newSize + HEADER_SIZE];
	//uint8_t packet[25];
	uint32_t staffedDataSize = newSize;
	uint32_t staffedDataSizeField = staffedDataSize;
	uint32_t dataSizeField = dataSize;

	packet[0] = 0b01111110;
	packet[1] = 2; //pc_2 address
	packet[2] = 1; //pc_1 address

	for (int8_t i = 3; i >= 0; --i) {
		packet[3 + i] = dataSizeField;
		dataSizeField = dataSizeField >> 8;
	}
	  
	for (int8_t i = 3; i >= 0; --i) {
		packet[7 + i] = staffedDataSizeField;
		staffedDataSizeField = staffedDataSizeField >> 8;
	}

	for (size_t i = 0; i < staffedDataSize; i++)
	{
		packet[11 + i] = newBuffer[i];
	}

	DWORD feedback;
	if (!WriteFile(comPort, packet, staffedDataSize + HEADER_SIZE, &feedback, 0) || (feedback != (staffedDataSize + HEADER_SIZE))) {
		return false;
	}

	return true;
}

bool SerialPort::comReadPacket(uint8_t* data, uint32_t packetSize) {
	DWORD feedback = 0;
	uint8_t attempts = 3;
	//const uint32_t recieveMaxSize = dataSize * 2;
	uint8_t* packet = new uint8_t[packetSize];

	if (asyncModeEnabled == 1) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	while (attempts) {
		attempts--;

		if (ReadFile(comPort, (LPVOID)packet, packetSize, &feedback, 0)) {

			uint8_t* staffedBuffer = new uint8_t[packetSize - HEADER_SIZE];

			for (size_t i = 0; i < packetSize - HEADER_SIZE; i++)
			{
				staffedBuffer[i] = packet[i + HEADER_SIZE];
			}


			//==============================Destaffing area==================================

			uint32_t amountToRecieve = 0;

			for (int32_t i = 3; i < 7; i++) {
				amountToRecieve += packet[i];
				if (i == 6) {
					break;
				}
				amountToRecieve = amountToRecieve << 8;
			}


			uint32_t recievedSize = packetSize - HEADER_SIZE;
			uint32_t initialSize = packetSize;
			uint32_t bitArraySize = 8 * recievedSize;
			uint8_t decimalNumber;
			uint8_t binaryNumber[] = { 0,0,0,0,0,0,0,0 };
			uint32_t bitArrayPointer = 0;
			uint8_t* bitArray = (uint8_t*)malloc(bitArraySize);
			uint8_t j = 0;

			//==================Initial Bit Array====================

			for (uint32_t i = 0; i < recievedSize; i++)
			{
				j = 0;
				decimalNumber = staffedBuffer[i];
				while (decimalNumber > 1) {
					binaryNumber[j] = decimalNumber % 2;
					decimalNumber /= 2;
					j++;
				}
				binaryNumber[j] = decimalNumber;

				for (uint8_t k = 0; k < 8; k++, bitArrayPointer++)
				{
					bitArray[bitArrayPointer] = binaryNumber[7 - k];
					binaryNumber[7 - k] = 0;
				}
			}

			//=================Destaffing=============================


			for (uint32_t i = 0; i < bitArraySize - 4; i++)
			{
				if (bitArray[i] && bitArray[i + 1] && bitArray[i + 2] && bitArray[i + 3] && bitArray[i + 4]) {

					for (uint32_t j = i + 5; j < bitArraySize - 1; j++)
					{
						bitArray[j] = bitArray[j + 1];
					}

					bitArraySize--;
					i += 5;
					bitArray = (uint8_t*)realloc(bitArray, bitArraySize);
				}
			}

			//===============Convert into bytes===================

			//uint8_t* newBuffer = (uint8_t*)malloc(amountToRecieve);

			for (uint32_t i = 0; i < amountToRecieve * 8; i++)
			{
				decimalNumber = 0;
				for (uint8_t j = 0; j < 8; j++, i++)
				{
					decimalNumber += bitArray[i] * pow(2, 7 - j);
				}

				i--;

				data[i / 8] = decimalNumber;
			}


			//=======================End of Destaffing area==================================

			return true;
		}
	}
	return false;
}

bool SerialPort::comRead(uint8_t* data, uint32_t dataSize) {
	DWORD feedback = 0;
	uint8_t attempts = 3;

	if (asyncModeEnabled == 1) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	while (attempts) {
		attempts--;

		if (ReadFile(comPort, (LPVOID)data, dataSize, &feedback, 0)) {
			return true;
		}
	}
	return false;
}

//bool SerialPort::comReadPacket(uint8_t* data, uint32_t dataSize) {
//	DWORD feedback = 0;
//	uint8_t attempts = 3;
//	//const uint32_t recieveMaxSize = dataSize * 2;
//	uint8_t* packet = new uint8_t[dataSize * 2 + 11];
//
//	if (asyncModeEnabled == 1) {
//		return false;
//	}
//
//	if (comPort == 0) {
//		return false;
//	}
//
//	while (attempts) {
//		attempts--;
//
//		if (ReadFile(comPort, (LPVOID)packet, dataSize+11, &feedback, 0)) {
//
//			for (size_t i = 0; i < dataSize; i++)
//			{
//				data[i] = packet[i + 11];
//			}
//
//			return true;
//		}
//	}
//	return false;
//}

bool SerialPort::comWriteAsync(const uint8_t* data, uint32_t dataSize) {
	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = writtingFinished;

	if (asyncModeEnabled == 0) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	DWORD feedback;
	WriteFile(comPort, data, dataSize, &feedback, &overlapped);

	return true;
}

void SerialPort::waitForReadingFinished() {
	WaitForSingleObject(readingFinished, INFINITE);
}

bool SerialPort::comReadAsync(uint8_t* data, uint32_t dataSize) {
	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = readingFinished;

	if (asyncModeEnabled == 0) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	DWORD feedback = 0;

	ReadFile(comPort, (LPVOID)data, dataSize, &feedback, &overlapped);

	return true;
}

void SerialPort::waitForWrittingFinished() {
	WaitForSingleObject(writtingFinished, INFINITE);
}

bool SerialPort::comSetBaudRate(uint32_t baud) {
	DCB comParams;
	if (!GetCommState(comPort, &comParams)) {
		return false;
	}

	comParams.BaudRate = baud;

	if (!SetCommState(comPort, &comParams)) {
		return false;
	}

	return true;
}
