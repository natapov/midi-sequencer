#include "windows.h"
#include "shellapi.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stdio.h"
#include "GLFW/glfw3.h"
#include "irrKlang.h"
#include "assert.h"
#include "scales.h"

using namespace ImGui;
using namespace irrklang;
//---- Tip: You can add extra functions within the ImGui:: namespace, here or in your own headers files.
/*
namespace ImGui
{
    void MyFunction(const char* name, const MyMatrix44& v);
}
*/
enum : char{
	state_empty  = 0,
	state_start  = 1,
	state_middle = 2,
	state_end    = 4,
};

typedef int Note;
enum{
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

const short SCALE_MASK = 0xfff;

// CONFIG
// these are mostly sizes of diffrent ui elements 
const int CELLS_PER_BEAT = 8;//this is the minimun note size a beat is typically 1/4 so if CELLS_PER_BEAT = 8 the minimum note is 1/32

const int CELL_GRID_NUM_H = 35;
const int CELL_GRID_NUM_W = 40 * CELLS_PER_BEAT;

const int CELL_SIZE_W = 4;
const int CELL_SIZE_H = 20;

const int MENU_BAR = 35;
const int TOP_BAR  = 70 + MENU_BAR;
const int SIDE_BAR = 40;

const int GRID_W = CELL_SIZE_W * CELL_GRID_NUM_W + 1;
const int GRID_H = CELL_SIZE_H * CELL_GRID_NUM_H;
const int WINDOW_W = GRID_W + SIDE_BAR;
const int WINDOW_H = GRID_H + TOP_BAR;
const int LINE_W = 1; //this is multiplied by two since line seperates two cells symetrically

const int BPM_BOX_WIDTH   = 105;
const int SCALE_BOX_WIDTH = 300;
const int BASE_BOX_WIDTH  = 85;

const int FONT_SIZE = 20;

// COLORS
const ImU32 BAR_LINE_COL    = IM_COL32(100, 100, 100, 255);
const ImU32 BEAT_LINE_COL   = IM_COL32(40 , 40 , 40 , 255);
const ImU32 PLAYHEAD_COL    = IM_COL32(250, 250, 0  , 180);
const ImU32 GRID_NOTE_COL   = IM_COL32(38 , 158, 255, 255);
const ImU32 BLACK_COL       = IM_COL32(0  , 0  , 0  , 255);
const ImU32 BLACK_NOTE_COL  = IM_COL32(250, 250, 250, 255);
const ImU32 WHITE_NOTE_COL  = IM_COL32(10 , 10 , 10 , 255);
const ImU32 GRID_BG_COL     = IM_COL32(75 , 100, 150, 255);
const ImU32 NOTE_BORDER_COL = IM_COL32(200, 200, 100, 255);
// FLAGS
const ImGuiWindowFlags MAIN_WINDOW_FLAGS = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

const char* regular_note_names[12] = {"DO", "DO#", "RE", "RE#", "MI", "FA", "FA#", "SO", "SO#", "LA", "LA#", "SI"};
const char* english_note_names[12] = {"C",  "C#",  "D",  "D#",  "E",  "F",  "F#",  "G",  "G#",  "A",  "A#",  "B" };

const int HIGHEST_NOTE = So_; 
const int HIGHEST_NOTE_OCTAVE = 5;

// GLOBAL VARIABLES 

//Sound library stuff
ISoundSource* snd_src[CELL_GRID_NUM_H];
ISoundEngine* engine;

//general buffer for temporary strings
char buff[300]; 

//We use glfw for "Time", glfwGetTime() return a double of seconds since startup 
typedef double Time;
Time elapsed  = 0; //Seconds played since last loop
Time loop_time = 0;

int last_played_grid_col;// this is only global because it needs to be reset in reset()
int playhead_offset = 0; //The playhead is the line that moves when we press play, this is it's location on the grid

// State variables
int drawn_notes[12]; //The number of notes of each type currently on the grid
int number_of_drawn_notes = 0;
int note_histogram[12]; //Sum total of each note in all the matching scales
int matching_scales_count = 0;
char cell[CELL_GRID_NUM_H][CELL_GRID_NUM_W]; //here we store the currently drawn notes on the grid
bool need_prediction_update = true;
int max_x_cell = 0;
bool is_grid_hovered = false;

// User-modifiable variables:
int bpm = 180; //beats per minute
int grid_note_length = 8; // the size of notes the user is currently drawing, in cells
int note_length_idx  = 2;
int beats_per_bar = 4; // the time signiture of the music
bool playing = false;
bool predict_mode = true;
bool snap_to_grid = true;
bool english_notes = false;
bool auto_loop = true;
bool shortcut_window = false;

const char** note_names = english_notes ? english_note_names : regular_note_names;

//selector variables
Note selected_base_note = -1; 
int selected_scale_idx  = -1;

// FUNCTIONS
inline bool is_sharp(Note note){
	return note == La_ || note == So_ || note == Fa_ || note == Re_ || note == Do_;
}

inline bool scale_has_note(short scale, Note note){
	assert((0xf000 & scale) == 0);
	return scale & (1 << note);
}

inline bool scale_contains(short container, short contained){
	return (container & contained) == contained;
}

short scale_rotate(short s, Note base){
	short looped_bits = SCALE_MASK & (s << base);
	s = (s>>(12-base));
	return s|looped_bits;
}

short notes_array_to_scale(int arr[12]){
	short ret = 0;
	for(int i = 11; i >= 0; i--){
		if(arr[i]) ret += 1; 
		ret <<= 1;
	}
	ret >>= 1;
	return ret;
}

inline ImVec4 lerp(ImVec4 a, ImVec4 b, float t){
	return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t); 
}

