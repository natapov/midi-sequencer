#include "windows.h"
#include "shellapi.h"
#include "xaudio2.h"
#include "Objbase.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "stdio.h"
#include "GLFW/glfw3.h"
#include "assert.h"
#include "scales.h"
#include "grid.h"
#include "config.h"
#include "audio.h"
using namespace ImGui;

// GLOBAL VARIABLES 
bool english_notes = false; //user-modifiable variable
const char** note_names = english_notes ? english_note_names : regular_note_names;

//We use glfw for "Time", glfwGetTime() return a double of seconds since startup 
typedef double Time;
Time elapsed = 0; //Seconds played since last loop

int last_played_grid_col;//this is only global because it needs to be reset in reset()
int playhead_offset = 0; //The playhead is the line that moves when we press play, this is it's location on the grid

// Internal state variables
int note_histogram[12]; //Sum total of each note in all the matching scales
int matching_scales_count = 0;
int max_r_cell;

bool need_prediction_update = false;
bool is_grid_hovered = false;

// User-modifiable variables:
int bpm = 220; //beats per minute
int note_length_idx  = 2;
int beats_per_bar    = 4; // the time signiture of the music
bool playing         = false;
bool predict_mode    = true;
bool auto_loop       = true;
bool shortcut_window = false;

//selector variables
Note selected_base_note = -1; 
int selected_scale_idx  = -1;

// FUNCTIONS

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { 
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); 
}
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { 
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); 
}

//toda //maybe simplify?
void draw_note(int r, int start, int end, ImDrawList* draw_list) {
	const ImVec2 pos =  ImVec2(SIDE_BAR + start * CELL_SIZE_W, TOP_BAR + r * CELL_SIZE_H);
	const ImVec2 size = ImVec2(CELL_SIZE_W * (end - start), CELL_SIZE_H);
	const int nb = NOTE_BORDER_SIZE;
	draw_list->AddRectFilled(pos - ImVec2(nb, nb), pos + size + ImVec2(nb, nb), NOTE_BORDER_COL);
	draw_list->AddRectFilled(pos + ImVec2(nb, nb), pos + size - ImVec2(nb, nb), NOTE_COL);
	//todo: AddRectFilledMultiColor
}

inline bool is_sharp(Note note) {
	return note == La_ || note == So_ || note == Fa_ || note == Re_ || note == Do_;
}

inline bool scale_has_note(short scale, Note note) {
	assert((0xf000 & scale) == 0);
	return scale & (1<<note);
}

inline bool scale_contains(short container, short contained) {
	return (container & contained) == contained;
}

short scale_rotate(short s, Note base) {
	const short scale_mask = 0xfff;
	short looped_bits = scale_mask & (s<<base);
	s = s>>(12-base);
	return s|looped_bits;
}

short notes_array_to_scale(int arr[12]) {
	short ret = 0;
	for(int i = 11; i >= 0; i--) {
		if(arr[i]) ret += 1;
		ret <<= 1;
	}
	ret >>= 1;
	return ret;
}

inline ImVec4 lerp(ImVec4 a, ImVec4 b, float t) {
	return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t); 
}

inline int row_to_note(int n) {
	return 11 - ((n + 11 - HIGHEST_NOTE) % 12);
}

inline int time_to_pixels(Time t) {
	return (int) ((t * bpm * CELL_SIZE_W * CELLS_PER_BEAT) / 60.0);
}

inline Time pixels_to_time(int p) {
	return (((double)p * 60.0) / (double) (bpm * CELL_SIZE_W*CELLS_PER_BEAT));
}

inline void reset() {
	elapsed = 0;
	last_played_grid_col = 0;
	max_r_cell = -1;
}

inline void clear_selection() {
	selected_scale_idx = -1;
	selected_base_note = -1;
	need_prediction_update = true;
}

inline bool is_scale_selected() {
	return selected_scale_idx != -1 && selected_base_note != -1;
}

void update_elapsed_time() {
	Time loop_time, current = glfwGetTime();
	static Time last;
	if(playing)	 elapsed += current - last;
	last = current;

	playhead_offset = time_to_pixels(elapsed);
	const int cells_in_bar = CELLS_PER_BEAT * beats_per_bar;
	const int max_bar = (max_r_cell/cells_in_bar + 1) * cells_in_bar;
	if(max_r_cell == -1 || !auto_loop)
		loop_time = pixels_to_time(GRID_W);
	else
		loop_time = pixels_to_time(max_bar * CELL_SIZE_W);

	if(elapsed >= loop_time) 
		elapsed -= loop_time;
}

