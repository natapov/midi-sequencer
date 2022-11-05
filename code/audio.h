#ifndef AUDIO_H
#define AUDIO_H
#include "sequencer.h"
#include "fluidsynth.h"
 
fluid_synth_t* synth;
fluid_audio_driver_t* adriver;

void play_note(int r) {
    fluid_synth_noteon(synth, instrument, 60-r, volume);
}

void stop_all_notes() {
    //fluid_synth_all_sounds_off();
    fluid_synth_all_notes_off(synth, instrument);
}

void stop_note(int r) {
    fluid_synth_noteoff(synth, instrument, 60-r);
}
void load_soundfont() {
    int fluid_res;
    // put your own path here
    fluid_res = fluid_synth_sfload(synth, "../build/GeneralUser GS.sf2", 1);
    assert(fluid_res != FLUID_FAILED);
}

void init_synth() {
    fluid_settings_t* settings;
    settings = new_fluid_settings();
    fluid_settings_setint(settings, "synth.reverb.active", 0);
    fluid_settings_setint(settings, "synth.chorus.active", 0);
    synth = new_fluid_synth(settings);
    assert(synth);

    load_soundfont();
    adriver = new_fluid_audio_driver(settings, synth);
    assert(adriver);
}
 
void delete_synth() {
    delete_fluid_audio_driver(adriver);
    delete_fluid_synth(synth);
}
 

#endif