#pragma once
#include <SDL3/SDL.h>

class AudioFile {
private:
	bool usable = false;
	SDL_AudioSpec format;
	uint8_t* data;
	uint32_t data_length;

public:
	AudioFile(char const* path);
	~AudioFile();
	bool is_usable() { return this->usable; }
	SDL_AudioSpec const& get_format() { SDL_assert(this->usable); return this->format; }
	uint8_t const* get_data() { SDL_assert(this->usable); return this->data; }
	uint32_t get_data_length() { SDL_assert(this->usable); return this->data_length; }
};
