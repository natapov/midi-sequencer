#ifndef AUDIO_H
#define AUDIO_H
#include "sequencer.h"

WAVEFORMATEX wfx = {/* .wFormatTag      =*/ 1,
					/* .nChannels       =*/ 2,
					/* .nSamplesPerSec  =*/ 14400,
					/* .nAvgBytesPerSec =*/ 57600,
					/* .nBlockAlign     =*/ 4,
					/* .wBitsPerSample  =*/ 16,
					/* .cbSize          =*/ 0};

XAUDIO2_BUFFER buffer[CELL_GRID_NUM_H] = {0};
IXAudio2* pXAudio2;

// Parses a .wav file and loads it into xaudio2 structs 
void load_audio_data(int i) {
	StringCchPrintf(buff, BUFF_SIZE, "../audio/%d.WAV", CELL_GRID_NUM_H - i);

	HANDLE audioFile = CreateFile(buff, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	assert(audioFile != INVALID_HANDLE_VALUE);
	auto ret = SetFilePointer(audioFile, 0, NULL, FILE_BEGIN);
	assert(ret != INVALID_SET_FILE_POINTER);

	int chunks_proccessed = 0;
	DWORD chunkType;
	DWORD chunkDataSize;
	DWORD fileFormat;
	DWORD bytesRead = 0;

	ReadFile(audioFile, &chunkType, sizeof(DWORD), &bytesRead, NULL);     // First chunk is always RIFF chunk
	assert(chunkType == 'FFIR');                                           
	
	SetFilePointer(audioFile, sizeof(DWORD), NULL, FILE_CURRENT);         // Total data size (we don't use)
	ReadFile(audioFile, &fileFormat, sizeof(DWORD), &bytesRead, NULL);    // WAVE format
	assert(fileFormat == 'EVAW');

	BYTE* audioData;
	while(chunks_proccessed < 1){
		if(!ReadFile(audioFile, &chunkType, sizeof(DWORD), &bytesRead, NULL)) break;
		ReadFile(audioFile, &chunkDataSize, sizeof(DWORD), &bytesRead, NULL);
		if(chunkType == 'atad') {
			audioData = (BYTE*) malloc(chunkDataSize);
			assert(audioData);
			ReadFile(audioFile, audioData, chunkDataSize, &bytesRead, NULL);      // Read actual audio data
			buffer[i].AudioBytes = chunkDataSize;
			buffer[i].pAudioData = audioData;
			buffer[i].Flags = XAUDIO2_END_OF_STREAM;
			chunks_proccessed += 1;
		}
		else {
			SetFilePointer(audioFile, chunkDataSize, NULL, FILE_CURRENT);		  // Skip chunk
		}
	}
	assert(chunks_proccessed == 1);
	CloseHandle(audioFile);
}

inline void init_xaudio() {
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	pXAudio2 = NULL;
	XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	IXAudio2MasteringVoice* pMasterVoice = NULL;
	pXAudio2->CreateMasteringVoice(&pMasterVoice);
	for(int i = 0; i < CELL_GRID_NUM_H; i++) {
		load_audio_data(i);
	}
}
#endif