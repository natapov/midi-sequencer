#include "sequencer.h"
#include "imgui.h"
#include "audio.h"
using namespace ImGui;

typedef struct Node{
	int start;
	int end;
	struct Node* next;
}Node;

typedef enum {
    state_default,
    state_erasing,
    state_moving,
    state_resizing_start,
    state_resizing_end,
    state_hovering,
}State;

typedef enum {
    hovering_start,
    hovering_end,
    hovering_center,
}Hovering_state;

State current_state = state_default;
Hovering_state hovering_state = hovering_center;

Node* previous_node    = NULL; // the node previous to the node currently being changed
int moving_node_offset = -1;
int moving_node_row    = -1;
int moving_node_len    = -1;

Node row_array[CELL_GRID_NUM_H] = {-1, -1, NULL};

void free_all_nodes() {
    Node* temp;
	for(int i = 0; i < CELL_GRID_NUM_H; i++) {
		for(Node* n = row_array[i].next; n != NULL;) {
			temp = n->next;
			free(n);
			n = temp;
		}
		row_array[i].next = NULL;
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
    stop_note(r);
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
	for(ret = &row_array[r];; ret = ret->next) {
		if(ret->next == NULL)  break;
		if(ret->next->start > c) break;
		last = ret;
	}
	return ret;
}
Node* get_last_node_that_starts_before_c(int r, int c) { 
	Node* ret;
	for(ret = &row_array[r];; ret = ret->next) {
		if(ret->next == NULL)  break;
		if(ret->next->start > c) break;
	}
	return ret;
}

bool try_place_note(int r, int start, int end, Node* cur) {
	if(end > CELL_GRID_NUM_W)  return false;
	if(start < cur->end)       return false;

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
    assert(current_state == state_moving);
    assert(previous_node != NULL);

	Node* moving_node = previous_node->next;	
	c = snap_to_grid ? snap(c) : c - moving_node_offset;
	if(c == moving_node->start && r == moving_node_row)  return false;
    if(c + moving_node_len > CELL_GRID_NUM_W)  return false;
	previous_node->next = moving_node->next;
	
	Node* cur = get_last_node_that_starts_before_c(r, c);
	
	if(c >= cur->end && (!cur->next || c + moving_node_len <= cur->next->start)) {
		stop_note(moving_node_row);
        if(!playing && moving_node_row != r) {
            play_note(r);
        }

        moving_node->next  = cur->next;
		moving_node->start = c;
		moving_node->end   = c + moving_node_len;
		previous_node = cur;
		drawn_notes[row_to_note(moving_node_row)] -= 1;
		moving_node_row = r;
		drawn_notes[row_to_note(r)] += 1;
		cur->next = moving_node;

		return true;
	}
	assert(moving_node->next == previous_node->next);
	previous_node->next = moving_node;
	return false;
}

inline void start_moving_note(int r, int c, Node* prev) {
	current_state = state_moving;
    previous_node = prev;
	moving_node_offset = c - prev->next->start;
	moving_node_len = get_len(prev->next);
	moving_node_row = r;
}

void resize_start(int c) {
	Node* prev = previous_node;
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
	Node* node = previous_node->next;
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
	Node* prev;
	Node* node = get_last_node_that_starts_before_c(r, c, prev);
	Node* node_s;

	if(snap_to_grid) {
		snap_c = snap(c);
		node_s = get_last_node_that_starts_before_c(r, snap_c);
	}
	else {
		snap_c = c;
		node_s = node;
	}

    if(!IsMouseDown(0)) {
        current_state = state_default;
    }

    if(current_state == state_hovering || current_state == state_default) {
        if(c < node->end){
            current_state = state_hovering;
            const int node_start_pixels = node->start * CELL_SIZE_W + SIDE_BAR;
            const int node_end_pixels   = node->end   * CELL_SIZE_W + SIDE_BAR;
            const int max_size = (CELL_SIZE_W/3) * get_len(node);
            const int handle_size = RESIZE_HANDLE_SIZE < max_size ? RESIZE_HANDLE_SIZE : max_size;
            
            if(mouse_pos.x - node_start_pixels <= handle_size) {
                hovering_state = hovering_start;
            }
            else if(node_end_pixels - mouse_pos.x <= handle_size) {
                hovering_state = hovering_end;
            }
            else {
                hovering_state = hovering_center;
            }
        }
    }

    switch(current_state) {
    case state_default:
        if(!playing) {
            stop_all_notes();
        }
        if(IsMouseClicked(0)){
            if(try_place_note(r, snap_c, snap_c + grid_note_length, node_s)) {
                if(!playing)  play_note(r);
                start_moving_note(r, c, node_s);
                return true;
            }
        }
        return false;
    case state_resizing_start:
        SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        resize_start(c);
        return false;
    case state_resizing_end:
        SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        resize_end(c);
        return false;
    case state_moving:
        return try_move_note(r, c);
    case state_hovering:
        if(IsMouseDown(1)) {
            erase_note(r, c, node, prev);
            current_state = state_default;
            return true;
        }
        switch(hovering_state) {
        case hovering_center:
            SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            start_moving_note(r, c, prev);
            return false;
        case hovering_start:
            SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if(IsMouseClicked(0)) {
                previous_node = prev;
                current_state = state_resizing_start;
            }
            return false;
        case hovering_end:
            SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if(IsMouseClicked(0)) {
                previous_node = prev;
                current_state = state_resizing_end; 
            }
            return false;
        }
    }
	return false;
}