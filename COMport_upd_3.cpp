#include "COMport.h"
#define HEADER_SIZE 3

using namespace serialport;

bool setParityControl(uint8_t*& data, uint32_t dataSize) {
	uint32_t bitArraySize = 8 * dataSize;
	uint8_t* bitArray = (uint8_t*)malloc(bitArraySize);
	if (!bitArray) {
		return false;
	}
	uint8_t binaryNumber[] = { 0,0,0,0,0,0,0,0 };
	uint8_t decimalNumber;
	uint8_t j = 0;
	uint32_t bitArrayPointer = 0;
	uint32_t bitCounter = 0;
	uint8_t parityByte = 0;

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

	for (uint32_t i = 0; i < bitArraySize; i++)
	{
		if (bitArray[i]) {
			bitCounter++;
		}
	}

	if (bitCounter % 2) {
		parityByte = 1;
	}

	dataSize++;
	data = (uint8_t*)realloc(data, dataSize);
	if (!data) {
		free(bitArray);
		return false;
	}
	data[dataSize - 1] = parityByte;

	//uint8_t check = data[dataSize - 1];
	return true;
}

bool checkParityControl(uint8_t* data, uint32_t dataSize) {
	//uint8_t check = data[dataSize - 1];
	uint32_t bitArraySize = 8 * (dataSize - 1);
	uint8_t* bitArray = (uint8_t*)malloc(bitArraySize);
	uint8_t binaryNumber[] = { 0,0,0,0,0,0,0,0 };
	uint8_t decimalNumber;
	uint8_t j = 0;
	uint32_t bitArrayPointer = 0;
	uint32_t bitCounter = 0;
	uint8_t parityByte = 0;

	for (uint32_t i = 0; i < dataSize-1; i++)
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

	for (uint32_t i = 0; i < bitArraySize; i++)
	{
		if (bitArray[i]) {
			bitCounter++;
		}
	}

	if ((bitCounter % 2) == data[dataSize - 1]) {
		return true;
	}
	return false;
}

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

bool SerialPort::comWritePacket(const uint8_t* data, uint32_t dataSize) {
	if (asyncModeEnabled == 0) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	uint8_t* packet = (uint8_t*)malloc(dataSize + HEADER_SIZE);
	if (!packet) {
		return false;
	}

	packet[0] = 0b01111110;
	packet[1] = 2; //pc_2 address
	packet[2] = 1; //pc_1 address


	for (size_t i = 0; i < dataSize; i++)
	{
		packet[3 + i] = data[i];
	}

	//======================Staffing Area======================================================

	uint32_t bitArraySize = 8 * (dataSize + HEADER_SIZE - 1);
	uint8_t* bitArray = (uint8_t*)malloc(bitArraySize);
	if (!bitArray) {
		free(packet);
		return false;
	}
	uint32_t newSize;
	uint8_t binaryNumber[] = { 0,0,0,0,0,0,0,0 };
	uint8_t decimalNumber;
	uint8_t j = 0;
	uint32_t bitArrayPointer = 0;

	//==================Initial Bit Array====================
	for (uint32_t i = 1; i < dataSize + HEADER_SIZE; i++)
	{
		j = 0;
		decimalNumber = packet[i];
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
			if (!bitArray) {
				free(packet);
				return false;
			}
			for (uint32_t j = bitArraySize - 1; j >= i + 6; j--)
			{
				bitArray[j] = bitArray[j - 1];
			}
			bitArray[i + 5] = 0;
		}
	}

	//===============Convert into bytes===================

	newSize = (bitArraySize / 8) + 1;
	if (bitArraySize % 8) {
		newSize++;
	}

	uint8_t* newPacket = (uint8_t*)malloc(newSize);
	if (!newPacket) {
		free(packet);
		free(bitArray);
		return false;
	}

	int remainder = bitArraySize % 8;
	if (remainder) {
		remainder = 8 - remainder;
		bitArraySize += remainder;
		bitArray = (uint8_t*)realloc(bitArray, bitArraySize);
		if (!bitArray) {
			free(packet);
			free(newPacket);
			return false;
		}
		for (uint8_t i = 0; i < remainder; i++)
		{
			bitArray[bitArraySize - 1 - i] = 0;
		}
	}

	newPacket[0] = 0b01111110;

	for (uint32_t i = 0; i < bitArraySize; i++)
	{
		decimalNumber = 0;
		for (uint8_t j = 0; j < 8; j++, i++)
		{
			decimalNumber += bitArray[i] * pow(2, 7 - j);
		}

		i--;
		newPacket[(i / 8) + 1] = decimalNumber;

	}

	//=================End of staffing area====================================================

	if (!setParityControl(newPacket, newSize)) {
		free(packet);
		free(bitArray);
		if (newPacket) {
			free(newPacket);
		}
		return false;
	}
	//uint8_t check = packet[27];

	DWORD feedback;
	HANDLE writingPacketRecieved = CreateEvent(NULL, FALSE, FALSE, (LPCWSTR)"writingPacketRecieved1");
	if (writingPacketRecieved == NULL) {
		return false;
	}

	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = writingPacketRecieved;	

	WriteFile(comPort, newPacket, newSize + 1, &feedback, &overlapped);
	
	WaitForSingleObject(writingPacketRecieved, INFINITE);

	free(packet);
	free(bitArray);
	free(newPacket);
	CloseHandle(writingPacketRecieved);

	return true;
}

