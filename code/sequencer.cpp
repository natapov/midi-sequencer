#include "windows.h"
#include "xaudio2.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "strsafe.h"
#include "glfw/glfw3.h"
#include "scales.h"
#include "grid.h"
#include "audio.h"
#include "sequencer.h"
#include "dwmapi.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw/glfw3native.h"
#include "commdlg.h"

using namespace ImGui;

// FUNCTIONS
inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { 
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); 
}

void draw_note(int r, int start, int end, ImDrawList* draw_list) {
	const ImVec2 point1 = ImVec2(SIDE_BAR + start * CELL_SIZE_W, TOP_BAR + r     * CELL_SIZE_H);
	const ImVec2 point2 = ImVec2(SIDE_BAR + end   * CELL_SIZE_W, TOP_BAR + (r+1) * CELL_SIZE_H);
	const int nb = NOTE_BORDER_SIZE;

	draw_list->AddRectFilled(point1 + ImVec2(-nb,-nb), point2 + ImVec2( nb, nb), NOTE_COL, 5);
	draw_list->AddRectFilled(point1, point2, NOTE_BORDER_COL, 5);
	draw_list->AddRectFilled(point1 + ImVec2( nb, nb), point2 + ImVec2(-nb,-nb), NOTE_DARKER_COL, 5);
}

inline bool scale_has_note(short scale, Note note) {
	assert((0xf000 & scale) == 0);
	return scale & (1<<note);
}

inline bool scale_contains(short container, short contained) {
	return (container & contained) == contained;
}

short scale_rotate_left_by_one(short s) {
	const short scale_mask = 0xfff;
	short looped_bit = scale_mask & s<<11;
	s = s>>1;
	return s|looped_bit;
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
	return 11 - (n % 12);
}

inline int time_to_pixels(Time t) {
	return (int) ((t * bpm * CELL_SIZE_W * CELLS_PER_BEAT) / 60.0);
}

inline Time pixels_to_time(int p) {
	return (((double)p * 60.0) / (double) (bpm * CELL_SIZE_W * CELLS_PER_BEAT));
}

inline void reset(){
    stop_all_notes();
	elapsed = 0;
    last = 0;
	last_played_grid_col = -1;
	max_r_cell = -1;
}

inline void clear_selection() {
	selected_scale_idx = -1;
	selected_base_note = -1;
	need_prediction_update = true;
}

void update_elapsed_time() {
	Time loop_time, current = glfwGetTime();
	if(playing && last > 0)	 elapsed += current - last;//skip first frame after reset
	last = current;

	playhead_offset = time_to_pixels(elapsed);
	const int cells_in_bar = CELLS_PER_BEAT * beats_per_bar;
	//max_r_cell -= 1;
	const int max_bar = ((max_r_cell-1)/cells_in_bar + 1) * cells_in_bar;
	if(max_r_cell == -1 || !auto_loop)
		loop_time = pixels_to_time(GRID_W);
	else
		loop_time = pixels_to_time(max_bar * CELL_SIZE_W);

	if(elapsed >= loop_time){
		elapsed -= loop_time;
        // in the case of strong lag or erasing a note that shortens loop time
		if(elapsed > pixels_to_time(CELL_SIZE_W)/3)
			elapsed = 0;
	}
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
	number_of_matching_scales = 0;
	for(int j = 0; j < 12; j++) {
		
		short note_mask = 1 << j;
		
		for(int i = 0; i < NUM_SCALES; i++) {
			if(scale_contains(scale[i].notes, current_notes)) {
				
				scale[i].is_matching |= note_mask;
				assert(scale_has_note(scale[i].is_matching, j));
				//note histogram is limited to selected note
				
				if(selected_base_note == -1 || selected_base_note == j) {
					number_of_matching_scales += 1;
					
					for(int k = 0; k < 12; k++) {
						if(scale_has_note(scale[i].notes, k)) {
							note_histogram[(k + j) % 12] += 1;
						}
					}
				}
			}
			else {scale[i].is_matching &= (0xfff - note_mask);}
		}
		current_notes = scale_rotate_left_by_one(current_notes);		
	}
	need_prediction_update = false;
}

