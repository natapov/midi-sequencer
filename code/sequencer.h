#ifndef SEQUENCER_H
#define SEQUENCER_H

typedef int Note;
enum : Note {
	Si  = 11,
	La_ = 10,
	La  = 9,
	So_ = 8,
	So  = 7,
	Fa_ = 6,
	Fa  = 5,
	Mi  = 4,
	Re_ = 3,
	Re  = 2,
	Do_ = 1,
	Do  = 0,
};

// CONFIG
// these are mostly sizes of different ui elements 
const int CELLS_PER_BEAT = 8;//this numbers of cells in a beat (a quarter note)
const int CELL_GRID_NUM_H = 36;
const int CELL_GRID_NUM_W = 40 * CELLS_PER_BEAT;
const int MIN_NOTE_LEN = 2; // IN CELLS
const int HIGHEST_MIDI_NOTE = 83; //SI5 


int SCALE = 1;
#define FONT_SIZE  ( SCALE * 20 )
#define CELL_SIZE_W  ( SCALE * 4 )
#define CELL_SIZE_H  ( SCALE * 20 )
#define NOTE_BORDER_SIZE  ( SCALE  )
#define MENU_PADDING  ( SCALE * 5 )
#define BUFFER    (  SCALE * 5 )
#define MENU_BAR  (  FONT_SIZE + MENU_PADDING * 2 )  // this is also the defualt button height 
#define TOP_BAR   (  (MENU_BAR + BUFFER) * 2 )
#define BOTTOM_BAR  ( MENU_BAR + BUFFER * 2 )
#define SIDE_BAR  (  SCALE * 40 )
#define GRID_W  ( CELL_SIZE_W * CELL_GRID_NUM_W + SCALE )
#define GRID_H  ( CELL_SIZE_H * CELL_GRID_NUM_H )
#define WINDOW_W  ( GRID_W + SIDE_BAR )
#define WINDOW_H  ( GRID_H + TOP_BAR + BOTTOM_BAR )
#define LINE_W  ( SCALE ) //this is multiplied by two since a line seperates two cells symetricall 
#define RESIZE_HANDLE_SIZE  ( CELL_SIZE_W * 2 )
#define BPM_BOX_WIDTH  ( SCALE * 8 * 13 )
#define SCALE_BOX_WIDTH  ( SCALE * 16 * 18 )
#define BASE_BOX_WIDTH ( SCALE * 16 * 5 )


// COLORS
const ImU32 BAR_LINE_COL      = IM_COL32(100, 100, 100, 255);
const ImU32 BEAT_LINE_COL     = IM_COL32(40 , 40 , 40 , 255);
const ImU32 PLAYHEAD_COL      = IM_COL32(250, 250, 0  , 180);
const ImU32 NOTE_COL          = IM_COL32(200, 200, 100, 255);
const ImU32 NOTE_DARKER_COL   = IM_COL32(0,133,255,255);
const ImU32 NOTE_BORDER_COL   = IM_COL32(153,181,255,255);
const ImU32 BLACK_COL         = IM_COL32(0  , 0  , 0  , 255);
const ImU32 BLACK_NOTE_COL    = IM_COL32(10 , 10 , 10 , 255);
const ImU32 WHITE_NOTE_COL    = IM_COL32(250, 250, 250, 255);
const ImU32 GRID_BG_COL       = IM_COL32(75 , 100, 150, 255);

// FLAGS
const ImGuiWindowFlags MAIN_WINDOW_FLAGS =  ImGuiWindowFlags_NoScrollWithMouse | 
											ImGuiWindowFlags_NoBringToFrontOnFocus | 
											ImGuiWindowFlags_NoDecoration | 
											ImGuiWindowFlags_NoResize | 
											ImGuiWindowFlags_NoMove;

const char* regular_note_names[12] = {"DO", "DO#", "RE", "RE#", "MI", "FA", "FA#", "SO", "SO#", "LA", "LA#", "SI"};
const char* english_note_names[12] = {"C",  "C#",  "D",  "D#",  "E",  "F",  "F#",  "G",  "G#",  "A",  "A#",  "B" };

