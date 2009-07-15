/* This code is based on code from the ScummVM project
 * Copyright (C) 2001  Ludvig Strigeus
 * Copyright (C) 2001-2006 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include <cmath>
#include <samplerate.h>
#include <SDL_mixer.h>

#include "StdDef.h"

#include "Log.h"
#include "VocFile.h"

using namespace std;

enum {
    VOC_CODE_TERM = 0,
    VOC_CODE_DATA,
    VOC_CODE_CONT,
    VOC_CODE_SILENCE,
    VOC_CODE_MARKER,
    VOC_CODE_TEXT,
    VOC_CODE_LOOPBEGIN,
    VOC_CODE_LOOPEND,
    VOC_CODE_EXTENDED,
    VOC_CODE_DATA_16,
    NUM_SAMPLES_OF_SILENCE = 250
};


/**
 * Take a sample rate parameter as it occurs in a VOC sound header, and
 * return the corresponding sample frequency.
 *
 * This method has special cases for the standard rates of 11025 and 22050 kHz,
 * which due to limitations of the format, cannot be encoded exactly in a VOC
 * file. As a consequence, many game files have sound data sampled with those
 * rates, but the VOC marks them incorrectly as 11111 or 22222 kHz. This code
 * works around that and "unrounds" the sampling rates.
 */
uint32_t getSampleRateFromVOCRate(uint8_t vocSR) {
	if (vocSR == 0xa5 || vocSR == 0xa6) {
		return 11025;
	} else if (vocSR == 0xd2 || vocSR == 0xd3) {
		return 22050;
	} else {
		int sr = 1000000L / (256L - vocSR);
		// inexact sampling rates occur e.g. in the kitchen in Monkey Island,
		// very easy to reach right from the start of the game.
		//warning("inexact sample rate used: %i (0x%x)", sr, vocSR);
		return sr;
	}
}

