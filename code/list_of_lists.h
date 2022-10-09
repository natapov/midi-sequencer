#include "config.h"

typedef struct Node{
	int col;
	struct Node* next;
	int len;
}Node;

Node* moving_node_prev = NULL;
int moving_node_offset = -1;

int max_col; //todo

int drawn_notes[12]; //The number of notes of each type currently on the grid
int number_of_drawn_notes = 0;

Node row[CELL_GRID_NUM_H];

Node* init_node(int num, int len, Node* next = NULL) {
	Node* ret = (Node*) malloc(sizeof(Node));
	assert(ret);
	ret->col = num;
	ret->len = len;
	ret->next = next;
	return ret;
}

//bool try_move_note() 
bool try_erase_note(int r, int c) {
	Node* last = &row[r];
	for(Node* cur = row[r].next; cur != NULL; cur = cur->next){
		if(!cur->next || cur->next->col > c) {
			if(c >= cur->col && c < cur->col + cur->len) {
				last->next = cur->next;
				free(cur);
				return true;
			}
			return false;
		}
		last = cur;
	}
	assert(false); //why are we here?
}

bool try_place_note(int r, int c, int len) {

	//todo: if(r+c > sizeofgird)  return false
	const int original_c = c;
	c = snap_to_grid ? snap(c) : c;
	
	for(Node* cur = &row[r];; cur = cur->next){
		const Node* next = cur->next;
		if(!cur->next || c + len <= next->col) {
			Node* new_n = init_node(c, len, cur->next);
			assert(new_n->next != new_n);
			cur->next = new_n;
			moving_node_prev = cur;
			moving_node_offset = original_c - c;
			assert(moving_node_offset >= 0);
			return true;
		}
		if(original_c >= next->col && original_c < next->col + next->len) {
			moving_node_prev = cur;
			moving_node_offset = original_c - next->col;
			assert(moving_node_offset >= 0);
			return false;
		}
		if(c <= next->col) {
			moving_node_prev = NULL;
			return false;
		}
	}
	assert(false); //why are we here?
}
	
bool try_move_note(int r, int c) {
	Node* moving_node = moving_node_prev->next;
	assert(moving_node);
	const int len = moving_node->len;
	assert(moving_node_offset >= 0);
	c = snap_to_grid ? snap(c) : c - moving_node_offset;
	
	moving_node_prev->next = moving_node->next;

	for(Node* cur = &row[r];; cur = cur->next){
		if(!cur->next || c + len <= next->col) {
			moving_node->next = cur->next;
			moving_node->col = c;
			moving_node_prev = cur;
			next->next = moving_node;
			return true;
		}
		
		if(c < next->col + next->len) {
			break;
		}

	}
	assert(moving_node->next == moving_node_prev->next);
	assert(moving_node->next != moving_node);
	moving_node_prev->next = moving_node;
	assert(moving_node_prev->next != moving_node_prev);

	return false;
}