const char* midi_instruments[128] = {"1 Acoustic Grand Piano","2 Bright Acoustic Piano","3 Electric Grand Piano","4 Honky-tonk Piano","5 Electric Piano 1","6 Electric Piano 2","7 Harpsichord","8 Clavinet","9 Celesta","10 Glockenspiel","11 Music Box","12 Vibraphone","13 Marimba","14 Xylophone","15 Tubular Bells","16 Dulcimer","17 Drawbar Organ","18 Percussive Organ","19 Rock Organ","20 Church Organ","21 Reed Organ","22 Accordion","23 Harmonica","24 Tango Accordion","25 Acoustic Guitar (nylon)","26 Acoustic Guitar (steel)","27 Electric Guitar (jazz)","28 Electric Guitar (clean)","29 Electric Guitar (muted)","30 Overdriven Guitar","31 Distortion Guitar","32 Guitar harmonics","33 Acoustic Bass","34 Electric Bass (finger)","35 Electric Bass (pick)","36 Fretless Bass","37 Slap Bass 1","38 Slap Bass 2","39 Synth Bass 1","40 Synth Bass 2","41 Violin","42 Viola","43 Cello","44 Contrabass","45 Tremolo Strings","46 Pizzicato Strings","47 Orchestral Harp","48 Timpani","49 String Ensemble 1","50 String Ensemble 2","51 Synth Strings 1","52 Synth Strings 2","53 Choir Aahs","54 Voice Oohs","55 Synth Voice","56 Orchestra Hit","57 Trumpet","58 Trombone","59 Tuba","60 Muted Trumpet","61 French Horn","62 Brass Section","63 Synth Brass 1","64 Synth Brass 2","65 Soprano Sax","66 Alto Sax","67 Tenor Sax","68 Baritone Sax","69 Oboe","70 English Horn","71 Bassoon","72 Clarinet","73 Piccolo","74 Flute","75 Recorder","76 Pan Flute","77 Blown Bottle","78 Shakuhachi","79 Whistle","80 Ocarina","81 Lead 1 (square)","82 Lead 2 (sawtooth)","83 Lead 3 (calliope)","84 Lead 4 (chiff)","85 Lead 5 (charang)","86 Lead 6 (voice)","87 Lead 7 (fifths)","88 Lead 8 (bass + lead)","89 Pad 1 (new age)","90 Pad 2 (warm)","91 Pad 3 (polysynth)","92 Pad 4 (choir)","93 Pad 5 (bowed)","94 Pad 6 (metallic)","95 Pad 7 (halo)","96 Pad 8 (sweep)","97 FX 1 (rain)","98 FX 2 (soundtrack)","99 FX 3 (crystal)","100 FX 4 (atmosphere)","101 FX 5 (brightness)","102 FX 6 (goblins)","103 FX 7 (echoes)","104 FX 8 (sci-fi)","105 Sitar","106 Banjo","107 Shamisen","108 Koto","109 Kalimba","110 Bag pipe","111 Fiddle","112 Shanai","113 Tinkle Bell","114 Agogo","115 Steel Drums","116 Woodblock","117 Taiko Drum","118 Melodic Tom","119 Synth Drum","120 Reverse Cymbal","121 Guitar Fret Noise","122 Breath Noise","123 Seashore","124 Bird Tweet","125 Telephone Ring","126 Helicopter","127 Applause","128 Gunshot" };

const int HIGHEST_OCTAVE = 5;

//general buffer for temporary strings
const int BUFF_SIZE = MAX_PATH;
char buff[BUFF_SIZE]; 

// GLOBAL VARIABLES 
bool english_notes = false; //user-modifiable variable
const char** note_names = english_notes ? english_note_names : regular_note_names;

//we use glfwGetTime(). it returns a double of seconds since startup 
typedef double Time;
Time elapsed = 0; //Seconds played since last loop
Time last = 0; //Time taken last frame

int last_played_grid_col = -1;//this is only global because it needs to be reset in reset()
int playhead_offset = 0; //The playhead is the line that moves when we press play, this is it's location on the grid

// Internal state variables
int note_histogram[12]; //Sum total of each note in all the matching scales
int number_of_matching_scales = 0;
int max_r_cell;
int drawn_notes[12]; //The number of notes of each type currently on the grid
int total_drawn_notes = 0;

bool snap_to_grid    = true;
bool need_prediction_update = true;
bool is_grid_hovered = false;

// User-modifiable variables:
int bpm              = 220; //beats per minute
int volume           = 110;
int instrument       = 0;
int note_length_idx  = 2;
int beats_per_bar    = 4; // the time signiture of the music
int grid_note_length = 8; // the size of notes the user is currently drawing, in cells
bool predict_mode    = true;
bool auto_loop       = true;
bool shortcut_window = false;
bool playing         = false;

bool snap_to_grid_setting = true;
bool toggle_snap_to_grid  = false;;

//selector variables
Note selected_base_note = -1; 
int selected_scale_idx  = -1;

// headers 
inline int row_to_note(int n);
inline void reset();
#endif