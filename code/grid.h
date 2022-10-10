#include "config.h"
#include "imgui.h"
#include "audio.h"
using namespace ImGui;

typedef struct Node{
	int col;
	struct Node* next;
	int len;
}Node;

Node* moving_node_prev   = NULL;
Node* resizing_note_prev = NULL;
int moving_node_offset = -1;
int moving_node_row    = -1;
int max_col; //todo


Node* hovering_over = NULL;

int drawn_notes[12]; //The number of notes of each type currently on the grid
int total_drawn_notes = 0;

Node row[CELL_GRID_NUM_H] ={-1, NULL, -1};

//todo add proper header
inline int row_to_note(int n);

Node* init_node(int num, int len, Node* next = NULL) {
	Node* ret = (Node*) malloc(sizeof(Node));
	assert(ret);
	ret->col = num;
	ret->len = len;
	ret->next = next;
	return ret;
}

//bool try_move_note() 
void erase_note(int r, int c, Node* cur, Node* last) {
	assert(cur);
	last->next = cur->next;
	free(cur);
	drawn_notes[row_to_note(r)] -= 1;
	total_drawn_notes -= 1;
}

inline int snap(int x) {
	return (x - x % grid_note_length);
}

Node* get_last_node_that_starts_before_c(int r, int c, Node*& last) { 
	last = NULL;
	Node* ret;
	for(ret = &row[r];; ret = ret->next) {
		if(ret->next == NULL)  break;
		if(ret->next->col > c) break;
		last = ret;
	}
	return ret;
}
Node* get_last_node_that_starts_before_c(int r, int c) { 
	Node* ret;
	for(ret = &row[r];; ret = ret->next) {
		if(ret->next == NULL)  break;
		if(ret->next->col > c) break;
	}
	return ret;
}

bool try_place_note(int r, int c, int len, Node* cur) {
	//todo: if(r+c > sizeofgird)  return false

	Node* next = cur->next;
	if(!next || c + len <= next->col) {
		Node* new_n = init_node(c, len, next);
		cur->next = new_n;
		drawn_notes[row_to_note(r)] += 1;
		total_drawn_notes += 1;
		return true;
	}

	return false;
}
	
bool try_move_note(int r, int c) {
	Node* moving_node = moving_node_prev->next;
	assert(moving_node);
	
	moving_node_prev->next = moving_node->next;

	c = snap_to_grid ? snap(c) : c - moving_node_offset;
	Node* cur = get_last_node_that_starts_before_c(r, c);
	
	if(c >= cur->col + cur->len && (!cur->next || c + moving_node->len <= cur->next->col)) {
		moving_node->next = cur->next;
		moving_node->col = c;
		moving_node_prev = cur;
		drawn_notes[moving_node_row] -= 1;
		moving_node_row = row_to_note(r);
		drawn_notes[moving_node_row] += 1;
		cur->next = moving_node;
		return true;
	}
	assert(moving_node->next == moving_node_prev->next);
	moving_node_prev->next = moving_node;
	return false;
}

inline void start_moving_note(int r, int c, Node* prev) {
	moving_node_prev = prev;
	moving_node_offset = c - prev->next->col;
	moving_node_row = row_to_note(r);
}

bool try_update_grid() {	
	//get grid co-ordinates
	ImVec2 mouse_pos = GetMousePos();
	int r = (mouse_pos.y - TOP_BAR)  / CELL_SIZE_H;
	int c = (mouse_pos.x - SIDE_BAR) / CELL_SIZE_W;
	int snap_c;

	bool is_hovering_note = false;;
	Node* prev;
	Node* node = get_last_node_that_starts_before_c(r, c, prev);
	Node* node_s;
	if(node->col + node->len > c){
		is_hovering_note = true;
	}
	if(snap_to_grid) {
		snap_c = snap(c);
		node_s = get_last_node_that_starts_before_c(r, snap_c);
	}
	else {
		snap_c = c;
		node_s = node;
	}
	if(!IsMouseDown(0)) {
		resizing_note_prev = NULL;
		moving_node_prev = NULL;
	}
	if(is_hovering_note && IsMouseDown(1)) {
		erase_note(r,c, node, prev);
		return false;
	}
	if(!is_hovering_note && IsMouseClicked(0)) {
		if(try_place_note(r, snap_c, grid_note_length, node_s)) {
			play_sound(r);
			start_moving_note(r, c, node_s);
			return true;
		}
		else return false;
	}
	if(moving_node_prev) {
		if(try_move_note(r, c)){
			return true;
		}
		return false;
		assert(moving_node_prev);
	}
	else if(is_hovering_note && IsMouseDown(0)){
		start_moving_note(r, c, prev);
		return false;
	}
	
	return false;
}