inline int row_to_note(int n){
	return 11 - ((n + 11 - HIGHEST_NOTE) % 12);
}

inline int snap(int x){
	return (x - x % grid_note_length);
}

void zero_array(char arr[CELL_GRID_NUM_H][CELL_GRID_NUM_W]){
	for (int y = 0; y < CELL_GRID_NUM_H; y++)
		for (int x = 0; x < CELL_GRID_NUM_W; x++)
			arr[y][x] = 0; 
}

inline int time_to_pixels(Time t){
	return (int) ((t * bpm * CELL_SIZE_W*CELLS_PER_BEAT ) / 60.0);
}

inline Time pixels_to_time(int p){
	return (((double)p * 60.0) / (double) (bpm * CELL_SIZE_W*CELLS_PER_BEAT));
}

inline void reset(){
	elapsed = 0;
	last_played_grid_col = 0;
	max_x_cell = -1;
}

inline void clear_selection(){
	selected_scale_idx = -1;
	selected_base_note = -1;
	need_prediction_update = true;
}

inline bool is_scale_selected(){
	return selected_scale_idx != -1 && selected_base_note != -1;
}

void update_elapsed_time(){
	Time loop_time, current = glfwGetTime();
	static Time last;
	if(playing)	 elapsed += current - last;
	last = current;

	playhead_offset = time_to_pixels(elapsed);

	const int cells_in_bar = CELLS_PER_BEAT * beats_per_bar;
	const int max_bar = ((max_x_cell / cells_in_bar) + 1) * cells_in_bar;
	if(max_x_cell == -1 || !auto_loop)
		loop_time = pixels_to_time(GRID_W);
	else
		loop_time = pixels_to_time(max_bar * CELL_SIZE_W);

	if(elapsed >= loop_time) 
		elapsed -= loop_time;
}

