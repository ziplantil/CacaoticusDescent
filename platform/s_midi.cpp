/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License,
as described in copying.txt.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <mutex>

#include "platform/i_sound.h"
#include "platform/i_midi.h"
#include "s_midi.h"
#include "s_sequencer.h"
//#include "mem/mem.h" //[ISB] not thread safe
#include "misc/byteswap.h"
#include "misc/error.h"
#include "platform/timer.h"

//[ISB] Uncomment to enable MIDI file diagonstics. Extremely slow on windows. And probably linux tbh.
//Will probably overflow your console buffer, so make it really long if you must
//#define DEBUG_MIDI_READING

//#define DEBUG_SPECIAL_CONTROLLERS

int CurrentDevice = 0;

char SoundFontFilename[256] = "TestSoundfont.sf2";
MidiSynth* synth = nullptr;
MidiSequencer* sequencer = nullptr;
std::thread* midiThread = nullptr;

#ifdef DEBUG_MIDI_READING
char* EventNames[] = { "Unknown 0", "Unknown 1", "Unknown 2", "Unknown 3", "Unknown 4", "Unknown 5",
"Unknown 6", "Unknown 7", "Note Off", "Note On", "Aftertouch", "Controller", "Patch", "Pressure",
"Pitch", "Sysex" };

void S_PrintEvent(midievent_t* ev)
{
	if (ev->type == 0xFF || ev->type == EVENT_CONTROLLER)
	{
		if (ev->type != 0xFF)
		{
			fprintf(stderr, "  Event Type: %s\n  Affected channel: %d\n", EventNames[ev->type], ev->channel);
		}
		else
		{
			fprintf(stderr, "  Event Type: Meta\n");
		}
		fprintf(stderr, "  Delta: %d\n", ev->delta);

		switch (ev->type)
		{
		case EVENT_NOTEOFF:
		case EVENT_NOTEON:
		case EVENT_AFTERTOUCH:
			fprintf(stderr, "   Note: %d\n   Velocity: %d\n", ev->param1, ev->param2);
			break;
		case EVENT_CONTROLLER:
			fprintf(stderr, "   Controller ID: %d\n   Controller value: %d\n", ev->param1, ev->param2);
			break;
		case EVENT_PATCH:
			fprintf(stderr, "   Patch: %d\n", ev->param1);
			break;
		case EVENT_PITCH:
			fprintf(stderr, "   Pitch: %d\n", ev->param1 + (ev->param2 << 8));
			break;
		case 0xff:
			fprintf(stderr, "   Type: %d\n   Length: %d\n", ev->param1, ev->dataLength);
			break;
		}
	}
}

#endif

int S_InitMusic(int device)
{
	if (CurrentDevice != 0) return 0; //already initalized
	//[ISB] TODO: I really need to add a switcher to allow the use of multiple synths. Agh
	//I_InitMIDI();
	//MidiSynth* synth = nullptr;
	switch (device)
	{
	case _MIDI_GEN:
	{
#ifdef USE_FLUIDSYNTH
		MidiFluidSynth* fluidSynth = new MidiFluidSynth();
		if (fluidSynth == nullptr)
		{
			Error("S_InitMusic: Fatal: Cannot allocate fluid synth.");
			return 1;
		}
		fluidSynth->SetSampleRate(I_GetPreferredMIDISampleRate());
		fluidSynth->SetSoundfont(SoundFontFilename);
		synth = (MidiSynth*)fluidSynth;
#else
		synth = new DummyMidiSynth();
#endif
	}
		break;
	}
	if (synth == nullptr)
	{
		Warning("S_InitMusic: Unknown device.\n");
		return 1;
	}
	sequencer = new MidiSequencer(synth, I_GetPreferredMIDISampleRate());

	if (sequencer == nullptr)
	{
		Error("S_InitMusic: Fatal: Cannot allocate sequencer.");
		return 1;
	}
	//Start the platform-dependent MIDI system
	if (I_StartMIDI(sequencer))
	{
		Warning("S_InitMusic: I_StartMIDI failed.");
		return 1;
	}

	CurrentDevice = device;

	return 0;
}

void S_ShutdownMusic()
{
	if (CurrentDevice == 0) return;
	I_ShutdownMIDI();
	/*if (player != nullptr)
	{
		player->Shutdown();
	}*/
	if (synth)
	{
		synth->Shutdown();
		delete synth;
	}
	if (sequencer)
	{
		delete sequencer;
	}
	CurrentDevice = 0;
}

int S_ReadDelta(int* pointer, uint8_t* data)
{
	int done = 0, position = 0, value = 0;
	uint8_t b;
	while (!done)
	{
		b = data[*pointer];
		if (b & 0x80) //We're done now
		{
			done = 1;
			b &= 0x7F;
		}
		value += (b << (position * 7));
		position++;
		*pointer = *pointer + 1;
	}
	return value;
}