/**
	This method decodes a voc-file and returns a pointer the decoded data. This memory is
	allocated with malloc() and should be freed with free(). The size of the decoded data is
	returned through the parameter size and the sampling rate of this voc-file is returned
	through the parameter rate.
	The kind of voc-files that this function can decode is very restricted. Only voc-files
	with 8-bit unsigned sound samples, with no loops, no silence blocks, no extended blocks
	and no markers are supported.
	\param	rwop	An SDL_RWop that contains the voc-file
	\param	size	The size of the decoded data in bytes
	\param	rate	The sampling rate of the voc-file
	\return	A pointer to a memory block that contains the data. (Free it with free() when no longer needed)
*/
static uint8_t *LoadVOC_RW(SDL_RWops* rwop, uint32_t &size, uint32_t &rate) {
	uint8_t description[20];
	uint16_t offset;
	uint16_t version;
	uint16_t id;
	
	if(SDL_RWread(rwop,(char*) description,20,1) != 1) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid header!");
		return NULL;
	}

	if (memcmp(description, "Creative Voice File", 19) != 0) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid header!");
		return NULL;
	}
	
	if (description[19] != 0x1A) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid header!");
		return NULL;
	}
	
	if(SDL_RWread(rwop,&offset,sizeof(offset),1) != 1) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid header!");
		return NULL;
	}
	offset = SwapLE16(offset);
	
	if(SDL_RWread(rwop,&version,sizeof(version),1) != 1) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid header!");
		return NULL;
	}
	version = SwapLE16(version);
	
	if(SDL_RWread(rwop,&id,sizeof(id),1) != 1) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid header!");
		return NULL;
	}
	id = SwapLE16(id);
	
	if(offset != sizeof(description) + sizeof(offset) + sizeof(version) + sizeof(id)) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid datablock offset in header!");
		return NULL;
	}
	
	// 0x100 is an invalid VOC version used by German version of DOTT (Disk) and
	// French version of Simon the Sorcerer 2 (CD)
	if(!(version == 0x010A || version == 0x0114 || version == 0x0100)) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid version (0x%X) in header!",version);
		return NULL;
	}
	
	if(id != (~version + 0x1234)) {
		LOG_ERROR("VocFile", "loadVOCFromStream: Invalid id in header!");
		return NULL;
	}
	
	int len;
	uint8_t *ret_sound = NULL;
	size = 0;
	
	uint8_t code;
	rate = 0;
	while (SDL_RWread(rwop,&code,sizeof(uint8_t),1) == 1) {
		if(code == VOC_CODE_TERM) {
			return ret_sound;
		}
	
		uint8_t tmp[3];
		if(SDL_RWread(rwop,tmp,1,3) != 3) {
			LOG_ERROR("VocFile", "loadVOCFromStream: Invalid block length!");
			return ret_sound;
		}
		len = tmp[0];
		len |= tmp[1] << 8;
		len |= tmp[2] << 16;

		switch (code) {
			case VOC_CODE_DATA: {
				uint8_t time_constant;
				if(SDL_RWread(rwop,&time_constant,sizeof(uint8_t),1) != 1) {
					LOG_ERROR("VocFile", "loadVOCFromStream: Cannot read time constant!");
					return ret_sound;
				}
					
				uint8_t packing;
				if(SDL_RWread(rwop,&packing,sizeof(uint8_t),1) != 1) {
					LOG_ERROR("VocFile", "loadVOCFromStream: Cannot read packing!");
					return ret_sound;
				}
				len -= 2;
				uint32_t tmp_rate = getSampleRateFromVOCRate(time_constant);
				if((rate != 0) && (rate != tmp_rate)) {
					LOG_ERROR("VocFile", "This voc-file contains data blocks with different sampling rates: old rate: %d, new rate: %d",rate,tmp_rate);
				}
				rate = tmp_rate;
				
				//LOG_ERROR("VocFile", "VOC Data Block: Rate: %d, Packing: %d, Length: %d\n", rate, packing, len);
				
				if (packing == 0) {
					if (size) {
						ret_sound = (uint8_t *)realloc(ret_sound, size + len);
					} else {
						ret_sound = (uint8_t *)malloc(len);
					}
					
					if(SDL_RWread(rwop,ret_sound + size,1,len) != len) {
						LOG_ERROR("VocFile", "loadVOCFromStream: Cannot read data!");
						return ret_sound;
					}
					
					size += len;
				} else {
					LOG_ERROR("VocFile", "VOC file packing %d unsupported!\n", packing);
				}
			} break;
			
			case VOC_CODE_SILENCE: {
				uint16_t SilenceLength;
				if(SDL_RWread(rwop,&SilenceLength,sizeof(uint16_t),1) != 1) {
					LOG_ERROR("VocFile", "loadVOCFromStream: Cannot read silence length!");
					return ret_sound;
				}
				SilenceLength = SwapLE16(SilenceLength);
				
				uint8_t time_constant;
				if(SDL_RWread(rwop,&time_constant,sizeof(uint8_t),1) != 1) {
					LOG_ERROR("VocFile", "loadVOCFromStream: Cannot read time constant!");
					return ret_sound;
				}
				
				uint32_t SilenceRate = getSampleRateFromVOCRate(time_constant);
				
				
				uint32_t length = 0;
				if(rate != 0) {
					length = (uint32_t) ((((double) SilenceRate)/((double) rate)) * SilenceLength) + 1;
				} else {
					LOG_ERROR("VocFile", "The silence in this voc-file is right at the beginning. Therefore it is not possible to adjust the silence sample rate to the sample rate of the other sound data in this file!");
					length = SilenceLength; 
				}
				
				if (size) {
					ret_sound = (uint8_t *)realloc(ret_sound, size + length);
				} else {
					ret_sound = (uint8_t *)malloc(length);
				}
				
				memset(ret_sound + size,0x80,length);
				
				size += length;
				
			} break;
			
			case VOC_CODE_CONT:
			case VOC_CODE_MARKER:
			case VOC_CODE_TEXT:
			case VOC_CODE_LOOPBEGIN:
			case VOC_CODE_LOOPEND:
			case VOC_CODE_EXTENDED:
			case VOC_CODE_DATA_16:
			default:
				LOG_ERROR("VocFile", "Unhandled code in VOC file : %d", code);
				return ret_sound;
		}
	}
	return ret_sound;
