typedef struct Cnode{
	int col_num = -1;
	int len = -1;
	struct Cnode* next = NULL;
}

typedef struct Rnode{
	int row_num = -1;
	Cnode* first_cnode = NULL;
	struct Rnode* next = NULL;
}Rnode;

Rnode* trunk = NULL;

int drawn_notes[12]; //The number of notes of each type currently on the grid
int number_of_drawn_notes = 0;

//return -1 on fail
int find(Rnode* first, int r, int c){
	Cnode* first_cnode = NULL;
	for(Rnode* cur = first; cur != NULL; cur = cur->next){
		if(cur->row_num == r){
			first_cnode = cur;
			break;
		}
	}
	if(!first_cnode)  return -1;

	int val = -1;
	for(Cnode* cur = first_cnode; cur != NULL; cur = cur->next){
		if(cur->col_num == c){
			val = cur->value;
			return val;
		}
	}
	return -1;
}

/*
//return true on success
bool place_note(Rnode* trunk, int r, int val){
	assert(trunk->row_num == -1);
	Cnode* first_cnode = NULL;
	Rnode* last = NULL;
	for(Rnode* cur = trunk; cur != NULL; cur = cur->next) {
		if(cur->row_num == r) {
			first_cnode = cur->first_cnode;
		}
		else if(cur->next == NULL || cur->next->row_num >= r) {
			Cnode* new_c = init_cnode(c, val, NULL);
			Rnode* new_r = init_rnode(r, new_c, cur->next);
			curr->next = new_r;
			return true;	
		}
	}
	assert(first_cnode);
	
	for(Cnode* cur = first_cnode; cur != NULL; cur = cur->next){
		if(cur->col_num == c){
			return false;
		}
	}
	return -1;
}
*/
Cnode* init_cnode(int num, int len, Cnode* next = NULL) {
	Cnode* ret = malloc(sizeof(Cnode));
	assert(ret);
	ret->col_num = num;
	ret->len = len;
	ret->next = next;
	return ret;
}

Rnode* init_rnode(int num = -1, Cnode* first_cnode, Rnode* next = NULL) {
	Rnode* ret = malloc(sizeof(Rnode));
	assert(ret);
	ret->row_num = num;
	ret->first_cnode = first_cnode;
	ret->next = next;
	return ret;
}

bool place_note(Rnode* trunk, int r, int c, int len){
	assert(trunk->row_num == -1);
	//todo: if(r+c > sizeofgird)  return false;
	
	Cnode* first_cnode = NULL;
	Rnode* last = NULL;
	for(Rnode* cur = trunk; cur != NULL; cur = cur->next) {
		if(cur->row_num == r) {
			first_cnode = cur->first_cnode;
		}
		else if(cur->next == NULL || cur->next->row_num >= r) {
			Cnode* new_c = init_cnode(c, len, NULL);
			Rnode* new_r = init_rnode(r, new_c, cur->next);
			cur->next = new_r;
			return true;	
		}
	}
	assert(first_cnode);
	
	for(Cnode* cur = first_cnode; cur != NULL; cur = cur->next){
		if(cur->col_num == c){
			return false;
		}
		else if(cur->next == NULL) {
			Cnode* new_c = init_cnode(c, len, NULL);
			cur->next = new_c;
		}
		int next_note_start = cur->next->col_num;
		else if(next_note_start >= c && c + len < next_note_start) {
			Cnode* new_c = init_cnode(c, len, cur->next);
			cur->next = new_c;
			return true;
		}
		else return false;
	}
	assert(false); //why are we here?
	return false;
}