int S_ReadMIDIDelta(int* pointer, uint8_t* data)
{
	int done = 0, position = 0, value = 0;
	uint8_t b;
	while (!done)
	{
		b = data[*pointer];
		if ((b & 0x80) == 0) //We're done now
		{
			done = 1;
		}
		b &= 0x7F;
		value += (b << (position * 7));
		position++;
		*pointer = *pointer + 1;
	}
	return value;
}

int S_ReadHMPChunk(int pointer, uint8_t* data, midichunk_t* chunk, hmpheader_t* song)
{
	int done, oldpointer, position, value, i;
	int command;
	uint8_t b;
	int eventCount = 0;
	int basepointer = pointer; //[ISB] this code really needs to be made less terrible tbh

	chunk->chunkNum = BS_MakeInt(&data[pointer]);
	chunk->chunkLen = BS_MakeInt(&data[pointer + 4]);
	chunk->chunkTrack = BS_MakeInt(&data[pointer + 8]);

	//Count events. Couldn't we have gotten something useful in the header like an event count? eh
	pointer += 12; oldpointer = pointer;
	while (pointer < (basepointer + chunk->chunkLen))
	{
		done = 0; position = 0; value = 0;
		while (!done)
		{
			b = data[pointer];
			if (b & 0x80) //We're done now
			{
				done = 1;
				b &= 0x7F;
			}
			value += (b << (position * 7));
			position++;
			pointer++;
		}
		b = data[pointer]; pointer++;
		if (b == 0xff)
			command = 0xff;
		else
			command = (b >> 4) & 0xf;

		switch (command) //For now, just advance the pointer enough
		{
		case EVENT_NOTEOFF:
		case EVENT_NOTEON:
		case EVENT_AFTERTOUCH:
		case EVENT_CONTROLLER:
		case EVENT_PITCH:
			pointer += 2;
			break;
		case EVENT_PATCH:
		case EVENT_PRESSURE:
			pointer += 1;
			break;
		case 0xff:
 			pointer++; //skip command
			value = S_ReadMIDIDelta(&pointer, data); pointer += value;
			break;
		default:
			fprintf(stderr, "Unknown event %d in hmp file", command);
			break;
		}

		eventCount++;
	}

	//Actually allocate and read events now
	chunk->numEvents = eventCount;
	chunk->events = (midievent_t*)malloc(sizeof(midievent_t) * eventCount);
	if (chunk->events == NULL)
	{
		Error("Out of memory allocating a chunk's events");
		return pointer;
	}
	memset(chunk->events, 0, sizeof(midievent_t) * eventCount);

	if (chunk->events == nullptr)
	{
		fprintf(stderr, "Can't allocate MIDI events");
		exit(1);
	}

	pointer = oldpointer;

	for (i = 0; i < eventCount; i++)
	{
		done = 0; position = 0; value = 0;
		memset(&chunk->events[i], 0, sizeof(midievent_t));

		value = S_ReadDelta(&pointer, data);
		chunk->events[i].delta = (uint64_t)value * (I_GetPreferredMIDISampleRate() / song->bpm);
		b = data[pointer]; pointer++;
		if (b == 0xff)
			command = 0xff;
		else
			command = (b >> 4) & 0xf;
		chunk->events[i].type = command;
		chunk->events[i].channel = b & 0xf;

		switch (command) //Actually load the params now
		{
		case EVENT_NOTEOFF:
		case EVENT_NOTEON:
		case EVENT_AFTERTOUCH:
		case EVENT_CONTROLLER:
		case EVENT_PITCH:
			chunk->events[i].param1 = data[pointer++];
			chunk->events[i].param2 = data[pointer++];

			if (command == EVENT_CONTROLLER)
			{
				switch (chunk->events[i].param1)
				{
				case HMI_CONTROLLER_GLOBAL_LOOP_START:
				{
					int j;
					if (song->loopStart == 0) //descent 1 game02.hmp has two loop points. On different tracks, so probably need to match them.
					{
						for (j = 0; j < i; j++)
						{
							song->loopStart += chunk->events[j].delta;
						}
					}
#ifdef DEBUG_SPECIAL_CONTROLLERS
					printf("Loop start on track %d channel %d, event %d with delta %d. Loop start at %d, loop count %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, song->loopStart, chunk->events[i].param2);
#endif
				}
				break;
#ifdef DEBUG_SPECIAL_CONTROLLERS
				case HMI_CONTROLLER_GLOBAL_LOOP_END:
					printf("Loop end restore enable on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
				break;
				case HMI_CONTROLLER_ENABLE_CONTROLLER_RESTORE:
					printf("Controller restore enable on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
				break;
				case HMI_CONTROLLER_DISABLE_CONTROLLER_RESTORE:
					printf("Controller restore disable on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCAL_BRANCH_POS:
					printf("Local branch pos on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCAL_BRANCH:f 
					printf("Local branch on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_GLOBAL_BRANCH_POS:
					printf("Global branch pos on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_GLOBAL_BRANCH:
					printf("Global branch on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCAL_LOOP_START:
					printf("Local loop pos on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_CALL_TRIGGER:
					printf("Call trigger (but why) on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCK_CHAN:
					printf("Channel lock on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_CHAN_PRIORITY:
					printf("Channel priority on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
#endif
				}
			}
			break;
		case EVENT_PATCH:
		case EVENT_PRESSURE:
			chunk->events[i].param1 = data[pointer++];
			break;
		case 0xff:
			chunk->events[i].param1 = data[pointer++];
			chunk->events[i].dataLength = S_ReadMIDIDelta(&pointer, data);
			if (chunk->events[i].dataLength != 0)
			{
				chunk->events[i].data = (uint8_t*)malloc(chunk->events[i].dataLength * sizeof(uint8_t));
				memcpy(chunk->events[i].data, &data[pointer], chunk->events[i].dataLength); pointer += chunk->events[i].dataLength;
			}
			break;
		default:
			fprintf(stderr, "Unknown event %d in hmp file", command);
			break;
		}
	}

	//Ready the chunk for sequencing
	if (chunk->numEvents != 0)
	{
		chunk->nextEvent = 0;
		chunk->nextEventTime = chunk->events[0].delta;
		//chunk->nextEventTime = 0;
	}

	return pointer;
}

void S_FreeHMPData(hmpheader_t* song)
{
	int i, j;
	midichunk_t* chunk;
	midievent_t* ev;
	for (i = 0; i < song->numChunks; i++)
	{
		chunk = &song->chunks[i];
		//Need to check for special events. Agh
		for (j = 0; j < chunk->numEvents; j++)
		{
			ev = &chunk->events[j];
			if (ev->dataLength != 0)
			{
				free(ev->data);
			}
		}
		free(chunk->events);
	}
	free(song->chunks);
}

uint16_t S_StartSong(int length, uint8_t* data, bool loop, uint32_t* handle)
{
	if (CurrentDevice == 0) return 1;
	hmpheader_t* song = (hmpheader_t*)malloc(sizeof(hmpheader_t));
	if (S_LoadHMP(length, data, song)) 
	{
		*handle = 0xFFFF;
		//Failed to load MIDI, oops
		free(song);
		return 1;
	}
	*handle = 0; //heh
	I_StartMIDISong(song, loop);
	return 0;
}

uint16_t S_StopSong()
{
	if (CurrentDevice == 0) return 1;
	
	I_StopMIDISong();
	return 0;
}

int S_LoadHMP(int length, uint8_t* data, hmpheader_t* song)
{
	int i, pointer;
	memcpy(song->header, data, 16); //Check for version 1 HMP
	if (strcmp(song->header, "HMIMIDIP"))
	{
		return 1;
	}
	song->length = BS_MakeInt(&data[32]);
	song->numChunks = BS_MakeInt(&data[48]);
	song->bpm = BS_MakeInt(&data[56]);
	//printf("bpm %d\n", song->bpm);
	song->seconds = BS_MakeInt(&data[60]);
	song->loopStart = 0;

	song->chunks = (midichunk_t*)malloc(sizeof(midichunk_t) * song->numChunks);
	if (song->chunks == nullptr)
	{
		Error("Can't allocate MIDI chunk list\n");
		return 1;
	}
	memset(song->chunks, 0, sizeof(midichunk_t) * song->numChunks);

	pointer = 776;
	for (i = 0; i < song->numChunks; i++)
	{
		pointer = S_ReadHMPChunk(pointer, data, &song->chunks[i], song);
	}

	//DEBUG
	HMPFile test = HMPFile(length, data);

	return 0;
}

HMPTrack::HMPTrack(int chunknum, int tracknum)
{
	chunkNum = chunknum;
	num = tracknum;

	//Set basic sequencing data
	nextEvent = 0;
	nextEventTime = 0;

	//Create empty event list.
	events = std::vector<midievent_t>();
}

void HMPTrack::StartSequence()
{
	if (events.size() > 0)
	{
		nextEvent = 0;
		nextEventTime = events[0].delta;
	}
	else
		nextEvent = -1;
}

HMPFile::HMPFile(int len, uint8_t* data)
{
	int branchTableOffset, pointer, i;

	memcpy(header, data, 32); //Check for version 1 HMP

	branchTableOffset = BS_MakeInt(&data[32]);
	//3 dummy dwords here
	numChunks = BS_MakeInt(&data[48]);
	ticksPerQuarter = BS_MakeInt(&data[52]);
	tempo = BS_MakeInt(&data[56]);
	seconds = BS_MakeInt(&data[60]);

	//Up next would be device mapping information, but this isn't useful for Descent ATM.
	//It might become more vital when emulation of other sound cards is implemented.

	tracks = std::vector<HMPTrack>();
	//Jump forward and read all the chunks.
	pointer = 776; //Pointer is adjusted by the reading function
	for (i = 0; i < numChunks; i++)
	{
		pointer = ReadChunk(pointer, data);
	}
}

int HMPFile::ReadChunk(int ptr, uint8_t* data)
{
	uint8_t b;
	int command, delta, tick;
	int basePointer = ptr;
	int destPointer;
	int chunkNum = BS_MakeInt(&data[ptr]);
	int chunkLen = BS_MakeInt(&data[ptr + 4]);
	int chunkTrack = BS_MakeInt(&data[ptr + 8]);
	ptr += 12;

	//Chunk length contains the header fields.
	destPointer = basePointer + chunkLen;

	HMPTrack track = HMPTrack(chunkNum, chunkTrack);

	tick = 0; //Temp hack for loop point

#ifdef DEBUG_MIDI_READING
	fprintf(stderr, "Chunk %d:\n Length: %d\n Track: %d\n Events:\n", chunkNum, chunkLen, chunkTrack);
#endif

	while (ptr < destPointer) 
	{
		midievent_t ev;
		delta = S_ReadDelta(&ptr, data);
		b = data[ptr]; ptr++;
		tick += delta;

		if (b == 0xff)
			command = 0xff;
		else
			command = (b >> 4) & 0xf;

		ev.type = command;
		ev.channel = b & 0xf;
		ev.delta = delta;

		switch (command) //Actually load the params now
		{
		case EVENT_NOTEOFF:
		case EVENT_NOTEON:
		case EVENT_AFTERTOUCH:
		case EVENT_CONTROLLER:
		case EVENT_PITCH:
			ev.param1 = data[ptr++];
			ev.param2 = data[ptr++];

			if (command == EVENT_CONTROLLER)
			{
				switch (ev.param1)
				{
				case HMI_CONTROLLER_GLOBAL_LOOP_START:
				{
					int j;
					if (loopStart == 0) //descent 1 game02.hmp has two loop points. On different tracks, so probably need to match them.
					{
						loopStart = tick;
					}
#ifdef DEBUG_SPECIAL_CONTROLLERS
					printf("Loop start on track %d channel %d, event %d with delta %d. Loop start at %d, loop count %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, song->loopStart, chunk->events[i].param2);
#endif
				}
				break;
#ifdef DEBUG_SPECIAL_CONTROLLERS
				case HMI_CONTROLLER_GLOBAL_LOOP_END:
					printf("Loop end restore enable on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_ENABLE_CONTROLLER_RESTORE:
					printf("Controller restore enable on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_DISABLE_CONTROLLER_RESTORE:
					printf("Controller restore disable on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCAL_BRANCH_POS:
					printf("Local branch pos on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCAL_BRANCH:f
					printf("Local branch on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_GLOBAL_BRANCH_POS:
					printf("Global branch pos on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_GLOBAL_BRANCH:
					printf("Global branch on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCAL_LOOP_START:
					printf("Local loop pos on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_CALL_TRIGGER:
					printf("Call trigger (but why) on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_LOCK_CHAN:
					printf("Channel lock on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
				case HMI_CONTROLLER_CHAN_PRIORITY:
					printf("Channel priority on track %d channel %d, event %d with delta %d. Specified param %d\n", chunk->chunkNum, chunk->events[i].channel, i, value, chunk->events[i].param2);
					break;
#endif
				}
			}
			break;
		case EVENT_PATCH:
		case EVENT_PRESSURE:
			ev.param1 = data[ptr++];
			break;
		case 0xff:
			ev.param1 = data[ptr++];
			ev.dataLength = S_ReadMIDIDelta(&ptr, data);
			if (ev.dataLength != 0)
			{
				//TODO: Determine if any meta events whatsoever are important for us. 
				/*chunk->events[i].data = (uint8_t*)malloc(chunk->events[i].dataLength * sizeof(uint8_t));*/
				/*memcpy(chunk->events[i].data, &data[pointer], chunk->events[i].dataLength);*/
				ptr += ev.dataLength;
			}
			break;
		default:
			fprintf(stderr, "Unknown event %d in hmp file", command);
			break;
		}

#ifdef DEBUG_MIDI_READING
		S_PrintEvent(&ev);
#endif
		//TODO: Events are passed by value rather than by reference or via a pointer, so they get copied
		//This needs to be changed if dynamic allocation is used for meta events
		track.AddEvent(ev);
	}
	return ptr;
}