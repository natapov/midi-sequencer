#ifndef AUDIO_H
#define AUDIO_H
WAVEFORMATEX                  wfx[CELL_GRID_NUM_H] = {0};
XAUDIO2_BUFFER             buffer[CELL_GRID_NUM_H] = {0};
IXAudio2SourceVoice* pSourceVoice[CELL_GRID_NUM_H];

inline void stop_sound(int y){
	pSourceVoice[y]->Stop(0);
	pSourceVoice[y]->FlushSourceBuffers();
}

inline void play_sound(int y) {
	stop_sound(y);
	pSourceVoice[y]->SubmitSourceBuffer(&buffer[y], NULL);
	pSourceVoice[y]->Start(0);
}

// Parses a .wav file and loads it into xaudio2 structs 
void load_audio_data(int i) {
	sprintf(buff, "../audio/%d.WAV", CELL_GRID_NUM_H - i);

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
	while(chunks_proccessed < 2){
		if(!ReadFile(audioFile, &chunkType, sizeof(DWORD), &bytesRead, NULL)) break;
		ReadFile(audioFile, &chunkDataSize, sizeof(DWORD), &bytesRead, NULL);
		switch(chunkType){
		case ' tmf':
			ReadFile(audioFile, &wfx[i], chunkDataSize, &bytesRead, NULL);        // Wave format struct
			chunks_proccessed += 1;
			break;
		case 'atad':
			audioData = (BYTE*) malloc(chunkDataSize);
			assert(audioData);
			ReadFile(audioFile, audioData, chunkDataSize, &bytesRead, NULL);      // Read actual audio data
			buffer[i].AudioBytes = chunkDataSize;
			buffer[i].pAudioData = audioData;
			buffer[i].Flags = XAUDIO2_END_OF_STREAM;
			chunks_proccessed += 1;
			break;
		default:
			SetFilePointer(audioFile, chunkDataSize, NULL, FILE_CURRENT);		  // Skip chunk
		}
	}
	assert(chunks_proccessed == 2);
	CloseHandle(audioFile);
}

inline void init_xaudio() {
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	IXAudio2* pXAudio2 = NULL;
	XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	IXAudio2MasteringVoice* pMasterVoice = NULL;
	pXAudio2->CreateMasteringVoice(&pMasterVoice);
	for(int i = 0; i < CELL_GRID_NUM_H; i++) {
		load_audio_data(i);
		pXAudio2->CreateSourceVoice(&pSourceVoice[i], &wfx[i]);
	}
}
#endif