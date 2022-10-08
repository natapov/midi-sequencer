#include "config.h"

typedef struct Node{
	int col = -1;
	struct Node* next = NULL;
	int len = -1;
}Node;

enum {
	_RETURNED_NODE_IS_RNODE,
	_PLACED_NEW_NOTE,
};

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

//on not placement, or if there is aleady a note accupying this place this function will return a pointer to the node before it on the column list
Node* try_place_note(int r, int c, int len, bool& placed_new_note) {
	placed_new_note = false;
	//todo: if(r+c > sizeofgird)  return false
	
	for(Node* cur = &row[r];; cur = cur->next){
		if(!cur->next) {
			Node* new_c = init_node(c, len, NULL);
			cur->next = new_c;
			placed_new_note = true;
			return cur;
		}
		const Node* next = cur->next;
		
		if(c <= next->col) {
			if(c + len <= next->col) {
				Node* new_c = init_node(c, len, cur->next);
				cur->next = new_c;
				placed_new_note = true;
				return cur;
			}
			return NULL;
		}
		else if(next->col + next->len > c) {
			return cur;
		}
	}
	assert(false); //why are we here?
}