int resize_note(int y, int old_x, int cur_x){
	assert(cell[y][old_x] & (state_end|state_start));
	if(cur_x == old_x) return old_x;
	int new_x = -1, i;
	bool is_end = cell[y][old_x] & (state_end);

	if(snap_to_grid){
		cur_x = snap(cur_x) + grid_note_length;
		if(is_end) cur_x -=1;
	}
	for(i = old_x - 1; i >= cur_x; i--){
		if(cell[y][i] & (state_start|state_end)){
			new_x = i+1;
			break;
		}
	}
	if(i == cur_x - 1) new_x = cur_x;
	for(i = old_x + 1; i <= cur_x; i++){
		if(cell[y][i] & (state_start|state_end)){
			new_x = i-1;
			break;
		}
	}
	if(i == cur_x + 1) new_x = cur_x;
	assert(new_x != -1);

	if(is_end){
		for(i = old_x; i > new_x; i--)
			cell[y][i] = state_empty;
		for(i = old_x; i < new_x; i++)
			cell[y][i] = state_middle;
	}
	else{		
		for(i = old_x; i > new_x; i--)
			cell[y][i] = state_middle;
		for(i = old_x; i < new_x; i++)
			cell[y][i] = state_empty;
	}
	
	cell[y][new_x] = is_end ? state_end : state_start;
	return new_x;
}

bool place_note(int y, int x, int note_length){
	if(x<0) return false;
	for(int i = 0; i < note_length; i++){
		if(x+i >= CELL_GRID_NUM_W || cell[y][x+i] != state_empty)
			return false;
	}
	if(!drawn_notes[row_to_note(y)])
		number_of_drawn_notes += 1;
	drawn_notes[row_to_note(y)] += 1;
	assert(note_length > 1);
	cell[y][x] = state_start;
	for(int i = 1; i < note_length-1; i++)
		cell[y][x+i] = state_middle;
	cell[y][x+note_length-1] = state_end;
	return true;
}

void erase_note(int y, int x){
	if(drawn_notes[row_to_note(y)] == 1) 
		number_of_drawn_notes -= 1;
	drawn_notes[row_to_note(y)] -= 1;
	int cur_x;
	if(cell[y][x] != state_end){
		cur_x = x + 1;
		while(true){
			assert(cell[y][cur_x] != state_empty);
			if(cell[y][cur_x] == state_end){
				cell[y][cur_x] = state_empty;
				break;
			}
			cell[y][cur_x] = state_empty;
			cur_x++;
		}
	}
	if(cell[y][x] != state_start){
		cur_x = x - 1;
		while(true){
			assert(cell[y][cur_x] != state_empty);
			if(cell[y][cur_x] == state_start){
				cell[y][cur_x] = state_empty;
				break;
			}
			cell[y][cur_x] = state_empty;
			cur_x--;
		}
	}
	cell[y][x] = state_empty;

}

void select_scale(int n){
	selected_scale_idx = n;
	need_prediction_update = true;
	if((scale[n].is_matching & (1 << selected_base_note)) == 0){
		selected_base_note = -1;
	}
}

void make_scale_prediction(){
	short current_notes = notes_array_to_scale(drawn_notes);
	for(auto& it : note_histogram) it = 0;
	matching_scales_count = 0;
	for(int j = 0; j < 12; j++){
		short note_mask = j ? 1 << (12 - j) : 1;
		for(int i = 0; i < NUM_SCALES; i++){
			if(scale_contains(scale[i].notes, current_notes)){
				scale[i].is_matching |= note_mask;
				//note histogram is limited to selected note
				if(selected_base_note == -1 || selected_base_note == j){
					matching_scales_count += 1;
					for(int k = 0; k < 12; k++)
						if(scale_has_note(scale[i].notes, k))
							note_histogram[(12 + k - j) % 12] += 1;
				}
			}
			else{
				scale[i].is_matching &= (0xfff - note_mask);
			}

		}
		current_notes = scale_rotate(current_notes, 1);		
	}
}

