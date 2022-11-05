#include "sequencer.h"
#include "imgui.h"
#include "audio.h"
using namespace ImGui;

typedef struct Node{
	int start;
	int end;
	struct Node* next;
}Node;

Node* moving_node_prev    = NULL;
Node* resizing_node_start = NULL;
Node* resizing_node_end   = NULL;
int moving_node_offset    = -1;
int moving_node_row       = -1;
int moving_node_len       = -1;

Node row[CELL_GRID_NUM_H] = {-1, -1, NULL};

Node* temp;
void free_all_nodes() {
	for(int i = 0; i < CELL_GRID_NUM_H; i++) {
		for(Node* n = row[i].next; n != NULL;) {
			temp = n->next;
			free(n);
			n = temp;
		}
		row[i].next = NULL;
	}
}

inline int get_len(Node* n) {
	return n->end - n->start;
}


Node* init_node(int start, int end, Node* next = NULL) {
	Node* ret = (Node*) malloc(sizeof(Node));
	assert(ret);
	ret->start = start;
	ret->end = end;
	ret->next = next;
	return ret;
}

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
		if(ret->next->start > c) break;
		last = ret;
	}
	return ret;
}
Node* get_last_node_that_starts_before_c(int r, int c) { 
	Node* ret;
	for(ret = &row[r];; ret = ret->next) {
		if(ret->next == NULL)  break;
		if(ret->next->start > c) break;
	}
	return ret;
}

bool try_place_note(int r, int start, int end, Node* cur) {
	//todo: if(r+c > sizeofgird)  return false

	if(start < cur->end)  return false;

	Node* next = cur->next;
	if(!next || end <= next->start) {
		Node* new_n = init_node(start, end, next);
		cur->next = new_n;
		drawn_notes[row_to_note(r)] += 1;
		total_drawn_notes += 1;
		return true;
	}

	return false;
}
	
bool try_move_note(int r, int c) {
	Node* moving_node = moving_node_prev->next;	
	c = snap_to_grid ? snap(c) : c - moving_node_offset;
	if(c == moving_node->start && r == moving_node_row)  return false;

	moving_node_prev->next = moving_node->next;
	
	Node* cur = get_last_node_that_starts_before_c(r, c);
	
	if(c >= cur->end && (!cur->next || c + moving_node_len <= cur->next->start)) {
		moving_node->next = cur->next;
		moving_node->start = c;
		moving_node->end   = c + moving_node_len;
		moving_node_prev = cur;
		drawn_notes[row_to_note(moving_node_row)] -= 1;
		moving_node_row = r;
		drawn_notes[row_to_note(r)] += 1;
		cur->next = moving_node;
		return true;
	}
	assert(moving_node->next == moving_node_prev->next);
	moving_node_prev->next = moving_node;
	return false;
}

inline void start_moving_note(int r, int c, Node* prev) {
	moving_node_prev = prev;
	moving_node_offset = c - prev->next->start;
	moving_node_len = get_len(prev->next);
	moving_node_row = r;
}

void resize_start(int c) {
	Node* prev = resizing_node_start;
	Node* node = prev->next;

	const int last_c = node->start;
	c = snap_to_grid ? snap(c) : c;
	const int delta = last_c - c;
	const int new_len = get_len(node) + delta;
	if(prev->end <= c && new_len >= MIN_NOTE_LEN) {
		node->start = c;
	}
}
void resize_end(int c) {
	Node* node = resizing_node_end;
	Node* next = node->next;
	const int last_end = node->end;
	c = snap_to_grid ? snap(c) + grid_note_length : c + 1;
	const int delta = last_end - c;
	const int new_len = get_len(node) - delta;
	if((!next || next->start >= node->start + new_len) && new_len >= MIN_NOTE_LEN) {
		node->end = c;
	}		
}

bool try_update_grid() {	
	//get grid co-ordinates
	ImVec2 mouse_pos = GetMousePos();
	int r = (mouse_pos.y - TOP_BAR)  / CELL_SIZE_H;
	int c = (mouse_pos.x - SIDE_BAR) / CELL_SIZE_W;
	int snap_c;

	bool is_hovering_note  = false;
	bool is_hovering_start = false;
	bool is_hovering_end   = false;

	Node* prev;
	Node* node = get_last_node_that_starts_before_c(r, c, prev);
	Node* node_s;
	if(node->end > c){
		is_hovering_note = true;
		const int node_start_pixels = node->start * CELL_SIZE_W + SIDE_BAR;
		const int node_end_pixels   = node->end   * CELL_SIZE_W + SIDE_BAR;
		const int max_size = (CELL_SIZE_W/3) * get_len(node);
		const int handle_size = RESIZE_HANDLE_SIZE < max_size ? RESIZE_HANDLE_SIZE : max_size;
		if(mouse_pos.x - node_start_pixels <= handle_size) {
			is_hovering_start = true;
		}
		else if(node_end_pixels - mouse_pos.x <= handle_size) {
			is_hovering_end = true;
		}
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
		resizing_node_start = NULL;
		resizing_node_end   = NULL;
		moving_node_prev    = NULL;
	}
	if(resizing_node_end || resizing_node_start || is_hovering_end || is_hovering_start) {
		if(!moving_node_prev)
			SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if(is_hovering_note && IsMouseDown(1)) {
		erase_note(r, c, node, prev);
		return true;
	}
	if(IsMouseClicked(0)) {
		if(!is_hovering_note) {
			if(try_place_note(r, snap_c, snap_c + grid_note_length, node_s)) {
				if(!playing)  play_sound(r);
				start_moving_note(r, c, node_s);
				return true;
			}
			else return false;
		}
		if(is_hovering_start) {
			resizing_node_start = prev;
			return false;
		}
		if(is_hovering_end) {
			resizing_node_end = node;
			return false;
		}
	}
	if(resizing_node_start) {
		resize_start(c);
		return false;
	}
	if(resizing_node_end) {
		resize_end(c);
		return false;
	}	
	if(moving_node_prev) {
		return try_move_note(r, c);
	}
	else if(is_hovering_note && IsMouseDown(0)){
		start_moving_note(r, c, prev);
		return false;
	}
	return false;
}