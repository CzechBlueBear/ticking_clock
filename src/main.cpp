#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>

// Define the sample rate and buffer size (for generated waveforms)
const int SAMPLE_RATE = 44100;
const int BUFFER_SIZE = 2048;

// Interval between tick-tock sounds, in milliseconds
const int TICKTOCK_INTERVAL = 1000;

const std::string ASSET_DIR = "./assets/";

SDL_AudioDeviceID audio_device;
SDL_AudioStream* the_audio;

const SDL_AudioSpec DEFAULT_AUDIO_SPEC = {
	.format = SDL_AUDIO_S16,
	.channels = 2,
	.freq = SAMPLE_RATE
};

struct SoundWave {
	SDL_AudioSpec audio_spec = DEFAULT_AUDIO_SPEC;
	uint8_t* data = nullptr;
	uint32_t byte_size = 0;

	SoundWave() = default;
	SoundWave(SoundWave const& other) = delete;
	~SoundWave() { SDL_free(data); }
	static SoundWave* with_buffer(uint32_t byte_size);
	static SoundWave* from_file(std::string path);
	static SoundWave* from_sinewave(float frequency);

	/// Enqueue the sound to be played (can be done multiple times at once).
	void enqueue();
};

SoundWave* SoundWave::with_buffer(uint32_t byte_size)
{
	auto wave = new SoundWave;
	wave->data = static_cast<uint8_t*>(SDL_calloc(1, byte_size));
	if (!wave->data) {
		std::cerr << "error: SDL_calloc(): " << SDL_GetError() << " (wanted " << byte_size << " bytes)\n";
		delete wave;
		return nullptr;
	}
	wave->byte_size = byte_size;
	return wave;
}

SoundWave* SoundWave::from_file(std::string path)
{
	auto wave = new SoundWave;
	if (!SDL_LoadWAV(path.c_str(), &wave->audio_spec, &wave->data, &wave->byte_size)) {
		std::cerr << "error: SDL_LoadWAV(): " << SDL_GetError() << "\n";
		delete wave;
		return nullptr;
	}
	return wave;
}

SoundWave* SoundWave::from_sinewave(float frequency)
{
	auto wave = SoundWave::with_buffer(BUFFER_SIZE);
	if (!wave) {
		return nullptr; // alloc failed, message already printed
	}
	auto buffer = reinterpret_cast<int16_t*>(wave->data);
	auto count = wave->byte_size/2;	// 2 bytes per each int16_t value
	for (int i = 0; i < count; ++i) {
		buffer[i] = (int16_t)(32767 * sinf(2 * M_PI * frequency * i / SAMPLE_RATE));
	}
	return wave;
}

void SoundWave::enqueue()
{
    if (!SDL_PutAudioStreamData(the_audio, data, byte_size)) {
    	std::cerr << "error: SDL_PutAudioStreamData(): " << SDL_GetError() << "\n";
    }
}

static bool quit_requested = false;

int main(int argc, char *argv[]) {

	if (!SDL_Init(SDL_INIT_AUDIO|SDL_INIT_EVENTS)) {
		std::cerr << "error: SDL_Init(): " << SDL_GetError() << "\n";
		return -1;
	}

	// ensure SDL_Quit() is called at the end, even if we exit unexpectedly
	atexit(SDL_Quit);

	std::unique_ptr<SoundWave> tick_sound { SoundWave::from_file(ASSET_DIR + "/tick.wav") };
	std::unique_ptr<SoundWave> tock_sound { SoundWave::from_file(ASSET_DIR + "/tock.wav") };
	std::unique_ptr<SoundWave> bell_sound { SoundWave::from_file(ASSET_DIR + "/bell.wav") };
	std::unique_ptr<SoundWave> short_bell_sound { SoundWave::from_file(ASSET_DIR + "/short_bell.wav") };
	if (!tick_sound || !tock_sound || !bell_sound || !short_bell_sound) {
		return -1;	// error was already reported during loading
	}

	audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &DEFAULT_AUDIO_SPEC);
	if (!audio_device) {
		std::cerr << "error: SDL_OpenAudioDevice(): " << SDL_GetError() << "\n";
		return -1;
	}

	the_audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &tick_sound->audio_spec, nullptr, nullptr);
	if (!the_audio) {
		std::cerr << "error: SDL_OpenAudioDeviceStream(): " << SDL_GetError() << "\n";
		return -1;
	}

    // Start audio playback
	if (!SDL_ResumeAudioStreamDevice(the_audio)) {
		std::cerr << "error: SDL_ResumeAudioStreamDevice(): " << SDL_GetError() << "\n";
		return -1;
	}

	bool tick_flipflop = false;

	// we want sound the bell appropriate number of times per whole hour,
	// but only once per whole hour
	bool bell_already_sounded = false;

	Uint32 start_tick = SDL_GetTicks();
	while (!quit_requested) {
		Uint32 current_tick = SDL_GetTicks();

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				goto out;
			}
		}

		if ((current_tick - start_tick) >= TICKTOCK_INTERVAL) {

			// alternate between "tick" and "tock"
			tick_flipflop = !tick_flipflop;

			// enqueue a tick/tock sound but do not get into another sound (the bell)
			if (SDL_GetAudioStreamAvailable(the_audio) == 0) {
				if (tick_flipflop) {
					tick_sound->enqueue();
				} else {
					tock_sound->enqueue();
				}
			}

			start_tick = current_tick;

			// look what the real time it is, for bell purposes
			SDL_Time real_time;
			if (!SDL_GetCurrentTime(&real_time)) {
				std::cerr << "error: SDL_GetCurrentTime(): " << SDL_GetError() << "\n";
				return -1;
			}
			SDL_DateTime date_time;
			SDL_TimeToDateTime(real_time, &date_time, true);
			std::cout << date_time.hour << ":" << date_time.minute << "\r" << std::flush;

			// a full hour?
			if (date_time.minute == 0) {
				if (!bell_already_sounded) {
					bell_already_sounded = true;

					// enqueue the appropriate number of strikes (the last one is long)
					for (int i=0; i<date_time.hour-1; i++) {
						short_bell_sound->enqueue();
					}
					bell_sound->enqueue();
				}
			}
			else if (date_time.minute == 30) {
				if (!bell_already_sounded) {
					bell_already_sounded = true;

					// a single strike at half of an hour
					bell_sound->enqueue();
				}
			}
			else {

				// arm the bell for the next full hour
				bell_already_sounded = false;
			}
		}

		SDL_Delay(10); // Small delay to prevent CPU hogging
	}

out:
	// Cleanup
	SDL_CloseAudioDevice(audio_device);
	SDL_DestroyAudioStream(the_audio);
	return 0;
}