void play_notes() {
	const int current = (playhead_offset / CELL_SIZE_W) % CELL_GRID_NUM_W;
	for(int i = 0; i < CELL_GRID_NUM_H; i++){
		for(Node* n = row[i].next; n != NULL; n = n->next) {
			if(last_played_grid_col <= current) {
				if(n->start > last_played_grid_col && n->start <= current) {
					play_note(i);
				}
                if(n->end   > last_played_grid_col && n->end   <= current) {
                    stop_note(i);
                }
			}
			else {
                if(n->start <= current || n->start > last_played_grid_col) {
				    play_note(i);
                }
                if(n->end   <= current || n->end   > last_played_grid_col) {
                    stop_note(i);
                }
			}
		}
	}
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
	if(IsKeyPressed(ImGuiKey_Space)){
        playing = !playing;
        if(!playing) {
            stop_all_notes();
        }
    }
	if(IsKeyPressed(ImGuiKey_Backspace)) {
		playing = false;
		reset();
	}
	if(IsKeyPressed(ImGuiKey_Enter)) {
		playing = true;
		reset();
	}

	if( IsMouseDown(ImGuiMouseButton_Left) && (
		IsKeyPressed(ImGuiKey_LeftShift,  false) || 
		IsKeyPressed(ImGuiKey_RightShift, false)
		)) {
		toggle_snap_to_grid = !toggle_snap_to_grid;
	}
	if(IsMouseReleased(ImGuiMouseButton_Left)){
		toggle_snap_to_grid = false;
	}
	
	snap_to_grid = toggle_snap_to_grid ? !snap_to_grid_setting : snap_to_grid_setting;
}