return 0;
}

inline uint8_t Float2uint8_t(float x) {
	int val = lround(x*127.0 + 128.0);
	if(val < 0) {
		val = 0;
	} else if (val > 255) {
		val = 255;
	}
	
	return (uint8_t) val;
}

inline Sint8 Float2Sint8(float x) {
	int val = lround(x*127.0);
	if(val < -128) {
		val = -128;
	} else if (val > 127) {
		val = 127;
	}
	
	return (Sint8) val;
}

inline uint16_t Float2uint16_t(float x) {
	int val = lround(x*32767.0 + 32768.0);
	if(val < 0) {
		val = 0;
	} else if (val > 65535) {
		val = 65535;
	}
	
	return (uint16_t) val;
}

inline Sint16 Float2Sint16(float x) {
	int val = lround(x*32767.0);
	if(val < -32768) {
		val = -32768;
	} else if (val > 32767) {
		val = 32767;
	}
	
	return (Sint16) val;
}

Mix_Chunk* LoadVOC_RW(SDL_RWops* rwop, int freesrc) {
	if(rwop == NULL) {
		return NULL;
	}

	// Read voc file
	uint32_t RawData_Frequency;
	uint32_t RawData_Samples;
	uint8_t* RawDatauint8_t = LoadVOC_RW(rwop, RawData_Samples, RawData_Frequency);
	if(RawDatauint8_t == NULL) {
		if(freesrc) {
			SDL_RWclose(rwop);
		}
		return NULL;
	}

	// Convert to floats
	float* RawDataFloat;
	if((RawDataFloat = (float*) malloc((RawData_Samples+2*NUM_SAMPLES_OF_SILENCE)*sizeof(float))) == NULL) {
		free(RawDatauint8_t);
		if(freesrc) {
			SDL_RWclose(rwop);
		}
		return NULL;
	}
	
	for(uint32_t i=0; i < NUM_SAMPLES_OF_SILENCE; i++) {
		RawDataFloat[i] = 0.0;
	}
	
	for(uint32_t i=NUM_SAMPLES_OF_SILENCE; i < RawData_Samples+NUM_SAMPLES_OF_SILENCE; i++) {
		RawDataFloat[i] = (((float) RawDatauint8_t[i-NUM_SAMPLES_OF_SILENCE])/128.0) - 1.0;
	}
	
	for(uint32_t i=RawData_Samples+NUM_SAMPLES_OF_SILENCE; i < RawData_Samples + 2*NUM_SAMPLES_OF_SILENCE; i++) {
		RawDataFloat[i] = 0.0;
	}
	
	free(RawDatauint8_t);
	
	RawData_Samples += 2*NUM_SAMPLES_OF_SILENCE;
	
	// To prevent strange invalid read in src_linear
	RawData_Samples--;
	
	// Get audio device specifications
	int TargetFrequency, channels;
	uint16_t TargetFormat;
	if(Mix_QuerySpec(&TargetFrequency, &TargetFormat, &channels) == 0) {
		free(RawDatauint8_t);
		free(RawDataFloat);
		if(freesrc) {
			SDL_RWclose(rwop);
		}
		return NULL;
	}
	
	// Convert to audio device frequency
	float ConversionRatio = ((float) TargetFrequency) / ((float) RawData_Frequency);
	uint32_t TargetDataFloat_Samples = (uint32_t) ((float) RawData_Samples * ConversionRatio) + 1;
	float* TargetDataFloat;
	if((TargetDataFloat = (float*) malloc(TargetDataFloat_Samples*sizeof(float))) == NULL) {
		free(RawDataFloat);
		if(freesrc) {
			SDL_RWclose(rwop);
		}
		return NULL;
	}
	
	SRC_DATA src_data;
	src_data.data_in = RawDataFloat;
	src_data.input_frames = RawData_Samples;
	src_data.src_ratio = ConversionRatio;
	src_data.data_out = TargetDataFloat;
	src_data.output_frames = TargetDataFloat_Samples;

	if(src_simple(&src_data, SRC_LINEAR, 1) != 0) {
		free(RawDataFloat);
		free(TargetDataFloat);
		if(freesrc) {
			SDL_RWclose(rwop);
		}
		return NULL;
	}
	
	uint32_t TargetData_Samples = src_data.output_frames_gen;
	free(RawDataFloat);
	
	
	// Equalize if neccessary
	float distance = 0.0;
	for(uint32_t i=0; i < TargetData_Samples; i++) {
		if(fabs(TargetDataFloat[i]) > distance) {
			distance = fabs(TargetDataFloat[i]);
		}
	}
	
	if(distance > 1.0) {
		//Equalize
		for(uint32_t i=0; i < TargetData_Samples; i++) {
			TargetDataFloat[i] = TargetDataFloat[i] / distance;
		}
	}
	

	// Convert floats back to integers but leave out 3/4 of silence
	int ThreeQuaterSilenceLength = (int) ((NUM_SAMPLES_OF_SILENCE * ConversionRatio)*(3.0/4.0));
	TargetData_Samples -= 2*ThreeQuaterSilenceLength;
	
	Mix_Chunk* myChunk;
	if((myChunk = (Mix_Chunk*) calloc(sizeof(Mix_Chunk),1)) == NULL) {
		free(TargetDataFloat);
		if(freesrc) {
			SDL_RWclose(rwop);
		}
		return NULL;
	}
	
	myChunk->volume = 128;
	myChunk->allocated = 1;	
	
	switch(TargetFormat) {
		case AUDIO_U8:
		{
			uint8_t* TargetData;
			int SizeOfTargetSample = sizeof(uint8_t) * channels;
			if((TargetData = (uint8_t*) malloc(TargetData_Samples * SizeOfTargetSample)) == NULL) {
				free(TargetDataFloat);
				free(myChunk);
				if(freesrc) {
					SDL_RWclose(rwop);
				}
				return NULL;
			}
			
			for(uint32_t i=0; i < TargetData_Samples*channels; i+=channels) {
				TargetData[i] = Float2uint8_t(TargetDataFloat[(i/channels)+ThreeQuaterSilenceLength]);
				for(int j = 1; j < channels; j++) {
					TargetData[i+j] = TargetData[i];
				}
				
			}
			
			free(TargetDataFloat);
			
			myChunk->abuf = (uint8_t*) TargetData;
			myChunk->alen = TargetData_Samples * SizeOfTargetSample;
			
		} break;
		
		case AUDIO_S8:
		{
			Sint8* TargetData;
			int SizeOfTargetSample = sizeof(Sint8) * channels;
			if((TargetData = (Sint8*) malloc(TargetData_Samples * SizeOfTargetSample)) == NULL) {
				free(TargetDataFloat);
				free(myChunk);
				if(freesrc) {
					SDL_RWclose(rwop);
				}
				return NULL;
			}
			
			for(uint32_t i=0; i < TargetData_Samples*channels; i+=channels) {
				TargetData[i] = Float2Sint8(TargetDataFloat[(i/channels)+ThreeQuaterSilenceLength]);
				for(int j = 1; j < channels; j++) {
					TargetData[i+j] = TargetData[i];
				}
				
			}
			
			free(TargetDataFloat);
			
			myChunk->abuf = (uint8_t*) TargetData;
			myChunk->alen = TargetData_Samples * SizeOfTargetSample;
			
		} break;
				
		case AUDIO_U16LSB:
		{
			uint16_t* TargetData;
			int SizeOfTargetSample = sizeof(uint16_t) * channels;
			if((TargetData = (uint16_t*) malloc(TargetData_Samples * SizeOfTargetSample)) == NULL) {
				free(TargetDataFloat);
				free(myChunk);
				if(freesrc) {
					SDL_RWclose(rwop);
				}
				return NULL;
			}
			
			for(uint32_t i=0; i < TargetData_Samples*channels; i+=channels) {
				TargetData[i] = SwapLE16(Float2uint16_t(TargetDataFloat[(i/channels)+ThreeQuaterSilenceLength]));
				for(int j = 1; j < channels; j++) {
					TargetData[i+j] = TargetData[i];
				}
				
			}
			
			free(TargetDataFloat);
			
			myChunk->abuf = (uint8_t*) TargetData;
			myChunk->alen = TargetData_Samples * SizeOfTargetSample;
			
		} break;
				
        case AUDIO_S16LSB:
		{
			Sint16* TargetData;
			int SizeOfTargetSample = sizeof(Sint16) * channels;
			if((TargetData = (Sint16*) malloc(TargetData_Samples * SizeOfTargetSample)) == NULL) {
				free(TargetDataFloat);
				free(myChunk);
				if(freesrc) {
					SDL_RWclose(rwop);
				}
				return NULL;
			}
			
			for(uint32_t i=0; i < TargetData_Samples*channels; i+=channels) {
				TargetData[i] = SwapLE16(Float2Sint16(TargetDataFloat[(i/channels)+ThreeQuaterSilenceLength]));
				for(int j = 1; j < channels; j++) {
					TargetData[i+j] = TargetData[i];
				}
				
			}
			
			free(TargetDataFloat);
			
			myChunk->abuf = (uint8_t*) TargetData;
			myChunk->alen = TargetData_Samples * SizeOfTargetSample;
			
		} break;
		
        case AUDIO_U16MSB:
		{
			uint16_t* TargetData;
			int SizeOfTargetSample = sizeof(uint16_t) * channels;
			if((TargetData = (uint16_t*) malloc(TargetData_Samples * SizeOfTargetSample)) == NULL) {
				free(TargetDataFloat);
				free(myChunk);
				if(freesrc) {
					SDL_RWclose(rwop);
				}
				return NULL;
			}
			
			for(uint32_t i=0; i < TargetData_Samples*channels; i+=channels) {
				TargetData[i] = SwapBE16(Float2uint16_t(TargetDataFloat[(i/channels)+ThreeQuaterSilenceLength]));
				for(int j = 1; j < channels; j++) {
					TargetData[i+j] = TargetData[i];
				}
				
			}
			
			free(TargetDataFloat);
			
			myChunk->abuf = (uint8_t*) TargetData;
			myChunk->alen = TargetData_Samples * SizeOfTargetSample;
			
		} break;
		
		case AUDIO_S16MSB:
		{
			Sint16* TargetData;
			int SizeOfTargetSample = sizeof(Sint16) * channels;
			if((TargetData = (Sint16*) malloc(TargetData_Samples * SizeOfTargetSample)) == NULL) {
				free(TargetDataFloat);
				free(myChunk);
				if(freesrc) {
					SDL_RWclose(rwop);
				}
				return NULL;
			}
			
			for(uint32_t i=0; i < TargetData_Samples*channels; i+=channels) {
				TargetData[i] = SwapBE16(Float2Sint16(TargetDataFloat[(i/channels)+ThreeQuaterSilenceLength]));
				for(int j = 1; j < channels; j++) {
					TargetData[i+j] = TargetData[i];
				}
				
			}
			
			free(TargetDataFloat);
			
			myChunk->abuf = (uint8_t*) TargetData;
			myChunk->alen = TargetData_Samples * SizeOfTargetSample;

		} break;
		
		default:
		{
			free(TargetDataFloat);
			free(myChunk);
			if(freesrc) {
				SDL_RWclose(rwop);
			}
			return NULL;
		} break;
	}
	
	if(freesrc) {
		SDL_RWclose(rwop);
	}

return myChunk;
}
