#ifndef AUDIO_H
#define AUDIO_H
#include "sequencer.h"
#include "fluidsynth/fluidsynth.h"

fluid_synth_t* synth;
fluid_audio_driver_t* adriver;

void play_note(int r) {
    fluid_synth_noteon(synth, 0, HIGHEST_MIDI_NOTE-r, volume);
}

void stop_all_notes() {
    //fluid_synth_all_sounds_off();
    fluid_synth_all_notes_off(synth, 0);
}

void stop_note(int r) {
    fluid_synth_noteoff(synth, 0, HIGHEST_MIDI_NOTE-r);
}

void init_synth() {
    auto settings = new_fluid_settings();
    //fluid_settings_setint(settings, "synth.reverb.active", 0);
    //fluid_settings_setint(settings, "synth.chorus.active", 0);
    synth = new_fluid_synth(settings);
    assert(synth);
    fluid_synth_sfload(synth, "TimGM.sf2", 1);
    adriver = new_fluid_audio_driver(settings, synth);
    assert(adriver);
}

void delete_synth() {
    delete_fluid_audio_driver(adriver);
    delete_fluid_synth(synth);
}

#endif