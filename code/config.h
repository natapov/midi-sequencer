#ifndef CONFIG_H
#define CONFIG_H

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
int grid_note_length = 8; // the size of notes the user is currently drawing, in cells
bool snap_to_grid    = true;

//general buffer for temporary strings
char buff[300]; 

// CONFIG
// these are mostly sizes of different ui elements 
const int CELLS_PER_BEAT = 8;//this numbers of cells in a beat (a quarter note)

const int CELL_GRID_NUM_H = 35;
const int CELL_GRID_NUM_W = 40 * CELLS_PER_BEAT;
const int CELL_SIZE_W = 4;
const int CELL_SIZE_H = 20;
const int NOTE_BORDER_SIZE = 1;

const int MENU_BAR = 35;
const int TOP_BAR  = 70 + MENU_BAR;
const int SIDE_BAR = 40;
const int GRID_W = CELL_SIZE_W * CELL_GRID_NUM_W + 1;
const int GRID_H = CELL_SIZE_H * CELL_GRID_NUM_H;
const int WINDOW_W = GRID_W + SIDE_BAR;
const int WINDOW_H = GRID_H + TOP_BAR;
const int LINE_W = 1; //this is multiplied by two since a line seperates two cells symetrically

const int RESIZE_HANDLE_SIZE = CELL_SIZE_W * 2;

const int BPM_BOX_WIDTH   = 105;
const int SCALE_BOX_WIDTH = 300;
const int BASE_BOX_WIDTH  = 85;

const int FONT_SIZE = 20;

// COLORS
const ImU32 BAR_LINE_COL    = IM_COL32(100, 100, 100, 255);
const ImU32 BEAT_LINE_COL   = IM_COL32(40 , 40 , 40 , 255);
const ImU32 PLAYHEAD_COL    = IM_COL32(250, 250, 0  , 180);
const ImU32 NOTE_COL        = IM_COL32(38 , 158, 255, 255);
const ImU32 BLACK_COL       = IM_COL32(0  , 0  , 0  , 255);
const ImU32 BLACK_NOTE_COL  = IM_COL32(250, 250, 250, 255);
const ImU32 WHITE_NOTE_COL  = IM_COL32(10 , 10 , 10 , 255);
const ImU32 GRID_BG_COL     = IM_COL32(75 , 100, 150, 255);
const ImU32 NOTE_BORDER_COL = IM_COL32(200, 200, 100, 255);

// FLAGS
const ImGuiWindowFlags MAIN_WINDOW_FLAGS =  ImGuiWindowFlags_NoScrollWithMouse | 
											ImGuiWindowFlags_NoBringToFrontOnFocus | 
											ImGuiWindowFlags_NoDecoration | 
											ImGuiWindowFlags_NoResize | 
											ImGuiWindowFlags_NoMove;

const char* regular_note_names[12] = {"DO", "DO#", "RE", "RE#", "MI", "FA", "FA#", "SO", "SO#", "LA", "LA#", "SI"};
const char* english_note_names[12] = {"C",  "C#",  "D",  "D#",  "E",  "F",  "F#",  "G",  "G#",  "A",  "A#",  "B" };

const int HIGHEST_NOTE = So_; 
const int HIGHEST_NOTE_OCTAVE = 5;

#endif