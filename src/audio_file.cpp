#include "audio_file.hpp"
#include <iostream>

AudioFile::AudioFile(char const* path)
{
	if (!SDL_LoadWAV(path, &this->format, &this->data, &this->data_length)) {
		std::cerr << "error loading audio file: " << SDL_GetError() << "\n";
		return;
	}
	this->usable = true;
}

AudioFile::~AudioFile()
{
	SDL_free(this->data);
	SDL_memset(this, 0, sizeof(*this));
}
