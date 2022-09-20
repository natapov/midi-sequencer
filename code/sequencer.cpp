#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stdio.h"
#include "GLFW/glfw3.h"
#include "irrKlang.h"
#include "assert.h"
#include "imgui_internal.h"
#include "scales.h"

using namespace ImGui;
using namespace irrklang;
//todo: switch out of predict mode when selecting scale, auto select base note
enum : char{
	state_empty  = 0,
	state_start  = 1,
	state_middle = 2,
	state_end    = 4,
};

typedef int Note;
enum {
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
// these are mostly sizes of diffrent ui elements as well some default values for glabal variables
// todo: dpi scaling
// todo: large scales don't exact match

const int CELLS_PER_BEAT = 8;//this is the minimun note size a beat is typically 1/4 so if CELLS_PER_BEAT = 8 the minimum note is 1/32

const int CELL_GRID_NUM_H = 35;
const int CELL_GRID_NUM_W = 40*CELLS_PER_BEAT;

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
const ImGuiWindowFlags WINDOW_FLAGS = MAIN_WINDOW_FLAGS | ImGuiWindowFlags_NoBackground;
const ImGuiWindowFlags INVISIBLE_WINDOW_FLAGS = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

const char* regular_note_names[12] = {"DO", "DO#", "RE", "RE#", "MI", "FA", "FA#", "SO", "SO#", "LA", "LA#", "SI"};
const char* english_note_names[12] = {"C",  "C#",  "D",  "D#",  "E",  "F",  "F#",  "G",  "G#",  "A",  "A#",  "B" };

const int HIGHEST_NOTE = So_; 
const int HIGHEST_NOTE_OCTAVE = 5;


// GLOBAL VARIABLES 

ImGuiID grid_id;
//Sound library stuff
ISoundSource* snd_src[CELL_GRID_NUM_H];
ISoundEngine* engine;

//general buffer for temporary strings
char buff[1114]; //todo: this doesn't seem like the thing to do

//We use glfw for "Time", glfwGetTime() return a double of seconds since startup 
typedef double Time;
Time elapsed  = 0; //Seconds played since last loop
Time max_time = 0; //Maximum elapsed time value

int last_played_grid_col;// this is only global because it needs to be reset in reset()
int playhead_offset = 0; //The playhead is the line that moves when we press play, this is it's location on the grid

// State variables
int drawn_notes[12]; //The number of notes of each type currently on the grid
int number_of_drawn_notes = 0;
int note_histogram[12]; //Sum total of each note in all the matching scales
int matching_scales_count = 0;
char cell[CELL_GRID_NUM_H][CELL_GRID_NUM_W]; //here we store the currently drawn notes on the grid

// User-modifiable variables:
int bpm = 180; //beats per minute
int grid_note_length = 8; // the size of notes the user is currently drawing, in cells
int note_length_idx  = 2;

int beats_per_bar = 4; // the time signiture of the music
bool playing = false;
bool predict_mode = true;
bool snap_to_grid = true;
bool english_notes = false;
bool debug_window = false;

const char** note_names = english_notes ? english_note_names : regular_note_names;

//selector variables
Note selected_base_note = Do; 
int selected_scale_idx = -1;

// FUNCIONS
//for debugging
inline bool is_note(int n){
	return (n <= Si && n>= Do);
}

bool is_sharp(Note note){
	assert(is_note(note));
	return note == La_ || note == So_ || note == Fa_ || note == Re_ || note == Do_;
}

inline bool scale_has_note(short scale, Note note){
	assert(is_note(note));
	assert((0xf000 & scale) == 0);
	return scale & (1 << note);
}

inline bool scale_contains(short container, short contained){
	return (container & contained) == contained;
}

short scale_rotate(short s, Note base){
	short looped_bits = SCALE_MASK & s <<base;
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

int row_to_note(int n){
	return 11 - ((n + 11 - HIGHEST_NOTE) % 12);
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

void reset(){
	elapsed = 0;
	last_played_grid_col = 0;
}

int resize(int y, int old_x, int cur_x){
	assert(cell[y][old_x] & (state_end|state_start));
	if(cur_x == old_x) return old_x;
	int new_x = -1, i;
	bool is_end = cell[y][old_x] & (state_end);

	if(snap_to_grid){
		cur_x += grid_note_length - cur_x % grid_note_length;
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
	for(int i = 1; i < note_length; i++){
		if(cell[y][x+i] != state_empty) return false;
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

void update_grid(){
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
	ImVec2 mouse_pos = GetMousePos();
	SetNextWindowPos(ImVec2(SIDE_BAR,TOP_BAR));
	SetNextWindowSize(ImVec2(WINDOW_W - SIDE_BAR, WINDOW_H));

	Begin("note grid overlay", NULL, INVISIBLE_WINDOW_FLAGS);
	if(IsWindowHovered()){
		// just make these global todo
		ImGuiIO& io = GetIO();
		
		if(io.MouseWheel){
			note_length_idx -= (int) io.MouseWheel;
			if(note_length_idx < 0) note_length_idx = 0;
			if(note_length_idx > 4) note_length_idx = 4;
			grid_note_length = (CELLS_PER_BEAT*4) >> note_length_idx;
		}
		
		int y = (mouse_pos.y - TOP_BAR)  / CELL_SIZE_H;
		int x = (mouse_pos.x - SIDE_BAR) / CELL_SIZE_W;
		static bool resizing_note = false;
		static int  start_x;
		static int  start_y;
		static bool moving_note = false;
		static int cells_to_left;
		static int cells_to_right;
		//reset mouse cursor / state
		if(!IsMouseDown(0)){
			resizing_note = false;
			moving_note = false;
		}
		
		if((cell[y][x] == (state_middle)) && !moving_note){
			if(IsMouseDown(0)){
				moving_note = true;
				cells_to_left = 0;
				cells_to_right =0;
				int cur_x = x;
				while(cell[y][cur_x] != state_start){
					cur_x--;
					cells_to_left++;
				}
				cur_x = x;
				while(cell[y][cur_x] != state_end){
					cur_x++;
					cells_to_right++;
				}
			}
		}
		//set mouse dragging/resizing state
		else if((cell[y][x] & (state_end|state_start)) && !resizing_note){
			SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			if(IsMouseDown(0)){
				resizing_note = true;
				start_x = x;
				start_y = y;
			}
		}
		
		if(moving_note){
			//move(start_y, start_x, y, x);
		}

//		void move(int& start_y, int& start_x, int y, int x, int cells_to_left, int cells_to_right){

		else if(resizing_note){
			SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			start_x = resize(start_y, start_x, x);
		}

		//erase when right mouse button is pressed
		else if(IsMouseDown(1) && cell[y][x]){
			erase_note(y,x);
		}

		//place note
		else if(IsMouseDown(0) && !cell[y][x] && !IsMouseDown(1)) {
			int start_x = snap_to_grid ? (x - x % grid_note_length) : x;
			if(place_note(y, start_x, grid_note_length))
				 engine->play2D(snd_src[y]);
		}
	}
	End();
}

void draw_one_frame(){
	SetNextWindowSize(ImVec2(WINDOW_W, WINDOW_H));
	SetNextWindowPos(ImVec2(0,0));
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); 
	Begin("main window", NULL, MAIN_WINDOW_FLAGS);
	{// TOP BAR
		SetNextWindowPos(ImVec2(0,MENU_BAR));
		BeginChild("Top Bar", ImVec2(WINDOW_W, TOP_BAR), false, WINDOW_FLAGS);
		PopStyleVar();
		if(BeginMainMenuBar()){
			if(BeginMenu("Settings")){
				if(MenuItem("Predict Mode", NULL, predict_mode)){
					predict_mode = !predict_mode;
					selected_scale_idx = -1;
				}
				if(MenuItem("Snap to grid", NULL, snap_to_grid)){
					snap_to_grid = !snap_to_grid;
				}
				if(MenuItem("English note names", NULL, english_notes)){
					english_notes = !english_notes;
					note_names = english_notes ? english_note_names : regular_note_names;
				}
				if(MenuItem("Show debug window", NULL, debug_window)){
					debug_window = !debug_window;
				}
				ImGui::EndMenu();
			}
			if(BeginMenu("File")){
				if(MenuItem("Export MIDI")){}
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
		const char* note_preview_value = note_names[selected_base_note];  // Pass in the preview value visible before opening the combo (it could be anything)
		SetNextItemWidth(BASE_BOX_WIDTH);
		if(BeginCombo("Base Note", note_preview_value, 0)){
			for(int n = 0; n < 12; n++){
				const bool is_selected = (selected_base_note == n);
				if(Selectable(note_names[n], is_selected))
					selected_base_note = n;
			}
			EndCombo();
		}
		SameLine();

		const char* scale_preview_value;
		if(predict_mode){
			sprintf(buff, "%d Possible scales", matching_scales_count);
			scale_preview_value = buff;
		}
		else{
			if(selected_scale_idx != -1){
				scale_preview_value = scale[selected_scale_idx].name;
			}
			else{
				scale_preview_value = "Select scale";
			}
		}
		//if(scale_set) = scale[selected_scale_idx].name;
		SetNextItemWidth(SCALE_BOX_WIDTH);
		if(BeginCombo("Scale", scale_preview_value, 0)){
			for(int n = 0; n < NUM_SCALES; n++){
				const bool is_selected = (selected_scale_idx == n);
				//we just don't draw scales that don't match
				if(!predict_mode || scale[n].is_matching){
					if(predict_mode && scale[n].number_of_notes == number_of_drawn_notes){
						sprintf(buff, "%s [Exact match!]", scale[n].name);
						if(Selectable(buff, is_selected))
							selected_scale_idx = n;
					}
					else if(Selectable(scale[n].name, is_selected)) 
						selected_scale_idx = n;
				}
			}
			EndCombo();
		}

		//SECOND LINE
		if(Button("Clear All")){
			zero_array(cell);
			reset();
			playing = false;
			number_of_drawn_notes = 0;
			for(auto& it : drawn_notes) it = 0;
		}
		SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH);
		if(InputInt("BPM", &bpm)){
			if(bpm > 999) bpm = 999;
			if(bpm < 10)  bpm =  10;
			max_time = pixels_to_time(GRID_W);
			elapsed  = pixels_to_time(playhead_offset); //how much time _would have_ passed had we gotten to this pixel at current bpm
		}
		SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH*4);
		if(Combo("a", &note_length_idx, "1\0001/2\0001/4\0001/8\0001/16\0\0")){
			grid_note_length = (CELLS_PER_BEAT*4) >> note_length_idx;
		}
		EndChild();
	}

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); 

	{// NOTE COLOUMN
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
		SetNextWindowPos(ImVec2(0, TOP_BAR));
		BeginChild("Note names", ImVec2(WINDOW_W, WINDOW_H - TOP_BAR), false, WINDOW_FLAGS);
		ImDrawList* draw_list = GetWindowDrawList();
		for(int i = 0; i < CELL_GRID_NUM_H; i++){
			Note note = row_to_note(i);
			int octave = HIGHEST_NOTE_OCTAVE - i/12;
			const char* name = NULL;
			assert(is_note(note));
			name = note_names[note];
			
			ImVec4 color;
			//draw grid background
			if(predict_mode){
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
		EndChild();
	}

	

	{// GRID LINES + PLAYHEAD
		SetNextWindowPos(ImVec2(0,TOP_BAR));
		BeginChild("LINES", ImVec2(WINDOW_W, WINDOW_H - TOP_BAR), false, ImGuiWindowFlags_NoMouseInputs | WINDOW_FLAGS); 
		ImDrawList* draw_list = GetWindowDrawList();

		for(int i = 0; i<= CELL_GRID_NUM_H; i++){
			draw_list->AddRectFilled(ImVec2(0, TOP_BAR + CELL_SIZE_H*i - LINE_W), ImVec2(GRID_W + SIDE_BAR, TOP_BAR + CELL_SIZE_H*i + LINE_W), BEAT_LINE_COL);
		}
		for(int i = 0; i<= CELL_GRID_NUM_W; i++){
			if(i % grid_note_length) continue;
			int cells_per_bar = CELLS_PER_BEAT * beats_per_bar;
			ImU32 color = i % cells_per_bar == 0 ? BAR_LINE_COL : BEAT_LINE_COL;
			draw_list->AddRectFilled(ImVec2(SIDE_BAR + CELL_SIZE_W*i - LINE_W, 0), ImVec2(SIDE_BAR + CELL_SIZE_W*i + LINE_W, GRID_H + TOP_BAR), color);
		}
		EndChild();
	}

	{// NOTE GRID
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
		SetNextWindowPos(ImVec2(SIDE_BAR,TOP_BAR));
		BeginChild("note grid", ImVec2(WINDOW_W - SIDE_BAR, WINDOW_H - TOP_BAR), false, WINDOW_FLAGS);
		PushStyleColor(ImGuiCol_Header, GRID_NOTE_COL);
		PushStyleColor(ImGuiCol_Border, NOTE_BORDER_COL);
		for (int y = 0; y < CELL_GRID_NUM_H; y++){
			for (int x = 0; x < CELL_GRID_NUM_W; x++){
				if (x > 0) SameLine();
				Cell(cell[y][x], ImVec2(CELL_SIZE_W, CELL_SIZE_H), LINE_W);
			}
		}
		PopStyleVar();
		PopStyleColor(2);

		//draw playhead
		const int playhead = SIDE_BAR + playhead_offset;
		GetWindowDrawList()->AddRectFilled(ImVec2(playhead - LINE_W, 0), ImVec2(playhead + LINE_W, GRID_H + TOP_BAR), PLAYHEAD_COL);
		EndChild();
	}

	PopStyleVar();// reset window padding
	End();//main window
}

int main(){
	engine = createIrrKlangDevice();
	if(!engine) return 1; // error starting up the engine

	//SetProcessDPIAware();

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
	ImGuiIO& io = GetIO();
	int FONT_SIZE = 20;
	io.Fonts->AddFontFromFileTTF("../Lucida Console Regular.ttf", FONT_SIZE);
	
	ImGuiStyle& style = GetStyle();
	style.WindowBorderSize = 0;

	for(int i = 0; i < CELL_GRID_NUM_H; i++){
		sprintf(buff, "../audio/%d.WAV", CELL_GRID_NUM_H - i);
		snd_src[i] = engine->addSoundSourceFromFile(buff); 
	}

	//We "play" a sound to warm to force the files to be paged in (i think). This avoids latency when playing the first sound.
	engine->play2D(snd_src[0]);
	engine->stopAllSoundsOfSoundSource(snd_src[0]);
	
	// Time variables
	Time current = 0; //seconds since startup
	Time last    = 0; //time last frame
	max_time     = pixels_to_time(GRID_W); //time to reach the end of note grid

	// Main loop
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		NewFrame();
		
		// Keyboard
		if(IsKeyPressed(ImGuiKey_Space)) playing = !playing;
		if(IsKeyPressed(ImGuiKey_Backspace)){
			playing = false;
			reset();
		}
		if(IsKeyPressed(ImGuiKey_Enter)){
			playing = true;
			reset();
		}
		// Time
		current = glfwGetTime();
		if(playing)	elapsed += current - last;
		last = current;

		//loop playhead
		if(elapsed >= max_time)  elapsed -= max_time;
		playhead_offset = time_to_pixels(elapsed);

		for(auto& it : note_histogram) it = 0;
		matching_scales_count = 0;

		short current_notes = notes_array_to_scale(drawn_notes);
		if(predict_mode){
			for(int j = 0; j < 12; j++){
				short note_mask = 1<<j;
				//if(there_is_note_selected && it's not this note) continue;
				for(int i = 0; i < NUM_SCALES; i++){
					if(scale_contains(scale[i].notes, current_notes)){
						matching_scales_count += 1;
						scale[i].is_matching |= note_mask;
						for(int k = 0; k < 12; k++)
							if(scale_has_note(scale[i].notes, k))
								note_histogram[(12 + k - j) % 12] += 1;
					}
					else{
						scale[i].is_matching &= (0xfff - note_mask);
					}
				}
				current_notes = scale_rotate(current_notes, 1);		
			}
		}
		draw_one_frame();
		update_grid();
		// Debug window
		if(debug_window){
			//SetNextWindowFocus();
			Begin("Debug");
			sprintf(buff, "Mouse Wheel: %f", io.MouseWheel);
			Text(buff);
			sprintf(buff, "Elapsed time: %f", elapsed);
			Text(buff);
			sprintf(buff, "Current time: %f", current);
			Text(buff);
			sprintf(buff,"Notes: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", drawn_notes[11], drawn_notes[10], drawn_notes[9], drawn_notes[8], drawn_notes[7], drawn_notes[6], drawn_notes[5], drawn_notes[4], drawn_notes[3], drawn_notes[2], drawn_notes[1], drawn_notes[0]);
			Text(buff);
			sprintf(buff,"Note histogram: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", note_histogram[11], note_histogram[10], note_histogram[9], note_histogram[8], note_histogram[7], note_histogram[6], note_histogram[5], note_histogram[4], note_histogram[3], note_histogram[2], note_histogram[1], note_histogram[0]);
			Text(buff);
			sprintf(buff,"Number of drawn notes: %d", number_of_drawn_notes);
			Text(buff);
			End();
		}

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