bool SerialPort::comReadPacket(uint8_t* data, uint32_t dataSize) {
	uint32_t packetSize = 1;
	DWORD feedback = 0;
	uint8_t attempts = 3;
	//const uint32_t recieveMaxSize = dataSize * 2;
	uint32_t i = 0;

	if (asyncModeEnabled == 0) {
		return false;
	}

	if (comPort == 0) {
		return false;
	}

	HANDLE readingPacketRecieved = CreateEvent(NULL, FALSE, FALSE, (LPCWSTR)"readingPacketRecieved1");
	if (readingPacketRecieved == NULL) {
		return false;
	}

	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = readingPacketRecieved;

	uint8_t* packet = (uint8_t*)malloc(1);
	if (!packet) {
		CloseHandle(readingPacketRecieved);
		return false;
	}
	uint8_t* byteRecieved = (uint8_t*)malloc(1);
	if (!byteRecieved) {
		free(packet);
		CloseHandle(readingPacketRecieved);
		return false;
	}

	while (1) {
		overlapped.InternalHigh = 0;
		ReadFile(comPort, (LPVOID)byteRecieved, 1, &feedback, &overlapped);
		if (overlapped.InternalHigh != 1) {
			break;
		}
		packetSize++;
		packet = (uint8_t*)realloc(packet, packetSize);
		if (!packet) {
			free(byteRecieved);
			CloseHandle(readingPacketRecieved);
			return false;
		}
		packet[i++] = byteRecieved[0];
	}
	CloseHandle(readingPacketRecieved);

	packetSize--;
	packet = (uint8_t*)realloc(packet, packetSize);
	if (!packet) {
		free(byteRecieved);
		return false;
	}

	uint8_t check = packet[packetSize - 1];

	if (!checkParityControl(packet, packetSize)) {
		free(packet);
		free(byteRecieved);
		return false;
	}

	uint8_t* staffedBuffer = (uint8_t*)malloc(packetSize);
	if (!staffedBuffer) {
		free(packet);
		free(byteRecieved);
		return false;
	}

	for (size_t i = 0; i < packetSize; i++)
	{
		staffedBuffer[i] = packet[i];
	}

	//==============================Destaffing area==================================

	uint32_t recievedSize = packetSize - 2;
	uint32_t bitArraySize = 8 * recievedSize;
	uint8_t decimalNumber;
	uint8_t binaryNumber[] = { 0,0,0,0,0,0,0,0 };
	uint32_t bitArrayPointer = 0;
	uint8_t* bitArray = (uint8_t*)malloc(bitArraySize);
	if (!bitArray) {
		free(packet);
		free(byteRecieved);
		free(staffedBuffer);
		return false;
	}

	uint8_t j = 0;

	//==================Initial Bit Array====================

	for (uint32_t i = 1; i < recievedSize + 1; i++)
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

			for (uint32_t j = i + 5; j < bitArraySize - 2; j++)
			{
				bitArray[j] = bitArray[j + 1];
			}

			bitArraySize--;
			i += 4;
			bitArray = (uint8_t*)realloc(bitArray, bitArraySize);
			if (!bitArray) {
				free(packet);
				free(byteRecieved);
				free(staffedBuffer);
				return false;
			}
		}
	}

	//===============Convert into bytes===================

	//uint8_t* newBuffer = (uint8_t*)malloc(amountToRecieve);

	if ((bitArraySize / 8) < dataSize) {
		dataSize = bitArraySize / 8;
	}

	for (uint32_t i = 16; i < dataSize * 8 + 3; i++)
	{
		decimalNumber = 0;
		for (uint8_t j = 0; j < 8; j++, i++)
		{
			decimalNumber += bitArray[i] * pow(2, 7 - j);
		}

		i--;

		data[(i / 8) - 2] = decimalNumber;
	}

	//=======================End of Destaffing area==================================

	free(packet);
	free(byteRecieved);
	free(staffedBuffer);
	free(bitArray);

	return true;
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