void draw_one_frame(GLFWwindow* window) {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	NewFrame();

    if(shortcut_window) {
        Begin("Keyboard Shortcuts", &shortcut_window, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
        Text("Space - Play/Pause\nBackspace - Stop\nEnter - Play from start\nHold Shift - Toggle snap to grid\nMouse Wheel - Change note length");
        End();
    }

    SetNextWindowSize(ImVec2(WINDOW_W, WINDOW_H));
    SetNextWindowPos(ImVec2(0,0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    Begin("main window", NULL, MAIN_WINDOW_FLAGS);
	PopStyleVar();

	static ImDrawList* draw_list = GetWindowDrawList();

    {// BOTTOM BAR
        SetCursorPos(ImVec2(BUFFER, TOP_BAR + GRID_H + BUFFER));
        AlignTextToFramePadding();
        Text("Scale:");
        SameLine();
        const char* scale_preview_value;
        if(selected_scale_idx != -1)
            scale_preview_value = scale[selected_scale_idx].name;   
        else if(predict_mode) {
            StringCchPrintf(buff, BUFF_SIZE, "%d Possible scales", number_of_matching_scales);
            scale_preview_value = buff;
        }
        else  scale_preview_value = "Select scale";
        SetNextItemWidth(SCALE_BOX_WIDTH);
        if(BeginCombo("##Scale", scale_preview_value)) {
            for(int n = 0; n < NUM_SCALES; n++) {
                const bool is_selected = (selected_scale_idx == n);
                if(predict_mode) {
                    if((selected_base_note == -1 && scale[n].is_matching) || (scale[n].is_matching & (1<<selected_base_note))) {
                        if(scale[n].number_of_notes == total_drawn_notes) {
                            StringCchPrintf(buff, BUFF_SIZE, "%s [Exact match]", scale[n].name);
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
        SetCursorPosY(TOP_BAR + GRID_H + BUFFER);
        Text("Base Note:");
        SameLine();
        bool open_note_selection = false;
        if(selected_scale_idx != -1 && predict_mode && selected_base_note == -1 && !need_prediction_update) {
            int matching_notes_count = 0;
            Note to_select;
            for(int n = 0; n < 12; n++) {
                short note_mask = 1<<n;
                if(scale[selected_scale_idx].is_matching & note_mask) {
                    to_select = n;
                    matching_notes_count += 1;
                }
            }
            assert(matching_notes_count != 0);
            if(matching_notes_count == 1) {
                selected_base_note = to_select;
            }
            else if(matching_notes_count > 1) {
                open_note_selection = true;
            }
        }
        
        const char* note_preview_value;
        if(selected_base_note != -1)
            note_preview_value = note_names[selected_base_note];
        else  note_preview_value = "---";

        SetNextItemWidth(BASE_BOX_WIDTH);
        if(BeginCombo("##Base Note", note_preview_value, open_note_selection, ImGuiComboFlags_HeightLargest)) {
            for(int n = 0; n < 12; n++) {
                if(!predict_mode || selected_scale_idx == -1 || scale_has_note(scale[selected_scale_idx].is_matching, n)) {
                    if(Selectable(note_names[n], selected_base_note == n)) {
                        selected_base_note = n;
                        need_prediction_update = true;
                    }
                }
            }
            EndCombo();
        }

        SameLine();
        if(Button("Clear Scale Selection")) {
            clear_selection();
        }
        SetCursorPos(ImVec2(WINDOW_W - SCALE * 9 , TOP_BAR + GRID_H + BUFFER));
        if(Button("Clear Notes")) {
            reset();
            playing = false;
            total_drawn_notes = 0;
            for(auto& it : drawn_notes) it = 0;
            free_all_nodes();
            need_prediction_update = true;
        }
    }

	{// TOP BAR
        PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(-1, MENU_PADDING));

        PushStyleColor(ImGuiCol_Border, IM_COL32(80,80,86,128));
        PushStyleColor(ImGuiCol_PopupBg, IM_COL32(36,36,36,255));
		auto ret = BeginMainMenuBar();
        PopStyleVar();
        if(ret) {
			if(BeginMenu("Settings")) {
				if(MenuItem("Predict Mode", NULL, predict_mode)) {
					predict_mode = !predict_mode;
					if(predict_mode) clear_selection();
				}
				if(MenuItem("Snap to grid", NULL, snap_to_grid_setting)) {
					snap_to_grid_setting = !snap_to_grid_setting;
				}
				if(MenuItem("English note names", NULL, english_notes)) {
					english_notes = !english_notes;
					note_names = english_notes ? english_note_names : regular_note_names;
				}
				if(BeginMenu("Time Signature")) {
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
				if(MenuItem("Load Soundfont File")) {
                    load_soundfont();
                }
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
        //PopStyleVar();
		PopStyleColor(2);

        SetCursorPos(ImVec2(BUFFER, MENU_BAR + BUFFER));
		if(MyButton("Play"))  playing = true;
		SameLine();

		if(MyButton("Pause")) {
            playing = false;
            stop_all_notes();
        }
		SameLine();

		if(MyButton("Stop")) {
			playing = false;
			reset();
		}
		SameLine();
		Text("Instrument:");
        SameLine();

        const char* instrument_preview_value = midi_instruments[instrument];
        SetNextItemWidth(SCALE * 24);
        if(BeginCombo("##no label", instrument_preview_value)) {
            for(int n = 0; n < 128; n++) {
                if(Selectable(midi_instruments[n], instrument == n)) {
                    instrument = n;
                    const int res = fluid_synth_program_change(synth, 0, instrument);
                    assert(res == FLUID_OK);
                }
                if (instrument == n)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }
		SameLine();
        Text("BPM:");
        SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH);
		if(InputInt("##BPM", &bpm)) {
			if(bpm > 999) bpm = 999;
			if(bpm < 10)  bpm =  10;
			elapsed = pixels_to_time(playhead_offset); //how much time _would have_ passed had we gotten to this pixel at current bpm
		}
		SameLine();
        Text("Note Length:");
        SameLine();
		SetNextItemWidth(BPM_BOX_WIDTH);
		if(Combo("##", &note_length_idx, "1\0001/2\0001/4\0001/8\0001/16\0\0")) {
			grid_note_length = (CELLS_PER_BEAT*4) >> note_length_idx;
		}
	}

	{// Grid Background
		SetCursorPos(ImVec2(0, TOP_BAR));
		for(int i = 0; i < CELL_GRID_NUM_H; i++) {
			const Note note = row_to_note(i);
			ImVec4 color;
			//draw grid background
			const bool is_scale_selected = selected_scale_idx != -1 && selected_base_note != -1;
			if(is_scale_selected) {
				if(scale_has_note(scale[selected_scale_idx].notes, (12 + note - selected_base_note)%12))
					draw_list->AddRectFilled(ImVec2(SIDE_BAR, TOP_BAR + CELL_SIZE_H*i), ImVec2(SIDE_BAR + GRID_W, TOP_BAR + CELL_SIZE_H*(i+1) - 1), GRID_BG_COL);
			}
			else if(predict_mode) {
				if(total_drawn_notes) {
					float density = (float) note_histogram[note] / ((float) number_of_matching_scales);
					color = lerp(ColorConvertU32ToFloat4(BLACK_COL), ColorConvertU32ToFloat4(GRID_BG_COL), density);
					draw_list->AddRectFilled(ImVec2(SIDE_BAR, TOP_BAR + CELL_SIZE_H*i), ImVec2(SIDE_BAR + GRID_W, TOP_BAR + CELL_SIZE_H*(i+1) - 1), ColorConvertFloat4ToU32(color));
				}
			}
		}
	}

	{// GRID LINES + PLAYHEAD
		for(int i = 0; i <= CELL_GRID_NUM_H; i++)
			draw_list->AddRectFilled(ImVec2(0, TOP_BAR + CELL_SIZE_H*i - LINE_W), ImVec2(GRID_W + SIDE_BAR, TOP_BAR + CELL_SIZE_H*i + LINE_W), BEAT_LINE_COL);		
		for(int i = 0; i <= CELL_GRID_NUM_W; i += grid_note_length) {
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
			const int octave = HIGHEST_OCTAVE - y/12;
			const bool is_sharp = note == La_ || note == So_ || note == Fa_ || note == Re_ || note == Do_;
			const ImU32 box_col = is_sharp ? BLACK_NOTE_COL : WHITE_NOTE_COL;
			StringCchPrintf(buff, BUFF_SIZE, "%s%d", note_names[note], octave);
			TextBox(buff, ImVec2(SIDE_BAR, CELL_SIZE_H-2), ImVec2(0,0), box_col);
		}
		PopStyleVar();
		PopStyleColor();

	}
	max_r_cell = -1;
	for(int i = 0; i < CELL_GRID_NUM_H; i++) { 
		for(Node* cur_c = row[i].next; cur_c != NULL; cur_c = cur_c->next) {
			draw_note(i, cur_c->start, cur_c->end, draw_list);
			if(max_r_cell < cur_c->end)
				max_r_cell = cur_c->end;
		}
	}

	// PLAYHEAD
	const int playhead = SIDE_BAR + playhead_offset;
	draw_list->AddRectFilled(ImVec2(playhead - LINE_W, TOP_BAR), ImVec2(playhead + LINE_W, GRID_H + TOP_BAR), PLAYHEAD_COL);

	//we use a invisible button to tell us when to capture mouse input for the grid
	SetCursorPos(ImVec2(SIDE_BAR,TOP_BAR));
	InvisibleButton("grid_overlay", ImVec2(GRID_W, GRID_H), 0);
	is_grid_hovered = IsItemHovered();	
	End();//main window
}
void window_content_scale_callback(GLFWwindow* window, float xscale, float yscale){
    //SCALE = 16.0f * xscale;
    glfwSetWindowSize(window, WINDOW_W, WINDOW_H);
}

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	assert(!_set_error_mode(_OUT_TO_STDERR));//send asserts to stderr
    //SCALE *= 2;
    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, false);
    glfwWindowHint(GLFW_RESIZABLE, false);
    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    float xscale, yscale;
    glfwGetMonitorContentScale(primary, &xscale, &yscale);
    SCALE = 16.0f * xscale;
    GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Sequencer", NULL, NULL);
    glfwSetWindowContentScaleCallback(window, window_content_scale_callback);

    CreateContext();
    glfwMakeContextCurrent(window);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");


    ImGuiIO& io = GetIO();
    io.Fonts->AddFontFromFileTTF("Lucida Console Regular.ttf", FONT_SIZE);
    ImGuiStyle& style = GetStyle();
    style.WindowBorderSize = 0;
    style.FrameRounding    = 3;
    style.WindowPadding.x  = SCALE / 4;
    style.WindowPadding.y  = SCALE / 4;

    draw_one_frame(window);
    Render();
    ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
    
    // this is how we make the title bar black
    const BOOL USE_DARK_MODE = true;
    auto hWnd = glfwGetWin32Window(window);
    DwmSetWindowAttribute(hWnd, 20, &USE_DARK_MODE, sizeof(BOOL));
    SetWindowPos(hWnd, NULL, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
    SetWindowPos(hWnd, NULL, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
    
    glfwSwapBuffers(window);

    //initializations we do after displaying the first frame
    glfwSwapInterval(1); //Enable vsync
    io.IniFilename = NULL; //don't use imgui.ini file
    init_synth();
    
    // Main loop
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		if(predict_mode && need_prediction_update) {
			make_scale_prediction();
		}
        
		draw_one_frame(window);
		
		handle_input();
		
		update_elapsed_time();

		if(playing)  play_notes();
		
        #if 1
		// Debug window
		ShowStyleEditor();
		Begin("debug");
		StringCchPrintf(buff, BUFF_SIZE,"Notes: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", drawn_notes[11], drawn_notes[10], drawn_notes[9], drawn_notes[8], drawn_notes[7], drawn_notes[6], drawn_notes[5], drawn_notes[4], drawn_notes[3], drawn_notes[2], drawn_notes[1], drawn_notes[0]);
		Text(buff);
		StringCchPrintf(buff, BUFF_SIZE,"Note histogram: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", note_histogram[11], note_histogram[10], note_histogram[9], note_histogram[8], note_histogram[7], note_histogram[6], note_histogram[5], note_histogram[4], note_histogram[3], note_histogram[2], note_histogram[1], note_histogram[0]);
		Text(buff);
		StringCchPrintf(buff, BUFF_SIZE,"Number of drawn notes: %d", total_drawn_notes);
		Text(buff);
		StringCchPrintf(buff, BUFF_SIZE,"matching_scales_count: %d", number_of_matching_scales);
		Text(buff);
		StringCchPrintf(buff, BUFF_SIZE,"need preditdion update: %d", need_prediction_update);
		Text(buff);
		StringCchPrintf(buff, BUFF_SIZE,"fps :%f", GetIO().Framerate);
		Text(buff);
        int sfont_id, bank_num, preset_num;
        fluid_synth_get_program(synth, 0, &sfont_id, &bank_num, &preset_num);
        StringCchPrintf(buff, BUFF_SIZE,"sfont id :%d", sfont_id);
        Text(buff);
        StringCchPrintf(buff, BUFF_SIZE,"bank num :%d", bank_num);
        Text(buff);
        StringCchPrintf(buff, BUFF_SIZE,"preset num :%d", preset_num);
        Text(buff);
        StringCchPrintf(buff, BUFF_SIZE,"SCALE :%d", SCALE);
        Text(buff);
        End();
        #endif

        Render();
		ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());
		glfwSwapBuffers(window);
	}
	// Cleanup
    delete_synth();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 1;
}