void update_grid(){
	if(is_grid_hovered){
		int mouse_scroll= GetIO().MouseWheel;
		if(mouse_scroll){
			note_length_idx -= mouse_scroll;
			if(note_length_idx < 0) note_length_idx = 0;
			if(note_length_idx > 4) note_length_idx = 4;
			grid_note_length = (CELLS_PER_BEAT*4) >> note_length_idx;
		}
		
		//get grid co-ordinates
		ImVec2 mouse_pos = GetMousePos();
		int y = (mouse_pos.y - TOP_BAR)  / CELL_SIZE_H;
		int x = (mouse_pos.x - SIDE_BAR) / CELL_SIZE_W;
		
		//resizing / moving note variables 
		static bool resizing_note = false;
		static int  last_x;
		static int  last_y;
		static bool moving_note = false;
		static int  cells_to_left;
		static int  total_length;
		
		//reset mouse cursor / state
		if(!IsMouseDown(0)){
			resizing_note = false;
			moving_note = false;
		}
		
		if((cell[y][x] == (state_middle)) && !moving_note && !resizing_note){
			if(IsMouseDown(0)){
				last_x = x;
				last_y = y;
				moving_note = true;
				cells_to_left = 0;		
				int cur_x = x;
				while(cell[y][cur_x] != state_start){
					cur_x--;
					cells_to_left++;
				}
				assert(cell[y][x-cells_to_left] == state_start);
				cur_x = x;
				total_length = cells_to_left + 1;
				while(cell[y][cur_x] != state_end){
					cur_x++;
					total_length++;
				}
			}
		}
		//set mouse dragging/resizing state
		else if((cell[y][x] & (state_end|state_start)) && !resizing_note && !moving_note){
			if(!IsAnyMouseDown())
				SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			if(IsMouseClicked(0)){
				resizing_note = true;
				last_x = x;
				last_y = y;
			}
		}

		if(moving_note && (last_x != x || last_y != y)){
			int note_start_x = snap_to_grid ? snap(x - cells_to_left) : x - cells_to_left;

			//we do nothing if the note hasn't actually moved
			if(!snap_to_grid || note_start_x != last_x - cells_to_left || last_y != y){
				// we erase the note first so it consider itself when checking for space
				erase_note(last_y, last_x);
				if(place_note(y, note_start_x, total_length)){
					last_x = note_start_x + cells_to_left; //this keeps the values consistent when snapping to grid;
					last_y = y;
					need_prediction_update = true;
				}
				else{
					//undo note erase
					auto res = place_note(last_y, last_x - cells_to_left, total_length);
					assert(res);
				}
			}
		}

		else if(resizing_note){
			SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			last_x = resize_note(last_y, last_x, x);
		}

		//erase when right mouse button is pressed
		else if(IsMouseDown(1) && cell[y][x]){
			erase_note(y,x);
			need_prediction_update = true;
		}

		//place note
		else if(IsMouseClicked(0) && !cell[y][x] && !IsMouseDown(1)) {
			int last_x = snap_to_grid ? snap(x) : x;
			if(place_note(y, last_x, grid_note_length)){
				need_prediction_update = true;

				if(!playing)
					engine->play2D(snd_src[y]);
			}
		}
	}
}

inline void handle_keyboard_input(){
	if(IsKeyPressed(ImGuiKey_Space)) playing = !playing;
	if(IsKeyPressed(ImGuiKey_Backspace)){
		playing = false;
		reset();
	}
	if(IsKeyPressed(ImGuiKey_Enter)){
		playing = true;
		reset();
	}
	if( IsKeyPressed(ImGuiKey_LeftShift,  false) || 
		IsKeyReleased(ImGuiKey_LeftShift)        ||
		IsKeyPressed(ImGuiKey_RightShift, false) || 
		IsKeyReleased(ImGuiKey_RightShift)       ){
		snap_to_grid = !snap_to_grid;
	}		

	if(predict_mode && need_prediction_update){
		make_scale_prediction();
		need_prediction_update = false;
	}
}