void select_scale(int n) {
	selected_scale_idx = n;
	need_prediction_update = true;
	if(scale[n].is_matching & (1<<selected_base_note) == 0)
		selected_base_note = -1;
}

void make_scale_prediction() {
	short current_notes = notes_array_to_scale(drawn_notes);
	for(auto& it : note_histogram) it = 0;
	matching_scales_count = 0;
	for(int j = 0; j < 12; j++) {
		
		short note_mask = j ? 1<<(12 - j) : 1;
		
		for(int i = 0; i < NUM_SCALES; i++) {
			if(scale_contains(scale[i].notes, current_notes)) {
				
				scale[i].is_matching |= note_mask;
				//note histogram is limited to selected note
				
				if(selected_base_note == -1 || selected_base_note == j) {
					matching_scales_count += 1;
					
					for(int k = 0; k < 12; k++)
						if(scale_has_note(scale[i].notes, k))
							note_histogram[(12 + k - j) % 12] += 1;
				}
			}
			else {scale[i].is_matching &= (0xfff - note_mask);}
		}
		current_notes = scale_rotate(current_notes, 1);		
	}
}

//todo:
inline void play_notes() {
	const int current = (playhead_offset / CELL_SIZE_W) % CELL_GRID_NUM_W;
	/*  todo
	for(int x = last_played_grid_col; x != current; x = (x+1)%CELL_GRID_NUM_W)
		for(int y = 0; y < CELL_GRID_NUM_H; y++)
			if(cell[y][x] == state_start) {
				play_sound(y);
			}
	*/
	last_played_grid_col = current;
}

inline void update_note_legth() {
	int mouse_scroll = GetIO().MouseWheel;
	note_length_idx -= mouse_scroll;
	if(note_length_idx < 0) note_length_idx = 0;
	if(note_length_idx > 4) note_length_idx = 4;
	grid_note_length = (CELLS_PER_BEAT * 4) >> note_length_idx;
}

inline void handle_input() {
	if(is_grid_hovered) {
		need_prediction_update = try_update_grid();
		update_note_legth();
	}
	if(IsKeyPressed(ImGuiKey_Space))  playing = !playing;
	if(IsKeyPressed(ImGuiKey_Backspace)) {
		playing = false;
		reset();
	}
	if(IsKeyPressed(ImGuiKey_Enter)) {
		playing = true;
		reset();
	}
	if( IsKeyPressed(ImGuiKey_LeftShift,  false) || 
		IsKeyReleased(ImGuiKey_LeftShift)        ||
		IsKeyPressed(ImGuiKey_RightShift, false) || 
		IsKeyReleased(ImGuiKey_RightShift)) {
		snap_to_grid = !snap_to_grid;
	}
}

