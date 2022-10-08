typedef struct Cnode{
	int col_num = -1;
	int len = -1;
	struct Cnode* next = NULL;
}Cnode;

typedef struct Rnode{
	int row_num = -1;
	Cnode* first_cnode = NULL;
	struct Rnode* next = NULL;
}Rnode;

int max_col_num; //todo
Rnode trunk_t = {-1, NULL, NULL};
Rnode* trunk = &trunk_t;

int drawn_notes[12]; //The number of notes of each type currently on the grid
int number_of_drawn_notes = 0;

Cnode* init_cnode(int num, int len, Cnode* next = NULL) {
	Cnode* ret = (Cnode*) malloc(sizeof(Cnode));
	assert(ret);
	ret->col_num = num;
	ret->len = len;
	ret->next = next;
	return ret;
}

Rnode* init_rnode(int num, Cnode* first_cnode, Rnode* next = NULL) {
	Rnode* ret = (Rnode*) malloc(sizeof(Rnode));
	assert(ret);
	ret->row_num = num;
	ret->first_cnode = first_cnode;
	ret->next = next;
	return ret;
}
bool try_erase_note(int r, int c) {
	assert(trunk&&trunk->row_num == -1);
	//todo: if(r+c > sizeofgird)  return false;
	
	Cnode* first_cnode = NULL;
	Rnode* last_rnode = NULL;
	Rnode* cur_r;
	for(cur_r = trunk;; cur_r = cur_r->next) {
		if(cur_r->row_num == r) {
			first_cnode = cur_r->first_cnode;
			break;
		}
		if(cur_r->next == NULL || cur_r->next->row_num > r) {
			return false;	
		}
		last_rnode = cur_r;
	}
	assert(first_cnode);

	Cnode* last = NULL;
	for(Cnode* cur = first_cnode;; cur = cur->next){
		if(cur->next == NULL || cur->next->col_num > c) {
			if(c >= cur->col_num && c < cur->col_num + cur->len) {
				if(cur == first_cnode) {	
					if(cur->next == NULL) {
						free(cur);
						last_rnode->next = cur_r->next;
						free(cur_r);
						return true;
					}
					else {
						cur_r->first_cnode = cur->next;
						free(cur);
						return true;
					}
				}
				assert(last);
				last->next = cur->next;
				free(cur);
				return true;
			}
			return false;
		}
		last = cur;
	}
	assert(false); //why are we here?
	return false;

}
bool try_place_note(int r, int c, int len) {
	assert(trunk&&trunk->row_num == -1);
	//todo: if(r+c > sizeofgird)  return false;
	Cnode* first_cnode = NULL;
	for(Rnode* cur = trunk;; cur = cur->next) {
		if(cur->row_num == r) {
			first_cnode = cur->first_cnode;
			break;
		}
		if(cur->next == NULL || cur->next->row_num > r) {
			assert(!first_cnode);
			Cnode* new_c = init_cnode(c, len, NULL);
			Rnode* new_r = init_rnode(r, new_c, cur->next);
			cur->next = new_r;
			return true;	
		}
	}
	assert(first_cnode);
	
	for(Cnode* cur = first_cnode;; cur = cur->next){
		if(cur->col_num == c){
			return false;
		}
		if(cur->next == NULL) {
			Cnode* new_c = init_cnode(c, len, NULL);
			cur->next = new_c;
			return true;
		}
		int next_note_start = cur->next->col_num;
		if(next_note_start >= c) {
			if(c + len <= next_note_start) {
				Cnode* new_c = init_cnode(c, len, cur->next);
				cur->next = new_c;
				return true;
			}
			return false;
		}
	}
	assert(false); //why are we here?
	return false;
}