void draw_one_frame(){
	SetNextWindowSize(ImVec2(WINDOW_W, WINDOW_H));
	SetNextWindowPos(ImVec2(0,0));
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); 
	Begin("main window", NULL, MAIN_WINDOW_FLAGS);
	PopStyleVar();
	{// TOP BAR
		SetCursorPos(ImVec2(0,MENU_BAR));
		if(BeginMainMenuBar()){
			if(BeginMenu("Settings")){
				if(MenuItem("Predict Mode", NULL, predict_mode)){
					predict_mode = !predict_mode;
					if(predict_mode) clear_selection(); //selected scale might not match notes
				}
				if(MenuItem("Snap to grid", NULL, snap_to_grid)){
					snap_to_grid = !snap_to_grid;
				}
				if(MenuItem("English note names", NULL, english_notes)){
					english_notes = !english_notes;
					note_names = english_notes ? english_note_names : regular_note_names;
				}
				if(BeginMenu("Time Signiture")){
					if(MenuItem("1/4", NULL, beats_per_bar == 1)){
						beats_per_bar = 1;
					}
					if(MenuItem("2/4", NULL, beats_per_bar == 2)){
						beats_per_bar = 2;
					}
					if(MenuItem("3/4", NULL, beats_per_bar == 3)){
						beats_per_bar = 3;
					}
					if(MenuItem("4/4", NULL, beats_per_bar == 4)){
						beats_per_bar = 4;
					}
					if(MenuItem("5/4", NULL, beats_per_bar == 5)){
						beats_per_bar = 5;
					}
					ImGui::EndMenu();
				}
				if(MenuItem("Auto loop", NULL, auto_loop)){
					auto_loop = !auto_loop;
				}
				ImGui::EndMenu();
			}
			if(BeginMenu("File")){
				if(MenuItem("Export MIDI")){}
				ImGui::EndMenu();
			}
			if(BeginMenu("Help")){
				if(MenuItem("Keyboard Shortcuts")){
					shortcut_window = true;
				}
				if(MenuItem("Git Repository")){
					ShellExecute(NULL, NULL, "https://github.com/natapov/midi-sequencer", NULL, NULL, SW_SHOWNORMAL);
				}
				ImGui::EndMenu();
			}
			EndMainMenuBar();
		}
		if(MyButton("Play"))  playing = true;
		SameLine();
		if(MyButton("Pause")) playing = false;
		SameLine();
		if(MyButton("Stop")){
			playing = false;
			reset();
		}
		SameLine();
		const char* note_preview_value;

		if(selected_base_note != -1)
			note_preview_value = note_names[selected_base_note];
		else note_preview_value = "---";
		
		if(shortcut_window){
			Begin("Keyboard Shortcuts", &shortcut_window, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			Text("Space - Play/Pause\nBackspace - Stop\nEnter - Play from start\nHold Shift - Toggle snap to grid\nMouse Wheel - Change note length");
			End();
		}
		SetNextItemWidth(BASE_BOX_WIDTH);
		if(BeginCombo("Base Note", note_preview_value, 0)){
			for(int n = 0; n < 12; n++){
				short note_mask = 1<<n;
				const bool is_selected = (selected_base_note == n);
				if(!predict_mode || selected_scale_idx == -1 || (scale[selected_scale_idx].is_matching & note_mask)){
					if(Selectable(note_names[n], is_selected)){
						selected_base_note = n;
						need_prediction_update = true;
					}
				}
			}
			EndCombo();
		}
		SameLine();

		const char* scale_preview_value;
		
		if(selected_scale_idx != -1){
			scale_preview_value = scale[selected_scale_idx].name;
		}
		else if(predict_mode){
			sprintf(buff, "%d Possible scales", matching_scales_count);
			scale_preview_value = buff;
		}
		else{
			scale_preview_value = "Select scale";
		}

		SetNextItemWidth(SCALE_BOX_WIDTH);
		if(BeginCombo("Scale", scale_preview_value, 0)){
			for(int n = 0; n < NUM_SCALES; n++){
				const bool is_selected = (selected_scale_idx == n);
				if(predict_mode){
					if((selected_base_note == -1 && scale[n].is_matching) || (scale[n].is_matching & 1<<selected_base_note)){
						if(scale[n].number_of_notes == number_of_drawn_notes){
							sprintf(buff, "%s [Exact match]", scale[n].name);
							if(Selectable(buff, is_selected)){
								select_scale(n);
							}
						}
						else if(Selectable(scale[n].name, is_selected)){
							select_scale(n);
						}
					}
				}
				else if(Selectable(scale[n].name, is_selected)){
					select_scale(n);
				}
			}
			EndCombo();
		}
		SameLine();
		if(Button("Clear Selection")){
			clear_selection();
		}

		//SECOND LINE
		if(Button("Clear All")){
			zero_array(cell);
			reset();
			playing = false;
			number_of_drawn_notes = 0;
			for(auto& it : drawn_notes) it = 0;
			need_prediction_update = true;
		}
		SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH);
		if(InputInt("BPM", &bpm)){
			if(bpm > 999) bpm = 999;
			if(bpm < 10)  bpm =  10;
			elapsed  = pixels_to_time(playhead_offset); //how much time _would have_ passed had we gotten to this pixel at current bpm
		}
		SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH*4);
		if(Combo("a", &note_length_idx, "1\0001/2\0001/4\0001/8\0001/16\0\0")){
			grid_note_length = (CELLS_PER_BEAT*4) >> note_length_idx;
		}
	}

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); 

	{// NOTE COLOUMN
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
		SetCursorPos(ImVec2(0, TOP_BAR));
		ImDrawList* draw_list = GetWindowDrawList();
		for(int i = 0; i < CELL_GRID_NUM_H; i++){
			Note note = row_to_note(i);
			int octave = HIGHEST_NOTE_OCTAVE - i/12;
			const char* name = NULL;
			assert(note <= Si && note>= Do);
			name = note_names[note];
			
			ImVec4 color;
			//draw grid background
			if(predict_mode && !is_scale_selected()){
				if(number_of_drawn_notes){
					float density = (float) note_histogram[note] / ((float) matching_scales_count);
					color = lerp(ColorConvertU32ToFloat4(BLACK_COL), ColorConvertU32ToFloat4(GRID_BG_COL), density);
					draw_list->AddRectFilled(ImVec2(SIDE_BAR, TOP_BAR + CELL_SIZE_H*i), ImVec2(SIDE_BAR + GRID_W, TOP_BAR + CELL_SIZE_H*(i+1) - 1), ColorConvertFloat4ToU32(color));
				}
			}
			else if(scale_has_note(scale_rotate(scale[selected_scale_idx].notes, selected_base_note), note))
				draw_list->AddRectFilled(ImVec2(SIDE_BAR, TOP_BAR + CELL_SIZE_H*i), ImVec2(SIDE_BAR + GRID_W, TOP_BAR + CELL_SIZE_H*(i+1) - 1), GRID_BG_COL);
			sprintf(buff, "%s%d", name, octave);
			PushStyleColor(ImGuiCol_Text, BLACK_COL);
			PushStyleColor(ImGuiCol_Header, !is_sharp(note) ? BLACK_NOTE_COL : WHITE_NOTE_COL);
			TextBox(buff, ImVec2(SIDE_BAR, CELL_SIZE_H));
			PopStyleColor(2);
		}
		PopStyleVar();
	}

	{// GRID LINES + PLAYHEAD
		ImDrawList* draw_list = GetWindowDrawList();
		for(int i = 0; i<= CELL_GRID_NUM_H; i++){
			draw_list->AddRectFilled(ImVec2(0, TOP_BAR + CELL_SIZE_H*i - LINE_W), ImVec2(GRID_W + SIDE_BAR, TOP_BAR + CELL_SIZE_H*i + LINE_W), BEAT_LINE_COL);
		}
		for(int i = 0; i<= CELL_GRID_NUM_W; i++){
			if(i % grid_note_length) continue;
			int cells_per_bar = CELLS_PER_BEAT * beats_per_bar;
			ImU32 color = i % cells_per_bar == 0 ? BAR_LINE_COL : BEAT_LINE_COL;
			draw_list->AddRectFilled(ImVec2(SIDE_BAR + CELL_SIZE_W*i - LINE_W, TOP_BAR), ImVec2(SIDE_BAR + CELL_SIZE_W*i + LINE_W, GRID_H + TOP_BAR), color);
		}
	}

	{// NOTE GRID
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
		PushStyleColor(ImGuiCol_Header, GRID_NOTE_COL);
		PushStyleColor(ImGuiCol_Border, NOTE_BORDER_COL);
		SetCursorPos(ImVec2(SIDE_BAR,TOP_BAR));
		max_x_cell = -1;
		for(int y = 0; y < CELL_GRID_NUM_H; y++){
			for(int x = 0; x < CELL_GRID_NUM_W; x++){
				if(x > 0) SameLine();
				else SetCursorPos(ImVec2(SIDE_BAR, TOP_BAR + y*CELL_SIZE_H));
				Cell(cell[y][x], ImVec2(CELL_SIZE_W, CELL_SIZE_H), LINE_W);
				if(cell[y][x] && x > max_x_cell){
					max_x_cell = x;
				}
			}
		}
		PopStyleVar();
		PopStyleColor(2);

		//draw playhead
		const int playhead = SIDE_BAR + playhead_offset;
		GetWindowDrawList()->AddRectFilled(ImVec2(playhead - LINE_W, TOP_BAR), ImVec2(playhead + LINE_W, GRID_H + TOP_BAR), PLAYHEAD_COL);
	}

	PopStyleVar();// reset window padding

	SetCursorPos(ImVec2(SIDE_BAR,TOP_BAR));
	InvisibleButton("grid_overlay", ImVec2(WINDOW_W - SIDE_BAR, WINDOW_H), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	is_grid_hovered = IsItemHovered();
	End();//main window
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow){
	engine = createIrrKlangDevice();
	if(!engine) return 1; // error starting up the engine

	// Create window with graphics context
	if(!glfwInit()) return 1;
	GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Project A", NULL, NULL);
	if(!window) return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Dear ImGui setup
	CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");
	GetIO().Fonts->AddFontFromFileTTF("../Lucida Console Regular.ttf", FONT_SIZE);
	
	ImGuiStyle& style = GetStyle();
	style.WindowBorderSize = 0;

	for(int i = 0; i < CELL_GRID_NUM_H; i++){
		sprintf(buff, "../audio/%d.WAV", CELL_GRID_NUM_H - i);
		snd_src[i] = engine->addSoundSourceFromFile(buff); 
	}

	//We "play" a sound to warm to force the files to be paged in (i think). This avoids latency when playing the first sound.
	engine->play2D(snd_src[0]);
	engine->stopAllSoundsOfSoundSource(snd_src[0]);
	
	// Main loop
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		NewFrame();
		
		handle_keyboard_input();
		update_elapsed_time();

		//play notes
		if(playing){
			int current = (playhead_offset / CELL_SIZE_W) % CELL_GRID_NUM_W;
			for(int x = last_played_grid_col; x != current; x = (x+1)%CELL_GRID_NUM_W){
				for(int y = 0; y < CELL_GRID_NUM_H; y++){
					if(cell[y][x] == state_start)
						engine->play2D(snd_src[y]);
				}
			}
			last_played_grid_col = current;
		}

		draw_one_frame();
		update_grid();

		//ShowDemoWindow(NULL);

		// Rendering
		Render();
		ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
		glfwSwapBuffers(window);
	}

	// Cleanup
	engine->drop();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
}