void draw_one_frame(GLFWwindow* window) {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	NewFrame();
	SetNextWindowSize(ImVec2(WINDOW_W, WINDOW_H));
	SetNextWindowPos(ImVec2(0,0));
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0)); 
	Begin("main window", NULL, MAIN_WINDOW_FLAGS);
	PopStyleVar();
	static ImDrawList* draw_list = GetWindowDrawList();

	{// TOP BAR
		SetCursorPos(ImVec2(0,MENU_BAR));
		if(BeginMainMenuBar()) {
			if(BeginMenu("Settings")) {
				if(MenuItem("Predict Mode", NULL, predict_mode)) {
					predict_mode = !predict_mode;
					if(predict_mode) clear_selection();
				}
				if(MenuItem("Snap to grid", NULL, snap_to_grid)) {
					snap_to_grid = !snap_to_grid;
				}
				if(MenuItem("English note names", NULL, english_notes)) {
					english_notes = !english_notes;
					note_names = english_notes ? english_note_names : regular_note_names;
				}
				if(BeginMenu("Time Signiture")) {
					if(MenuItem("1/4", NULL, beats_per_bar == 1)) {
						beats_per_bar = 1;
					}
					if(MenuItem("2/4", NULL, beats_per_bar == 2)) {
						beats_per_bar = 2;
					}
					if(MenuItem("3/4", NULL, beats_per_bar == 3)) {
						beats_per_bar = 3;
					}
					if(MenuItem("4/4", NULL, beats_per_bar == 4)) {
						beats_per_bar = 4;
					}
					if(MenuItem("5/4", NULL, beats_per_bar == 5)) {
						beats_per_bar = 5;
					}
					ImGui::EndMenu();
				}
				if(MenuItem("Auto loop", NULL, auto_loop)) {
					auto_loop = !auto_loop;
				}
				ImGui::EndMenu();
			}
			if(BeginMenu("File")) {
				if(MenuItem("Export MIDI")) {}
				ImGui::EndMenu();
			}
			if(BeginMenu("Help")) {
				if(MenuItem("Keyboard Shortcuts")) {
					shortcut_window = true;
				}
				if(MenuItem("Git Repository")) {
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

		if(MyButton("Stop")) {
			playing = false;
			reset();
		}
		SameLine();

		const char* note_preview_value;
		if(selected_base_note != -1)
			note_preview_value = note_names[selected_base_note];
		else  note_preview_value = "---";
		if(shortcut_window) {
			Begin("Keyboard Shortcuts", &shortcut_window, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
			Text("Space - Play/Pause\nBackspace - Stop\nEnter - Play from start\nHold Shift - Toggle snap to grid\nMouse Wheel - Change note length");
			End();
		}
		
		SetNextItemWidth(BASE_BOX_WIDTH);
		if(BeginCombo("Base Note", note_preview_value, 0)) {
			for(int n = 0; n < 12; n++) {
				short note_mask = 1<<n;
				if(!predict_mode || selected_scale_idx == -1 || (scale[selected_scale_idx].is_matching & note_mask)) {
					if(Selectable(note_names[n], selected_base_note == n)) {
						selected_base_note = n;
						need_prediction_update = true;
					}
				}
			}
			EndCombo();
		}
		SameLine();

		const char* scale_preview_value;
		if(selected_scale_idx != -1)
			scale_preview_value = scale[selected_scale_idx].name;	
		else if(predict_mode) {
			sprintf(buff, "%d Possible scales", matching_scales_count);
			scale_preview_value = buff;
		}
		else  scale_preview_value = "Select scale";
		SetNextItemWidth(SCALE_BOX_WIDTH);
		if(BeginCombo("Scale", scale_preview_value, 0)) {
			for(int n = 0; n < NUM_SCALES; n++) {
				const bool is_selected = (selected_scale_idx == n);
				if(predict_mode) {
					if((selected_base_note == -1 && scale[n].is_matching) || (scale[n].is_matching & (1<<selected_base_note))) {
						if(scale[n].number_of_notes == total_drawn_notes) {
							sprintf(buff, "%s [Exact match]", scale[n].name);
							if(Selectable(buff, is_selected))
								select_scale(n);
						}
						else if(Selectable(scale[n].name, is_selected))
							select_scale(n);			
					}
				}
				else if(Selectable(scale[n].name, is_selected))
					select_scale(n);
			}
			EndCombo();
		}
		SameLine();
		if(Button("Clear Selection")) {
			clear_selection();
		}

		//SECOND LINE
		if(Button("Clear All")) {
			reset();
			playing = false;
			total_drawn_notes = 0;
			for(auto& it : drawn_notes) it = 0;
			need_prediction_update = true;
		}
		SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH);
		if(InputInt("BPM", &bpm)) {
			if(bpm > 999) bpm = 999;
			if(bpm < 10)  bpm =  10;
			elapsed = pixels_to_time(playhead_offset); //how much time _would have_ passed had we gotten to this pixel at current bpm
		}
		SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH*4);
		if(Combo("a", &note_length_idx, "1\0001/2\0001/4\0001/8\0001/16\0\0")) {
			grid_note_length = (CELLS_PER_BEAT*4) >> note_length_idx;
		}
	}

	{// Grid Background
		SetCursorPos(ImVec2(0, TOP_BAR));
		for(int i = 0; i < CELL_GRID_NUM_H; i++) {
			const Note note = row_to_note(i);
			ImVec4 color;
			//draw grid background
			if(predict_mode && !is_scale_selected()) {
				if(total_drawn_notes) {
					float density = (float) note_histogram[note] / ((float) matching_scales_count);
					color = lerp(ColorConvertU32ToFloat4(BLACK_COL), ColorConvertU32ToFloat4(GRID_BG_COL), density);
					draw_list->AddRectFilled(ImVec2(SIDE_BAR, TOP_BAR + CELL_SIZE_H*i), ImVec2(SIDE_BAR + GRID_W, TOP_BAR + CELL_SIZE_H*(i+1) - 1), ColorConvertFloat4ToU32(color));
				}
			}
			else if(scale_has_note(scale_rotate(scale[selected_scale_idx].notes, selected_base_note), note))
				draw_list->AddRectFilled(ImVec2(SIDE_BAR, TOP_BAR + CELL_SIZE_H*i), ImVec2(SIDE_BAR + GRID_W, TOP_BAR + CELL_SIZE_H*(i+1) - 1), GRID_BG_COL);
		}
	}

	{// GRID LINES + PLAYHEAD
		for(int i = 0; i<= CELL_GRID_NUM_H; i++)
			draw_list->AddRectFilled(ImVec2(0, TOP_BAR + CELL_SIZE_H*i - LINE_W), ImVec2(GRID_W + SIDE_BAR, TOP_BAR + CELL_SIZE_H*i + LINE_W), BEAT_LINE_COL);		
		for(int i = 0; i<= CELL_GRID_NUM_W; i+=grid_note_length) {
			const int cells_per_bar = CELLS_PER_BEAT * beats_per_bar;
			ImU32 color = i % cells_per_bar == 0 ? BAR_LINE_COL : BEAT_LINE_COL;
			draw_list->AddRectFilled(ImVec2(SIDE_BAR + CELL_SIZE_W*i - LINE_W, TOP_BAR), ImVec2(SIDE_BAR + CELL_SIZE_W*i + LINE_W, GRID_H + TOP_BAR), color);
		}
	}

	{// NOTE LABELS
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,2));
		PushStyleColor(ImGuiCol_Text, BLACK_COL);
		SetCursorPos(ImVec2(0,TOP_BAR));
		for(int y = 0; y < CELL_GRID_NUM_H; y++) {
			const Note note = row_to_note(y);
			const int octave = HIGHEST_NOTE_OCTAVE - y/12;
			const ImU32 box_col = !is_sharp(note) ? BLACK_NOTE_COL : WHITE_NOTE_COL;
			sprintf(buff, "%s%d", note_names[note], octave);
			TextBox(buff, ImVec2(SIDE_BAR, CELL_SIZE_H-2), ImVec2(0,0), box_col);
		}
		PopStyleVar();
		PopStyleColor();

		//draw playhead in foreground
		const int playhead = SIDE_BAR + playhead_offset;
		GetForegroundDrawList()->AddRectFilled(ImVec2(playhead - LINE_W, TOP_BAR), ImVec2(playhead + LINE_W, GRID_H + TOP_BAR), PLAYHEAD_COL);
	}
	max_r_cell = -1;
	for(int i = 0; i < CELL_GRID_NUM_H; i++) { 
		for(Node* cur_c = row[i].next; cur_c != NULL; cur_c = cur_c->next) {
			draw_note(i, cur_c->start, cur_c->end, draw_list);
			if(max_r_cell < cur_c->end)
				max_r_cell = cur_c->end;
		}
	}
	//we use a invisible button to tell us when to capture mouse input for the grid
	SetCursorPos(ImVec2(SIDE_BAR,TOP_BAR));
	SetItemUsingMouseWheel(); 
	InvisibleButton("grid_overlay", ImVec2(WINDOW_W - SIDE_BAR, WINDOW_H), 0);
	is_grid_hovered = IsItemHovered();	
	End();//main window
}

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	assert(!_set_error_mode(_OUT_TO_STDERR));
	if(!glfwInit())  return -1;
	GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Sequencer", NULL, NULL);
	assert(window);

	glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); //Enable vsync
	CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");
	ImGuiIO& io = GetIO();

	io.Fonts->AddFontFromFileTTF("../Lucida Console Regular.ttf", FONT_SIZE);
	io.IniFilename = NULL;//don't use imgui.ini file

	init_xaudio();

	ImGuiStyle& style = GetStyle();
	style.WindowBorderSize = 0;

	// Main loop
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if(predict_mode && need_prediction_update) {
			make_scale_prediction();
			need_prediction_update = false;
		}
		draw_one_frame(window);
		
		handle_input();
	    
		update_elapsed_time();
		
		if(playing)  play_notes();

		#if 1
		// Debug window
		Begin("debug");
		sprintf(buff,"Notes: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", drawn_notes[11], drawn_notes[10], drawn_notes[9], drawn_notes[8], drawn_notes[7], drawn_notes[6], drawn_notes[5], drawn_notes[4], drawn_notes[3], drawn_notes[2], drawn_notes[1], drawn_notes[0]);
		Text(buff);
		sprintf(buff,"Note histogram: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", note_histogram[11], note_histogram[10], note_histogram[9], note_histogram[8], note_histogram[7], note_histogram[6], note_histogram[5], note_histogram[4], note_histogram[3], note_histogram[2], note_histogram[1], note_histogram[0]);
		Text(buff);
		sprintf(buff,"Number of drawn notes: %d", total_drawn_notes);
		Text(buff);
		sprintf(buff,"matching_scales_count: %d", matching_scales_count);
		Text(buff);
		sprintf(buff,"need predicdion update: %d", need_prediction_update);
		Text(buff);
        End();
        #endif

        Render();
        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
		glfwSwapBuffers(window);
	}
	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 1;
}